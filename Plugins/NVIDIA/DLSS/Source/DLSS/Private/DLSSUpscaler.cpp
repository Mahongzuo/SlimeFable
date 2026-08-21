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

#include "DLSSUpscaler.h"

#include "DLSS.h"
#include "DLSSSettings.h"
#include "DLSSUpscalerHistory.h"
#include "DLSSUpscalerModularFeature.h"
#include "DLSSUpscalerPrivate.h"
#include "GBufferResolvePass.h"
#include "VelocityCombinePass.h"
#include "BiasCurrentColorPass.h"

#include "DynamicResolutionState.h"
#include "Engine/GameViewportClient.h"
#include "LegacyScreenPercentageDriver.h"
#include "PostProcess/PostProcessEyeAdaptation.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Misc/EngineVersionComparison.h"

#include "ScenePrivate.h"
#include "SceneTextureParameters.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "PostProcess/SceneRenderTargets.h"
#endif


#define LOCTEXT_NAMESPACE "FDLSSModule"

static TAutoConsoleVariable<int32> CVarNGXDLSSEnable(
	TEXT("r.NGX.DLSS.Enable"), 0,
	TEXT("Enable/Disable DLSS SR or RR at runtime.\n")
	TEXT("  0: Disable DLSS-SR and DLSS-RR (default)\n")
	TEXT("  1: Enable DLSS-SR or DLSS-RR. Use r.NGX.DLSS.DenoiserMode to switch between DLSS-SR and DLSS-RR.\n"),
	ECVF_RenderThreadSafe);

// corresponds to EDLSSPreset
static TAutoConsoleVariable<int32> CVarNGXDLSSPresetSetting(
	TEXT("r.NGX.DLSS.Preset"),
	0,
	TEXT("DLSS-SR/DLAA preset setting. Allows selecting a different DL model than the default\n")
	TEXT("  0: Use default preset or ini value\n")
	TEXT("  1: Force preset A\n")
	TEXT("  2: Force preset B\n")
	TEXT("  3: Force preset C\n")
	TEXT("  4: Force preset D\n")
	TEXT("  5: Force preset E\n")
	TEXT("  6: Force preset F\n")
	TEXT("  7: Force preset G\n")
	TEXT("  8,9: Unsupported preset\n")
	TEXT(" 10: Force preset J\n")
	TEXT(" 11: Force preset K\n")
	TEXT(" 12: Force preset L\n")
	TEXT(" 13: Force preset M\n")
	TEXT(" 14: Force preset N\n")
	TEXT(" 15: Force preset O"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarNGXDLSSRRPresetSetting(
	TEXT("r.NGX.DLSSRR.Preset"),
	0,
	TEXT("DLSS-RR/DLAA preset setting. Allows selecting a different DL model than the default\n")
	TEXT("  0: Use default preset or ini value\n")
	TEXT("  1: Force preset A\n")
	TEXT("  2: Force preset B\n")
	TEXT("  3: Force preset C\n")
	TEXT("  4: Force preset D\n")
	TEXT("  5: Force preset E\n")
	TEXT("  6: Force preset F\n")
	TEXT("  7: Force preset G\n")
	TEXT("  8: Force preset H\n")
	TEXT("  9: Force preset I\n")
	TEXT(" 10: Force preset J\n")
	TEXT(" 11: Force preset K\n")
	TEXT(" 12: Force preset L\n")
	TEXT(" 13: Force preset M\n")
	TEXT(" 14: Force preset N\n")
	TEXT(" 15: Force preset O"), 
	ECVF_RenderThreadSafe);


static TAutoConsoleVariable<int32> CVarNGXDLSSAutoExposure(
	TEXT("r.NGX.DLSS.AutoExposure"), 1,
	TEXT("0: Use the engine-computed exposure value for input images to DLSS - in some cases this may reduce artifacts\n")
	TEXT("1: Enable DLSS internal auto-exposure instead of the application provided one (default)\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarNGXDLSSExposureScale(
	TEXT("r.NGX.DLSS.ExposureScale"),
	1.0f,
	TEXT("Default: 1.0f. Valid range [0.0f, 1.0f]"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarNGXDLSSBiasCurrentColorMask(
	TEXT("r.NGX.DLSS.BiasCurrentColorMask"), 0,
	TEXT("Enable/Disable support for BiasCurrentColorMask."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarNGXDLSSReleaseMemoryOnDelete(
	TEXT("r.NGX.DLSS.ReleaseMemoryOnDelete"), 
	1,
	TEXT("Enabling/disable releasing DLSS related memory on the NGX side when DLSS features get released.(default=1)"),
	ECVF_RenderThreadSafe);


static TAutoConsoleVariable<int32> CVarNGXDLSSFeatureCreationNode(
	TEXT("r.NGX.DLSS.FeatureCreationNode"), -1,
	TEXT("Determines which GPU the DLSS feature is getting created on\n")
	TEXT("-1: Create on the GPU the command list is getting executed on (default)\n")
	TEXT(" 0: Create on GPU node 0 \n")
	TEXT(" 1: Create on GPU node 1 \n"),
	ECVF_RenderThreadSafe);


static TAutoConsoleVariable<int32> CVarNGXDLSSFeatureVisibilityMask(
	TEXT("r.NGX.DLSS.FeatureVisibilityMask"), -1,
	TEXT("Determines which GPU the DLSS feature is visible to\n")
	TEXT("-1: Visible to the GPU the command list is getting executed on (default)\n")
	TEXT(" 1: visible to GPU node 0 \n")
	TEXT(" 2:  visible to GPU node 1 \n")
	TEXT(" 3:  visible to GPU node 0 and GPU node 1\n"),
	ECVF_RenderThreadSafe);


static TAutoConsoleVariable<int32> CVarNGXDLSSDenoiserMode(
	TEXT("r.NGX.DLSS.DenoiserMode"),
	0,
	TEXT("Configures how DLSS denoises if DLSS-SR/RR are enabled via r.NGX.DLSS.Enable\n")
	TEXT("0: DLSS-SR enabled, just upscaling, no denoising (default)\n")
	TEXT("1: DLSS-RR enabled, denoising and upscaling \n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNGXEnableAlphaUpscaling(
	TEXT("r.NGX.DLSS.EnableAlphaUpscaling"), -1,
	TEXT("Enables Alpha channel upscaling\n")
	TEXT("Note: r.PostProcessing.PropagateAlpha MUST be enabled for this feature to work.\n")
	TEXT(" -1: based of r.PostProcessing.PropagateAlpha (default);\n")
	TEXT("  0: disabled;\n")
	TEXT("  1: enabled.\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNGXDLSSBiasCurrentColorMaskStencilValue(
	TEXT("r.NGX.DLSS.BiasCurrentColorMaskStencilValue"),
	-1,
	TEXT("The value that would be considered as Bias Color in the custom depth stencil buffer, Must not be set to 0!\n")
	TEXT(" -1: Use project settings value\n")
	TEXT(">=1: Use CVar Value as stencil value, Note: must be positive, non-zero.\n"),
	ECVF_RenderThreadSafe
);


DEFINE_GPU_STAT(DLSS);

// ViewRects can be quantized and that can occasionally push fractions more than 1% away from the target screen percentage
static const float kDLSSResolutionFractionError = 0.02f;

BEGIN_SHADER_PARAMETER_STRUCT(FDLSSShaderParameters, )

	// Input images
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorInput)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthInput)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, EyeAdaptation)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneVelocityInput)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BiasCurrentColorInput)

	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DiffuseAlbedo)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SpecularAlbedo)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Normal)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Roughness)

#if SUPPORT_GUIDE_GBUFFER
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ReflectionHitDistance)

	SHADER_PARAMETER(FMatrix44f, ViewMatrix)
	SHADER_PARAMETER(FMatrix44f, ProjectionMatrix)
#endif

#if SUPPORT_GUIDE_SSS_DOF
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SSSGuideBuffer)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DOFGuideBuffer)
#endif



	// Output images
	RDG_TEXTURE_ACCESS(SceneColorOutput, ERHIAccess::UAVCompute)

#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
	SHADER_PARAMETER_STRUCT_INCLUDE(FDebugLayerCompatibilityShaderParameters, DebugLayerCompatibility)
#endif


END_SHADER_PARAMETER_STRUCT()

