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

#include "Engine/DeveloperSettings.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#define UE_API STREAMLINERHI_API

#include "StreamlineSettings.generated.h"

UENUM()
enum class EStreamlineSettingOverride : uint8
{
	Enabled UMETA(DisplayName = "True"),
	Disabled UMETA(DisplayName = "False"),
	UseProjectSettings UMETA(DisplayName = "Use project settings"),
};

UCLASS(MinimalAPI, Config = Engine, ProjectUserConfig)
class UStreamlineOverrideSettings : public UObject
{
	GENERATED_BODY()

public:


	/**
	 * Load the Streamline debug overlay in non-Shipping configurations. Note that the overlay requires DLSS Frame Generation to be available.
	 * Modifying this setting requires an editor restart to take effect. Saved to local user config only
	 */
	UPROPERTY(Config, EditAnywhere, Category = "General Settings (Local)", DisplayName = "Load Debug Overlay", meta = (ConfigRestartRequired = true))
	EStreamlineSettingOverride LoadDebugOverlayOverride = EStreamlineSettingOverride::UseProjectSettings;

	/** Allow OTA updates of Streamline features */
	UPROPERTY(Config, EditAnywhere, Category = "General Settings (Local)", DisplayName = "Allow OTA update", meta = (ConfigRestartRequired = true))
	EStreamlineSettingOverride AllowOTAUpdateOverride = EStreamlineSettingOverride::UseProjectSettings;

	/**
	* Enable DLSS Frame Generation in New Editor Window Play In Editor mode.
	* Saved to local user config only.
	* Note: DLSS Frame Generation is not supported in editor viewports
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Editor (Local)", DisplayName = "Enable DLSS-FG in New Editor Window (PIE) mode")
	EStreamlineSettingOverride EnableDLSSFGInPlayInEditorViewportsOverride = EStreamlineSettingOverride::UseProjectSettings;

	/** Use deprecated slSetTag instead of slSetTagForFrame.  Saved to local user config only **/
	UPROPERTY(Config, EditAnywhere, Category = "Compatibility (Local)", DisplayName = "Use slSetTag (deprecated)", AdvancedDisplay, meta = (ConfigRestartRequired = true))
	EStreamlineSettingOverride UseSlSetTagOverride = EStreamlineSettingOverride::UseProjectSettings;

	/** Use Slate callbacks (OnSlateWindowDestroyed, OnPreResizeWindowBackBuffer) for swapchain tracking for DLSS-FG, instead of the DXGI swapchain proxy. Only supported for UE 5.7 and older. Saved to local user config only */
	UPROPERTY(Config, EditAnywhere, Category = "Compatibility (Local)", DisplayName = "Use Slate callbacks for Swapchain tracking (deprecated)", AdvancedDisplay, meta = (ConfigRestartRequired = true))
	EStreamlineSettingOverride UseSlateCallbacksForSwapchainTrackingOverride = EStreamlineSettingOverride::UseProjectSettings;
};

UCLASS(MinimalAPI, Config = Engine, DefaultConfig)
class UStreamlineSettings: public UObject
{
	GENERATED_BODY()

public:

	/**
 * Load the Streamline debug overlay in non-Shipping configurations. Note that the overlay requires DLSS Frame Generation to be available.
 * This project wide setting can be locally overridden in the NVIDIA DLSS Frame Generation (Local) settings.
 * Modifying this setting requires an editor restart to take effect
 */
	UPROPERTY(Config, EditAnywhere, Category = "General Settings", DisplayName = "Load Debug Overlay", meta = (ConfigRestartRequired = true))
	bool bLoadDebugOverlay = false;

	/** Allow OTA updates of Streamline features */
	UPROPERTY(Config, EditAnywhere, Category = "General Settings", DisplayName = "Allow OTA update", meta = (ConfigRestartRequired = true))
	bool bAllowOTAUpdate = true;

	/** By default the DLSS Frame Generation plugin uses the UE Project ID to initialize Streamline. In some cases NVIDIA might provide a separate NVIDIA Application ID, which should be put here. */
	UPROPERTY(Config, EditAnywhere, Category = "General Settings", DisplayName = "NVIDIA NGX Application ID", AdvancedDisplay, meta = (ConfigRestartRequired = true))
	int32 NVIDIANGXApplicationId = 0;

	
	/** Enable plugin features for D3D12, if the driver supports it at runtime */
	UPROPERTY(Config, EditAnywhere, Category = "General Settings", DisplayName = "Enable plugin features for the D3D12RHI", meta = (ConfigRestartRequired = true))
	bool bEnableStreamlineD3D12 = PLATFORM_WINDOWS;

	/** Enable plugin features for D3D11, if the driver supports it at runtime */
	UPROPERTY(Config, EditAnywhere, Category = "General Settings", DisplayName = "Enable plugin features for the D3D11RHI (Reflex only)", meta = (ConfigRestartRequired = true))
	bool bEnableStreamlineD3D11 = PLATFORM_WINDOWS;

	/**
	 * Enable DLSS Frame Generation in New Editor Window Play In Editor mode. Not supported for Network PIE with multiple floating PIE windows.
	 * This project wide setting can be locally overridden in the NVIDIA DLSS Frame Generation (Local) settings.
	 * Note: DLSS Frame Generation is not supported in editor viewports
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Editor", DisplayName = "Enable DLSS-FG in New Editor Window (PIE) mode")
	bool bEnableDLSSFGInPlayInEditorViewports = true;


	/** Use deprecated slSetTag instead of slSetTagForFrame.*/
	UPROPERTY(Config, EditAnywhere, Category = "Compatibility", DisplayName = "Use slSetTag (deprecated)", AdvancedDisplay, meta = (ConfigRestartRequired = true))
	bool bUseSlSetTag = false;

	/** Use Slate callbacks (OnSlateWindowDestroyed, OnPreResizeWindowBackBuffer) for swapchain tracking for DLSS-FG, instead of the DXGI swapchain proxy. Only supported for UE 5.7 and older. */
	UPROPERTY(Config,    EditAnywhere, Category = "Compatibility", DisplayName = "Use Slate callbacks for Swapchain tracking (deprecated)", AdvancedDisplay, meta = (ConfigRestartRequired = true))
	bool bUseSlateCallbacksForSwapchainTracking = false;


	// when we need to read some of the settings in FStreamlineRHIModule::InitializeStreamline() we don't have the UOBject system loaded
	// so we can't use GetDefault/GetMutableDefault
	// also can't use UStreamlineSettings() since the UStreamlineSettings::ctor is only used by the UObject system, which we don't have initialized yet
	// so we recreate that manually at the call site later by explicitly loading from the config file
	// but if the config file doesn't have the settings then we want to use the defaults that are here in C++
	static TObjectPtr <UStreamlineSettings> CppDefaults()
	{
		// This generates the following expected log output
		// LogObj: Display: Attempting to load config data for Default__StreamlineSettings before the Class has been constructed/registered/linked (likely during module loading or early startup). This will result in the load silently failing and should be fixed.
		TObjectPtr<UStreamlineSettings> Result = NewObject<UStreamlineSettings>();
		return Result;
		
	}
};

#undef UE_API
