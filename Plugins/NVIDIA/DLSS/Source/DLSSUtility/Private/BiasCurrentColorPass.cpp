/*
* Copyright (c) 2020 - 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
* NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
* property and proprietary rights in and to this material, related
* documentation and any modifications thereto. Any use, reproduction,
* disclosure or distribution of this material and related documentation
* without an express license agreement from NVIDIA CORPORATION or
* its affiliates is strictly prohibited.
*/

#include "BiasCurrentColorPass.h"
#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
#include "DataDrivenShaderPlatformInfo.h"
#endif
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "PostProcess/SceneRenderTargets.h"
#endif
#include "RenderGraphUtils.h"
#include "ScenePrivate.h"

const int32 kBiasCurrentColorComputeTileSizeX = FComputeShaderUtils::kGolden2DGroupSize;
const int32 kBiasCurrentColorComputeTileSizeY = FComputeShaderUtils::kGolden2DGroupSize;


class FDilateMotionVectorsDimTest : SHADER_PERMUTATION_BOOL("DILATE_MOTION_VECTORS");

class FCreateBiasCurrentColorCS : public FGlobalShader
{
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// Only cook for the platforms/RHIs where DLSS is supported, which is DX11, DX12 and Vulkan [on Win64]
		return 	IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
				IsPCPlatform(Parameters.Platform) && (
				IsVulkanPlatform(Parameters.Platform) || IsD3DPlatform(Parameters.Platform));
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), kBiasCurrentColorComputeTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), kBiasCurrentColorComputeTileSizeY);
		OutEnvironment.SetDefine(TEXT("STENCIL_MASK"), STENCIL_TEMPORAL_RESPONSIVE_AA_MASK);
	}

	DECLARE_GLOBAL_SHADER(FCreateBiasCurrentColorCS);
	SHADER_USE_PARAMETER_STRUCT(FCreateBiasCurrentColorCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Input
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, StencilTexture)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, DepthStencil)

		SHADER_PARAMETER(int, CustomOffset)

		// Output
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutBiasCurrentColorTexture)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, BiasCurrentColor)

		END_SHADER_PARAMETER_STRUCT()
};


IMPLEMENT_GLOBAL_SHADER(FCreateBiasCurrentColorCS, "/Plugin/DLSS/Private/CreateBiasCurrentColor.usf", "CreateBiasCurrentColorMain", SF_Compute);

FRDGTextureRef AddBiasCurrentColorPass(
	FRDGBuilder& GraphBuilder,
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	const FSceneView& View,
#else
	const FViewInfo& View,
#endif
	const FIntRect& InputViewRect,
	FRDGTextureRef InSceneDepthTexture,
	uint32 biasCurrentColorMaskCustomOffset
)
{

	const FIntRect OutputViewRect = FIntRect(FIntPoint::ZeroValue, InputViewRect.Size());
	
	FRDGTextureDesc BiasCurrentColorDesc = FRDGTextureDesc::Create2D(
		OutputViewRect.Size(),
		PF_R16F,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV);
	const TCHAR* OutputName = TEXT("DLSSBiasCurrentColor");

	FRDGTextureRef BiasCurrentColorTexture = GraphBuilder.CreateTexture(BiasCurrentColorDesc, OutputName);

	FCreateBiasCurrentColorCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCreateBiasCurrentColorCS::FParameters>();

	// input stencil
	{
		PassParameters->StencilTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateWithPixelFormat(InSceneDepthTexture, PF_X24_G8));
		
		FScreenPassTextureViewport depthStencilViewport(InSceneDepthTexture, InputViewRect);
		FScreenPassTextureViewportParameters depthStencilViewportParameters = GetScreenPassTextureViewportParameters(depthStencilViewport);
		PassParameters->DepthStencil = depthStencilViewportParameters;
	}
	// input custom offset
	{
		PassParameters->CustomOffset = biasCurrentColorMaskCustomOffset;
	}
	// output constructed DLSS BiasCurrentColorMask
	{
		PassParameters->OutBiasCurrentColorTexture = GraphBuilder.CreateUAV(BiasCurrentColorTexture);

		FScreenPassTextureViewport BiasCurrentColorViewport(BiasCurrentColorTexture, OutputViewRect);
		FScreenPassTextureViewportParameters BiasCurrentColorViewportParameters = GetScreenPassTextureViewportParameters(BiasCurrentColorViewport);
		PassParameters->BiasCurrentColor = BiasCurrentColorViewportParameters;
	}
	const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
	TShaderMapRef<FCreateBiasCurrentColorCS> ComputeShader(ShaderMap);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("Create BiasCurrentColorMask %s (%dx%d -> %dx%d)",
			TEXT("BiasCurrentColor"),
			InputViewRect.Width(), InputViewRect.Height(),
			OutputViewRect.Width(), OutputViewRect.Height()
		),
		ComputeShader,
		PassParameters,
		FComputeShaderUtils::GetGroupCount(OutputViewRect.Size(), FComputeShaderUtils::kGolden2DGroupSize));

	return BiasCurrentColorTexture;
}