static FDLSSUpscaler* GetGlobalDLSSUpscaler()
{
	IDLSSModuleInterface* DLSSModule = &FModuleManager::LoadModuleChecked<IDLSSModuleInterface>("DLSS");
	check(DLSSModule);

	return DLSSModule->GetDLSSUpscaler();
}

static ENGXDLSSDenoiserMode GetDenoiserMode(const FDLSSUpscaler* Upscaler)
{
	if ((Upscaler != nullptr) && Upscaler->GetNGXRHI()->IsRRSupportedByRHI() &&  Upscaler->GetNGXRHI()->IsDLSSRRAvailable())
	{
		static_assert (int(ENGXDLSSDenoiserMode::MaxValue) == 1, "dear DLSS plugin NVIDIA developer, please update this code to handle the new ENGXDLSSDenoiserMode enum values");
		const ENGXDLSSDenoiserMode DenoiserMode = static_cast<ENGXDLSSDenoiserMode>(FMath::Clamp<int32>(CVarNGXDLSSDenoiserMode.GetValueOnRenderThread(), int32(ENGXDLSSDenoiserMode::Off), int32(ENGXDLSSDenoiserMode::MaxValue)));
		return DenoiserMode;
	}

	return ENGXDLSSDenoiserMode::Off;
}

FIntPoint FDLSSPassParameters::GetOutputExtent() const
{
	check(Validate());
	check(SceneColorInput);

	FIntPoint InputExtent = SceneColorInput->Desc.Extent;

	FIntPoint QuantizedPrimaryUpscaleViewSize;
	QuantizeSceneBufferSize(OutputViewRect.Size(), QuantizedPrimaryUpscaleViewSize);

	return FIntPoint(
		FMath::Max(InputExtent.X, QuantizedPrimaryUpscaleViewSize.X),
		FMath::Max(InputExtent.Y, QuantizedPrimaryUpscaleViewSize.Y));
}

bool FDLSSPassParameters::Validate() const
{
	checkf(OutputViewRect.Min == FIntPoint::ZeroValue,TEXT("The DLSS OutputViewRect %dx%d must be non-zero"), OutputViewRect.Min.X, OutputViewRect.Min.Y);
	return true;
}

static EDLSSPreset GetDLSSPresetFromCVarValue(int32 InCVarValue)
{
	if (InCVarValue >= 0 && InCVarValue < static_cast<int32>(EDLSSPreset::MAX))
	{
		return static_cast<EDLSSPreset>(InCVarValue);
	}
	UE_LOG(LogDLSS, Warning, TEXT("Invalid r.NGX.DLSS.DLSSPreset value %d"), InCVarValue);
	return EDLSSPreset::Default;
}

static EDLSSRRPreset GetDLSSRRPresetFromCVarValue(int32 InCVarValue)
{
	if (InCVarValue >= 0 && InCVarValue < static_cast<int32>(EDLSSRRPreset::MAX))
	{
		return static_cast<EDLSSRRPreset>(InCVarValue);
	}
	UE_LOG(LogDLSS, Warning, TEXT("Invalid r.NGX.DLSS.DLSSPreset value %d"), InCVarValue);
	return EDLSSRRPreset::Default;
}


static NVSDK_NGX_DLSS_Hint_Render_Preset ToNGXDLSSPreset(EDLSSPreset DLSSPreset)
{
	switch (DLSSPreset)
	{
		case EDLSSPreset::A:
		case EDLSSPreset::B:
		case EDLSSPreset::C:
		case EDLSSPreset::D:
			/* fall through*/
			ensureMsgf(false, TEXT("ToNGXDLSSPreset should not be called with a deprecated value"));
		case EDLSSPreset::Default:
			return NVSDK_NGX_DLSS_Hint_Render_Preset_Default;

		case EDLSSPreset::E:
			return NVSDK_NGX_DLSS_Hint_Render_Preset_E;

		case EDLSSPreset::F:
			return NVSDK_NGX_DLSS_Hint_Render_Preset_F;

		case EDLSSPreset::G:
			return NVSDK_NGX_DLSS_Hint_Render_Preset_G;

		case EDLSSPreset::H:
			return NVSDK_NGX_DLSS_Hint_Render_Preset_H_Reserved;

		case EDLSSPreset::I:
			return NVSDK_NGX_DLSS_Hint_Render_Preset_I_Reserved;
		
		case EDLSSPreset::J:
			return NVSDK_NGX_DLSS_Hint_Render_Preset_J;

		case EDLSSPreset::K:
			return NVSDK_NGX_DLSS_Hint_Render_Preset_K;

		case EDLSSPreset::L:
			return NVSDK_NGX_DLSS_Hint_Render_Preset_L;

		case EDLSSPreset::M:
			return NVSDK_NGX_DLSS_Hint_Render_Preset_M;

		case EDLSSPreset::N:
			return NVSDK_NGX_DLSS_Hint_Render_Preset_N;

		case EDLSSPreset::O:
			return NVSDK_NGX_DLSS_Hint_Render_Preset_O;
		default:
			checkf(false, TEXT("ToNGXDLSSPreset should not be called with an out of range EDLSSPreset from the higher level code"));
			return NVSDK_NGX_DLSS_Hint_Render_Preset_Default; // Won't be reached, but avoids compiler warnings
	}
}

static NVSDK_NGX_RayReconstruction_Hint_Render_Preset ToNGXDLSSRRPreset(EDLSSRRPreset DLSSRRPreset)
{
	switch (DLSSRRPreset)
	{
	case EDLSSRRPreset::A:
	case EDLSSRRPreset::B: 
	case EDLSSRRPreset::C:
		/* fall through*/
		ensureMsgf(false, TEXT("ToNGXDLSSRRPreset should not be called with a deprecated value"));
	case EDLSSRRPreset::Default:
		return NVSDK_NGX_RayReconstruction_Hint_Render_Preset_Default;

	case EDLSSRRPreset::D:
		return NVSDK_NGX_RayReconstruction_Hint_Render_Preset_D;

	case EDLSSRRPreset::E:
		return NVSDK_NGX_RayReconstruction_Hint_Render_Preset_E;

	case EDLSSRRPreset::F:
		return NVSDK_NGX_RayReconstruction_Hint_Render_Preset_F;

	case EDLSSRRPreset::G:
		return NVSDK_NGX_RayReconstruction_Hint_Render_Preset_G;

	case EDLSSRRPreset::H:
		return NVSDK_NGX_RayReconstruction_Hint_Render_Preset_H;

	case EDLSSRRPreset::I:
		return NVSDK_NGX_RayReconstruction_Hint_Render_Preset_I;

	case EDLSSRRPreset::J:
		return NVSDK_NGX_RayReconstruction_Hint_Render_Preset_J;

	case EDLSSRRPreset::K:
		return NVSDK_NGX_RayReconstruction_Hint_Render_Preset_K;

	case EDLSSRRPreset::L:
		return NVSDK_NGX_RayReconstruction_Hint_Render_Preset_L;

	case EDLSSRRPreset::M:
		return NVSDK_NGX_RayReconstruction_Hint_Render_Preset_M;

	case EDLSSRRPreset::N:
		return NVSDK_NGX_RayReconstruction_Hint_Render_Preset_N;

	case EDLSSRRPreset::O:
		return NVSDK_NGX_RayReconstruction_Hint_Render_Preset_O;
	
	default:
		checkf(false, TEXT("ToNGXDLSSRRPreset should not be called with an out of range EDLSSRRPreset from the higher level code"));
		return NVSDK_NGX_RayReconstruction_Hint_Render_Preset_Default; // Won't be reached, but avoids compiler warnings
	}
}


static NVSDK_NGX_DLSS_Hint_Render_Preset GetNGXDLSSPresetFromQualityMode(EDLSSQualityMode QualityMode)
{

	EDLSSPreset DLSSPreset = EDLSSPreset::Default;
	switch (QualityMode)
	{
		case EDLSSQualityMode::UltraPerformance:
			DLSSPreset = GetDefault<UDLSSSettings>()->DLSSUltraPerformancePreset;
			break;

		case EDLSSQualityMode::Performance:
			DLSSPreset = GetDefault<UDLSSSettings>()->DLSSPerformancePreset;
			break;

		case EDLSSQualityMode::Balanced:
			DLSSPreset = GetDefault<UDLSSSettings>()->DLSSBalancedPreset;
			break;

		case EDLSSQualityMode::Quality:
			DLSSPreset = GetDefault<UDLSSSettings>()->DLSSQualityPreset;
			break;

		case EDLSSQualityMode::UltraQuality:
			DLSSPreset = GetDefault<UDLSSSettings>()->DLSSUltraQualityPreset;
			break;

		case EDLSSQualityMode::DLAA:
			DLSSPreset = GetDefault<UDLSSSettings>()->DLAAPreset;
			break;

		default:
			checkf(false, TEXT("GetNGXDLSSPresetFromQualityMode called with an out of range EDLSSQualityMode"));
			break;
	}
	int32 DLSSPresetCVarVal = CVarNGXDLSSPresetSetting.GetValueOnAnyThread();
	if (DLSSPresetCVarVal != 0)
	{
		DLSSPreset = GetDLSSPresetFromCVarValue(DLSSPresetCVarVal);
	}
	return ToNGXDLSSPreset(DLSSPreset);
}

