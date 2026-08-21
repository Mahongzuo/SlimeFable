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
#include "DLSSEditor.h"

#include "DLSSUpscaler.h"
#include "DLSS.h"
#include "DLSSSettings.h"
#include "NGXRHI.h"

#include "CoreMinimal.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "UObject/Class.h"
#include "UObject/WeakObjectPtr.h"



#define LOCTEXT_NAMESPACE "FDLSSEditorModule"

DEFINE_LOG_CATEGORY(LogDLSSEditor);

void FDLSSEditorModule::StartupModule()
{
	UE_LOG(LogDLSSEditor, Log, TEXT("%s Enter"), ANSI_TO_TCHAR(__FUNCTION__));
	
	check(GIsEditor);

	bool bIsDLSS_SR_Available = false;
	bool bIsDLSS_RR_Available = false;
	
	// verify that the other DLSS modules are correctly hooked up
	{
		IDLSSModuleInterface* DLSSModule = &FModuleManager::LoadModuleChecked<IDLSSModuleInterface>(TEXT("DLSS"));

		bIsDLSS_SR_Available = DLSSModule->QueryDLSSSRSupport() == EDLSSSupport::Supported;
		bIsDLSS_RR_Available = DLSSModule->QueryDLSSRRSupport() == EDLSSSupport::Supported;

		UE_LOG(LogDLSSEditor, Log, TEXT("DLSS module=%p, DLSS supported DLSS-SR=%u, DLSS-RR=%u DLSSUpscaler = %p"), DLSSModule, 
			bIsDLSS_SR_Available, bIsDLSS_RR_Available, DLSSModule->GetDLSSUpscaler());
	}

	// register settings
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			{
				auto Settings = GetMutableDefault<UDLSSSettings>();
				if(bIsDLSS_SR_Available)
				{
					IDLSSModuleInterface* DLSSModule = &FModuleManager::LoadModuleChecked<IDLSSModuleInterface>(TEXT("DLSS"));
					const NGXRHI* NGXRHIExtensions = DLSSModule->GetDLSSUpscaler()->GetNGXRHI();
					Settings->GenericDLSSSRBinaryPath = NGXRHIExtensions->GetDLSSSRGenericBinaryInfo().Get<0>();
					Settings->bGenericDLSSSRBinaryExists = NGXRHIExtensions->GetDLSSSRGenericBinaryInfo().Get<1>();

					Settings->CustomDLSSSRBinaryPath = NGXRHIExtensions->GetDLSSSRCustomBinaryInfo().Get<0>();
					Settings->bCustomDLSSSRBinaryExists = NGXRHIExtensions->GetDLSSSRCustomBinaryInfo().Get<1>();


					Settings->GenericDLSSRRBinaryPath = NGXRHIExtensions->GetDLSSRRGenericBinaryInfo().Get<0>();
					Settings->bGenericDLSSRRBinaryExists = NGXRHIExtensions->GetDLSSRRGenericBinaryInfo().Get<1>();

					Settings->CustomDLSSRRBinaryPath = NGXRHIExtensions->GetDLSSRRCustomBinaryInfo().Get<0>();
					Settings->bCustomDLSSRRBinaryExists = NGXRHIExtensions->GetDLSSRRCustomBinaryInfo().Get<1>();



				}

				ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "DLSS",
					LOCTEXT("DLSSSettingsName", "NVIDIA DLSS"),
					LOCTEXT("DLSSSettingsDescription", "Configure the NVIDIA DLSS plug-in."),
					Settings
				);
			}

			{
				auto Settings = GetMutableDefault<UDLSSOverrideSettings>();

				ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "DLSSOverride",
					LOCTEXT("DLSSOverrideSettingsName", "NVIDIA DLSS Overrides (Local)"),
					LOCTEXT("DLSSOverrideSettingsDescription", "Configure the local settings for the NVIDIA DLSS plug-in."),
					Settings
				);
			}
		}
	}

	UE_LOG(LogDLSSEditor, Log, TEXT("%s Leave"), ANSI_TO_TCHAR(__FUNCTION__));
}

void FDLSSEditorModule::ShutdownModule()
{
	UE_LOG(LogDLSSEditor, Log, TEXT("%s Enter"), ANSI_TO_TCHAR(__FUNCTION__));
	
	UE_LOG(LogDLSSEditor, Log, TEXT("%s Leave"), ANSI_TO_TCHAR(__FUNCTION__));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDLSSEditorModule, DLSSEditor)

