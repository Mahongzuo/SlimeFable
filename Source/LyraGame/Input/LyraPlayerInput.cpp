// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPlayerInput.h"
#include "Performance/LatencyMarkerModule.h"
#include "Settings/LyraSettingsLocal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPlayerInput)

ULyraPlayerInput::ULyraPlayerInput()
	: Super()
{
	// Don't bind to any settings delegates on the CDO, otherwise there would be a constant bound listener
	// and it wouldn't even do anything because it doesn't get ticked/process input
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}
	
	BindToLatencyMarkerSettingChange();
}

ULyraPlayerInput::~ULyraPlayerInput()
{
	UnbindLatencyMarkerSettingChangeListener();
}

bool ULyraPlayerInput::InputKey(const FInputKeyEventArgs& Params)
{
	const bool bResult = Super::InputKey(Params);

	// Note: Since Lyra is only going to support the "Reflex" plugin to handle latency markers,
	// we could #if PLATFORM_DESKTOP this away to save on other platforms. However, for the sake
	// of extensibility for this same project we will not do that. 
	ProcessInputEventForLatencyMarker(Params);

	return bResult;
}

void ULyraPlayerInput::ProcessInputEventForLatencyMarker(const FInputKeyEventArgs& Params)
{
	if (!bShouldTriggerLatencyFlash)
	{
		return;
	}
	
	// Flash the latency marker on left mouse down
	if (Params.Key == EKeys::LeftMouseButton)
	{
		TArray<ILatencyMarkerModule*> LatencyMarkerModules = IModularFeatures::Get().GetModularFeatureImplementations<ILatencyMarkerModule>(ILatencyMarkerModule::GetModularFeatureName());

		for (ILatencyMarkerModule* LatencyMarkerModule : LatencyMarkerModules)
		{
			// TRIGGER_FLASH is 7
			LatencyMarkerModule->SetCustomLatencyMarker(7, GFrameCounter);
		}
	}
}

void ULyraPlayerInput::BindToLatencyMarkerSettingChange()
{
	if (!ULyraSettingsLocal::DoesPlatformSupportLatencyMarkers())
	{
		return;
	}
	
	ULyraSettingsLocal* Settings = ULyraSettingsLocal::Get();
	if (!Settings)
	{
		return;
	}

	Settings->OnLatencyFlashInidicatorSettingsChangedEvent().AddUObject(this, &ThisClass::HandleLatencyMarkerSettingChanged);

	// Initalize the settings and make sure that the input latency modules are enabled
	HandleLatencyMarkerSettingChanged();
}

void ULyraPlayerInput::UnbindLatencyMarkerSettingChangeListener()
{
	ULyraSettingsLocal* Settings = ULyraSettingsLocal::Get();
	if (!Settings)
	{
		return;
	}

	Settings->OnLatencyFlashInidicatorSettingsChangedEvent().RemoveAll(this);
}

void ULyraPlayerInput::HandleLatencyMarkerSettingChanged()
{
	// Make sure that we only ever get this callback on platforms which support latency markers
	ensure(ULyraSettingsLocal::DoesPlatformSupportLatencyMarkers());
	
	const ULyraSettingsLocal* Settings = ULyraSettingsLocal::Get();
	if (!Settings)
	{
		return;
	}

	// Enable or disable the latency flash on all the marker modules according to the settings change
	bShouldTriggerLatencyFlash = Settings->GetEnableLatencyFlashIndicators();
	
	TArray<ILatencyMarkerModule*> LatencyMarkerModules = IModularFeatures::Get().GetModularFeatureImplementations<ILatencyMarkerModule>(ILatencyMarkerModule::GetModularFeatureName());
	for (ILatencyMarkerModule* LatencyMarkerModule : LatencyMarkerModules)
	{
		LatencyMarkerModule->SetFlashIndicatorEnabled(bShouldTriggerLatencyFlash);
	}
}