static NVSDK_NGX_RayReconstruction_Hint_Render_Preset GetNGXDLSSRRPresetFromQualityMode(EDLSSQualityMode QualityMode)
{

	EDLSSRRPreset DLSSRRPreset = EDLSSRRPreset::Default;
	switch (QualityMode)
	{
	case EDLSSQualityMode::UltraPerformance:
		DLSSRRPreset = GetDefault<UDLSSSettings>()->DLSSRRUltraPerformancePreset;
		break;

	case EDLSSQualityMode::Performance:
		DLSSRRPreset = GetDefault<UDLSSSettings>()->DLSSRRPerformancePreset;
		break;

	case EDLSSQualityMode::Balanced:
		DLSSRRPreset = GetDefault<UDLSSSettings>()->DLSSRRBalancedPreset;
		break;

	case EDLSSQualityMode::Quality:
		DLSSRRPreset = GetDefault<UDLSSSettings>()->DLSSRRQualityPreset;
		break;

	case EDLSSQualityMode::UltraQuality:
		DLSSRRPreset = GetDefault<UDLSSSettings>()->DLSSRRUltraQualityPreset;
		break;

	case EDLSSQualityMode::DLAA:
		DLSSRRPreset = GetDefault<UDLSSSettings>()->DLAARRPreset;
		break;

	default:
		checkf(false, TEXT("GetNGXDLSSPresetFromQualityMode called with an out of range EDLSSQualityMode"));
		break;
	}
	int32 DLSSPresetCVarVal = CVarNGXDLSSRRPresetSetting.GetValueOnAnyThread();
	if (DLSSPresetCVarVal != 0)
	{
		DLSSRRPreset = GetDLSSRRPresetFromCVarValue(DLSSPresetCVarVal);
	}
	return ToNGXDLSSRRPreset(DLSSRRPreset);
}

static uint8 GetBiasCurrentColorStencilValueFromSettings()
{
	uint8 CVarValue = (uint8)FMath::Clamp(CVarNGXDLSSBiasCurrentColorMaskStencilValue.GetValueOnAnyThread(),0,255);

	return CVarValue > 0 ? CVarValue : GetDefault<UDLSSSettings>()->BiasCurrentColorStencilValue;
}


static NVSDK_NGX_PerfQuality_Value ToNGXQuality(EDLSSQualityMode Quality)
{
	static_assert(int32(EDLSSQualityMode::NumValues) == 6, "dear DLSS plugin NVIDIA developer, please update this code to handle the new EDLSSQualityMode enum values");
	switch (Quality)
	{
		case EDLSSQualityMode::UltraPerformance:
			return NVSDK_NGX_PerfQuality_Value_UltraPerformance;

		default:
			checkf(false, TEXT("ToNGXQuality should not be called with an out of range EDLSSQualityMode from the higher level code"));
		case EDLSSQualityMode::Performance:
			return NVSDK_NGX_PerfQuality_Value_MaxPerf;

		case EDLSSQualityMode::Balanced:
			return NVSDK_NGX_PerfQuality_Value_Balanced;

		case EDLSSQualityMode::Quality:
			return NVSDK_NGX_PerfQuality_Value_MaxQuality;
		
		case EDLSSQualityMode::UltraQuality:
			return NVSDK_NGX_PerfQuality_Value_UltraQuality;
		case EDLSSQualityMode::DLAA:
			return NVSDK_NGX_PerfQuality_Value_DLAA;

	}
}

NGXRHI* FDLSSUpscaler::NGXRHIExtensions;
float FDLSSUpscaler::MinDynamicResolutionFraction = TNumericLimits <float>::Max();
float FDLSSUpscaler::MaxDynamicResolutionFraction = TNumericLimits <float>::Min();
uint32 FDLSSUpscaler::NumRuntimeQualityModes = 0;
TArray<FDLSSOptimalSettings> FDLSSUpscaler::ResolutionSettings;


bool FDLSSUpscalerViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	static const IDLSSModuleInterface* DLSSModule = &FModuleManager::LoadModuleChecked<IDLSSModuleInterface>(TEXT("DLSS"));
	if (DLSSModule->QueryDLSSSRSupport() != EDLSSSupport::Supported)
	{
		return false;
	}

#if ENGINE_SUPPORTS_UPSCALER_MODULAR_FEATURE
	// The UpscalerModularFeature has a Functor that can enable or disable the DLSS ViewExtension for this frame.
	if (const FDLSSTemporalUpscalerModularFeature *DLSSModularFeature = FDLSSTemporalUpscalerModularFeature::Get())
	{
		TOptional<bool> IsActiveResult = DLSSModularFeature->SceneViewExtensionIsActive(this, Context);
		if (IsActiveResult.IsSet())
		{
			return *IsActiveResult;
		}
	}
#endif

	// Do not setup if not available.
	if (!GetGlobalDLSSUpscaler()->IsDLSSActive())
	{
		return false;
	}

	// Verify this is for a viewport client
	if (Context.Viewport == nullptr || !GEngine)
	{
		return false;
	}

	if (GIsEditor)
#if WITH_EDITOR
	{
		if (Context.Viewport->IsPlayInEditorViewport())
		{
			bool bEnableDLSSInPlayInEditorViewports = false;
			if (GetDefault<UDLSSOverrideSettings>()->EnableDLSSInPlayInEditorViewportsOverride == EDLSSSettingOverride::UseProjectSettings)
			{
				bEnableDLSSInPlayInEditorViewports = GetDefault<UDLSSSettings>()->bEnableDLSSInPlayInEditorViewports;
			}
			else
			{
				bEnableDLSSInPlayInEditorViewports = GetDefault<UDLSSOverrideSettings>()->EnableDLSSInPlayInEditorViewportsOverride == EDLSSSettingOverride::Enabled;
			}
#if !NO_LOGGING
			static bool bLoggedPIEWarning = false;
			if (!bLoggedPIEWarning && GIsPlayInEditorWorld && bEnableDLSSInPlayInEditorViewports)
			{
				if (FStaticResolutionFractionHeuristic::FUserSettings::EditorOverridePIESettings())
				{
					UE_LOG(LogDLSS, Warning, TEXT("r.ScreenPercentage for DLSS quality mode will be ignored because overridden by editor settings (r.Editor.Viewport.OverridePIEScreenPercentage). Change this behavior in Edit -> Editor Preferences -> Performance"));
					bLoggedPIEWarning = true;
				}
			}
#endif
			return GIsPlayInEditorWorld && bEnableDLSSInPlayInEditorViewports;
		}
		else
		{
			bool bEnableDLSSInEditorViewports = false;
			if (GetDefault<UDLSSOverrideSettings>()->EnableDLSSInEditorViewportsOverride == EDLSSSettingOverride::UseProjectSettings)
			{
				bEnableDLSSInEditorViewports = GetDefault<UDLSSSettings>()->bEnableDLSSInEditorViewports;
			}
			else
			{
				bEnableDLSSInEditorViewports = GetDefault<UDLSSOverrideSettings>()->EnableDLSSInEditorViewportsOverride == EDLSSSettingOverride::Enabled;
			}
			return bEnableDLSSInEditorViewports;
		}
	}
#else
	{
		return false;
	}
#endif
	else
	{
		const bool bIsGameViewport = Context.Viewport->GetClient() == GEngine->GameViewport;
		return bIsGameViewport;
	}
}

