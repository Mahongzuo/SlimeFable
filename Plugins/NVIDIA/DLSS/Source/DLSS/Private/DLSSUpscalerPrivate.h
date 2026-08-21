/*
* Copyright (c) 2020 - 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include "RendererInterface.h"
#include "Runtime/Launch/Resources/Version.h"
#include "SceneViewExtension.h"

#include "DLSSUpscaler.h"
#include "NGXRHI.h"
#include "StreamlineNGXRHI.h"
#include "StreamlineNGXRenderer.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "TemporalUpscaler.h"
using ITemporalUpscaler = UE::Renderer::Private::ITemporalUpscaler;
#else
#include "PostProcess/TemporalAA.h"
#endif

DECLARE_LOG_CATEGORY_EXTERN(LogDLSS, Verbose, All);

DECLARE_GPU_STAT_NAMED_EXTERN(DLSS, TEXT("DLSS"));

class FDLSSUpscaler;
struct FTemporalAAHistory;

struct FDLSSPassParameters
{
	FIntRect InputViewRect;
	FIntRect OutputViewRect;
	FVector2f TemporalJitterPixels;
	float PreExposure;

	ENGXDLSSDenoiserMode DenoiserMode = ENGXDLSSDenoiserMode::Off;

	FRDGTexture* SceneColorInput = nullptr;
	FRDGTexture* SceneVelocityInput = nullptr;
	FRDGTexture* SceneDepthInput = nullptr;
	FRDGTexture* BiasCurrentColorInput = nullptr;

	// Used by denoisers
	FRDGTexture* DiffuseAlbedo = nullptr;
	FRDGTexture* SpecularAlbedo = nullptr;
	FRDGTexture* Normal = nullptr;
	FRDGTexture* Roughness = nullptr;
	FRDGTexture* ReflectionHitDistance = nullptr;
	FRDGTexture* SSSGuide = nullptr;
	FRDGTexture* DOFGuide = nullptr;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	FRDGTexture* EyeAdaptation = nullptr;

	FDLSSPassParameters(const ITemporalUpscaler::FInputs& PassInputs)
		: InputViewRect(PassInputs.SceneColor.ViewRect)
		, OutputViewRect(PassInputs.OutputViewRect)
		, TemporalJitterPixels(PassInputs.TemporalJitterPixels)
		, PreExposure(PassInputs.PreExposure)
		, SceneColorInput(PassInputs.SceneColor.Texture)
		, SceneDepthInput(PassInputs.SceneDepth.Texture)
		, EyeAdaptation(PassInputs.EyeAdaptationTexture)
#else
	FDLSSPassParameters(const FViewInfo& View, const ITemporalUpscaler::FPassInputs& PassInputs)
		: InputViewRect(View.ViewRect)
		, OutputViewRect(FIntPoint::ZeroValue, View.GetSecondaryViewRectSize())
		, TemporalJitterPixels(View.TemporalJitterPixels)
		, PreExposure(View.PreExposure)
		, SceneColorInput(PassInputs.SceneColorTexture)
		, SceneDepthInput(PassInputs.SceneDepthTexture)
#endif
	{
	}

	/** Returns the texture resolution that will be output. */
	FIntPoint GetOutputExtent() const;

	/** Validate the settings of TAA, to make sure there is no issue. */
	bool Validate() const;
};

struct FDLSSOutputs
{
	FRDGTexture* SceneColor = nullptr;
};

class FDLSSUpscalerViewExtension final : public FSceneViewExtensionBase
{
public:
	FDLSSUpscalerViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister)
	{ }

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) final override {}
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) final override {}
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;
};

class FDLSSSceneViewFamilyUpscaler final : public ITemporalUpscaler
{
public:
	FDLSSSceneViewFamilyUpscaler(const FDLSSUpscaler* InUpscaler, EDLSSQualityMode InDLSSQualityMode)
		: Upscaler(InUpscaler)
		, DLSSQualityMode(InDLSSQualityMode)
	{ }

