// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraSettingValueDiscrete_Display.h"

#include "GameFramework/GameUserSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "UnrealEngine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraSettingValueDiscrete_Display)

#define LOCTEXT_NAMESPACE "LyraSettings"

ULyraSettingValueDiscrete_Display::ULyraSettingValueDiscrete_Display()
{
}

void ULyraSettingValueDiscrete_Display::BeginDestroy()
{
	Super::BeginDestroy();

	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<class GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication();
		if (PlatformApplication.IsValid())
		{
			GenericApplication::FOnDisplayMetricsChanged& DisplayMetricsChangedEvent = PlatformApplication->OnDisplayMetricsChanged();
			DisplayMetricsChangedEvent.Remove(DisplayMetricsChangedHandle);
		}
	}
}

void ULyraSettingValueDiscrete_Display::OnInitialized()
{
	Super::OnInitialized();

	TSharedPtr<class GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication();
	if (ensure(PlatformApplication.IsValid()))
	{
		FDisplayMetrics::RebuildDisplayMetrics(CurrentDisplayMetrics);

		GenericApplication::FOnDisplayMetricsChanged& DisplayMetricsChangedEvent = PlatformApplication->OnDisplayMetricsChanged();
		if (!DisplayMetricsChangedEvent.IsBoundToObject(this))
		{
			DisplayMetricsChangedHandle = DisplayMetricsChangedEvent.AddUObject(this, &ULyraSettingValueDiscrete_Display::OnDisplayMetricsChanged);
		}
	}
}

void ULyraSettingValueDiscrete_Display::StoreInitial()
{
	const UGameUserSettings* const UserSettings = GEngine->GetGameUserSettings();
	InitialMonitorID = UserSettings->GetDisplayID();
	InitialMonitorIndex = UserSettings->GetDisplayIndex();
}

void ULyraSettingValueDiscrete_Display::ResetToDefault()
{
	// Initially not implemented.
}

void ULyraSettingValueDiscrete_Display::RestoreToInitial()
{
	// Initially not implemented.
}

void ULyraSettingValueDiscrete_Display::SetDiscreteOptionByIndex(int32 Index)
{
	if (CurrentDisplayMetrics.MonitorInfo.IsValidIndex(Index))
	{
		GEngine->GetGameUserSettings()->SetDisplayProperties(CurrentDisplayMetrics.MonitorInfo[Index].ID, Index);
		NotifySettingChanged(EGameSettingChangeReason::Change);
	}
}

int32 ULyraSettingValueDiscrete_Display::GetDiscreteOptionIndex() const
{
	const UGameUserSettings* const UserSettings = GEngine->GetGameUserSettings();

	return CurrentDisplayMetrics.GetClosestMonitorFromIDAndIndex(UserSettings->GetDisplayID(), UserSettings->GetDisplayIndex());
}

int32 ULyraSettingValueDiscrete_Display::GetDiscreteOptionDefaultIndex() const
{
	return CurrentDisplayMetrics.GetClosestMonitorFromIDAndIndex(InitialMonitorID, InitialMonitorIndex);
}

TArray<FText> ULyraSettingValueDiscrete_Display::GetDiscreteOptions() const
{
	TArray<FText> Options;

	static FText UnknownDisplayText = LOCTEXT("UnknownDisplay", "[Unknown]");
	if (CurrentDisplayMetrics.MonitorInfo.IsEmpty())
	{
		Options.Emplace(UnknownDisplayText);
	}
	else
	{
		for (const FMonitorInfo& Monitor : CurrentDisplayMetrics.MonitorInfo)
		{
			Options.Emplace(Monitor.FriendlyName.IsEmpty()
				? (Monitor.Name.IsEmpty()
					? UnknownDisplayText
					: FText::FromString(Monitor.Name))
				: FText::FromString(Monitor.FriendlyName));
		}
	}

	return Options;
}

void ULyraSettingValueDiscrete_Display::OnDependencyChanged()
{
	UGameUserSettings* const UserSettings = GEngine->GetGameUserSettings();
	const FString DisplayID = UserSettings->GetDisplayID();
	const int32 DisplayIndex = UserSettings->GetDisplayIndex();
	SetDiscreteOptionByIndex(CurrentDisplayMetrics.GetClosestMonitorFromIDAndIndex(DisplayID, DisplayIndex));
}

void ULyraSettingValueDiscrete_Display::OnDisplayMetricsChanged(const FDisplayMetrics& NewDisplayMetrics)
{
	CurrentDisplayMetrics = NewDisplayMetrics;
}

#undef LOCTEXT_NAMESPACE