void FDLSSUpscalerViewExtension::BeginRenderViewFamily(FSceneViewFamily& ViewFamily)
{
	if (ViewFamily.ViewMode != EViewModeIndex::VMI_Lit ||
		ViewFamily.Scene == nullptr ||
		ViewFamily.Scene->GetShadingPath() != EShadingPath::Deferred ||
		!ViewFamily.bRealtimeUpdate)
	{
		return;
	}

	// Early returns if none of the view have a view state or if primary temporal upscaling isn't requested
	bool bFoundPrimaryTemporalUpscale = false;
	for (const FSceneView* View : ViewFamily.Views)
	{
		if (View->State == nullptr)
		{
			return;
		}

		if (View->bIsSceneCapture)
		{
			return;
		}

		if (View->PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale)
		{
			bFoundPrimaryTemporalUpscale = true;
		}
	}
	if (!bFoundPrimaryTemporalUpscale)
	{
		return;
	}

	// Early returns if AA is disabled.
	if (!ViewFamily.EngineShowFlags.AntiAliasing)
	{
		return;
	}

	if (!ViewFamily.GetTemporalUpscalerInterface())
	{
		GetGlobalDLSSUpscaler()->SetupViewFamily(ViewFamily);
	}
	else
	{
		UE_LOG(LogDLSS, Error, TEXT("Another plugin already set FSceneViewFamily::SetTemporalUpscalerInterface()"));
		return;
	}
}

FDLSSUpscaler::FDLSSUpscaler(NGXRHI* InNGXRHIExtensions): PreviousResolutionFraction(-1.0f)
{
	UE_LOG(LogDLSS, VeryVerbose, TEXT("%s Enter"), ANSI_TO_TCHAR(__FUNCTION__));
	
	
	checkf(!NGXRHIExtensions, TEXT("static member NGXRHIExtensions should only be assigned once by this ctor when called during module startup") );
	NGXRHIExtensions = InNGXRHIExtensions;

	ResolutionSettings.Init(FDLSSOptimalSettings(), int32(EDLSSQualityMode::NumValues));

	static_assert(int32(EDLSSQualityMode::NumValues) == 6, "dear DLSS plugin NVIDIA developer, please update this code to handle the new EDLSSQualityMode enum values");
	for (auto QualityMode : { EDLSSQualityMode::UltraPerformance,  EDLSSQualityMode::Performance , EDLSSQualityMode::Balanced, EDLSSQualityMode::Quality, EDLSSQualityMode::UltraQuality,  EDLSSQualityMode::DLAA})
	{
		check(ToNGXQuality(QualityMode) < ResolutionSettings.Num());
		check(ToNGXQuality(QualityMode) >= 0);

		FDLSSOptimalSettings OptimalSettings = NGXRHIExtensions->GetDLSSOptimalSettings(ToNGXQuality(QualityMode));
		
		ResolutionSettings[ToNGXQuality(QualityMode)] = OptimalSettings;

		// we only consider non-fixed resolutions for the overall min / max resolution fraction
		if (OptimalSettings.bIsSupported && !OptimalSettings.IsFixedResolution())
		{
			MinDynamicResolutionFraction = FMath::Min(MinDynamicResolutionFraction, OptimalSettings.MinResolutionFraction);
			MaxDynamicResolutionFraction = FMath::Max(MaxDynamicResolutionFraction, OptimalSettings.MaxResolutionFraction);
		}
		if (OptimalSettings.bIsSupported)
		{
			++NumRuntimeQualityModes;
		}

		UE_LOG(LogDLSS, Log, TEXT("QualityMode %d: bSupported = %u, ResolutionFraction = %.4f. MinResolutionFraction=%.4f,  MaxResolutionFraction %.4f"),
			static_cast<int>(QualityMode), OptimalSettings.bIsSupported, OptimalSettings.OptimalResolutionFraction, OptimalSettings.MinResolutionFraction, OptimalSettings.MaxResolutionFraction);
	}

	// the DLSS module will report DLSS as not supported if there are no supported quality modes at runtime
	UE_LOG(LogDLSS, Log, TEXT("NumRuntimeQualityModes=%u, MinDynamicResolutionFraction=%.4f,  MaxDynamicResolutionFraction=%.4f"), NumRuntimeQualityModes, MinDynamicResolutionFraction, MaxDynamicResolutionFraction);

	// Higher levels of the code (e.g. UI) should check whether each mode is actually supported
	// But for now verify early that the DLSS 2.0 modes are supported. Those checks could be removed in the future
	check(IsQualityModeSupported(EDLSSQualityMode::Performance));
	check(IsQualityModeSupported(EDLSSQualityMode::Balanced));
	check(IsQualityModeSupported(EDLSSQualityMode::Quality));


	UE_LOG(LogDLSS, VeryVerbose, TEXT("%s Leave"), ANSI_TO_TCHAR(__FUNCTION__));
}

// this gets explicitly called during module shutdown
void FDLSSUpscaler::ReleaseStaticResources()
{
	UE_LOG(LogDLSS, VeryVerbose, TEXT("%s Enter"), ANSI_TO_TCHAR(__FUNCTION__));
	ResolutionSettings.Empty();
	UE_LOG(LogDLSS, VeryVerbose, TEXT("%s Leave"), ANSI_TO_TCHAR(__FUNCTION__));
}

static const TCHAR* const GDLSSSceneViewFamilyUpscalerDebugName = TEXT("FDLSSSceneViewFamilyUpscaler");
static const TCHAR* const GDLSSRRSceneViewFamilyUpscalerDebugName = TEXT("FDLSSSceneViewFamilyUpscaler(DLSS-RR)");

const TCHAR* FDLSSSceneViewFamilyUpscaler::GetDebugName() const
{
	static_assert (int(ENGXDLSSDenoiserMode::MaxValue) == 1, "dear DLSS plugin NVIDIA developer, please update this code to handle the new ENGXDLSSDenoiserMode enum values");
	ENGXDLSSDenoiserMode DenoiserMode = GetDenoiserMode(Upscaler);
	return (DenoiserMode == ENGXDLSSDenoiserMode::DLSSRR) ? GDLSSRRSceneViewFamilyUpscalerDebugName : GDLSSSceneViewFamilyUpscalerDebugName;
}

static bool IsDLSSUpscalerName(const TCHAR* Name)
{
	return (Name == GDLSSSceneViewFamilyUpscalerDebugName) || (Name == GDLSSRRSceneViewFamilyUpscalerDebugName);
}

// static
bool FDLSSSceneViewFamilyUpscaler::IsDLSSTemporalUpscaler(const ITemporalUpscaler* TemporalUpscaler)
{
	return (TemporalUpscaler != nullptr) && IsDLSSUpscalerName(TemporalUpscaler->GetDebugName());
}

float FDLSSSceneViewFamilyUpscaler::GetMinUpsampleResolutionFraction() const
{
	return Upscaler->GetMinResolutionFractionForQuality(DLSSQualityMode);
}

float FDLSSSceneViewFamilyUpscaler::GetMaxUpsampleResolutionFraction() const
{
	return Upscaler->GetMaxResolutionFractionForQuality(DLSSQualityMode);
}

ITemporalUpscaler* FDLSSSceneViewFamilyUpscaler::Fork_GameThread(const class FSceneViewFamily& ViewFamily) const
{
	return new FDLSSSceneViewFamilyUpscaler(Upscaler, DLSSQualityMode);
}

