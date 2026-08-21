/* 
* Copyright (c) 2023 - 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
* NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
* property and proprietary rights in and to this material, related
* documentation and any modifications thereto. Any use, reproduction,
* disclosure or distribution of this material and related documentation
* without an express license agreement from NVIDIA CORPORATION or
* its affiliates is strictly prohibited.
*/

#include "GBufferResolvePass.h"
#include "DLSSUtility.h"
#if __has_include("DataDrivenShaderPlatformInfo.h")
#include "DataDrivenShaderPlatformInfo.h"
#endif
#include "RHIResources.h"
#include "Runtime/Launch/Resources/Version.h"
#include "RenderGraphUtils.h"
#include "SceneTextureParameters.h"
#include "SystemTextures.h"
#include "SceneRendering.h"

#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION < 3
#if SUPPORT_GUIDE_GBUFFER
#error Guidebuffers unsupported on engine versions prior to 5.3
#endif
#endif

static TAutoConsoleVariable<bool> CVarNGXDLSSDisableSubsurfaceCheckerboard(
	TEXT("r.NGX.DLSS.DisableSubsurfaceCheckerboard"),
	true,
	TEXT("Skip checkerboard decoding for GBuffer Resolve Pass\n"),
	ECVF_RenderThreadSafe
);

class FDiffuseSpecularAlbedoDim : SHADER_PERMUTATION_BOOL("DIFFUSE_SPECULAR_ALBEDO");
class FForceDisableSubsurfaceCheckerboardDim : SHADER_PERMUTATION_BOOL("FORCE_DISABLE_SUBSURFACE_CHECKERBOARD");
class FOutputSpecularHitTDim : SHADER_PERMUTATION_BOOL("SPECULAR_HITT");
class FOutputSSSTDim : SHADER_PERMUTATION_BOOL("SSS");
class FOutputDOFTDim : SHADER_PERMUTATION_BOOL("DOF");
class FPassthroughDim : SHADER_PERMUTATION_BOOL("PASSTHROUGH_FEATURE_BUFFERS");
class FGBufferResolvePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGBufferResolvePS);
	SHADER_USE_PARAMETER_STRUCT(FGBufferResolvePS, FGlobalShader);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FOutputSpecularHitTDim>() == true)
		{
#if !SUPPORT_GUIDE_GBUFFER
			return false;
#endif
		}

		if (PermutationVector.Get<FOutputSSSTDim>() == true)
		{
#if !SUPPORT_GUIDE_SSS_DOF
			return false;
#endif
		}

		if (PermutationVector.Get<FOutputDOFTDim>() == true)
		{
#if !SUPPORT_GUIDE_SSS_DOF
			return false;
#endif
		}

		// Only cook for the platforms/RHIs where DLSS is supported, which is DX11,DX12 and Vulkan [on Win64]
		return 	IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
				IsPCPlatform(Parameters.Platform) && (
					IsVulkanPlatform(Parameters.Platform) ||
					IsD3DPlatform(Parameters.Platform));
	}

	using FPermutationDomain = TShaderPermutationDomain<FDiffuseSpecularAlbedoDim, FForceDisableSubsurfaceCheckerboardDim
		, FPassthroughDim, FOutputSpecularHitTDim, FOutputSSSTDim, FOutputDOFTDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_TEXTURE(Texture2D, PreIntegratedGF)
		SHADER_PARAMETER_SAMPLER(SamplerState, PreIntegratedGFSampler)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InputViewPort)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, OutputViewPort)

		// Should we explicitly ifdef these out for configs that can't use them
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, PassthroughDiffuse)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, PassthroughSpecular)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, PassthroughNormalRoughness)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, PassthroughDepth)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, ReflectionHitDistance)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, SubsurfaceScatteringGuide)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, DepthOfFieldGuide)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FGBufferResolvePS, "/Plugin/DLSS/Private/GBufferResolve.usf", "GBufferResolvePixelShader", SF_Pixel);

static TAutoConsoleVariable<bool> CVarNGXDLSSGBufferResolveCompute(
	TEXT("r.NGX.DLSS.GBufferResolve.Compute"),
	true,
	TEXT("Use compute shader for the GBuffer Resolve pass instead of the pixel shader\n"),
	ECVF_RenderThreadSafe
);

class FGBufferResolveCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGBufferResolveCS);
	SHADER_USE_PARAMETER_STRUCT(FGBufferResolveCS, FGlobalShader);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("GBUFFER_RESOLVE_COMPUTE"), 1);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), 8);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 8);
		OutEnvironment.CompilerFlags.Add(CFLAG_InlineRayTracing);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FOutputSpecularHitTDim>() == true)
		{
#if !SUPPORT_GUIDE_GBUFFER
			return false;
#endif
		}

		if (PermutationVector.Get<FOutputSSSTDim>() == true)
		{
#if !SUPPORT_GUIDE_SSS_DOF
			return false;
#endif
		}

		if (PermutationVector.Get<FOutputDOFTDim>() == true)
		{
#if !SUPPORT_GUIDE_SSS_DOF
			return false;
#endif
		}

		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6) &&
			IsPCPlatform(Parameters.Platform) && (
				IsVulkanPlatform(Parameters.Platform) ||
				IsD3DPlatform(Parameters.Platform));
	}

	using FPermutationDomain = TShaderPermutationDomain<FDiffuseSpecularAlbedoDim, FForceDisableSubsurfaceCheckerboardDim
		, FPassthroughDim, FOutputSpecularHitTDim, FOutputSSSTDim, FOutputDOFTDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_TEXTURE(Texture2D, PreIntegratedGF)
		SHADER_PARAMETER_SAMPLER(SamplerState, PreIntegratedGFSampler)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InputViewPort)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, OutputViewPort)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, PassthroughDiffuse)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, PassthroughSpecular)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, PassthroughNormalRoughness)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, PassthroughDepth)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, ReflectionHitDistance)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, SubsurfaceScatteringGuide)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, DepthOfFieldGuide)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutDiffuseAlbedo)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutSpecularAlbedo)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutNormal)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutRoughness)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutHitT)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutSubSurfaceScattering)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutDepthOfField)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FGBufferResolveCS, "/Plugin/DLSS/Private/GBufferResolve.usf", "GBufferResolveComputeShader", SF_Compute);

