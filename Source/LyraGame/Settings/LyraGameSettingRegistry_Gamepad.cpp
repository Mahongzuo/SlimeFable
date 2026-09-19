// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonInputBaseTypes.h"
#include "DataSource/GameSettingDataSource.h"
#include "EditCondition/WhenCondition.h"
#include "GameFramework/InputSettings.h"
#include "GameSettingCollection.h"
#include "GameSettingValueDiscreteDynamic.h"
#include "GameSettingValueScalarDynamic.h"
#include "LyraGameSettingRegistry.h"
#include "LyraSettingsLocal.h"
#include "LyraSettingsShared.h"
#include "NativeGameplayTags.h"
#include "Player/LyraLocalPlayer.h"

#define LOCTEXT_NAMESPACE "Lyra"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Platform_Trait_Input_SupportsGamepad, "Platform.Trait.Input.SupportsGamepad");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Platform_Trait_Input_SupportsTriggerHaptics, "Platform.Trait.Input.SupportsTriggerHaptics");

UGameSettingCollection* ULyraGameSettingRegistry::InitializeGamepadSettings(ULyraLocalPlayer* InLocalPlayer)
{
	UGameSettingCollection* Screen = NewObject<UGameSettingCollection>();
	Screen->SetDevName(TEXT("GamepadCollection"));
	Screen->SetDisplayName(LOCTEXT("GamepadCollection_Name", "Gamepad"));
	Screen->Initialize(InLocalPlayer);

	// Hardware
	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* Hardware = NewObject<UGameSettingCollection>();
		Hardware->SetDevName(TEXT("HardwareCollection"));
		Hardware->SetDisplayName(LOCTEXT("HardwareCollection_Name", "Hardware"));
		Screen->AddSetting(Hardware);
			
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueDiscreteDynamic* Setting = NewObject<UGameSettingValueDiscreteDynamic>();
			Setting->SetDevName(TEXT("ControllerHardware"));
			Setting->SetDisplayName(LOCTEXT("ControllerHardware_Name", "Controller Hardware"));
			Setting->SetDescriptionRichText(LOCTEXT("ControllerHardware_Description", "The type of controller you're using."));
			Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetControllerPlatform));
			Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetControllerPlatform));
			
			if (UCommonInputPlatformSettings* PlatformInputSettings = UPlatformSettingsManager::Get().GetSettingsForPlatform<UCommonInputPlatformSettings>())
			{
				const TArray<TSoftClassPtr<UCommonInputBaseControllerData>>& ControllerDatas = PlatformInputSettings->GetControllerData();
				for (TSoftClassPtr<UCommonInputBaseControllerData> ControllerDataPtr : ControllerDatas)
				{
					if (TSubclassOf<UCommonInputBaseControllerData> ControllerDataClass = ControllerDataPtr.LoadSynchronous())
					{
						const UCommonInputBaseControllerData* ControllerData = ControllerDataClass.GetDefaultObject();
						if (ControllerData->InputType == ECommonInputType::Gamepad)
						{
							Setting->AddDynamicOption(ControllerData->GamepadName.ToString(), ControllerData->GamepadDisplayName);
						}
					}
				}

				// Add the setting if we can select more than one game controller type on this platform
				// and we are allowed to change it
				if (Setting->GetDynamicOptions().Num() > 1 && PlatformInputSettings->CanChangeGamepadType())
				{
					Hardware->AddSetting(Setting);

					const FName CurrentControllerPlatform = GetDefault<ULyraSettingsLocal>()->GetControllerPlatform();
					if (CurrentControllerPlatform == NAME_None)
					{
						Setting->SetDiscreteOptionByIndex(0);
					}
					else
					{
						Setting->SetDefaultValueFromString(CurrentControllerPlatform.ToString());
					}
				}
			}
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueDiscreteDynamic_Bool* Setting = NewObject<UGameSettingValueDiscreteDynamic_Bool>();
			Setting->SetDevName(TEXT("GamepadVibration"));
			Setting->SetDisplayName(LOCTEXT("GamepadVibration_Name", "Vibration"));
			Setting->SetDescriptionRichText(LOCTEXT("GamepadVibration_Description", "Turns controller vibration on/off."));

			Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH(GetForceFeedbackEnabled));
			Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH(SetForceFeedbackEnabled));
			Setting->SetDefaultValue(GetDefault<ULyraSettingsShared>()->GetForceFeedbackEnabled());

			Hardware->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueDiscreteDynamic_Bool* Setting = NewObject<UGameSettingValueDiscreteDynamic_Bool>();
			Setting->SetDevName(TEXT("InvertVerticalAxis_Gamepad"));
			Setting->SetDisplayName(LOCTEXT("InvertVerticalAxis_Gamepad_Name", "Invert Vertical Axis"));
			Setting->SetDescriptionRichText(LOCTEXT("InvertVerticalAxis_Gamepad_Description", "Enable the inversion of the vertical look axis."));

			Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH(GetInvertVerticalAxis));
			Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH(SetInvertVerticalAxis));
			Setting->SetDefaultValue(GetDefault<ULyraSettingsShared>()->GetInvertVerticalAxis());

			Hardware->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueDiscreteDynamic_Bool* Setting = NewObject<UGameSettingValueDiscreteDynamic_Bool>();
			Setting->SetDevName(TEXT("InvertHorizontalAxis_Gamepad"));
			Setting->SetDisplayName(LOCTEXT("InvertHorizontalAxis_Gamepad_Name", "Invert Horizontal Axis"));
			Setting->SetDescriptionRichText(LOCTEXT("InvertHorizontalAxis_Gamepad_Description", "Enable the inversion of the Horizontal look axis."));

			Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH(GetInvertHorizontalAxis));
			Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH(SetInvertHorizontalAxis));
			Setting->SetDefaultValue(GetDefault<ULyraSettingsShared>()->GetInvertHorizontalAxis());

			Hardware->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
	}

	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* GamepadBinding = NewObject<UGameSettingCollection>();
		GamepadBinding->SetDevName(TEXT("GamepadBindingCollection"));
		GamepadBinding->SetDisplayName(LOCTEXT("GamepadBindingCollection_Name", "Controls"));
		Screen->AddSetting(GamepadBinding);
	}

	// Basic - Look Sensitivity
	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* BasicSensitivity = NewObject<UGameSettingCollection>();
		BasicSensitivity->SetDevName(TEXT("BasicSensitivityCollection"));
		BasicSensitivity->SetDisplayName(LOCTEXT("BasicSensitivityCollection_Name", "Sensitivity"));
		Screen->AddSetting(BasicSensitivity);

		const FText EGamepadSensitivityText[] = {
			FText::GetEmpty(),
			LOCTEXT("EFortGamepadSensitivity_Slow", "1 (Slow)"),
			LOCTEXT("EFortGamepadSensitivity_SlowPlus", "2 (Slow+)"),
			LOCTEXT("EFortGamepadSensitivity_SlowPlusPlus", "3 (Slow++)"),
			LOCTEXT("EFortGamepadSensitivity_Normal", "4 (Normal)"),
			LOCTEXT("EFortGamepadSensitivity_NormalPlus", "5 (Normal+)"),
			LOCTEXT("EFortGamepadSensitivity_NormalPlusPlus", "6 (Normal++)"),
			LOCTEXT("EFortGamepadSensitivity_Fast", "7 (Fast)"),
			LOCTEXT("EFortGamepadSensitivity_FastPlus", "8 (Fast+)"),
			LOCTEXT("EFortGamepadSensitivity_FastPlusPlus", "9 (Fast++)"),
			LOCTEXT("EFortGamepadSensitivity_Insane", "10 (Insane)"),
		};
		
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueDiscreteDynamic_Enum* Setting = NewObject<UGameSettingValueDiscreteDynamic_Enum>();
			Setting->SetDevName(TEXT("LookSensitivityPreset"));
			Setting->SetDisplayName(LOCTEXT("LookSensitivityPreset_Name", "Look Sensitivity"));
			Setting->SetDescriptionRichText(LOCTEXT("LookSensitivityPreset_Description", "How quickly your view rotates."));
			
			Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH(GetGamepadLookSensitivityPreset));
			Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH(SetLookSensitivityPreset));
			Setting->SetDefaultValue(GetDefault<ULyraSettingsShared>()->GetGamepadLookSensitivityPreset());

			for (int32 PresetIndex = 1; PresetIndex < (int32)ELyraGamepadSensitivity::MAX; PresetIndex++)
			{
				Setting->AddEnumOption(static_cast<ELyraGamepadSensitivity>(PresetIndex), EGamepadSensitivityText[PresetIndex]);
			}
			
			BasicSensitivity->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueDiscreteDynamic_Enum* Setting = NewObject<UGameSettingValueDiscreteDynamic_Enum>();
			Setting->SetDevName(TEXT("LookSensitivityPresetAds"));
			Setting->SetDisplayName(LOCTEXT("LookSensitivityPresetAds_Name", "Aim Sensitivity (ADS)"));
			Setting->SetDescriptionRichText(LOCTEXT("LookSensitivityPresetAds_Description", "How quickly your view rotates while aiming down sights (ADS)."));
			
			Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH(GetGamepadTargetingSensitivityPreset));
			Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH(SetGamepadTargetingSensitivityPreset));
			Setting->SetDefaultValue(GetDefault<ULyraSettingsShared>()->GetGamepadTargetingSensitivityPreset());

			for (int32 PresetIndex = 1; PresetIndex < (int32)ELyraGamepadSensitivity::MAX; PresetIndex++)
			{
				Setting->AddEnumOption(static_cast<ELyraGamepadSensitivity>(PresetIndex), EGamepadSensitivityText[PresetIndex]);
			}

			BasicSensitivity->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
	}

	// Dead Zone
	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* DeadZone = NewObject<UGameSettingCollection>();
		DeadZone->SetDevName(TEXT("DeadZoneCollection"));
		DeadZone->SetDisplayName(LOCTEXT("DeadZoneCollection_Name", "Controller DeadZone"));
		Screen->AddSetting(DeadZone);

		//----------------------------------------------------------------------------------
		{
			UGameSettingValueScalarDynamic* Setting = NewObject<UGameSettingValueScalarDynamic>();
			Setting->SetDevName(TEXT("MoveStickDeadZone"));
			Setting->SetDisplayName(LOCTEXT("MoveStickDeadZone_Name", "Left Stick DeadZone"));
			Setting->SetDescriptionRichText(LOCTEXT("MoveStickDeadZone_Description", "Increase or decrease the area surrounding the stick that we ignore input from.  Setting this value too low may result in the character continuing to move even after removing your finger from the stick."));

			Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH(GetGamepadMoveStickDeadZone));
			Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH(SetGamepadMoveStickDeadZone));
			Setting->SetDefaultValue(GetDefault<ULyraSettingsShared>()->GetGamepadMoveStickDeadZone());
			Setting->SetDisplayFormat(UGameSettingValueScalarDynamic::ZeroToOnePercent);
			Setting->SetMinimumLimit(0.05);
			Setting->SetMaximumLimit(0.95);

			DeadZone->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
		{
			UGameSettingValueScalarDynamic* Setting = NewObject<UGameSettingValueScalarDynamic>();
			Setting->SetDevName(TEXT("LookStickDeadZone"));
			Setting->SetDisplayName(LOCTEXT("LookStickDeadZone_Name", "Right Stick DeadZone"));
			Setting->SetDescriptionRichText(LOCTEXT("LookStickDeadZone_Description", "Increase or decrease the area surrounding the stick that we ignore input from.  Setting this value too low may result in the camera continuing to move even after removing your finger from the stick."));

			Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH(GetGamepadLookStickDeadZone));
			Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH(SetGamepadLookStickDeadZone));
			Setting->SetDefaultValue(GetDefault<ULyraSettingsShared>()->GetGamepadLookStickDeadZone());
			Setting->SetDisplayFormat(UGameSettingValueScalarDynamic::ZeroToOnePercent);
			Setting->SetMinimumLimit(0.05);
			Setting->SetMaximumLimit(0.95);

			DeadZone->AddSetting(Setting);
		}
		//----------------------------------------------------------------------------------
	}

	// Gamepad Input API
	////////////////////////////////////////////////////////////////////////////////////
	{
		UGameSettingCollection* GamepadInputAPI = NewObject<UGameSettingCollection>();
		GamepadInputAPI->SetDevName(TEXT("GamepadInputAPI"));
		GamepadInputAPI->SetDisplayName(LOCTEXT("GamepadInputAPI_Name", "Gamepad Input API"));
		Screen->AddSetting(GamepadInputAPI);

		// Enum option for which input API to use
		{
			UGameSettingValueDiscreteDynamic_Enum* Setting = NewObject<UGameSettingValueDiscreteDynamic_Enum>();
			Setting->SetDevName(TEXT("GamepadInputAPI_Option"));
			Setting->SetDisplayName(LOCTEXT("GamepadInputAPI_Option_Name", "Gamepad Input API"));
			Setting->SetDescriptionRichText(LOCTEXT("GamepadInputAPI_Option_Description", "Which API should be used for processing gamepad input on PC devices.\n\nChanging this may improve compatibility with more input devices.\n\nChanging this setting requires a restart to take effect."));

			Setting->SetDynamicGetter(GET_SHARED_SETTINGS_FUNCTION_PATH(GetGamepadInputAPIOption));
			Setting->SetDynamicSetter(GET_SHARED_SETTINGS_FUNCTION_PATH(SetGamepadInputAPIOption));
			Setting->SetDefaultValue(GetDefault<ULyraSettingsShared>()->GetGamepadInputAPIOption());

			// This feature is only available on PC
			const TSharedRef<FWhenCondition> WhenPlatformSupportsChangingGamepadAPI = MakeShared<FWhenCondition>(
			[](const ULocalPlayer*, FGameSettingEditableState& InOutEditState)
			{
				// GameInput is unavailable on WinArm64 now, this is expected to change by mid 2026.
				if (!PLATFORM_WINDOWS || (PLATFORM_CPU_ARM_FAMILY && !PLATFORM_WINDOWS_ARM64EC) )
				{
					InOutEditState.Kill(TEXT("Platform does not support changing gamepad input API"));
				}

				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				if (!GetDefault<UInputSettings>()->bEnablePreferredInputAPIPreferences)
				{
					InOutEditState.Kill(TEXT("Project does not have bEnablePreferredInputAPIPreferences enabled."));
				}
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
			});

			Setting->AddEditCondition(WhenPlatformSupportsChangingGamepadAPI);
			
			Setting->AddEnumOption(ELyraGamepadInputAPIOption::Legacy, LOCTEXT("GamepadAPI_Option_Legacy", "XInput + DualShock"));
			Setting->AddEnumOption(ELyraGamepadInputAPIOption::Modern, LOCTEXT("GamepadAPI_Option_Modern", "Game Input"));

			GamepadInputAPI->AddSetting(Setting);
		}
	}

	return Screen;
}

#undef LOCTEXT_NAMESPACE