ITemporalUpscaler::FOutputs FDLSSSceneViewFamilyUpscaler::AddPasses(
	FRDGBuilder& GraphBuilder,
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	const FSceneView& View,
	const ITemporalUpscaler::FInputs& PassInputs
#else
	const FViewInfo& View,
	const FPassInputs& PassInputs
#endif
) const
{
	ITemporalUpscaler::FOutputs Outputs;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	const TRefCountPtr<ITemporalUpscaler::IHistory> InputCustomHistory = PassInputs.PrevHistory;
	TRefCountPtr<ITemporalUpscaler::IHistory>* OutputCustomHistory = &Outputs.NewHistory;

	FRDGTextureRef InputVelocity = PassInputs.SceneVelocity.Texture;
	FIntRect InputViewRect = PassInputs.SceneDepth.ViewRect;
	FDLSSPassParameters DLSSParameters(PassInputs);
#else
	const FTemporalAAHistory& InputHistory = View.PrevViewInfo.TemporalAAHistory;
	const TRefCountPtr<ICustomTemporalAAHistory> InputCustomHistory = View.PrevViewInfo.CustomTemporalAAHistory;

	FTemporalAAHistory* OutputHistory = View.ViewState ? &(View.ViewState->PrevFrameViewInfo.TemporalAAHistory) : nullptr;
	TRefCountPtr<ICustomTemporalAAHistory>* OutputCustomHistory = View.ViewState ? &(View.ViewState->PrevFrameViewInfo.CustomTemporalAAHistory) : nullptr;

	FRDGTextureRef InputVelocity = PassInputs.SceneVelocityTexture;
	FIntRect InputViewRect = View.ViewRect;
	FDLSSPassParameters DLSSParameters(View, PassInputs);
#endif

	bool bIsDLAA = (InputViewRect == DLSSParameters.OutputViewRect);
	checkf(bIsDLAA || (View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale),
		TEXT("DLSS-SR requires TemporalUpscale. If you hit this assert, please set r.TemporalAA.Upscale=1"));

	{

		NV_RDG_EVENT_SCOPE(GraphBuilder, DLSS,"DLSS");
#if UE_VERSION_OLDER_THAN(5,8,0)
		RDG_GPU_STAT_SCOPE(GraphBuilder, DLSS);
#endif

		DLSSParameters.DenoiserMode = GetDenoiserMode(Upscaler);

		
		static_assert (int(ENGXDLSSDenoiserMode::MaxValue) == 1, "dear DLSS plugin NVIDIA developer, please update this code to handle the new ENGXDLSSDenoiserMode enum values");
		
		const bool bUseBiasCurrentColorMask = CVarNGXDLSSBiasCurrentColorMask.GetValueOnRenderThread() != 0;

		const uint8 BiasCurrentColorMaskCustomOffset = GetBiasCurrentColorStencilValueFromSettings();
		
		FRDGTextureRef BiasCurrentColorTexture = nullptr;

		if (bUseBiasCurrentColorMask)
		{
			FCustomDepthTextures CustomDepthTextures = ((FViewFamilyInfo*)View.Family)->GetSceneTextures().CustomDepth;

			if (CustomDepthTextures.IsValid() && CustomDepthTextures.Stencil != nullptr)
			{
				BiasCurrentColorTexture = AddBiasCurrentColorPass(
					GraphBuilder, View,
					InputViewRect,
					CustomDepthTextures,
					BiasCurrentColorMaskCustomOffset);
			}
		}
		

#if SUPPORT_GUIDE_GBUFFER
		FRDGTextureRef AlternateMotionVectorTexture = PassInputs.GuideBuffers.AlternateMotionVector.Texture;
#else
		FRDGTextureRef AlternateMotionVectorTexture = nullptr;
#endif

		FRDGTextureRef CombinedVelocityTexture = AddVelocityCombinePass(
			GraphBuilder, View,
			DLSSParameters.SceneDepthInput,
			InputVelocity,
			AlternateMotionVectorTexture,
			InputViewRect,
			DLSSParameters.OutputViewRect,
			DLSSParameters.TemporalJitterPixels
			);

		DLSSParameters.SceneVelocityInput = CombinedVelocityTexture;
		DLSSParameters.BiasCurrentColorInput = BiasCurrentColorTexture;

		if (DLSSParameters.DenoiserMode == ENGXDLSSDenoiserMode::DLSSRR)
		{
			FGBufferResolveOutputs ResolvedGBuffer = AddGBufferResolvePass(
				GraphBuilder, 
				View, 
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
				PassInputs, 
#endif
				InputViewRect, 
				true);

			DLSSParameters.SceneVelocityInput = CombinedVelocityTexture;

			DLSSParameters.DiffuseAlbedo = ResolvedGBuffer.DiffuseAlbedo;
			DLSSParameters.SpecularAlbedo = ResolvedGBuffer.SpecularAlbedo;

			DLSSParameters.Normal = ResolvedGBuffer.Normals;
			DLSSParameters.Roughness = ResolvedGBuffer.Roughness;
			DLSSParameters.SceneDepthInput = ResolvedGBuffer.LinearDepth;
#if SUPPORT_GUIDE_GBUFFER
			DLSSParameters.ReflectionHitDistance = ResolvedGBuffer.ReflectionHitDistance;
#endif

#if SUPPORT_GUIDE_SSS_DOF
			DLSSParameters.SSSGuide = ResolvedGBuffer.SubsurfaceScatteringGuide;
			DLSSParameters.DOFGuide = ResolvedGBuffer.DepthOfFieldGuide;
#endif
		}

		const FDLSSOutputs DLSSOutputs = AddDLSSPass(
			GraphBuilder,
			View,
			DLSSParameters,
#if ENGINE_MINOR_VERSION <= 2
			InputHistory,
			OutputHistory,
#endif
			InputCustomHistory,
			OutputCustomHistory
		);

		Outputs.FullRes.Texture = DLSSOutputs.SceneColor;
		Outputs.FullRes.ViewRect = DLSSParameters.OutputViewRect;
	}
	return Outputs;
}