FRDGTextureRef AddBiasCurrentColorPass(
	FRDGBuilder& GraphBuilder, 
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3 
	const FSceneView & View, 
#else
	const FViewInfo& View, 
#endif
	const FIntRect& InputViewRect, 
	struct FCustomDepthTextures CustomDepthTextures,
	uint8 CustomOffset)
{
	
	const FIntRect OutputViewRect = FIntRect(FIntPoint::ZeroValue, InputViewRect.Size());

	FRDGTextureDesc BiasCurrentColorDesc = FRDGTextureDesc::Create2D(
		OutputViewRect.Size(),
		PF_R16F,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV);
	const TCHAR* OutputName = TEXT("DLSSBiasCurrentColor");

	FRDGTextureRef BiasCurrentColorTexture = GraphBuilder.CreateTexture(BiasCurrentColorDesc, OutputName);

	FCreateBiasCurrentColorCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCreateBiasCurrentColorCS::FParameters>();

	// input stencil
	{
		PassParameters->StencilTexture = CustomDepthTextures.Stencil; //GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateWithPixelFormat(InSceneDepthTexture, PF_X24_G8));

		FScreenPassTextureViewport DepthStencilViewport(CustomDepthTextures.Depth, InputViewRect);
		FScreenPassTextureViewportParameters DepthStencilViewportParameters = GetScreenPassTextureViewportParameters(DepthStencilViewport);
		PassParameters->DepthStencil = DepthStencilViewportParameters;
	}
	// input custom offset
	{
		PassParameters->CustomOffset = CustomOffset;
	}
	// output constructed DLSS BiasCurrentColorMask
	{
		PassParameters->OutBiasCurrentColorTexture = GraphBuilder.CreateUAV(BiasCurrentColorTexture);

		FScreenPassTextureViewport BiasCurrentColorViewport(BiasCurrentColorTexture, OutputViewRect);
		FScreenPassTextureViewportParameters BiasCurrentColorViewportParameters = GetScreenPassTextureViewportParameters(BiasCurrentColorViewport);
		PassParameters->BiasCurrentColor = BiasCurrentColorViewportParameters;
	}
	const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
	TShaderMapRef<FCreateBiasCurrentColorCS> ComputeShader(ShaderMap);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("Create BiasCurrentColorMask %s (%dx%d -> %dx%d)",
			TEXT("BiasCurrentColor"),
			InputViewRect.Width(), InputViewRect.Height(),
			OutputViewRect.Width(), OutputViewRect.Height()
		),
		ComputeShader,
		PassParameters,
		FComputeShaderUtils::GetGroupCount(OutputViewRect.Size(), FComputeShaderUtils::kGolden2DGroupSize));

	return BiasCurrentColorTexture;

}
