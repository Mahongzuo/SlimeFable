/*
* Copyright (c) 2022 - 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
* NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
* property and proprietary rights in and to this material, related
* documentation and any modifications thereto. Any use, reproduction,
* disclosure or distribution of this material and related documentation
* without an express license agreement from NVIDIA CORPORATION or
* its affiliates is strictly prohibited.
*/

#include "StreamlineViewExtension.h"
#include "StreamlineShaders.h"
#include "StreamlineCorePrivate.h"
#include "StreamlineDLSSG.h"
#include "StreamlineDLSSGCustomPresent.h"
#include "StreamlineDeepDVC.h"
#include "StreamlineRHI.h"
#include "StreamlineAPI.h"
#include "StreamlineDXGISwapchainProxy.h"

#include "ClearQuad.h"
#include "Engine/GameViewportClient.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Runtime/Engine/Classes/GameFramework/PlayerController.h"
#include "Runtime/Engine/Classes/GameFramework/Pawn.h"
#include "SceneRendering.h"
#include "SceneView.h"
#include "SceneTextureParameters.h"
#include "SystemTextures.h"
#include "VelocityCombinePass.h"
#include "sl_helpers.h"
#include "sl_dlss_g.h"
#define LOCTEXT_NAMESPACE "FStreamlineViewExtension"

#if defined (ENGINE_STREAMLINE_VERSION) && (ENGINE_STREAMLINE_VERSION >= 1)
#define ENGINE_SUPPORTS_CLEARQUADALPHA 1
#else
#define ENGINE_SUPPORTS_CLEARQUADALPHA ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 2))
#endif

#ifndef SUPPORT_GUIDE_GBUFFER
#define SUPPORT_GUIDE_GBUFFER 0
#endif