FDLSSOutputs FDLSSSceneViewFamilyUpscaler::AddDLSSPass(
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
) const
{
	check(IsInRenderingThread());
	check(Upscaler->IsDLSSActive());

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	// TODO: check if this camera cut logic is sufficient
	const bool bCameraCut = View.bCameraCut || !InputCustomHistoryInterface.IsValid();
#else
	const bool bCameraCut = !InputHistory.IsValid() || View.bCameraCut || !OutputHistory;
#endif
	const FIntPoint OutputExtent = Inputs.GetOutputExtent();

	const FIntRect SrcRect = Inputs.InputViewRect;
	const FIntRect DestRect = Inputs.OutputViewRect;

	const float ScaleX = float(SrcRect.Width()) / float(DestRect.Width());
	const float ScaleY = float(SrcRect.Height()) / float(DestRect.Height());

	// FDLSSUpscaler::SetupMainGameViewFamily or FDLSSUpscalerEditor::SetupEditorViewFamily 
	// set DLSSQualityMode by setting an FDLSSUpscaler on the ViewFamily (from the pool in DLSSUpscalerInstancesPerViewFamily)
	
	checkf(DLSSQualityMode != EDLSSQualityMode::NumValues, TEXT("Invalid Quality mode, not initialized"));
	checkf(Upscaler->IsQualityModeSupported(DLSSQualityMode), TEXT("%u is not a valid Quality mode"), DLSSQualityMode);

	// This assert can accidentally hit with small viewrect dimensions (e.g. when resizing an editor view) due to floating point rounding & quantization issues
	// e.g. with 33% screen percentage at 1000 DestRect dimension we get 333/1000 = 0.33 but at 10 DestRect dimension we get 3/10 0.3, thus the assert hits

	ensureMsgf(
		(DestRect.Width() < 100 || GetMinUpsampleResolutionFraction() - kDLSSResolutionFractionError <= ScaleX && ScaleX <= GetMaxUpsampleResolutionFraction() + kDLSSResolutionFractionError) &&
		(DestRect.Height() < 100 || GetMinUpsampleResolutionFraction() - kDLSSResolutionFractionError <= ScaleY && ScaleY <= GetMaxUpsampleResolutionFraction() + kDLSSResolutionFractionError),
		TEXT("The current resolution fraction [%dx%d -> %dx%d] (%f,%f) is out of the supported DLSS range [%f ... %f] for quality mode %d."),
		SrcRect.Width(), SrcRect.Height(), DestRect.Width(), DestRect.Height(), ScaleX, ScaleY, GetMinUpsampleResolutionFraction(), GetMaxUpsampleResolutionFraction(), DLSSQualityMode);


	const TCHAR* PassName = TEXT("MainUpsampling");

	// Create outputs
	FDLSSOutputs Outputs;
	{
		FRDGTextureDesc SceneColorDesc = FRDGTextureDesc::Create2D(
			OutputExtent,
			PF_FloatRGBA,
			FClearValueBinding::Black,
			TexCreate_ShaderResource | TexCreate_UAV);

		const TCHAR* OutputName = TEXT("DLSSOutputSceneColor");

		Outputs.SceneColor = GraphBuilder.CreateTexture(
			SceneColorDesc,
			OutputName);
	}

	// The upscaler history could be the wrong type, in the case of multiple upscaler plugins. Make sure upscaler history is really ours
	const FDLSSUpscalerHistory* InputDLSSHistory = nullptr;
	if (InputCustomHistoryInterface.IsValid())
	{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		const ITemporalUpscaler::IHistory* InputCustomHistory = InputCustomHistoryInterface.GetReference();
		if (IsDLSSUpscalerName(InputCustomHistory->GetDebugName()))
		{
			InputDLSSHistory = static_cast<const FDLSSUpscalerHistory*>(InputCustomHistory);
		}
#else
		const ICustomTemporalAAHistory* InputCustomHistory = InputCustomHistoryInterface.GetReference();
		const char* ExpectedTypeName = typeid(FDLSSUpscalerHistory).name();
		const char* InputHistoryTypeName = typeid(*InputCustomHistory).name();
		if ((InputHistoryTypeName == ExpectedTypeName) || (0 == FCStringAnsi::Strcmp(InputHistoryTypeName, ExpectedTypeName)))
		{
			InputDLSSHistory = static_cast<const FDLSSUpscalerHistory*>(InputCustomHistory);
		}
#endif
	}
	FDLSSStateRef DLSSState = (InputDLSSHistory && InputDLSSHistory->DLSSState) ? InputDLSSHistory->DLSSState : MakeShared<FDLSSState, ESPMode::ThreadSafe>();

	{
		FDLSSShaderParameters* PassParameters = GraphBuilder.AllocParameters<FDLSSShaderParameters>();

		// Set up common shader parameters
		const FIntPoint InputExtent = Inputs.SceneColorInput->Desc.Extent;
		
		const FIntRect OutputViewRect = Inputs.OutputViewRect;

		// in some configurations we can end up with an InputViewRect that is larger (by a few pixels) than the actual texture dimensions
		// r.Test.ViewRectOffset = 3 can get into this state
		// this will error out at the NGX level so we are adjusting the extents that we pass downstream to be within the texture.
		// FRHIDLSSArguments::Validate does verify this downstream so we don't assert here
		
		FIntRect AdjustedInputViewRect = Inputs.InputViewRect;
		{
			const FIntPoint OverHang = AdjustedInputViewRect.Max - Inputs.SceneColorInput->Desc.Extent ;
			const FIntPoint OverHangAdjust = OverHang.ComponentMax(FIntPoint::ZeroValue);
			AdjustedInputViewRect.Max -= OverHangAdjust;
			
			const bool bHasOverhang = OverHang.GetMax() > 0;
			UE_CLOG(bHasOverhang,LogDLSS, Warning, TEXT("The DLSS InputViewRect %s %dx%d is larger by %dx%d pixels than the DLSS-SR/RR input texture '%s' of size %dx%d. Adjusting the input viewrect by %dx%d pixels to %s %dx%d to allow execution of DLSS."),
				*Inputs.InputViewRect.ToString(), Inputs.InputViewRect.Width(), Inputs.InputViewRect.Height(),
				OverHang.X, OverHang.Y,
				Inputs.SceneColorInput->Name, Inputs.SceneColorInput->Desc.Extent.X, Inputs.SceneColorInput->Desc.Extent.Y,
				OverHangAdjust.X, OverHangAdjust.Y,
				*AdjustedInputViewRect.ToString(), AdjustedInputViewRect.Width(), AdjustedInputViewRect.Height()
			);
		}

		// Input buffer shader parameters
		{
			PassParameters->SceneColorInput = Inputs.SceneColorInput;
			PassParameters->SceneDepthInput = Inputs.SceneDepthInput;
			PassParameters->SceneVelocityInput = Inputs.SceneVelocityInput;
			PassParameters->BiasCurrentColorInput = Inputs.BiasCurrentColorInput;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
			PassParameters->EyeAdaptation = Inputs.EyeAdaptation;
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 2
			PassParameters->EyeAdaptation = AddCopyEyeAdaptationDataToTexturePass(GraphBuilder, View);
#else
			PassParameters->EyeAdaptation = GetEyeAdaptationTexture(GraphBuilder, View);
#endif

			PassParameters->DiffuseAlbedo = Inputs.DiffuseAlbedo;
			PassParameters->SpecularAlbedo = Inputs.SpecularAlbedo;

			PassParameters->Normal = Inputs.Normal;
			PassParameters->Roughness = Inputs.Roughness;

#if SUPPORT_GUIDE_GBUFFER
			PassParameters->ReflectionHitDistance = Inputs.ReflectionHitDistance;

#if !UE_VERSION_OLDER_THAN(5,8,0)
			PassParameters->ViewMatrix = (FMatrix44f)View.ViewMatrices.GetWorldToView();
			PassParameters->ProjectionMatrix = (FMatrix44f)View.ViewMatrices.GetViewToClipNoAA();
#else
			PassParameters->ViewMatrix = (FMatrix44f)View.ViewMatrices.GetViewMatrix();
			PassParameters->ProjectionMatrix = (FMatrix44f)View.ViewMatrices.GetProjectionNoAAMatrix();
#endif
#endif

#if SUPPORT_GUIDE_SSS_DOF
			PassParameters->SSSGuideBuffer = Inputs.SSSGuide;
			PassParameters->DOFGuideBuffer = Inputs.DOFGuide;
#endif
		}

		// Outputs 
		{
			PassParameters->SceneColorOutput = Outputs.SceneColor;
		}


#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
		if (Upscaler->NGXRHIExtensions->NeedExtraPassesForDebugLayerCompatibility())
		{
			AddDebugLayerCompatibilitySetupPasses(GraphBuilder, &PassParameters->DebugLayerCompatibility);
		}
#endif

		const float DeltaWorldTimeMS = View.Family->Time.GetDeltaWorldTimeSeconds() * 1000.0f;

		const bool bUseAutoExposure = CVarNGXDLSSAutoExposure.GetValueOnRenderThread() != 0;
		const bool bUseBiasCurrentColorMask = CVarNGXDLSSBiasCurrentColorMask.GetValueOnRenderThread() != 0;
		const bool bReleaseMemoryOnDelete = CVarNGXDLSSReleaseMemoryOnDelete.GetValueOnRenderThread() != 0;

		//if r.PostProcessing.PropagateAlpha is not enabled no reason incur a 20% perf cost upscaling alpha channel.
		static auto PropagateAlphaCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessing.PropagateAlpha"));
		
		const bool bEnableAlphaUpscaling = CVarNGXEnableAlphaUpscaling.GetValueOnRenderThread() >= 0 ? (CVarNGXEnableAlphaUpscaling.GetValueOnRenderThread() > 0) : PropagateAlphaCVar && (PropagateAlphaCVar->GetBool());
		const float ExposureScale = FMath::Clamp(CVarNGXDLSSExposureScale.GetValueOnRenderThread(), 0.0f, 1.0f);

		NGXRHI* LocalNGXRHIExtensions = Upscaler->NGXRHIExtensions;
		const int32 NGXDLSSPreset = GetNGXDLSSPresetFromQualityMode(DLSSQualityMode);
		const int32 NGXDLSSRRPreset = GetNGXDLSSRRPresetFromQualityMode(DLSSQualityMode);
		const int32 NGXPerfQuality = ToNGXQuality(DLSSQualityMode);

		auto NGXDenoiserModeString = [](ENGXDLSSDenoiserMode NGXDenoiserMode)
		{
			static_assert (int(ENGXDLSSDenoiserMode::MaxValue) == 1, "dear DLSS plugin NVIDIA developer, please update this code to handle the new ENGXDLSSDenoiserMode enum values");
			switch (NGXDenoiserMode)
			{
			case ENGXDLSSDenoiserMode::Off: return TEXT("");
			case ENGXDLSSDenoiserMode::DLSSRR: return TEXT("DLSSRR");
			default:return TEXT("Invalid ENGXDLSSDenoiserMode");
			}
		};
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("DLSS %s%s %dx%d -> %dx%d",
				PassName,
				NGXDenoiserModeString(Inputs.DenoiserMode),
				AdjustedInputViewRect.Width(), AdjustedInputViewRect.Height(),
				DestRect.Width(), DestRect.Height()),
			PassParameters,
			ERDGPassFlags::Compute | ERDGPassFlags::Raster | ERDGPassFlags::Copy |  ERDGPassFlags::SkipRenderPass,
			// FRHICommandListImmediate forces it to run on render thread, FRHICommandList doesn't
			[LocalNGXRHIExtensions, PassParameters, Inputs, AdjustedInputViewRect, bCameraCut, DeltaWorldTimeMS, NGXDLSSPreset, NGXDLSSRRPreset, NGXPerfQuality, DLSSState, bUseAutoExposure, ExposureScale, bEnableAlphaUpscaling, bReleaseMemoryOnDelete, bUseBiasCurrentColorMask](FRHICommandListImmediate& RHICmdList)
			{
				FRHIDLSSArguments DLSSArguments;
				FMemory::Memzero(&DLSSArguments, sizeof(DLSSArguments));

				// input parameters
				DLSSArguments.SrcRect = AdjustedInputViewRect;
				DLSSArguments.DestRect = Inputs.OutputViewRect;

				DLSSArguments.bReset = bCameraCut;

				DLSSArguments.JitterOffset = Inputs.TemporalJitterPixels;
				DLSSArguments.MotionVectorScale = FVector2f::UnitVector;

				DLSSArguments.DeltaTimeMS = DeltaWorldTimeMS;
				DLSSArguments.bReleaseMemoryOnDelete = bReleaseMemoryOnDelete;

				DLSSArguments.DLSSPreset = NGXDLSSPreset;
				DLSSArguments.DLSSRRPreset = NGXDLSSRRPreset;
				DLSSArguments.PerfQuality = NGXPerfQuality;

				check(PassParameters->SceneColorInput);
				PassParameters->SceneColorInput->MarkResourceAsUsed();
				DLSSArguments.InputColor = PassParameters->SceneColorInput->GetRHI();


				check(PassParameters->SceneVelocityInput);
				PassParameters->SceneVelocityInput->MarkResourceAsUsed();
				DLSSArguments.InputMotionVectors = PassParameters->SceneVelocityInput->GetRHI();


				if (bUseBiasCurrentColorMask && PassParameters->BiasCurrentColorInput)
				{
					PassParameters->BiasCurrentColorInput->MarkResourceAsUsed();
					DLSSArguments.InputBiasCurrentColorMask = PassParameters->BiasCurrentColorInput->GetRHI();
					DLSSArguments.bUseBiasCurrentColorMask = bUseBiasCurrentColorMask;
				}
				else
				{
					DLSSArguments.InputBiasCurrentColorMask = nullptr;
					DLSSArguments.bUseBiasCurrentColorMask = false;
				}

				check(PassParameters->SceneDepthInput);
				PassParameters->SceneDepthInput->MarkResourceAsUsed();
				DLSSArguments.InputDepth = PassParameters->SceneDepthInput->GetRHI();

				check(PassParameters->EyeAdaptation);
				PassParameters->EyeAdaptation->MarkResourceAsUsed();
				DLSSArguments.InputExposure = PassParameters->EyeAdaptation->GetRHI();
				DLSSArguments.PreExposure = Inputs.PreExposure;
				DLSSArguments.bUseAutoExposure = bUseAutoExposure;
				DLSSArguments.ExposureScale = ExposureScale;

				DLSSArguments.bEnableAlphaUpscaling = bEnableAlphaUpscaling;

				DLSSArguments.DenoiserMode = Inputs.DenoiserMode;

				static_assert (int(ENGXDLSSDenoiserMode::MaxValue) == 1, "dear DLSS plugin NVIDIA developer, please update this code to handle the new ENGXDLSSDenoiserMode enum values");
				if (DLSSArguments.DenoiserMode == ENGXDLSSDenoiserMode::DLSSRR)
				{
					check(PassParameters->DiffuseAlbedo)
						PassParameters->DiffuseAlbedo->MarkResourceAsUsed();
					DLSSArguments.InputDiffuseAlbedo = PassParameters->DiffuseAlbedo->GetRHI();

					check(PassParameters->SpecularAlbedo)
						PassParameters->SpecularAlbedo->MarkResourceAsUsed();
					DLSSArguments.InputSpecularAlbedo = PassParameters->SpecularAlbedo->GetRHI();

					check(PassParameters->Normal)
						PassParameters->Normal->MarkResourceAsUsed();
					DLSSArguments.InputNormals = PassParameters->Normal->GetRHI();

					check(PassParameters->Roughness)
						PassParameters->Roughness->MarkResourceAsUsed();
					DLSSArguments.InputRoughness = PassParameters->Roughness->GetRHI();

#if SUPPORT_GUIDE_GBUFFER
					if (PassParameters->ReflectionHitDistance)
					{
						PassParameters->ReflectionHitDistance->MarkResourceAsUsed();
						DLSSArguments.InputReflectionHitDistance = PassParameters->ReflectionHitDistance->GetRHI();

						FMemory::Memcpy(DLSSArguments.ViewMatrix, PassParameters->ViewMatrix.M, sizeof(float) * 16);
						FMemory::Memcpy(DLSSArguments.ProjectionMatrix, PassParameters->ProjectionMatrix.M, sizeof(float) * 16);
						//DLSSArguments.ProjectionMatrix = PassParameters->ProjectionMatrix;
					}
#endif

#if SUPPORT_GUIDE_SSS_DOF
					if (PassParameters->SSSGuideBuffer)
					{
						PassParameters->SSSGuideBuffer->MarkResourceAsUsed();
						DLSSArguments.InputSSS = PassParameters->SSSGuideBuffer->GetRHI();
					}
					if (PassParameters->DOFGuideBuffer)
					{
						PassParameters->DOFGuideBuffer->MarkResourceAsUsed();
						DLSSArguments.InputDOF = PassParameters->DOFGuideBuffer->GetRHI();
					}
#endif
				}

				


				// output images
				check(PassParameters->SceneColorOutput);
				PassParameters->SceneColorOutput->MarkResourceAsUsed();
				DLSSArguments.OutputColor = PassParameters->SceneColorOutput->GetRHI();

#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
				if (LocalNGXRHIExtensions->NeedExtraPassesForDebugLayerCompatibility())
				{
					DebugLayerCompatibilityRHISetup(PassParameters->DebugLayerCompatibility, DLSSArguments);
				}
#endif

			RHICmdList.EnqueueLambda(
				[LocalNGXRHIExtensions, DLSSArguments, DLSSState](FRHICommandListImmediate& Cmd) mutable
			{
				const uint32 FeatureCreationNode = CVarNGXDLSSFeatureCreationNode.GetValueOnRenderThread();
				const uint32 FeatureVisibilityMask = CVarNGXDLSSFeatureVisibilityMask.GetValueOnRenderThread();

				DLSSArguments.GPUNode = FeatureCreationNode == -1 ? Cmd.GetGPUMask().ToIndex() : FMath::Clamp(FeatureCreationNode, 0u, GNumExplicitGPUsForRendering - 1);
				DLSSArguments.GPUVisibility = FeatureVisibilityMask == -1 ? Cmd.GetGPUMask().GetNative() : (Cmd.GetGPUMask().All().GetNative() & FeatureVisibilityMask) ;
				LocalNGXRHIExtensions->ExecuteDLSS(Cmd, DLSSArguments, DLSSState);
			});
		});
	}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	check(OutputCustomHistoryInterface);
	(*OutputCustomHistoryInterface) = new FDLSSUpscalerHistory(DLSSState, Inputs.DenoiserMode);
