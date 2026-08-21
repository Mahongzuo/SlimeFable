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
#pragma once

#include "CoreMinimal.h"
#include "Misc/CoreDelegates.h"
#include "RendererInterface.h"
#include "RHIResources.h"
#include "ScreenPass.h"
#include "PostProcess/PostProcessMaterial.h"
#include "SceneViewExtension.h"
#include "StreamlineReflex.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Misc/EngineVersionComparison.h"
#include "StreamlineShaders.h"
#include "StreamlineRHI.h"
#include "StreamlineCorePrivate.h"

#ifndef DEBUG_STREAMLINE_VIEW_TRACKING
#define DEBUG_STREAMLINE_VIEW_TRACKING (!(UE_BUILD_TEST || UE_BUILD_SHIPPING))
#endif

class FSceneTextureParameters;
class FRHITexture;
class FStreamlineRHI;
class SWindow;

struct FTrackedView
{
	FIntRect ViewRect;
	FIntRect UnscaledViewRect;
	FIntRect UnconstrainedViewRect;
	FTextureRHIRef Texture;
	uint32_t ViewKey = 0;
};

BEGIN_SHADER_PARAMETER_STRUCT(FSLUIHintTagShaderParameters, )
RDG_TEXTURE_ACCESS(BackBuffer, ERHIAccess::CopySrc)
RDG_TEXTURE_ACCESS(UIColorAndAlpha, ERHIAccess::CopySrc)

#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
SHADER_PARAMETER_STRUCT_INCLUDE(FDebugLayerCompatibilityShaderParameters, DebugLayerCompatibility)
#endif
END_SHADER_PARAMETER_STRUCT()



extern void AddStreamlineUIHintTagPass(
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
);

class FStreamlineViewExtension final : public FSceneViewExtensionBase
{
public:
	FStreamlineViewExtension(const FAutoRegister& AutoRegister, FStreamlineRHI* InStreamlineRHI);
	~FStreamlineViewExtension();

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void SetupViewPoint(APlayerController* Player, FMinimalViewInfo& InViewInfo) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;

	
#if ENGINE_MAJOR_VERSION == 4
	typedef FRHICommandListImmediate FGraphBuilderOrCmdList;
#else
	typedef FRDGBuilder FGraphBuilderOrCmdList;
#endif

	virtual void PreRenderView_RenderThread(FGraphBuilderOrCmdList&, FSceneView& InView) final;
	virtual void PreRenderViewFamily_RenderThread(FGraphBuilderOrCmdList&, FSceneViewFamily& InViewFamily) final;

	virtual void PostRenderViewFamily_RenderThread(FGraphBuilderOrCmdList&, FSceneViewFamily& InViewFamily) final;
	virtual void PostRenderView_RenderThread(FGraphBuilderOrCmdList&, FSceneView& InView) final;
#if UE_VERSION_OLDER_THAN(5,5,0)	
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;
#else
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& InView, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;
#endif

public:
	static void AddTrackedView(const FSceneView& InView);

	// that might need to get indexed by the viewfamily or smth
private: static TArray<FTrackedView> TrackedViews;
public:

	static bool DebugViewTracking();
	static void LogViewNotTrackedReason(const TCHAR* Callsite, const FSceneView& View);
	static const bool IsProperGraphicsView(const FSceneView& InView);
#if DEBUG_STREAMLINE_VIEW_TRACKING
	static bool bLogStreamlineLogTrackedViews;
#endif

	static void LogTrackedViews(const TCHAR* CallSite);
	static TArray<FTrackedView>& GetTrackedViews()
	{
		return TrackedViews;
	}
	void UntrackViewsForBackbuffer(void *InViewport);

	static int32 GetViewIndex(const FSceneView* InView)
	{
		check(InView->Family);
		check(InView->Family->Views.Contains(InView));

		const TArray<const FSceneView*>& Views = InView->Family->Views;
		int32 ViewIndex = 0;

		for (; ViewIndex < Views.Num(); ++ ViewIndex)
		{
			if (Views[ViewIndex] == InView)
			{
				break;
			}
		}

		check(ViewIndex < InView->Family->Views.Num());
		return ViewIndex;
	}

private:
	FScreenPassTexture PostProcessPassAtEnd_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& InOutInputs);
	
	FStreamlineRHI* StreamlineRHIExtensions;

	// Frame id, view id
	TArray< TTuple<uint64, uint32> > FramesWhereStreamlineConstantsWereSet;
	static FDelegateHandle OnPreResizeWindowBackBufferHandle;
	static FDelegateHandle OnSlateWindowDestroyedHandle;

	class FStreamlineDLSSGCustomPresent* SLCustomPresent = nullptr;
};