FGBufferResolveOutputs AddGBufferResolvePass(FRDGBuilder& GraphBuilder,
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	const FSceneView& View,
	const ITemporalUpscaler::FInputs& PassInputs,
#else
	const FViewInfo& View,
#endif
	FIntRect InputViewRect,
	const bool bComputeDiffuseSpecularAlbedo
)
{
	FGBufferResolveOutputs Outputs;

	// whether the engine has produced a set of precomposited reflection data
	bool bPrecomposite = false;
	bool bApplyHitT = false;
	bool bApplySSS = false;
	bool bApplyDOF = false;

	//Compute shader requires Inline ray tracing and that is only available on SM6
	if(CVarNGXDLSSGBufferResolveCompute.GetValueOnRenderThread() )
	{
		if ( View.FeatureLevel == ERHIFeatureLevel::SM6)
		{
			FGBufferResolveCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGBufferResolveCS::FParameters>();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
			FSceneTextureShaderParameters SceneTextures = CreateSceneTextureShaderParameters(GraphBuilder, View, ESceneTextureSetupMode::All
			);
#else
			FSceneTextureShaderParameters SceneTextures = CreateSceneTextureShaderParameters(GraphBuilder, View.GetSceneTexturesChecked(), View.GetFeatureLevel(), ESceneTextureSetupMode::All);
#endif
			PassParameters->SceneTextures = SceneTextures;
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->PreIntegratedGF = GSystemTextures.PreintegratedGF->GetRHI();
			PassParameters->PreIntegratedGFSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

			const FIntPoint OutputExtent = InputViewRect.Size();
			if (bComputeDiffuseSpecularAlbedo)
			{
				FRDGTextureDesc AlbedoDesc(FRDGTextureDesc::Create2D(
					OutputExtent,
					PF_FloatR11G11B10,
					FClearValueBinding::None,
					TexCreate_UAV | TexCreate_ShaderResource
				));
				Outputs.DiffuseAlbedo = GraphBuilder.CreateTexture(AlbedoDesc, TEXT("DLSS.DiffuseAlbedo"));
				Outputs.SpecularAlbedo = GraphBuilder.CreateTexture(AlbedoDesc, TEXT("DLSS.SpecularAlbedo"));
				PassParameters->OutDiffuseAlbedo = GraphBuilder.CreateUAV(Outputs.DiffuseAlbedo);
				PassParameters->OutSpecularAlbedo = GraphBuilder.CreateUAV(Outputs.SpecularAlbedo);

				FRDGTextureDesc NormalDesc(FRDGTextureDesc::Create2D(
					OutputExtent,
					PF_FloatRGBA,
					FClearValueBinding::None,
					TexCreate_UAV | TexCreate_ShaderResource
				));
				Outputs.Normals = GraphBuilder.CreateTexture(NormalDesc, TEXT("DLSS.Normal"));
				PassParameters->OutNormal = GraphBuilder.CreateUAV(Outputs.Normals);

				FRDGTextureDesc RoughnessDesc(FRDGTextureDesc::Create2D(
					OutputExtent,
					PF_R32_FLOAT,
					FClearValueBinding::None,
					TexCreate_UAV | TexCreate_ShaderResource
				));
				Outputs.Roughness = GraphBuilder.CreateTexture(RoughnessDesc, TEXT("DLSS.Roughness"));
				PassParameters->OutRoughness = GraphBuilder.CreateUAV(Outputs.Roughness);

				FRDGTextureDesc DepthDesc(FRDGTextureDesc::Create2D(
					OutputExtent,
					PF_R32_FLOAT,
					FClearValueBinding::None,
					TexCreate_UAV | TexCreate_ShaderResource
				));
				Outputs.LinearDepth = GraphBuilder.CreateTexture(DepthDesc, TEXT("DLSS.Depth"));
				PassParameters->OutDepth = GraphBuilder.CreateUAV(Outputs.LinearDepth);

#if SUPPORT_GUIDE_GBUFFER
				if (bComputeDiffuseSpecularAlbedo && PassInputs.GuideBuffers.ReflectionHitDistance.IsValid())
				{
					FRDGTextureDesc ReflectionHitDistanceTDesc(FRDGTextureDesc::Create2D(
						OutputExtent,
						PF_R32_FLOAT,
						FClearValueBinding::None,
						TexCreate_UAV | TexCreate_ShaderResource
					));
					Outputs.ReflectionHitDistance = GraphBuilder.CreateTexture(ReflectionHitDistanceTDesc, TEXT("DLSS.SpecularHitT"));
					PassParameters->OutHitT = GraphBuilder.CreateUAV(Outputs.ReflectionHitDistance);
					PassParameters->ReflectionHitDistance = PassInputs.GuideBuffers.ReflectionHitDistance.Texture;
					bApplyHitT = true;
				}

				bPrecomposite =
					PassInputs.GuideBuffers.DiffuseGuideBuffer.IsValid() &&
					PassInputs.GuideBuffers.SpecularGuideBuffer.IsValid() &&
					PassInputs.GuideBuffers.NormalRoughnessGuideBuffer.IsValid() &&
					PassInputs.GuideBuffers.DepthGuideBuffer.IsValid();

				PassParameters->PassthroughDiffuse = PassInputs.GuideBuffers.DiffuseGuideBuffer.Texture;
				PassParameters->PassthroughSpecular = PassInputs.GuideBuffers.SpecularGuideBuffer.Texture;
				PassParameters->PassthroughNormalRoughness = PassInputs.GuideBuffers.NormalRoughnessGuideBuffer.Texture;
				PassParameters->PassthroughDepth = PassInputs.GuideBuffers.DepthGuideBuffer.Texture;
#endif

#if SUPPORT_GUIDE_SSS_DOF
				if (PassInputs.GuideBuffers.SSSGuideBuffer.IsValid())
				{
					FRDGTextureDesc SubsurfaceScatteringGuideDesc(FRDGTextureDesc::Create2D(
						OutputExtent,
						PF_R16F,
						FClearValueBinding::None,
						TexCreate_UAV | TexCreate_ShaderResource
					));
					Outputs.SubsurfaceScatteringGuide = GraphBuilder.CreateTexture(SubsurfaceScatteringGuideDesc, TEXT("DLSS.SubsurfaceScatteringGuide"));
					PassParameters->OutSubSurfaceScattering = GraphBuilder.CreateUAV(Outputs.SubsurfaceScatteringGuide);
					PassParameters->SubsurfaceScatteringGuide = PassInputs.GuideBuffers.SSSGuideBuffer.Texture;
					bApplySSS = true;
				}

				if (PassInputs.GuideBuffers.DOFGuideBuffer.IsValid())
				{
					FRDGTextureDesc DepthOfFieldGuideDesc(FRDGTextureDesc::Create2D(
						OutputExtent,
						PF_R16F,
						FClearValueBinding::None,
						TexCreate_UAV | TexCreate_ShaderResource
					));
					Outputs.DepthOfFieldGuide = GraphBuilder.CreateTexture(DepthOfFieldGuideDesc, TEXT("DLSS.DepthOfFieldGuide"));
					PassParameters->OutDepthOfField = GraphBuilder.CreateUAV(Outputs.DepthOfFieldGuide);
					PassParameters->DepthOfFieldGuide = PassInputs.GuideBuffers.DOFGuideBuffer.Texture;
					bApplyDOF = true;
				}
#endif
			}

			FGBufferResolveCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FDiffuseSpecularAlbedoDim>(bComputeDiffuseSpecularAlbedo);
			PermutationVector.Set<FOutputSpecularHitTDim>(bApplyHitT);
			PermutationVector.Set<FOutputSSSTDim>(bApplySSS);
			PermutationVector.Set<FOutputDOFTDim>(bApplyDOF);
			PermutationVector.Set<FForceDisableSubsurfaceCheckerboardDim>(CVarNGXDLSSDisableSubsurfaceCheckerboard.GetValueOnRenderThread());
			PermutationVector.Set<FPassthroughDim>(bPrecomposite);

			const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
			TShaderMapRef<FGBufferResolveCS> ComputeShader(ShaderMap, PermutationVector);
			FScreenPassTextureViewport InputViewport(InputViewRect);
			FScreenPassTextureViewport OutputViewport(OutputExtent);
			PassParameters->InputViewPort = GetScreenPassTextureViewportParameters(InputViewport);
			PassParameters->OutputViewPort = GetScreenPassTextureViewportParameters(OutputViewport);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GBufferResolveCS%s%s%s%s"
					, bComputeDiffuseSpecularAlbedo ? TEXT(" DiffuseSpecularAlbedo") : TEXT("")
					, bApplyHitT ? TEXT(" ReflectionDistance") : TEXT("")
					, bApplySSS ? TEXT(" SSS") : TEXT("")
					, bApplyDOF ? TEXT(" DOF") : TEXT("")
				),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(OutputExtent, FIntPoint(8, 8))
			);

			return Outputs;
		}
		else
		{
			UE_LOG(LogDLSSUtility, Warning, TEXT("Compute DLSS Gbuffer resolve is only with SM6, reverting to pixel shader path"));
		}
	}

	FGBufferResolvePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGBufferResolvePS::FParameters>();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	FSceneTextureShaderParameters SceneTextures = CreateSceneTextureShaderParameters(GraphBuilder, View, ESceneTextureSetupMode::All
	);