#else
	if (!View.bStatePrevViewInfoIsReadOnly && OutputHistory)
	{
		OutputHistory->SafeRelease();

		GraphBuilder.QueueTextureExtraction(Outputs.SceneColor, &OutputHistory->RT[0]);

		OutputHistory->ViewportRect = DestRect;
		OutputHistory->ReferenceBufferSize = OutputExtent;
	}


	if (!View.bStatePrevViewInfoIsReadOnly && OutputCustomHistoryInterface)
	{
		if (!OutputCustomHistoryInterface->GetReference())
		{
			(*OutputCustomHistoryInterface) = new FDLSSUpscalerHistory(DLSSState, Inputs.DenoiserMode);
		}
	}
#endif
	return Outputs;
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
const TCHAR* FDLSSUpscalerHistory::GetDebugName() const
{
	static_assert (int(ENGXDLSSDenoiserMode::MaxValue) == 1, "dear DLSS plugin NVIDIA developer, please update this code to handle the new ENGXDLSSDenoiserMode enum values");
	return (DenoiserMode == ENGXDLSSDenoiserMode::DLSSRR) ? GDLSSRRSceneViewFamilyUpscalerDebugName : GDLSSSceneViewFamilyUpscalerDebugName;
}

uint64 FDLSSUpscalerHistory::GetGPUSizeBytes() const
{
	// TODO
	return 0;
}
#endif

