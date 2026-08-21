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
#include "Slate/SceneViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "StreamlineRHI.h"
#include "StreamlineNGXRHI.h"
#include "StreamlineNGXRenderer.h"

DECLARE_LOG_CATEGORY_EXTERN(LogStreamline, Verbose, All);
DECLARE_GPU_STAT_NAMED_EXTERN(Streamline, TEXT("Streamline"));


bool ShouldTagStreamlineBuffers();
bool ForceTagStreamlineBuffers();
bool ShouldTrackViews();
bool NeedStreamlineViewIdOverride();

namespace sl
{
	enum class Result;
}
namespace Streamline
{
	enum class EStreamlineFeatureSupport;
}

Streamline::EStreamlineFeatureSupport TranslateStreamlineResult(sl::Result Result);

#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
BEGIN_SHADER_PARAMETER_STRUCT(FDebugLayerCompatibilityShaderParameters, )
	RDG_TEXTURE_ACCESS(DebugLayerCompatibilityHelperSource, ERHIAccess::CopySrc)
	RDG_TEXTURE_ACCESS(DebugLayerCompatibilityHelperDest, ERHIAccess::CopyDest)
END_SHADER_PARAMETER_STRUCT()

inline void AddDebugLayerCompatibilitySetupPasses(FRDGBuilder& GraphBuilder, FDebugLayerCompatibilityShaderParameters* PassParameters)
{
	NV_RDG_EVENT_SCOPE(GraphBuilder,Streamline, "UE5.5AndOlderDebugLayerCompatibilitySetup");
	FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(FIntPoint(1, 1), PF_FloatRGBA, FClearValueBinding::Black, TexCreate_RenderTargetable);
	PassParameters->DebugLayerCompatibilityHelperSource = GraphBuilder.CreateTexture(Desc, TEXT("UE5.5AndOlderDebugLayerCompatibilityHelperSource"));
	PassParameters->DebugLayerCompatibilityHelperDest = GraphBuilder.CreateTexture(Desc, TEXT("UE5.5AndOlderDebugLayerCompatibilityHelperDest"));
	AddClearRenderTargetPass(GraphBuilder, PassParameters->DebugLayerCompatibilityHelperSource);
	AddClearRenderTargetPass(GraphBuilder, PassParameters->DebugLayerCompatibilityHelperDest);
}

inline void DebugLayerCompatibilityRHISetup(const FDebugLayerCompatibilityShaderParameters& PassParameters, FRHIStreamlineResource& Texture)
{
	check(PassParameters.DebugLayerCompatibilityHelperSource);
	PassParameters.DebugLayerCompatibilityHelperSource->MarkResourceAsUsed();

	check(PassParameters.DebugLayerCompatibilityHelperDest);
	PassParameters.DebugLayerCompatibilityHelperDest->MarkResourceAsUsed();

	Texture.DebugLayerCompatibilityHelperSource = PassParameters.DebugLayerCompatibilityHelperSource->GetRHI();
	Texture.DebugLayerCompatibilityHelperDest = PassParameters.DebugLayerCompatibilityHelperDest->GetRHI();
}

template<typename Array>
inline void DebugLayerCompatibilityRHISetup(const FDebugLayerCompatibilityShaderParameters& PassParameters, Array& Textures)
{
	for (FRHIStreamlineResource& Texture : Textures)
	{
		DebugLayerCompatibilityRHISetup(PassParameters, Texture);
	}
}
#endif 

BEGIN_SHADER_PARAMETER_STRUCT(FSLSetStateShaderParameters, )
END_SHADER_PARAMETER_STRUCT()

template<typename StateOnRenderThreadLambda, typename RHIThreadLambda >
void AddStreamlineStateRenderPass(const TCHAR* FeatureName, FRDGBuilder& GraphBuilder, uint32 ViewID, const FIntRect& SecondaryViewRect, StateOnRenderThreadLambda&& StateOnRenderThread, RHIThreadLambda&& OnRHIThread)
{
	FSLSetStateShaderParameters* PassParameters = GraphBuilder.AllocParameters<FSLSetStateShaderParameters>();

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Streamline %s State ViewID = % u", FeatureName, ViewID),
		PassParameters,
		ERDGPassFlags::Compute | ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass | ERDGPassFlags::NeverCull,
		[PassParameters, ViewID, SecondaryViewRect, &OnRHIThread, &StateOnRenderThread](FRHICommandListImmediate& RHICmdList) mutable
		{
			auto Options = StateOnRenderThread(ViewID, SecondaryViewRect);

			RHICmdList.EnqueueLambda(
				[ViewID, SecondaryViewRect, Options, &OnRHIThread](FRHICommandListImmediate& Cmd) mutable
				{
					OnRHIThread(Cmd, ViewID, SecondaryViewRect, Options);
				});
			});
}
