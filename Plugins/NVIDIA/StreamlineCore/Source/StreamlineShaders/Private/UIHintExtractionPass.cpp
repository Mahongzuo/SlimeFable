/*
* Copyright (c) 2022 - 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
* NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
* property and proprietary rights in and to this material, related
* documentation and any modifications thereto. Any use, reproduction,
* disclosure or distribution of this material and related documentation
* without an express license agreement from NVIDIA CORPORATION or
* its affiliates is strictly prohibited.
*/

#include "UIHintExtractionPass.h"

#include "Runtime/Launch/Resources/Version.h"
#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 2)
#include "DataDrivenShaderPlatformInfo.h"
#endif

static const int32 kUIHintExtractionComputeTileSizeX = FComputeShaderUtils::kGolden2DGroupSize;
static const int32 kUIHintExtractionComputeTileSizeY = FComputeShaderUtils::kGolden2DGroupSize;

class FStreamlineUIHintExtractionCS : public FGlobalShader
{
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// Only cook for the platforms/RHIs where DLSS-FG is supported, which is DX11,DX12 [on Win64]
		return 	IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
				IsPCPlatform(Parameters.Platform) && IsD3DPlatform(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), kUIHintExtractionComputeTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), kUIHintExtractionComputeTileSizeY);
	}
	DECLARE_GLOBAL_SHADER(FStreamlineUIHintExtractionCS);
	SHADER_USE_PARAMETER_STRUCT(FStreamlineUIHintExtractionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(float, AlphaThreshold)
		// Input images
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BackBuffer)
		
		// Output images
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutUIHintTexture)
	END_SHADER_PARAMETER_STRUCT()
};


IMPLEMENT_GLOBAL_SHADER(FStreamlineUIHintExtractionCS, "/Plugin/StreamlineCore/Private/UIHintExtraction.usf", "UIHintExtractionMain", SF_Compute);

FRDGTextureRef AddStreamlineUIHintExtractionPass(
	FRDGBuilder& GraphBuilder,
	const float InAlphaThreshold,
	const FTextureRHIRef& InBackBuffer

)
{

	FIntPoint BackBufferDimension = { int32(InBackBuffer->GetTexture2D()->GetSizeX()), int32(InBackBuffer->GetTexture2D()->GetSizeY()) };

	const FIntRect InputViewRect = { FIntPoint::ZeroValue,BackBufferDimension };
	const FIntRect OutputViewRect = { FIntPoint::ZeroValue,BackBufferDimension };

	FRDGTextureDesc UIHintTextureDesc =
	FRDGTextureDesc::Create2D(

		OutputViewRect.Size(),
		PF_B8G8R8A8,
		FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV);
	const TCHAR* OutputName = TEXT("Streamline.UIColorAndAlpha");

	FRDGTextureRef UIHintTexture = GraphBuilder.CreateTexture(
		UIHintTextureDesc,
		OutputName);

	FStreamlineUIHintExtractionCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStreamlineUIHintExtractionCS::FParameters>();
	PassParameters->AlphaThreshold = FMath::Clamp(InAlphaThreshold, 0.0f, 1.0f);

	// backbuffer contains UI transparency in the .alpha channek. Possibly quantized due to low amount of alphA bits in the backbuffer pixelformat
	{
		
		PassParameters->BackBuffer = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(InBackBuffer, TEXT("InBackBuffer")));
	}
	
	{
		PassParameters->OutUIHintTexture = GraphBuilder.CreateUAV(UIHintTexture);
	}


	FStreamlineUIHintExtractionCS::FPermutationDomain PermutationVector;
	
	TShaderMapRef<FStreamlineUIHintExtractionCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("Streamline UI Hint extraction (%dx%d) [%d,%d -> %d,%d]", 
			OutputViewRect.Width(), OutputViewRect.Height(),
			OutputViewRect.Min.X, OutputViewRect.Min.Y,
			OutputViewRect.Max.X, OutputViewRect.Max.Y
		),
		ComputeShader,
		PassParameters,
		FComputeShaderUtils::GetGroupCount(OutputViewRect.Size(), FComputeShaderUtils::kGolden2DGroupSize));
		
	return UIHintTexture;
}