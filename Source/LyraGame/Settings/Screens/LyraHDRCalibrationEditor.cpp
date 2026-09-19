// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraHDRCalibrationEditor.h"

#include "ColorManagement/TransferFunctions.h"
#include "CommonButtonBase.h"
#include "CommonRichTextBlock.h"
#include "Components/Image.h"
#include "Components/WidgetSwitcher.h"
#include "GameSettingValueScalar.h"
#include "Settings/LyraSettingsLocal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraHDRCalibrationEditor)

struct FGeometry;

#define LOCTEXT_NAMESPACE "Lyra"

namespace HDRCalibrationEditor
{
	const float JoystickDeadZone = .2f;
	const float HDRCalibrationChangeSpeed = .01f;
	const float HDRCalibrationMinimumPQ = .51f; // about 102 nits, first hundredths value about SDR
}

ULyraHDRCalibrationEditor::ULyraHDRCalibrationEditor(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
}

void ULyraHDRCalibrationEditor::NativeOnActivated()
{
	Super::NativeOnActivated();

	// Render UI to 10000 nits for calibration.
	IConsoleVariable* const CVarUILevel = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HDR.UI.Level"));
	bStartingUILevel = CVarUILevel->GetFloat();
	CVarUILevel->SetWithCurrentPriority(1.f);
	ULyraSettingsLocal* const Settings = ULyraSettingsLocal::Get();
	bStartingUILuminance = Settings->GetHDRUILuminanceNits();
	Settings->SetHDRUILuminanceNits(10000.f);
	bStartingUILuminanceSeparate = Settings->IsHDRUILuminanceSeparate();
	Settings->SetHDRUILuminanceSeparate(true);

	const float MaxLuminance = Settings->GetMaximumHDRDisplayNits() / 10000.f;
	MaxLuminancePQ = UE::Color::EncodeNormalizedToST2084(MaxLuminance);
	OnMaxLuminanceChange(MaxLuminance);

	Button_Done->OnClicked().AddUObject(this, &ULyraHDRCalibrationEditor::HandleDoneClicked);

	Button_Back->SetVisibility((bCanCancel)? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bCanCancel)
	{
		Button_Back->OnClicked().AddUObject(this, &ULyraHDRCalibrationEditor::HandleBackClicked);
	}
}

void ULyraHDRCalibrationEditor::NativeOnDeactivated()
{
	// Restore UI CVars.
	IConsoleVariable* const CVarUILevel = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HDR.UI.Level"));
	CVarUILevel->SetWithCurrentPriority(bStartingUILevel);
	ULyraSettingsLocal* const Settings = ULyraSettingsLocal::Get();
	Settings->SetHDRUILuminanceNits(bStartingUILuminance);
	Settings->SetHDRUILuminanceSeparate(bStartingUILuminanceSeparate);

	Super::NativeOnDeactivated();
}

bool ULyraHDRCalibrationEditor::ExecuteActionForSetting_Implementation(FGameplayTag ActionTag, UGameSetting* InSetting)
{
	if (InSetting)
	{
		TArray<UGameSetting*> ChildSettings = InSetting->GetChildSettings();
		if (!ChildSettings.IsEmpty())
		{
			ValueSetting = Cast<UGameSettingValueScalar>(ChildSettings[0]);
		}
	}

	return true;
}

FReply ULyraHDRCalibrationEditor::NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent)
{
	if (InAnalogEvent.GetKey() == EKeys::Gamepad_LeftY && FMath::Abs(InAnalogEvent.GetAnalogValue()) >= HDRCalibrationEditor::JoystickDeadZone)
	{
		const float UnclampedPQ = MaxLuminancePQ + InAnalogEvent.GetAnalogValue() * HDRCalibrationEditor::HDRCalibrationChangeSpeed;
		MaxLuminancePQ = FMath::Clamp(UnclampedPQ, HDRCalibrationEditor::HDRCalibrationMinimumPQ, 1.f);
		OnMaxLuminanceChange(UE::Color::DecodeNormalizedFromST2084(MaxLuminancePQ));
		
		return FReply::Handled();
	}
	return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogEvent);
}

FReply ULyraHDRCalibrationEditor::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const float UnclampedPQ = MaxLuminancePQ + InMouseEvent.GetWheelDelta() * HDRCalibrationEditor::HDRCalibrationChangeSpeed;
	MaxLuminancePQ = FMath::Clamp(UnclampedPQ, HDRCalibrationEditor::HDRCalibrationMinimumPQ, 1.f);
	OnMaxLuminanceChange(UE::Color::DecodeNormalizedFromST2084(MaxLuminancePQ));

	return FReply::Handled();
}

void ULyraHDRCalibrationEditor::HandleBackClicked()
{
	DeactivateWidget();
}

void ULyraHDRCalibrationEditor::HandleDoneClicked()
{
	const float MaxLuminance = UE::Color::DecodeST2084(MaxLuminancePQ);
	if (ValueSetting.IsValid())
	{
		ValueSetting.Get()->SetValue(MaxLuminance);
	}
	else
	{
		ULyraSettingsLocal::Get()->SetMaximumHDRDisplayNits(MaxLuminance);
	}
	DeactivateWidget();
}

#undef LOCTEXT_NAMESPACE
