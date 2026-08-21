/*
* Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
* NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
* property and proprietary rights in and to this material, related
* documentation and any modifications thereto. Any use, reproduction,
* disclosure or distribution of this material and related documentation
* without an express license agreement from NVIDIA CORPORATION or
* its affiliates is strictly prohibited.
*/
#pragma once

#include "Modules/ModuleManager.h"


#define UE_API STREAMLINEEXTENSION_API


namespace sl
{
	using Feature = uint32_t;
}

struct FSLFeatureDesc
{
	// ctor covers most common cases
	//
	//  CommandLineSuffix is a lowercase version of InFeatureName with any non-alphanumeric characters removed
	//  LoadCVar is "r.Streamline.Load." + InFeatureName with any non-alphanumeric characters removed
	//
	// For example, if InFeatureName = "Reflex" then CommandLineSuffix = "reflex" and LoadCVar = "r.Streamline.Load.Reflex"
	UE_API FSLFeatureDesc(sl::Feature InSLFeature, const FString& InUEPluginName, const FString& InFeatureName);
	// default ctor in case you want to manually set all the fields yourself
	FSLFeatureDesc() = default;

	sl::Feature SLFeature;
	FString UEPluginName;
	FString FeatureName;
	FString CommandLineSuffix;
	FString LoadCVar;
	TArray<FString> BinaryDirectories;
	bool bAllowByDefault = true;
};


// This module exists for plugins to register their Streamline feature during PostConfigInit before Streamline is initialized in PostSplashScreen
class FStreamlineExtensionModule: public IModuleInterface
{
public:
	//////////////
	// Common API
	static UE_API FStreamlineExtensionModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FStreamlineExtensionModule>(FName("StreamlineExtension"));
	}

	//////////////////////////////////////////////
	// API for plugins that implement SL features

	// Must be called before streamline init. Returns false if feature failed to register.
	UE_API bool RegisterFeature(const FSLFeatureDesc& Feature);

	// Gets default base directory for extra binaries from plugin name.
	// Use this if you store binaries in the {PluginDir}\Binaries\ThirdParty\{Platform} folder.
	// Note that NGX binaries (nvngx_*.dll) are ok to store here but SL binaries (sl.*.dll) won't work due to SL limitations
	static UE_API FString GetDefaultBinaryBaseDir(const FStringView PluginName);

	/////////////////////////
	// API for StreamlineRHI

	UE_API const TArray<FSLFeatureDesc>& GetRegisteredFeatures() const;
	UE_API void SetSLInitialized();

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	bool bIsStreamlineInitialized = false;
	TArray<FSLFeatureDesc> Features;
};

#undef UE_API

