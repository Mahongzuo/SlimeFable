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
#include "StreamlineCorePrivate.h"
#include "StreamlineDXGISwapchainProxy.h"

#include "SceneRendering.h"

#ifndef XR_WORKAROUND
#define XR_WORKAROUND 0
#endif

TArray<FTrackedView> FStreamlineViewExtension::TrackedViews;

#if DEBUG_STREAMLINE_VIEW_TRACKING
bool FStreamlineViewExtension::bLogStreamlineLogTrackedViews = false;
static FAutoConsoleVariableRef CVarStreamlineLogTrackedViews(
	TEXT("r.Streamline.LogTrackedViews"),
	FStreamlineViewExtension::bLogStreamlineLogTrackedViews,
	TEXT("Enable/disable whether to log which views & backbuffers are associated with each other at various parts of rendering. Most useful when developing & debugging multi view port multi window code. Can be overriden with -sl{no}logviewtracking\n"),
	ECVF_Default);
#endif

FDelegateHandle FStreamlineViewExtension::OnPreResizeWindowBackBufferHandle;
FDelegateHandle FStreamlineViewExtension::OnSlateWindowDestroyedHandle;


bool FStreamlineViewExtension::DebugViewTracking()
{
#if DEBUG_STREAMLINE_VIEW_TRACKING


	return bLogStreamlineLogTrackedViews && ShouldTrackViews();
#else
	return false;
#endif
}

void FStreamlineViewExtension::LogTrackedViews(const TCHAR* CallSite)
{
#if DEBUG_STREAMLINE_VIEW_TRACKING
	if (!DebugViewTracking())
	{
		return;
	}
	const FString ViewRectString = FString::JoinBy(TrackedViews, TEXT(", "), [](const FTrackedView& State)
	{
		FString TextureName = TEXT("Call me nobody");
		FString TextureDimensionAsString = TEXT("HerpxDerp");

		if (FRHITexture* Texture = State.Texture)
		{
			if (Texture && Texture->IsValid())
			{
				TextureName = FString::Printf(TEXT("%s %p"), *Texture->GetName().ToString(), Texture->GetTexture2D());
#if (ENGINE_MAJOR_VERSION  == 4) || ((ENGINE_MAJOR_VERSION  == 5) && (ENGINE_MINOR_VERSION < 1))
				TextureDimensionAsString = Texture->GetSizeXYZ().ToString();
#else
				TextureDimensionAsString = Texture->GetSizeXY().ToString();
#endif
			}
		}
		return FString::Printf(TEXT("%u %s (%ux%u) %s %s"), State.ViewKey, *State.ViewRect.ToString(), State.ViewRect.Width(), State.ViewRect.Height(), *TextureName, *TextureDimensionAsString);
	}
	);

	UE_LOG(LogStreamline, Log, TEXT("%2u# %s %s"), TrackedViews.Num(), CallSite, *ViewRectString);
#endif
}

// When editing this, please make sure to also update IsProperGraphicsView
void FStreamlineViewExtension::LogViewNotTrackedReason(const TCHAR* Callsite, const FSceneView& View)
{
	if (View.bIsSceneCapture)
	{
		FStreamlineViewExtension::LogTrackedViews(*FString::Printf(TEXT("%s return View.bIsSceneCapture Key=%u, %s"), Callsite, View.GetViewKey(), *CurrentThreadName()));
	}

	if (View.bIsOfflineRender)
	{
		FStreamlineViewExtension::LogTrackedViews(*FString::Printf(TEXT("%s return View.bIsOfflineRender Key=%u, %s"), Callsite, View.GetViewKey(), *CurrentThreadName()));
	}

	if (!View.bIsGameView)
	{
		FStreamlineViewExtension::LogTrackedViews(*FString::Printf(TEXT("%s return !View.bIsGameView Key=%u, %s"), Callsite, View.GetViewKey(), *CurrentThreadName()));
	}
#if !XR_WORKAROUND
	if (View.StereoPass != EStereoscopicPass::eSSP_FULL)
	{
		FStreamlineViewExtension::LogTrackedViews(*FString::Printf(TEXT("%s return View.StereoPass != EStereoscopicPass::eSSP_FULL Key=%u, %s"), Callsite, View.GetViewKey(), *CurrentThreadName()));
	}
#endif

}

// When editing this, please make sure to also update LogViewNotTrackedReason
const bool FStreamlineViewExtension::IsProperGraphicsView(const FSceneView& InView)
{
	if (InView.bIsSceneCapture)
	{
		return false;
	}

	// MRQ
	if (InView.bIsOfflineRender)
	{
		return false;
	}

	// TODO this might need work once we render FG in the main editor view
	if (!InView.bIsGameView)
	{
		return false;
	}

	//For vr rendering we disable FG
#if !XR_WORKAROUND
	if (InView.StereoPass != EStereoscopicPass::eSSP_FULL)
	{
		return false;
	}
#endif
	return true;
}