void FDLSSUpscaler::Tick(FRHICommandListImmediate& RHICmdList)
{
	check(NGXRHIExtensions);
	check(IsInRenderingThread());
	// Pass it over to the RHI thread which handles the lifetime of the NGX DLSS resources
	RHICmdList.EnqueueLambda(
		[this](FRHICommandListImmediate& Cmd)
	{
		NGXRHIExtensions->TickPoolElements();
	});
}

bool FDLSSUpscaler::IsQualityModeSupported(EDLSSQualityMode InQualityMode) const
{
	return ResolutionSettings[ToNGXQuality(InQualityMode)].bIsSupported;
}

bool FDLSSUpscaler::IsDLSSActive() const
{
	static const auto CVarTemporalAAUpscaler = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TemporalAA.Upscaler"));
	static const IDLSSModuleInterface* DLSSModule = &FModuleManager::LoadModuleChecked<IDLSSModuleInterface>(TEXT("DLSS"));
	check(DLSSModule);
	check(CVarTemporalAAUpscaler);
	const bool bDLSSActive =
		DLSSModule->QueryDLSSSRSupport() == EDLSSSupport::Supported &&
		CVarTemporalAAUpscaler && (CVarTemporalAAUpscaler->GetInt() != 0) &&
		(CVarNGXDLSSEnable.GetValueOnAnyThread() != 0);
	return bDLSSActive;
}


void FDLSSUpscaler::SetupViewFamily(FSceneViewFamily& ViewFamily)
{
	const FIntPoint MinViewportSize(32, 32);

	for (const FSceneView* View : ViewFamily.Views)
	{
		if (View->UnscaledViewRect.Width() < MinViewportSize.X || View->UnscaledViewRect.Height() < MinViewportSize.Y)
		{
			UE_LOG(LogDLSS, Warning, TEXT("Could not setup DLSS upscaler for a view with UnscaledViewRect size (%d,%d). Minimum is (%d,%d)"), View->UnscaledViewRect.Width() , View->UnscaledViewRect.Height(), MinViewportSize.X, MinViewportSize.Y);
			return;
		}
	}

	const ISceneViewFamilyScreenPercentage* ScreenPercentageInterface = ViewFamily.GetScreenPercentageInterface();
	float DesiredResolutionFraction = ScreenPercentageInterface->GetResolutionFractionsUpperBound()[GDynamicPrimaryResolutionFraction];

	TOptional<EDLSSQualityMode> SelectedDLSSQualityMode;
	bool bAdaptQuality = true;

#if ENGINE_SUPPORTS_UPSCALER_MODULAR_FEATURE
	// Override quality from modular feature
	if (const FDLSSTemporalUpscalerModularFeature *DLSSModularFeature = FDLSSTemporalUpscalerModularFeature::Get())
	{
		if (ViewFamily.Views.Num() > 0 && ViewFamily.Views[0])
		{
			if (const FInstancedPropertyBag *CustomSettings = DLSSModularFeature->GetCustomSettings(*ViewFamily.Views[0]))
			{
				// Override quality mode
				int32 PixelCount = ViewFamily.Views[0]->UnscaledViewRect.Area();
				SelectedDLSSQualityMode = FDLSSTemporalUpscalerModularFeature::GetQualityMode(*CustomSettings, PixelCount);
				bAdaptQuality = false;
			}
		}
	}
#endif

	static_assert(int32(EDLSSQualityMode::NumValues) == 6, "dear DLSS plugin NVIDIA developer, please update this code to handle the new EDLSSQualityMode enum values");
	for (EDLSSQualityMode DLSSQualityMode : { EDLSSQualityMode::UltraPerformance,  EDLSSQualityMode::Performance , EDLSSQualityMode::Balanced, EDLSSQualityMode::Quality,  EDLSSQualityMode::UltraQuality, EDLSSQualityMode::DLAA })
	{
		if (!bAdaptQuality)
		{
			break;
		}

		bool bIsSupported = FDLSSUpscaler::IsQualityModeSupported(DLSSQualityMode);
		if (!bIsSupported)
		{
			continue;
		}

		float MinResolutionFraction = FDLSSUpscaler::GetMinResolutionFractionForQuality(DLSSQualityMode);
		float MaxResolutionFraction = FDLSSUpscaler::GetMaxResolutionFractionForQuality(DLSSQualityMode);
		float TargetResolutionFraction = FDLSSUpscaler::GetOptimalResolutionFractionForQuality(DLSSQualityMode);

		bool bIsCompatible = DesiredResolutionFraction <= 1.0 &&
			DesiredResolutionFraction >= (MinResolutionFraction - kDLSSResolutionFractionError) &&
			DesiredResolutionFraction <= (MaxResolutionFraction + kDLSSResolutionFractionError);
		bool bIsClosestYet = false;
		if (SelectedDLSSQualityMode.IsSet())
		{
			float SelectedTargetResolutionFraction = FDLSSUpscaler::GetOptimalResolutionFractionForQuality(SelectedDLSSQualityMode.GetValue());
			bIsClosestYet = FMath::Abs(TargetResolutionFraction - DesiredResolutionFraction) < FMath::Abs(SelectedTargetResolutionFraction - DesiredResolutionFraction);
		}
		else if (bIsCompatible)
		{
			bIsClosestYet = true;
		}

		if (bIsCompatible && bIsClosestYet)
		{
			SelectedDLSSQualityMode = DLSSQualityMode;
		}
	}

	if (SelectedDLSSQualityMode.IsSet())
	{
		ViewFamily.SetTemporalUpscalerInterface(new FDLSSSceneViewFamilyUpscaler(this, SelectedDLSSQualityMode.GetValue()));
	}
	else if (DesiredResolutionFraction != PreviousResolutionFraction)
	{
		UE_LOG(LogDLSS, Warning, TEXT("Could not setup DLSS upscaler for screen percentage = %f"), DesiredResolutionFraction * 100.0f);
	}
	PreviousResolutionFraction = DesiredResolutionFraction;
}

TOptional<EDLSSQualityMode> FDLSSUpscaler::GetAutoQualityModeFromPixels(int PixelCount) const
{
	if (PixelCount >= 8'300'000 && IsQualityModeSupported(EDLSSQualityMode::UltraPerformance))
	{
		return EDLSSQualityMode::UltraPerformance;
	}
	else if (PixelCount >= 3'690'000 && IsQualityModeSupported(EDLSSQualityMode::Performance))
	{
		return EDLSSQualityMode::Performance;
	}
	else if (PixelCount >= 2'030'000 && IsQualityModeSupported(EDLSSQualityMode::Quality))
	{
		return EDLSSQualityMode::Quality;
	}

	return TOptional<EDLSSQualityMode> {};
}


bool FDLSSUpscaler::EnableDLSSInPlayInEditorViewports() const
{
	if (GetDefault<UDLSSOverrideSettings>()->EnableDLSSInPlayInEditorViewportsOverride == EDLSSSettingOverride::UseProjectSettings)
	{
		return GetDefault<UDLSSSettings>()->bEnableDLSSInPlayInEditorViewports;
	}
	else
	{
		return GetDefault<UDLSSOverrideSettings>()->EnableDLSSInPlayInEditorViewportsOverride == EDLSSSettingOverride::Enabled;
	}
}

float FDLSSUpscaler::GetOptimalResolutionFractionForQuality(EDLSSQualityMode Quality) const
{
	checkf(IsQualityModeSupported(Quality),TEXT("%u is not a valid Quality mode"), Quality);
	return ResolutionSettings[ToNGXQuality(Quality)].OptimalResolutionFraction;
}

float FDLSSUpscaler::GetMinResolutionFractionForQuality(EDLSSQualityMode Quality) const
{
	checkf(IsQualityModeSupported(Quality), TEXT("%u is not a valid Quality mode"), Quality);
	return ResolutionSettings[ToNGXQuality(Quality)].MinResolutionFraction;
}

float FDLSSUpscaler::GetMaxResolutionFractionForQuality(EDLSSQualityMode Quality) const
{
	checkf(IsQualityModeSupported(Quality), TEXT("%u is not a valid Quality mode"), Quality);
	return ResolutionSettings[ToNGXQuality(Quality)].MaxResolutionFraction;
}

bool FDLSSUpscaler::IsFixedResolutionFraction(EDLSSQualityMode Quality) const
{
	checkf(IsQualityModeSupported(Quality), TEXT("%u is not a valid Quality mode"), Quality);
	return ResolutionSettings[ToNGXQuality(Quality)].IsFixedResolution();
}

#undef LOCTEXT_NAMESPACE