static TAutoConsoleVariable<bool> CVarStreamlineTagSceneColorWithoutHUD(
	TEXT("r.Streamline.TagSceneColorWithoutHUD"),
	true,
	TEXT("Pass scene color without HUD into DLSS Frame Generation (default = true)\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<bool> CVarStreamlineTagEditorSceneColorWithoutHUD(
	TEXT("r.Streamline.Editor.TagSceneColorWithoutHUD"),
	true,
	TEXT("Pass scene color without HUD into DLSS Frame Generation in the editor (default = true)\n"),
	ECVF_RenderThreadSafe);


static TAutoConsoleVariable<bool> CVarStreamlineTagVelocities(
	TEXT("r.Streamline.TagVelocities"),
	true,
	TEXT("Pass motion vectors into Streamline  (default = true)\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<bool> CVarStreamlineForceTagging(
	TEXT("r.Streamline.ForceTagging"),
	false,
	TEXT("Force tagging Streamline resources even if they are not required based on active Streamline features (default = false)\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarStreamlineDilateMotionVectors(
	TEXT("r.Streamline.DilateMotionVectors"),
	0,
	TEXT(" 0: pass low resolution motion vectors into DLSS Frame Generation (default)\n")
	TEXT(" 1: pass dilated high resolution motion vectors into DLSS Frame Generation. This can help with improving image quality of thin details."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarStreamlineMotionVectorScale(
	TEXT("r.Streamline.MotionVectorScale"),
	1.0f,
	TEXT("Scale DLSS Frame Generation motion vectors by this constant, in addition to the scale by 1/ the view rect size. (default = 1)\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarStreamlineCustomCameraNearPlane(
	TEXT("r.Streamline.CustomCameraNearPlane"),
	0.01f,
	TEXT("Custom distance to camera near plane. Used for internal DLSS Frame Generation purposes, does not need to match corresponding value used by engine. (default = 0.01f)\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarStreamlineCustomCameraFarPlane(
	TEXT("r.Streamline.CustomCameraFarPlane"),
	75000.0f,
	TEXT("Custom distance to camera far plane. Used for internal DLSS Frame Generation purposes, does not need to match corresponding value used by engine. (default = 75000.0f)\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarStreamlineViewIdOverride(
	TEXT("r.Streamline.ViewIdOverride"), -1,
	TEXT("Replace the view id passed into Streamline based on\n")
	TEXT("-1: Automatic, based on the state of r.Streamline.ViewIndexToTag (default)\n")
	TEXT("0: use ViewState.UniqueID \n")
	TEXT("1: overrride to 0 )\n"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarStreamlineViewIndexToTag(
	TEXT("r.Streamline.ViewIndexToTag"), -1,
	TEXT("Which view of a view family to tag\n")
	TEXT("-1: all views (default)\n")
	TEXT("0: first view\n")
	TEXT("1..n: nth view, typically up to 3 when having 4 player split screen view families\n"),
	ECVF_Default);

static TAutoConsoleVariable<bool> CVarStreamlineClearColorAlpha(
	TEXT("r.Streamline.ClearSceneColorAlpha"),
	true,
	TEXT("Clear alpha of scenecolor at the end of the Streamline view extension to allow subsequent UI drawcalls be represented correctly in the alpha channel (default = true)\n"),
	ECVF_RenderThreadSafe);

DEFINE_GPU_STAT(Streamline);
DECLARE_GPU_STAT(StreamlineDeepDVC);


int GetViewIndexToTag()
{
	return CVarStreamlineViewIndexToTag->GetInt();
}

bool NeedStreamlineViewIdOverride()
{
	if (CVarStreamlineViewIdOverride->GetInt() == -1)
	{
		return GetViewIndexToTag() != -1;
	}
	else
	{
		return CVarStreamlineViewIdOverride->GetInt() == 1;
	}
}

bool ForceTagStreamlineBuffers()
{
	static bool bStreamlineForceTagging = FParse::Param(FCommandLine::Get(), TEXT("slforcetagging"));
	return bStreamlineForceTagging || CVarStreamlineForceTagging.GetValueOnAnyThread();
}

bool ShouldTagStreamlineBuffers()
{
	return ForceTagStreamlineBuffers() || IsDLSSGActive();
}

static FViewportRHIRef FindGameRHIViewport()
{
	// somewhat inspired by UE::DisplayCluster::Projection::EasyBlend::FindRHIViewport
	FViewportRHIRef ViewportRHI{};
	if (GEngine && GEngine->GameViewport)
	{
#if UE_VERSION_OLDER_THAN(5,8,0)
		if (GEngine->GameViewport->Viewport)
		{
			ViewportRHI = GEngine->GameViewport->Viewport->GetViewportRHI();
		}
#endif
		if (!ViewportRHI)
		{
			// since that didn't work, try slate instead
			FSceneViewport* SceneViewport = GEngine->GameViewport->GetGameViewport();
			if (SceneViewport)
			{
				TSharedPtr<SWindow> Window = SceneViewport->FindWindow();
				if (Window)
				{
					FSlateRenderer* SlateRenderer = FSlateApplication::Get().GetRenderer();
					void* ViewportResource = SlateRenderer->GetViewportResource(*Window);
#if !UE_VERSION_OLDER_THAN(5,8,0)
					ViewportRHI = static_cast<FRHIViewport*>(ViewportResource);
#else
					ViewportRHI = *((FViewportRHIRef*)ViewportResource);
#endif
				}
			}
		}
	}
	return ViewportRHI;
}

FStreamlineViewExtension::FStreamlineViewExtension(const FAutoRegister& AutoRegister, FStreamlineRHI* InStreamlineRHIExtensions)
	: FSceneViewExtensionBase(AutoRegister)
	, StreamlineRHIExtensions(InStreamlineRHIExtensions)
{
	UE_LOG(LogStreamline, Log, TEXT("%s Enter %s"), ANSI_TO_TCHAR(__FUNCTION__), *CurrentThreadName());
	check(StreamlineRHIExtensions);
	FSceneViewExtensionIsActiveFunctor IsActiveFunctor;
	IsActiveFunctor.IsActiveFunction = [this](const ISceneViewExtension* SceneViewExtension, const FSceneViewExtensionContext& Context)
	{
		return StreamlineRHIExtensions->IsStreamlineAvailable();
	};

	IsActiveThisFrameFunctions.Add(IsActiveFunctor);
	if (!FStreamlineDXGISwapChainProxy::IsEnabled())
	{
		checkf(UE_VERSION_OLDER_THAN(5, 8, 0) == true, TEXT("Slate callbacks for viewport tracking/ are not supported for UE 5.8+"));

		// Those Slate callbacks are gone in 5.8+
#if UE_VERSION_OLDER_THAN(5,8,0)
		check(FSlateApplication::IsInitialized());

		FSlateRenderer* SlateRenderer = FSlateApplication::Get().GetRenderer();
		OnPreResizeWindowBackBufferHandle = SlateRenderer->OnPreResizeWindowBackBuffer().AddRaw(this, &FStreamlineViewExtension::UntrackViewsForBackbuffer);

		OnSlateWindowDestroyedHandle = FSlateApplication::Get().GetRenderer()->OnSlateWindowDestroyed().AddLambda(
			[this](void* InViewport)
		{
			FViewportRHIRef ViewportReference = *(FViewportRHIRef*)InViewport;
			void* NativeSwapchain = ViewportReference->GetNativeSwapChain();
			StreamlineRHIExtensions->OnSwapchainDestroyed(NativeSwapchain);
		});

		// ShutdownModule is too late for this
		FSlateApplication::Get().OnPreShutdown().AddLambda(
			[]()
		{
			FSlateRenderer* SlateRenderer = FSlateApplication::Get().GetRenderer();
			check(SlateRenderer);

			UE_LOG(LogStreamline, Log, TEXT("Unregistering of OnPreResizeWindowBackBuffer callback during FSlateApplication::OnPreShutdown"));
			SlateRenderer->OnPreResizeWindowBackBuffer().Remove(OnPreResizeWindowBackBufferHandle);

			UE_LOG(LogStreamline, Log, TEXT("Unregistering of OnSlateWindowDestroyed callback during FSlateApplication::OnPreShutdown"));
			SlateRenderer->OnSlateWindowDestroyed().Remove(OnSlateWindowDestroyedHandle);
		}
		);
#endif
	}
#if DEBUG_STREAMLINE_VIEW_TRACKING
	if (FParse::Param(FCommandLine::Get(), TEXT("sllogviewtracking")))
	{
		bLogStreamlineLogTrackedViews = true;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("slnologviewtracking")))
	{
		bLogStreamlineLogTrackedViews = false;
	}
#endif

	if (IsStreamlineDLSSGSupported() && FStreamlineDXGISwapChainProxy::IsEnabled())
	{
		SLCustomPresent = new FStreamlineDLSSGCustomPresent();
		SLCustomPresent->AddRef();
	}

	UE_LOG(LogStreamline, Log, TEXT("%s Leave %s"), ANSI_TO_TCHAR(__FUNCTION__), *CurrentThreadName());
}

FStreamlineViewExtension::~FStreamlineViewExtension()
{
	UE_LOG(LogStreamline, Log, TEXT("%s Enter %s"), ANSI_TO_TCHAR(__FUNCTION__), *CurrentThreadName());

	// remove our custom present handler if necessary
	if (SLCustomPresent)
	{
		FViewportRHIRef ViewportRHI = FindGameRHIViewport();
		if (ViewportRHI && (ViewportRHI->GetCustomPresent() == SLCustomPresent))
		{
			ViewportRHI->SetCustomPresent(nullptr);
		}
		// FStreamlineDLSSGCustomPresent is an FRHIResource so it will eventually be cleaned up by DeleteResources()
		SLCustomPresent->Release();
	}

	if (!TrackedViews.IsEmpty())
	{
		FStreamlineViewExtension::LogTrackedViews(*FString::Printf(TEXT("%s Stale Views %s"), ANSI_TO_TCHAR(__FUNCTION__), *CurrentThreadName()));
	}

	UE_LOG(LogStreamline, Log, TEXT("%s Leave %s"), ANSI_TO_TCHAR(__FUNCTION__), *CurrentThreadName());
}

void FStreamlineViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FStreamlineViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
}

void FStreamlineViewExtension::SetupViewPoint(APlayerController* Player, FMinimalViewInfo& InViewInfo)
{
}

void FStreamlineViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	if (IsDLSSGActive())
	{
		if (SLCustomPresent)
		{
			// Set CustomPresent if it's not already set.
			FViewportRHIRef ViewportRHI = FindGameRHIViewport();
			if (ViewportRHI)
			{
				FRHICustomPresent* CustomPresent = ViewportRHI->GetCustomPresent();
				if (CustomPresent != SLCustomPresent)
				{
					if (CustomPresent != nullptr)
					{
						// Other plugins may also set a custom present, for example nDisplay and XR related plugins. FG is incompatible with these.
						UE_LOG(LogStreamline, Warning, TEXT("Overriding someone else's custom present for DLSS-FG"));
					}
					ViewportRHI->SetCustomPresent(SLCustomPresent);
				}
			}
		}

		BeginRenderViewFamilyDLSSG(InViewFamily);
	}
}


#define FIVE_FOUR_PLUS_RDG_VALIDATION_WORKAROUND (RDG_ENABLE_DEBUG && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)

void FStreamlineViewExtension::PreRenderViewFamily_RenderThread(FGraphBuilderOrCmdList& GraphBuilderOrCmd, FSceneViewFamily& InViewFamily)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
	// UE 5.4 shipped with a bug that will cause RDG validation errors in game if a view extension subscribes to EPostProcessingPass::VisualizeDepthOfField (and others)
	// In the editor, the engine renders int a "BufferedRT" (created with the SRV flag) and then blits that to "ViewFamily" texture, which is the swapchain dummy backbuffer (that doesn't have that flag set)
	// In game mode (-game or packaged) hovever there is no "BufferedRT" and the Scenecolor is the "ViewFamily" texture/dummy swapchain backbuffer, so RDG validation catches that when the engine  is preparing 
	// the inputs for the sceneview exten postprocessing passes.
	// We fix up the texture flags here to prevent the validation error
	bool bDoRDGWorkaround = FIVE_FOUR_PLUS_RDG_VALIDATION_WORKAROUND;
	if (FParse::Param(FCommandLine::Get(), TEXT("slrdgworkaround")))
	{
		bDoRDGWorkaround = true;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("slnordgworkaround")))
	{
		bDoRDGWorkaround = false;
	}
	if (bDoRDGWorkaround)
	{
		if (const FRenderTarget* RenderTarget = InViewFamily.RenderTarget)
		{
			if (const FTextureRHIRef& Texture = RenderTarget->GetRenderTargetTexture())
			{
				FRHITextureDesc& ChangeIsTheOnlyConstant = const_cast<FRHITextureDesc&>(Texture->GetDesc());
				EnumAddFlags(ChangeIsTheOnlyConstant.Flags, ETextureCreateFlags::ShaderResource);
			}
		}
	}
#endif
	
	// we should be done with older frames so remove those frame ids
	TArray<uint32> StaleViews;
	TArray<uint32> ActiveViews;
	FramesWhereStreamlineConstantsWereSet.RemoveAllSwap([&StaleViews, &ActiveViews](TTuple<uint64, uint32>  Item)
	{
		const uint64 FrameCounterRenderThread = GFrameCounterRenderThread;
		// D3D12 RHI has this unaccessible static const uint32 WindowsDefaultNumBackBuffers = 3; so adding some slack 🤞
		constexpr uint64 MaxFramesInFlight = 3 + 2;
		// we add here since so we don't have to deal with subtracting uint64 and overflows
		bool bRemove = FrameCounterRenderThread > Item.Get<0>() + MaxFramesInFlight;
			
		if (bRemove)
		{
			StaleViews.AddUnique(Item.Get<1>());
		}
		else
		{
			ActiveViews.AddUnique(Item.Get<1>());
		}
		
		return bRemove;
	}
	);

	StaleViews.RemoveAllSwap([&ActiveViews](uint32 Item) { return ActiveViews.Contains(Item); });
	
	for (uint32 StaleView : StaleViews)
	{
		// an alternative to this could be to add "GetCommandListFromEither" function in the header...
#if ENGINE_MAJOR_VERSION == 4 
		GraphBuilderOrCmd.
#else
		GraphBuilderOrCmd.RHICmdList.
#endif
		EnqueueLambda([this, StaleView](FRHICommandList& Cmd)
		{
			UE_CLOG(DebugViewTracking(), LogStreamline, Log, TEXT("%s %s freeing resources for View Id %u"), ANSI_TO_TCHAR(__FUNCTION__), *CurrentThreadName(), StaleView);
			StreamlineRHIExtensions->ReleaseStreamlineResourcesForAllFeatures(StaleView);
		});
	}
}

void FStreamlineViewExtension::PreRenderView_RenderThread(FGraphBuilderOrCmdList&, FSceneView& InView)
{
}

void FStreamlineViewExtension::PostRenderView_RenderThread(FGraphBuilderOrCmdList&, FSceneView& InView)
{
}

void FStreamlineViewExtension::PostRenderViewFamily_RenderThread(FGraphBuilderOrCmdList&, FSceneViewFamily& InViewFamily)
{
}

void AddStreamlineUIHintTagPass(
	FRDGBuilder& GraphBuilder,
	bool bTagBackbuffer,
	bool bTagUIColorAlpha,
	const FIntPoint &BackBufferDimension,
	FSLUIHintTagShaderParameters* PassParameters,
	uint32 ViewId,
	FStreamlineRHI* RHIExtensions,
	TArray<FTrackedView>& ViewsInThisBackBuffer,
	const FIntRect &WindowClientAreaRect,
	bool HasViewIdOverride
)
{
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Streamline Tag {Backbuffer=%u UIColorAndAlpha=%u} NumViews=%u  WindowClient%dx%d [%d,%d -> %d,%d] Texture=%s",
			bTagBackbuffer, bTagUIColorAlpha,
			ViewsInThisBackBuffer.Num(),
			WindowClientAreaRect.Width(), WindowClientAreaRect.Height(),
			WindowClientAreaRect.Min.X, WindowClientAreaRect.Min.Y,
			WindowClientAreaRect.Max.X, WindowClientAreaRect.Max.Y
			,  *BackBufferDimension.ToString()),
		PassParameters,
		ERDGPassFlags::Raster | ERDGPassFlags::Compute | ERDGPassFlags::Copy
		| ERDGPassFlags::NeverCull | ERDGPassFlags::NeverMerge | ERDGPassFlags::SkipRenderPass,
		[RHIExtensions, bTagBackbuffer, bTagUIColorAlpha, PassParameters, WindowClientAreaRect, ViewsInThisBackBuffer, HasViewIdOverride](FRHICommandListImmediate& RHICmdList) mutable
		{
			for (const FTrackedView& View : ViewsInThisBackBuffer)
			{
				TArray<FRHIStreamlineResource, TInlineAllocator<2>> TexturesToTagOrUntag;
				
				check(!!PassParameters->BackBuffer == bTagBackbuffer);
				TexturesToTagOrUntag.Add(FRHIStreamlineResource::FromRDGTextureAccess(PassParameters->BackBuffer, View.UnscaledViewRect, EStreamlineResource::Backbuffer));
				if (bTagBackbuffer)
				{
					check(PassParameters->BackBuffer);
					PassParameters->BackBuffer->MarkResourceAsUsed();
				}

				check(!!PassParameters->UIColorAndAlpha == bTagUIColorAlpha);
				TexturesToTagOrUntag.Add(FRHIStreamlineResource::FromRDGTextureAccess(PassParameters->UIColorAndAlpha, View.UnscaledViewRect, EStreamlineResource::UIColorAndAlpha));
				if (bTagUIColorAlpha)
				{
					check(PassParameters->UIColorAndAlpha);
					PassParameters->UIColorAndAlpha->MarkResourceAsUsed();
				}

				const uint32 ViewID = HasViewIdOverride ? 0 : View.ViewKey;
				const uint64 LocalGFrameCounter = GFrameCounterRenderThread;

#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
				if (RHIExtensions->NeedExtraPassesForDebugLayerCompatibility())
				{
					DebugLayerCompatibilityRHISetup(PassParameters->DebugLayerCompatibility, TexturesToTagOrUntag);
				}
#endif
				RHICmdList.EnqueueLambda(
					[RHIExtensions, TexturesToTagOrUntag, ViewID, LocalGFrameCounter](FRHICommandListImmediate& Cmd) mutable
				{
						sl::FrameToken* FrameToken = FStreamlineCoreModule::GetStreamlineRHI()->GetFrameToken(LocalGFrameCounter);
						RHIExtensions->TagTextures(Cmd, ViewID, *FrameToken, TexturesToTagOrUntag);
				});
			}
	});
	
	GraphBuilder.Execute();
}


void FStreamlineViewExtension::SubscribeToPostProcessingPass(
	EPostProcessingPass Pass
#if !UE_VERSION_OLDER_THAN(5,5,0)	
	, const FSceneView& InView
#endif
	,FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	if (Pass == EPostProcessingPass::VisualizeDepthOfField)
	{
		check(StreamlineRHIExtensions);
		check(StreamlineRHIExtensions->IsStreamlineAvailable());
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FStreamlineViewExtension::PostProcessPassAtEnd_RenderThread));
	}
}


/*

At the Streamline SDK level when we pass in the D3D resource into the SL SDK we
also need to pass in the *exact* D3D resource state as well.

In 5.6+ RDG optimizes and batches resource state transitions across RDG passes to avoid 
redundant read to read transitions. This means the actual D3D resource state in the RHI lambda
inside the RDG lambda might have *more* D3D resource state bit set than what the RDG shader parameter
struct ERHIAccess is describing.

This is a particular issue for Depth/SceneDepthZ that is used across the frame before us
so RDG ends up adding |DEPTH_READ | NON_PIXEL_SHADER_RESOURCE | PIXEL_SHADER_RESOURCE to the
COPY_SOURCE bit that we actually want, in a non-deterministic way. So we explicitely add  
| ERHIAccess::DSVRead | ERHIAccess::SRVMask) to Depth so we then can reliably in side our StreamlineD3D12RHI
translate ERRHIAccess

Additionally SceneDepthZ has two different subresources (depth plane and stenci plane) that are
in different resource states, eg. the stencil plane can be in depth write state whereas the depth plane 
might be in depth read. However the SL SDK API does not allow per sub-resource state so we need to
have the RDG/RHI transition both subresources into the same state.

Other SL inpout resources like Velocity or SceneColorWithoutHUD don't tend to run into those issues since they are
created by a UE plugin side renderpass so there are no other passes with more complicated resource state
transitions involved. 

Note: We are using COPY_SOURCE as SL input resources since we tag them with sl::ResourceLifecycle::eOnlyValidNow 
which means SL will make a copy of every input resource and if needed transition to and from COPY_SOURCE 
In SL as of version 2.8 those COPY_SRC and back transitions are not batched up so when we pass in 4 resources
at once, SL will make 8 individual resource barrier calls if the resource does not have the COPY_SRC bit set.
To avoid those extra transitions we are using the RDG/RHI to put the resources into ERHIAccess::CopySrc/COPY_SOURCE state.



*/
BEGIN_SHADER_PARAMETER_STRUCT(FSLShaderParameters, )
RDG_TEXTURE_ACCESS(Depth, ERHIAccess::CopySrc | ERHIAccess::DSVRead | ERHIAccess::SRVMask)
RDG_TEXTURE_ACCESS(Velocity, ERHIAccess::CopySrc)
RDG_TEXTURE_ACCESS(SceneColorWithoutHUD, ERHIAccess::CopySrc)

#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
SHADER_PARAMETER_STRUCT_INCLUDE(FDebugLayerCompatibilityShaderParameters, DebugLayerCompatibility)
#endif
END_SHADER_PARAMETER_STRUCT()

FScreenPassTexture FStreamlineViewExtension::PostProcessPassAtEnd_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& InOutInputs)
{
	check(IsInRenderingThread());
	check(View.bIsViewInfo);

	if (ShouldTrackViews())
	{
		AddTrackedView(View);
	}

	const int ViewIndexToTag = GetViewIndexToTag();
	const bool bTagAllViews = -1 == GetViewIndexToTag();
	const bool bTagThisView = bTagAllViews || (ViewIndexToTag == GetViewIndex(&View));

	if (FramesWhereStreamlineConstantsWereSet.Contains( MakeTuple(GFrameCounterRenderThread, View.GetViewKey())) || !bTagThisView || !IsProperGraphicsView(View))
	{

#if DEBUG_STREAMLINE_VIEW_TRACKING
		if (DebugViewTracking())
		{
			if (FramesWhereStreamlineConstantsWereSet.Contains(MakeTuple(GFrameCounterRenderThread, View.GetViewKey())))
			{
				FStreamlineViewExtension::LogTrackedViews(*FString::Printf(TEXT("%s return FramesWhereStreamlineConstantsWereSet.Contains(GFrameCounterRenderThread) Key=%u, %s"), ANSI_TO_TCHAR(__FUNCTION__), View.GetViewKey(), *CurrentThreadName()));
			}
			LogViewNotTrackedReason(ANSI_TO_TCHAR(__FUNCTION__), View);
		}
#endif

		// no point in running DLSS-FG for scene captures if the engine can't use the extra frames anyway. Just pass through the appropriate texture
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
		return InOutInputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
#else
		if (InOutInputs.OverrideOutput.IsValid())
		{
			return InOutInputs.OverrideOutput;
		}
		else
		{
			return InOutInputs.Textures[(uint32)EPostProcessMaterialInput::SceneColor];
		}
#endif
	}

	FramesWhereStreamlineConstantsWereSet.AddUnique(MakeTuple(GFrameCounterRenderThread, View.GetViewKey()));

	FStreamlineViewExtension::LogTrackedViews(*FString::Printf(TEXT("%s Key=%u, %s"), ANSI_TO_TCHAR(__FUNCTION__), View.GetViewKey(), *CurrentThreadName()));



	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
	const FScreenPassTexture SceneColor = FScreenPassTexture::CopyFromSlice(GraphBuilder, InOutInputs.GetInput(EPostProcessMaterialInput::SceneColor));
#else
	const FScreenPassTexture& SceneColor = InOutInputs.Textures[(uint32)EPostProcessMaterialInput::SceneColor];
#endif
	const uint32 ViewID = NeedStreamlineViewIdOverride() ? 0 : ViewInfo.GetViewKey();
	const uint64 FrameID = GFrameCounterRenderThread;
	const FIntRect ViewRect = ViewInfo.ViewRect;
	const FIntRect SecondaryViewRect = FIntRect(FIntPoint::ZeroValue, ViewInfo.GetSecondaryViewRectSize());

	NV_RDG_EVENT_SCOPE(GraphBuilder, Streamline, "Streamline ViewID=%u %dx%d [%d,%d -> %d,%d]",
		ViewID, ViewRect.Width(), ViewRect.Height(),
		ViewRect.Min.X, ViewRect.Min.Y,
		ViewRect.Max.X, ViewRect.Max.Y);
#if UE_VERSION_OLDER_THAN(5,8,0)
	RDG_GPU_STAT_SCOPE(GraphBuilder, Streamline);
#endif

	if (ShouldTagStreamlineBuffers())
	{
		const uint64 FrameNumber = GFrameNumberRenderThread;

#if ENGINE_MAJOR_VERSION == 4
		FSceneRenderTargets& SceneTextures = FSceneRenderTargets::Get(GraphBuilder.RHICmdList);
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 1
		FSceneTextures SceneTextures = FSceneTextures::Get(GraphBuilder);
#else
		FSceneTextures SceneTextures = ViewInfo.GetSceneTextures();
#endif

		// input color
		FRDGTextureRef SceneColorAfterTonemap = SceneColor.Texture;
		check(SceneColorAfterTonemap);

		// input motion vectors
#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)
		FRDGTextureRef SceneVelocity = FScreenPassTexture::CopyFromSlice(GraphBuilder, InOutInputs.GetInput(EPostProcessMaterialInput::Velocity)).Texture;
#else
		FRDGTextureRef SceneVelocity = InOutInputs.Textures[(uint32)EPostProcessMaterialInput::Velocity].Texture;
#endif
		if (!SceneVelocity)
		{
#if ENGINE_MAJOR_VERSION == 4
			SceneVelocity = GraphBuilder.RegisterExternalTexture(SceneTextures.SceneVelocity);
#else
			SceneVelocity = SceneTextures.Velocity;
#endif
		}

		//input depth
#if ENGINE_MAJOR_VERSION == 4
		FRDGTextureRef SceneDepth = GraphBuilder.RegisterExternalTexture(SceneTextures.SceneDepthZ);
#else
		FRDGTextureRef SceneDepth = SceneTextures.Depth.Resolve;
#endif
		check(SceneDepth);

#if SUPPORT_GUIDE_GBUFFER
		FRDGTextureRef AlternateMotionVector = SceneTextures.AlternateMotionVector;
#else
		FRDGTextureRef AlternateMotionVector = nullptr;
#endif

		FStreamlineRHI* LocalStreamlineRHIExtensions = this->StreamlineRHIExtensions;

		FSLShaderParameters* PassParameters = GraphBuilder.AllocParameters<FSLShaderParameters>();

		// Those are consumed directly by SL
		// FRDGTextureRef SLDepth = SceneDepth;

		// Those hold the outputs from various render passes that convert from engine textures into the SL specific formats
		FRDGTextureRef SLVelocity = nullptr;
		FRDGTextureRef SLSceneColorWithoutHUD = nullptr;

		const bool bTagSceneColorWithoutHUD = GIsEditor ? CVarStreamlineTagEditorSceneColorWithoutHUD.GetValueOnRenderThread() : CVarStreamlineTagSceneColorWithoutHUD.GetValueOnRenderThread();
		if (bTagSceneColorWithoutHUD)
		{
			FRDGTextureDesc Desc = SceneColor.Texture->Desc;
			EnumAddFlags(Desc.Flags, TexCreate_ShaderResource | TexCreate_UAV);
			EnumRemoveFlags(Desc.Flags, TexCreate_Presentable);
			EnumRemoveFlags(Desc.Flags, TexCreate_ResolveTargetable);
			SLSceneColorWithoutHUD = GraphBuilder.CreateTexture(Desc, TEXT("Streamline.SceneColorWithoutHUD"));
			AddDrawTexturePass(GraphBuilder, ViewInfo, SceneColor.Texture, SLSceneColorWithoutHUD, FIntPoint::ZeroValue, FIntPoint::ZeroValue, FIntPoint::ZeroValue);

			PassParameters->SceneColorWithoutHUD = SLSceneColorWithoutHUD;
		}

		const bool bTagMotionVectors = CVarStreamlineTagVelocities.GetValueOnRenderThread() != 0;
		const bool bDilateMotionVectors = CVarStreamlineDilateMotionVectors.GetValueOnRenderThread() != 0;
		
		if (bTagMotionVectors)
		{
			SLVelocity = AddStreamlineVelocityCombinePass(GraphBuilder, ViewInfo, SceneDepth, SceneVelocity, AlternateMotionVector, bDilateMotionVectors);
			PassParameters->Velocity = SLVelocity;
		}

		PassParameters->Depth = SceneDepth;


		FRHIStreamlineArguments StreamlineArguments = {};
		FMemory::Memzero(StreamlineArguments);

		StreamlineArguments.FrameId = FrameID;
		StreamlineArguments.ViewId = ViewID;

		// TODO STREAMLINE check for other conditions, similar to DLSS
		StreamlineArguments.bReset = View.bCameraCut;
		
		StreamlineArguments.bIsDepthInverted = true;

		StreamlineArguments.JitterOffset = { float(ViewInfo.TemporalJitterPixels.X), float(ViewInfo.TemporalJitterPixels.Y) }; // LWC_TODO: Precision loss

		StreamlineArguments.CameraNear = CVarStreamlineCustomCameraNearPlane.GetValueOnRenderThread();
		StreamlineArguments.CameraFar = CVarStreamlineCustomCameraFarPlane.GetValueOnRenderThread();
		StreamlineArguments.CameraFOV = ViewInfo.FOV;
		StreamlineArguments.CameraAspectRatio = float(ViewInfo.ViewRect.Width()) / float(ViewInfo.ViewRect.Height());
		const float MotionVectorScale = CVarStreamlineMotionVectorScale.GetValueOnRenderThread();
		if (bDilateMotionVectors)
		{
			StreamlineArguments.MotionVectorScale = { MotionVectorScale / ViewInfo.GetSecondaryViewRectSize().X, MotionVectorScale / ViewInfo.GetSecondaryViewRectSize().Y };
		}
		else
		{
			StreamlineArguments.MotionVectorScale = { MotionVectorScale / ViewInfo.ViewRect.Width() , MotionVectorScale / ViewInfo.ViewRect.Height() };
		}
		StreamlineArguments.bAreMotionVectorsDilated = bDilateMotionVectors;

		FViewUniformShaderParameters ViewUniformShaderParameters = *ViewInfo.CachedViewUniformShaderParameters;

		StreamlineArguments.bIsOrthographicProjection = !View.IsPerspectiveProjection();
		StreamlineArguments.ClipToCameraView = ViewUniformShaderParameters.ClipToView;
		StreamlineArguments.ClipToLenseClip = FRHIStreamlineArguments::FMatrix44f::Identity;
		StreamlineArguments.ClipToPrevClip = ViewUniformShaderParameters.ClipToPrevClip;
		StreamlineArguments.PrevClipToClip = ViewUniformShaderParameters.ClipToPrevClip.Inverse();

#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 4
		// TODO STREAMLINE : LWC_TODO verify that this works correctly with large world coordinates
		StreamlineArguments.CameraOrigin = ViewUniformShaderParameters.ViewOriginLow;
#else
		// TODO STREAMLINE : LWC_TODO verify that this works correctly with large world coordinates
		StreamlineArguments.CameraOrigin = ViewUniformShaderParameters.RelativeWorldCameraOrigin;
#endif
#else
		StreamlineArguments.CameraOrigin = ViewUniformShaderParameters.WorldCameraOrigin;
#endif
		StreamlineArguments.CameraUp = ViewUniformShaderParameters.ViewUp;
		StreamlineArguments.CameraRight = ViewUniformShaderParameters.ViewRight;
		StreamlineArguments.CameraForward = ViewUniformShaderParameters.ViewForward;
		StreamlineArguments.CameraViewToClip = ViewUniformShaderParameters.ViewToClip;

		StreamlineArguments.CameraPinholeOffset = FRHIStreamlineArguments::FVector2f::ZeroVector;
		
#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
		if (LocalStreamlineRHIExtensions->NeedExtraPassesForDebugLayerCompatibility())
		{
			AddDebugLayerCompatibilitySetupPasses(GraphBuilder, &PassParameters->DebugLayerCompatibility);
		}
#endif

		GraphBuilder.AddPass(
		RDG_EVENT_NAME("Streamline Common %dx%d FrameId=%u ViewID=%u", ViewRect.Width(), ViewRect.Height(), StreamlineArguments.FrameId, StreamlineArguments.ViewId),
			PassParameters,
			ERDGPassFlags::Raster | ERDGPassFlags::Compute | ERDGPassFlags::Copy
			| ERDGPassFlags::NeverCull | ERDGPassFlags::NeverMerge | ERDGPassFlags::SkipRenderPass,
			[LocalStreamlineRHIExtensions, PassParameters, StreamlineArguments, ViewRect, SecondaryViewRect, SceneColor, 
			bTagMotionVectors, bTagSceneColorWithoutHUD](FRHICommandListImmediate& RHICmdList) mutable
		{

			// first the constants
			RHICmdList.EnqueueLambda(
			[LocalStreamlineRHIExtensions, StreamlineArguments](FRHICommandListImmediate& Cmd) mutable
			{
				LocalStreamlineRHIExtensions->SetStreamlineData(Cmd, StreamlineArguments);
			});

			TArray<FRHIStreamlineResource, TInlineAllocator<4>> TexturesToTagOrUntag;
			check(PassParameters->Depth);
			PassParameters->Depth->MarkResourceAsUsed();
			TexturesToTagOrUntag.Add(FRHIStreamlineResource::FromRDGTextureAccess(PassParameters->Depth, ViewRect, EStreamlineResource::Depth));

			// motion vectors are in the top left corner after the Velocity Combine pass
			check(!!PassParameters->Velocity == bTagMotionVectors);
			TexturesToTagOrUntag.Add(FRHIStreamlineResource::FromRDGTextureAccess(PassParameters->Velocity, EStreamlineResource::MotionVectors));
			if (bTagMotionVectors)
			{
				check(PassParameters->Velocity)
				PassParameters->Velocity->MarkResourceAsUsed();
			}

			check(!!PassParameters->SceneColorWithoutHUD == bTagSceneColorWithoutHUD);
			TexturesToTagOrUntag.Add(FRHIStreamlineResource::FromRDGTextureAccess(PassParameters->SceneColorWithoutHUD, SceneColor.ViewRect, EStreamlineResource::HUDLessColor));
			if (bTagSceneColorWithoutHUD)
			{
				check(PassParameters->SceneColorWithoutHUD);
				PassParameters->SceneColorWithoutHUD->MarkResourceAsUsed();
			}

#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
			if (LocalStreamlineRHIExtensions->NeedExtraPassesForDebugLayerCompatibility())
			{
				DebugLayerCompatibilityRHISetup(PassParameters->DebugLayerCompatibility, TexturesToTagOrUntag);
			}
#endif
			// then tagging the resources
			const uint32 ViewId = StreamlineArguments.ViewId;
			const uint64 LocalGFrameCounter = GFrameCounterRenderThread;

			RHICmdList.EnqueueLambda(
			[LocalStreamlineRHIExtensions, ViewId, TexturesToTagOrUntag, LocalGFrameCounter](FRHICommandListImmediate& Cmd) mutable
			{
				sl::FrameToken* FrameToken = FStreamlineCoreModule::GetStreamlineRHI()->GetFrameToken(LocalGFrameCounter);
				LocalStreamlineRHIExtensions->TagTextures(Cmd, ViewId, *FrameToken, TexturesToTagOrUntag);
			});
		});

	}

	// this is always executed if DLSS-G is supported so we can turn DLSS-G off at the SL side (after we skipped the work above)
	if (IsStreamlineDLSSGSupported())
	{
		AddStreamlineDLSSGStateRenderPass(GraphBuilder, ViewID, SecondaryViewRect);
	}

	// DeepDVC render pass
	if(IsDeepDVCActive())
	{
		NV_RDG_EVENT_SCOPE(GraphBuilder, StreamlineDeepDVC, "Streamline DeepDVC %dx%d [%d,%d -> %d,%d]",
			SceneColor.ViewRect.Width(), SceneColor.ViewRect.Height(),
			SceneColor.ViewRect.Min.X, SceneColor.ViewRect.Min.Y,
			SceneColor.ViewRect.Max.X, SceneColor.ViewRect.Max.Y
		);
#if UE_VERSION_OLDER_THAN(5,8,0)
		RDG_GPU_STAT_SCOPE(GraphBuilder, StreamlineDeepDVC);
#endif
		// we wont need to run this always since (unlike FG) we skip the whole evaluate pass

		AddStreamlineDeepDVCStateRenderPass(GraphBuilder, ViewID, SecondaryViewRect);
		
		FRDGTextureRef SLSceneColorWithoutHUD = SceneColor.Texture;



		// This is still WIP:
		// 
		// DeepDVC is accessing the input/output resources as an UAV.
		// The scenecolor resource is not created by the engine with a ETextureCreateFlags::UAV
		// This is by the -d3ddebug layers 
		// D3D12 ERROR : ID3D12Device::CreateUnorderedAccessView : A UnorderedAccessView cannot be created of a Resource that did not specify the D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS Flag.[STATE_CREATION ERROR #340: CREATEUNORDEREDACCESSVIEW_INVALIDRESOURCE]
		// D3D12 : **BREAK** enabled for the previous message, which was : [ERROR STATE_CREATION #340: CREATEUNORDEREDACCESSVIEW_INVALIDRESOURCE]
		// To avoid that, wwe'll DeepDVC into an intermediate, UAV compatible resource and copy there & back again, like the good hobbits we are.
		// However when a Streamline swapchain provider is setup (say for DLSS-FG) we "know" (#yolo) that the the proxy backbuffer resources are "UAV compatible"
		// Then we can elide that copy

		//const bool bHasImplicitUAVCompatibilityViaStreamlneSwapChainProvider = StreamlineRHIExtensions->IsSwapchainProviderInstalled();
		const bool bHasImplicitUAVCompatibilityViaStreamlneSwapChainProvider = false;

		const bool bIsUAVCompatible = EnumHasAllFlags(SceneColor.Texture->Desc.Flags, TexCreate_UAV);
		const bool bNeedsCopies =!(bIsUAVCompatible || bHasImplicitUAVCompatibilityViaStreamlneSwapChainProvider);

		if (bNeedsCopies)
		{
			FRDGTextureDesc DeepDVCIntermediateDesc = SceneColor.Texture->Desc;
			EnumAddFlags(DeepDVCIntermediateDesc.Flags, TexCreate_ShaderResource | TexCreate_UAV);
			EnumRemoveFlags(DeepDVCIntermediateDesc.Flags, TexCreate_ResolveTargetable | TexCreate_Presentable);
			SLSceneColorWithoutHUD = GraphBuilder.CreateTexture(DeepDVCIntermediateDesc, TEXT("Streamline.SceneColorWithoutHUD.DeepDVC"));
			AddDrawTexturePass(GraphBuilder, ViewInfo, SceneColor.Texture, SLSceneColorWithoutHUD, FIntPoint::ZeroValue, FIntPoint::ZeroValue, FIntPoint::ZeroValue);
		}
		
		AddStreamlineDeepDVCEvaluateRenderPass(StreamlineRHIExtensions, GraphBuilder, ViewID, SceneColor.ViewRect, SLSceneColorWithoutHUD);
		
		if (bNeedsCopies)
		{
			AddDrawTexturePass(GraphBuilder, ViewInfo, SLSceneColorWithoutHUD, SceneColor.Texture, FIntPoint::ZeroValue, FIntPoint::ZeroValue, FIntPoint::ZeroValue);
		}
	}


#if ENGINE_SUPPORTS_CLEARQUADALPHA
	if (ShouldTagStreamlineBuffers() &&  CVarStreamlineClearColorAlpha.GetValueOnRenderThread())
	{
		auto* PassParameters = GraphBuilder.AllocParameters<FRenderTargetParameters>();
		PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneColor.Texture, ERenderTargetLoadAction::ENoAction);
		
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("ClearSceneColorAlpha"),
			PassParameters,
			ERDGPassFlags::Raster,
			[SecondaryViewRect](FRHICommandList& RHICmdList)
			{
				RHICmdList.SetViewport(SecondaryViewRect.Min.X, SecondaryViewRect.Min.Y, 0.0f, SecondaryViewRect.Max.X, SecondaryViewRect.Max.Y, 1.0f);
				DrawClearQuadAlpha(RHICmdList, 0.0f);
			});
	}
#else
#error "Engine missing DrawClearQuadAlpha support. Apply latest custom engine patch using instructions from DLSS-FG plugin quick start guide or README.md"
#endif

	if (InOutInputs.OverrideOutput.IsValid())
	{
		AddDrawTexturePass(GraphBuilder, ViewInfo, SceneColor, InOutInputs.OverrideOutput);
		return InOutInputs.OverrideOutput;
	}
	else
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
		return FScreenPassTexture::CopyFromSlice(GraphBuilder, InOutInputs.GetInput(EPostProcessMaterialInput::SceneColor));
#else
		return InOutInputs.Textures[(uint32)EPostProcessMaterialInput::SceneColor];
#endif
	}
}
#undef LOCTEXT_NAMESPACE