	virtual const TCHAR* GetDebugName() const final override;
	virtual float GetMinUpsampleResolutionFraction() const final override;
	virtual float GetMaxUpsampleResolutionFraction() const final override;
	virtual ITemporalUpscaler* Fork_GameThread(const class FSceneViewFamily& ViewFamily) const final override;
	virtual ITemporalUpscaler::FOutputs AddPasses(
		FRDGBuilder& GraphBuilder,
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		const FSceneView& View,
		const ITemporalUpscaler::FInputs& PassInputs
#else
		const FViewInfo& View,
		const FPassInputs& PassInputs
#endif
	) const final override;

	static bool IsDLSSTemporalUpscaler(const ITemporalUpscaler* TemporalUpscaler);

private:
	const FDLSSUpscaler* Upscaler;
	const EDLSSQualityMode DLSSQualityMode;

	FDLSSOutputs AddDLSSPass(
		FRDGBuilder& GraphBuilder,
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		const FSceneView& View,
		const FDLSSPassParameters& Inputs,
		const TRefCountPtr<ITemporalUpscaler::IHistory> InputCustomHistoryInterface,
		TRefCountPtr<ITemporalUpscaler::IHistory>* OutputCustomHistoryInterface
#else
		const FViewInfo& View,
		const FDLSSPassParameters& Inputs,
		const FTemporalAAHistory& InputHistory,
		FTemporalAAHistory* OutputHistory,
		const TRefCountPtr<ICustomTemporalAAHistory> InputCustomHistoryInterface,
		TRefCountPtr<ICustomTemporalAAHistory>* OutputCustomHistoryInterface
#endif
	) const;
};

#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
BEGIN_SHADER_PARAMETER_STRUCT(FDebugLayerCompatibilityShaderParameters, )
	RDG_TEXTURE_ACCESS(DebugLayerCompatibilityHelperSource, ERHIAccess::CopySrc)
	RDG_TEXTURE_ACCESS(DebugLayerCompatibilityHelperDest, ERHIAccess::CopyDest)
END_SHADER_PARAMETER_STRUCT()

inline void AddDebugLayerCompatibilitySetupPasses(FRDGBuilder& GraphBuilder, FDebugLayerCompatibilityShaderParameters* PassParameters)
{
	NV_RDG_EVENT_SCOPE(GraphBuilder,DLSS, "UE5.5AndOlderDebugLayerCompatibilitySetup");
	FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(FIntPoint(1, 1), PF_FloatRGBA, FClearValueBinding::Black, TexCreate_RenderTargetable);
	PassParameters->DebugLayerCompatibilityHelperSource = GraphBuilder.CreateTexture(Desc, TEXT("UE5.5AndOlderDebugLayerCompatibilityHelperSource"));
	PassParameters->DebugLayerCompatibilityHelperDest = GraphBuilder.CreateTexture(Desc, TEXT("UE5.5AndOlderDebugLayerCompatibilityHelperDest"));
	AddClearRenderTargetPass(GraphBuilder, PassParameters->DebugLayerCompatibilityHelperSource);
	AddClearRenderTargetPass(GraphBuilder, PassParameters->DebugLayerCompatibilityHelperDest);
}

inline void DebugLayerCompatibilityRHISetup(const FDebugLayerCompatibilityShaderParameters& PassParameters, FRHIDLSSArguments& InDLSSArguments)
{
	check(PassParameters.DebugLayerCompatibilityHelperSource);
	PassParameters.DebugLayerCompatibilityHelperSource->MarkResourceAsUsed();

	check(PassParameters.DebugLayerCompatibilityHelperDest);
	PassParameters.DebugLayerCompatibilityHelperDest->MarkResourceAsUsed();

	InDLSSArguments.DebugLayerCompatibilityHelperSource = PassParameters.DebugLayerCompatibilityHelperSource->GetRHI();
	InDLSSArguments.DebugLayerCompatibilityHelperDest = PassParameters.DebugLayerCompatibilityHelperDest->GetRHI();
}

#endif 