#else
	FSceneTextureShaderParameters SceneTextures = CreateSceneTextureShaderParameters(GraphBuilder, View.GetSceneTexturesChecked(), View.GetFeatureLevel(), ESceneTextureSetupMode::All);
#endif
	PassParameters->SceneTextures = SceneTextures;

	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->PreIntegratedGF = GSystemTextures.PreintegratedGF->GetRHI();
	PassParameters->PreIntegratedGFSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	const FIntPoint OutputExtent = InputViewRect.Size();
	if (bComputeDiffuseSpecularAlbedo)
	{
		FRDGTextureDesc AlbedoDesc(FRDGTextureDesc::Create2D(
			OutputExtent,
			PF_FloatR11G11B10, // TODO PF_B8G8R8A8 enough?
			FClearValueBinding::None,
			TexCreate_RenderTargetable | TexCreate_ShaderResource
		));
		Outputs.DiffuseAlbedo = GraphBuilder.CreateTexture(AlbedoDesc, TEXT("DLSS.DiffuseAlbedo"));
		Outputs.SpecularAlbedo = GraphBuilder.CreateTexture(AlbedoDesc, TEXT("DLSS.SpecularAlbedo"));

		PassParameters->RenderTargets[0] = FRenderTargetBinding(Outputs.DiffuseAlbedo, ERenderTargetLoadAction::ENoAction);
		PassParameters->RenderTargets[1] = FRenderTargetBinding(Outputs.SpecularAlbedo, ERenderTargetLoadAction::ENoAction);

		FRDGTextureDesc NormalDesc(FRDGTextureDesc::Create2D(
			OutputExtent,
			PF_FloatRGBA, // TODO PF_B8G8R8A8 enough?
			FClearValueBinding::None,
			TexCreate_RenderTargetable | TexCreate_ShaderResource
		));
		Outputs.Normals = GraphBuilder.CreateTexture(NormalDesc, TEXT("DLSS.Normal"));

		PassParameters->RenderTargets[2] = FRenderTargetBinding(Outputs.Normals, ERenderTargetLoadAction::ENoAction);


		FRDGTextureDesc RoughnessDesc(FRDGTextureDesc::Create2D(
			OutputExtent,
			PF_R32_FLOAT, // TODO PF_B8G8R8A8 enough?
			FClearValueBinding::None,
			TexCreate_RenderTargetable | TexCreate_ShaderResource
		));
		Outputs.Roughness = GraphBuilder.CreateTexture(RoughnessDesc, TEXT("DLSS.Roughness"));

		PassParameters->RenderTargets[3] = FRenderTargetBinding(Outputs.Roughness, ERenderTargetLoadAction::ENoAction);


		FRDGTextureDesc DepthDesc(FRDGTextureDesc::Create2D(
			OutputExtent,
			PF_R32_FLOAT, // TODO PF_B8G8R8A8 enough?
			FClearValueBinding::None,
			TexCreate_RenderTargetable | TexCreate_ShaderResource
		));
		Outputs.LinearDepth = GraphBuilder.CreateTexture(DepthDesc, TEXT("DLSS.Depth"));

		PassParameters->RenderTargets[4] = FRenderTargetBinding(Outputs.LinearDepth, ERenderTargetLoadAction::ENoAction);

#if SUPPORT_GUIDE_GBUFFER

		if (bComputeDiffuseSpecularAlbedo && PassInputs.GuideBuffers.ReflectionHitDistance.IsValid())
		{
			FRDGTextureDesc ReflectionHitDistanceTDesc(FRDGTextureDesc::Create2D(
				OutputExtent,
				PF_R32_FLOAT,
				FClearValueBinding::None,
				TexCreate_RenderTargetable | TexCreate_ShaderResource
			));
			Outputs.ReflectionHitDistance = GraphBuilder.CreateTexture(ReflectionHitDistanceTDesc, TEXT("DLSS.SpecularHitT"));

			PassParameters->RenderTargets[5] = FRenderTargetBinding(Outputs.ReflectionHitDistance, ERenderTargetLoadAction::ENoAction);

			PassParameters->ReflectionHitDistance = PassInputs.GuideBuffers.ReflectionHitDistance.Texture;

			bApplyHitT = true;
		}


		// procomposited guide buffers from the engine
		bPrecomposite =
			PassInputs.GuideBuffers.DiffuseGuideBuffer.IsValid() &&
			PassInputs.GuideBuffers.SpecularGuideBuffer.IsValid() &&
			PassInputs.GuideBuffers.NormalRoughnessGuideBuffer.IsValid() &&
			PassInputs.GuideBuffers.DepthGuideBuffer.IsValid();

		PassParameters->PassthroughDiffuse = PassInputs.GuideBuffers.DiffuseGuideBuffer.Texture;
		PassParameters->PassthroughSpecular = PassInputs.GuideBuffers.SpecularGuideBuffer.Texture;
		PassParameters->PassthroughNormalRoughness = PassInputs.GuideBuffers.NormalRoughnessGuideBuffer.Texture;
		PassParameters->PassthroughDepth = PassInputs.GuideBuffers.DepthGuideBuffer.Texture;
#endif

#if SUPPORT_GUIDE_SSS_DOF
		if (/*bComputeDiffuseSpecularAlbedo &&*/ PassInputs.GuideBuffers.SSSGuideBuffer.IsValid())
		{
			FRDGTextureDesc SubsurfaceScatteringGuideDesc(FRDGTextureDesc::Create2D(
				OutputExtent,
				PF_R16F,
				FClearValueBinding::None,
				TexCreate_RenderTargetable | TexCreate_ShaderResource
			));
			Outputs.SubsurfaceScatteringGuide = GraphBuilder.CreateTexture(SubsurfaceScatteringGuideDesc, TEXT("DLSS.SubsurfaceScatteringGuide"));

			PassParameters->RenderTargets[6] = FRenderTargetBinding(Outputs.SubsurfaceScatteringGuide, ERenderTargetLoadAction::ENoAction);

			PassParameters->SubsurfaceScatteringGuide = PassInputs.GuideBuffers.SSSGuideBuffer.Texture;

			bApplySSS = true;
		}

		if (/*bComputeDiffuseSpecularAlbedo && */PassInputs.GuideBuffers.DOFGuideBuffer.IsValid())
		{
			FRDGTextureDesc DepthOfFieldGuideDesc(FRDGTextureDesc::Create2D(
				OutputExtent,
				PF_R16F,
				FClearValueBinding::None,
				TexCreate_RenderTargetable | TexCreate_ShaderResource
			));
			Outputs.DepthOfFieldGuide = GraphBuilder.CreateTexture(DepthOfFieldGuideDesc, TEXT("DLSS.DepthOfFieldGuide"));

			PassParameters->RenderTargets[7] = FRenderTargetBinding(Outputs.DepthOfFieldGuide, ERenderTargetLoadAction::ENoAction);

			PassParameters->DepthOfFieldGuide = PassInputs.GuideBuffers.DOFGuideBuffer.Texture;

			bApplyDOF = true;
		}
#endif
	}

	FGBufferResolvePS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FDiffuseSpecularAlbedoDim>(bComputeDiffuseSpecularAlbedo);
	PermutationVector.Set<FOutputSpecularHitTDim>(bApplyHitT);
	PermutationVector.Set<FOutputSSSTDim>(bApplySSS);
	PermutationVector.Set<FOutputDOFTDim>(bApplyDOF);
	PermutationVector.Set<FForceDisableSubsurfaceCheckerboardDim>(CVarNGXDLSSDisableSubsurfaceCheckerboard.GetValueOnRenderThread());
	PermutationVector.Set<FPassthroughDim>(bPrecomposite);

	const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
	TShaderMapRef<FGBufferResolvePS> PixelShader(ShaderMap, PermutationVector);
	FScreenPassTextureViewport InputViewport(InputViewRect);
	// shift output buffers to top left corner
	FScreenPassTextureViewport OutputViewport(OutputExtent);
	PassParameters->InputViewPort = GetScreenPassTextureViewportParameters(InputViewport);
	PassParameters->OutputViewPort = GetScreenPassTextureViewportParameters(OutputViewport);

	AddDrawScreenPass(
		GraphBuilder,
		RDG_EVENT_NAME("GBufferResolve%s%s%s%s"
			, bComputeDiffuseSpecularAlbedo ? TEXT(" DiffuseSpecularAlbedo") : TEXT("")
			, bApplyHitT ? TEXT(" ReflectionDistance") : TEXT("")
			, bApplySSS  ? TEXT(" SSS") : TEXT("")
			, bApplyDOF  ? TEXT(" DOF") : TEXT("")
		),
		View,
		OutputViewport,
		InputViewport,
		PixelShader,
		PassParameters);
	
	return Outputs;
}