void FStreamlineViewExtension::AddTrackedView(const FSceneView& InView)
{
	check(InView.bIsViewInfo);
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(InView);

	const uint32 NewViewKey = InView.GetViewKey();
	if (!IsProperGraphicsView(InView))
	{
#if DEBUG_STREAMLINE_VIEW_TRACKING
		LogViewNotTrackedReason(ANSI_TO_TCHAR(__FUNCTION__), ViewInfo);
#endif
		return;
	}

	FTextureRHIRef TargetTexture = nullptr;

	if (const FRenderTarget* Target = InView.Family->RenderTarget; Target && Target->GetRenderTargetTexture().IsValid())
	{
		TargetTexture = Target->GetRenderTargetTexture();
	}
	else
	{
		// The only reason we'd be tracking a view is for FG and it needs the target texture
#if DEBUG_STREAMLINE_VIEW_TRACKING
		FStreamlineViewExtension::LogTrackedViews(*FString::Printf(TEXT("%s no render target texture found Key=%u, %s"),
			ANSI_TO_TCHAR(__FUNCTION__), InView.GetViewKey(), *CurrentThreadName()));
#endif
		return;
	}

	FTrackedView* FoundTrackedView = TrackedViews.FindByPredicate([NewViewKey](const FTrackedView& State) { return State.ViewKey == NewViewKey; });

	if (!FoundTrackedView)
	{
		TrackedViews.Emplace();
		FoundTrackedView = &TrackedViews.Last();
		FoundTrackedView->ViewKey = NewViewKey;
	}

	if (TargetTexture && TargetTexture->GetName() != TEXT("HitProxyTexture"))
	{
		const bool bIsExpectedRenderTarget  =
		 (    (TargetTexture->GetName() == TEXT("BufferedRT"))
			|| (TargetTexture->GetName() == TEXT("BackBuffer0"))
			|| (TargetTexture->GetName() == TEXT("BackBuffer1"))
			|| (TargetTexture->GetName() == TEXT("BackBuffer2"))
			|| (TargetTexture->GetName() == TEXT("BackbufferReference"))
			|| (TargetTexture->GetName() == TEXT("FD3D11Viewport::GetSwapChainSurface")) // (⊙_⊙)？
	#if XR_WORKAROUND
			|| (TargetTexture->GetName().ToString().Contains(TEXT("XRSwapChainBackingTex")))
	#endif
			|| (ENGINE_MAJOR_VERSION == 4)
			|| ((ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 1))
		);

		if (!bIsExpectedRenderTarget)
		{

			FString TextureDimensionAsString = TEXT("HerpxDerp");

			const FString TextureName = FString::Printf(TEXT("%s %p"), *TargetTexture->GetName().ToString(), TargetTexture->GetTexture2D());
#if (ENGINE_MAJOR_VERSION  == 4) || ((ENGINE_MAJOR_VERSION  == 5) && (ENGINE_MINOR_VERSION < 1))
			TextureDimensionAsString = TargetTexture->GetSizeXYZ().ToString();
#else
			TextureDimensionAsString = TargetTexture->GetSizeXY().ToString();
#endif

			UE_LOG(LogStreamline, Error, TEXT("found unexpected Viewfamily rendertarget %s %s. This might cause instability in other parts of the Streamline plugin."),
				*TextureName,
				*TextureDimensionAsString
				);
		}
		FoundTrackedView->Texture = TargetTexture;
	}

	check(!ViewInfo.ViewRect.IsEmpty());
	FoundTrackedView->ViewRect = ViewInfo.ViewRect;

	check(!ViewInfo.UnscaledViewRect.IsEmpty());
	FoundTrackedView->UnscaledViewRect = ViewInfo.UnscaledViewRect;

	check(!ViewInfo.UnconstrainedViewRect.IsEmpty());
	FoundTrackedView->UnconstrainedViewRect = ViewInfo.UnconstrainedViewRect;

	FStreamlineViewExtension::LogTrackedViews(*FString::Printf(TEXT("%s Key=%u Target=%p, %s"), ANSI_TO_TCHAR(__FUNCTION__), NewViewKey, TargetTexture.GetReference()->GetTexture2D(), *CurrentThreadName()));
}

void FStreamlineViewExtension::UntrackViewsForBackbuffer(void* InViewport)
{
#if DEBUG_STREAMLINE_VIEW_TRACKING
	UE_CLOG(DebugViewTracking(), LogStreamline, Log, TEXT("%s %s Enter InViewport=%p"), ANSI_TO_TCHAR(__FUNCTION__), *CurrentThreadName(), InViewport);
#endif

	check(IsInGameThread());
	if (FStreamlineDXGISwapChainProxy::IsEnabled())
	{
#if DEBUG_STREAMLINE_VIEW_TRACKING
		UE_CLOG(DebugViewTracking(), LogStreamline, Log, TEXT("%s %s Exit (proxy enabled)"), ANSI_TO_TCHAR(__FUNCTION__), *CurrentThreadName());
#endif
		return;
	}

	if (InViewport)
	{
		FViewportRHIRef ViewportReference = *(FViewportRHIRef*)InViewport;

		if (ViewportReference)
		{
			const void* NativeBackbufferTexture = ViewportReference->GetNativeBackBufferTexture();
			TrackedViews.RemoveAllSwap([NativeBackbufferTexture](const FTrackedView& TrackedView)
			{
				bool bRemove = false;
				if (TrackedView.Texture && TrackedView.Texture.IsValid())
				{
					const void* NativeTracked = TrackedView.Texture->GetNativeResource();
					if (NativeTracked == NativeBackbufferTexture)
					{
						bRemove = true;
#if DEBUG_STREAMLINE_VIEW_TRACKING
						UE_CLOG(DebugViewTracking(), LogStreamline, Log, TEXT("Untracking backbuffer %s native %p ViewKey = %u"), *TrackedView.Texture->GetName().ToString(), NativeTracked, TrackedView.ViewKey);
#endif
					}
				}
				return bRemove;
			});
		}
	}

#if DEBUG_STREAMLINE_VIEW_TRACKING
	UE_CLOG(DebugViewTracking(), LogStreamline, Log, TEXT("%s %s Exit"), ANSI_TO_TCHAR(__FUNCTION__), *CurrentThreadName());
#endif
}
