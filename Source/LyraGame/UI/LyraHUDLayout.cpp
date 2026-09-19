// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraHUDLayout.h"

#include "CommonUIExtensions.h"
#include "CommonUISettings.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GameFramework/InputSettings.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Input/CommonUIInputTypes.h"
#include "ICommonUIModule.h"
#include "LyraLogChannels.h"
#include "NativeGameplayTags.h"
#include "UI/Foundation/LyraControllerDisconnectedScreen.h"
#include "UI/LyraActivatableWidget.h"

#if WITH_EDITOR
#include "CommonUIVisibilitySubsystem.h"
#endif	// WITH_EDITOR

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraHUDLayout)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_LAYER_MENU, "UI.Layer.Menu");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_ACTION_ESCAPE, "UI.Action.Escape");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Platform_Trait_Input_PrimarlyController, "Platform.Trait.Input.PrimarlyController");

ULyraHUDLayout::ULyraHUDLayout(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SpawnedControllerDisconnectScreen(nullptr)
{
	// By default, only primarily controller platforms require a disconnect screen. 
	PlatformRequiresControllerDisconnectScreen.AddTag(TAG_Platform_Trait_Input_PrimarlyController);
}

void ULyraHUDLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	RegisterUIActionBinding(FBindUIActionArgs(FUIActionTag::ConvertChecked(TAG_UI_ACTION_ESCAPE), false, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleEscapeAction)));

	// If we can display a controller disconnect screen, then listen for the controller state change delegates
	if (ShouldPlatformDisplayControllerDisconnectScreen())
	{
		// Bind to when input device connections change
		IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
		DeviceMapper.GetOnInputDeviceConnectionChange().AddUObject(this, &ThisClass::HandleInputDeviceConnectionChanged);
		DeviceMapper.GetOnInputDevicePairingChange().AddUObject(this, &ThisClass::HandleInputDevicePairingChanged);	
	}
}

void ULyraHUDLayout::NativeDestruct()
{
	Super::NativeDestruct();

	// Remove bindings to input device connection changing
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	DeviceMapper.GetOnInputDeviceConnectionChange().RemoveAll(this);
	DeviceMapper.GetOnInputDevicePairingChange().RemoveAll(this);

	if (RequestProcessControllerStateHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RequestProcessControllerStateHandle);
		RequestProcessControllerStateHandle.Reset();
	}
}

void ULyraHUDLayout::HandleEscapeAction()
{
	if (ensure(!EscapeMenuClass.IsNull()))
	{
		UCommonUIExtensions::PushStreamedContentToLayer_ForPlayer(GetOwningLocalPlayer(), TAG_UI_LAYER_MENU, EscapeMenuClass);
	}
}

void ULyraHUDLayout::HandleInputDeviceConnectionChanged(EInputDeviceConnectionState NewConnectionState, FPlatformUserId PlatformUserId, FInputDeviceId InputDeviceId)
{
	const FPlatformUserId OwningLocalPlayerId = GetOwningLocalPlayer()->GetPlatformUserId();

	ensure(OwningLocalPlayerId.IsValid());

	// This device connection change happened to a different player, ignore it for us.
	if (PlatformUserId != OwningLocalPlayerId)
	{
		return;
	}

	NotifyControllerStateChangeForDisconnectScreen();
}

void ULyraHUDLayout::HandleInputDevicePairingChanged(FInputDeviceId InputDeviceId, FPlatformUserId NewUserPlatformId, FPlatformUserId OldUserPlatformId)
{
	const FPlatformUserId OwningLocalPlayerId = GetOwningLocalPlayer()->GetPlatformUserId();

	ensure(OwningLocalPlayerId.IsValid());

	// If this pairing change was related to our local player, notify of a change.
	if (NewUserPlatformId == OwningLocalPlayerId || OldUserPlatformId == OwningLocalPlayerId)
	{
		NotifyControllerStateChangeForDisconnectScreen();	
	}
}

bool ULyraHUDLayout::ShouldPlatformDisplayControllerDisconnectScreen() const
{
	// We only want this menu on primarily controller platforms
	bool bHasAllRequiredTags = ICommonUIModule::GetSettings().GetPlatformTraits().HasAll(PlatformRequiresControllerDisconnectScreen);

	// Check the tags that we may be emulating in the editor too
#if WITH_EDITOR
	const FGameplayTagContainer& PlatformEmulationTags = UCommonUIVisibilitySubsystem::Get(GetOwningLocalPlayer())->GetVisibilityTags();
	bHasAllRequiredTags |= PlatformEmulationTags.HasAll(PlatformRequiresControllerDisconnectScreen);
#endif	// WITH_EDITOR

	return bHasAllRequiredTags;
}

void ULyraHUDLayout::NotifyControllerStateChangeForDisconnectScreen()
{
	// We should only ever get here if we have bound to the controller state change delegates
	ensure(ShouldPlatformDisplayControllerDisconnectScreen());

	// If we haven't already, queue the processing of device state for next tick.
	if (!RequestProcessControllerStateHandle.IsValid())
	{
		RequestProcessControllerStateHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime)
		{
			RequestProcessControllerStateHandle.Reset();
			ProcessControllerDevicesHavingChangedForDisconnectScreen();
			return false;
		}));
	}
}

void ULyraHUDLayout::ProcessControllerDevicesHavingChangedForDisconnectScreen()
{
	// We should only ever get here if we have bound to the controller state change delegates
	ensure(ShouldPlatformDisplayControllerDisconnectScreen());
	
	const FPlatformUserId OwningLocalPlayerId = GetOwningLocalPlayer()->GetPlatformUserId();
	
	ensure(OwningLocalPlayerId.IsValid());

	// Get all input devices mapped to our player
	const IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
    TArray<FInputDeviceId> MappedInputDevices;
    const int32 NumDevicesMappedToUser = DeviceMapper.GetAllInputDevicesForUser(OwningLocalPlayerId, OUT MappedInputDevices);

    // Check if there are any other connected GAMEPAD devices mapped to this platform user. 
    bool bHasConnectedController = false;

    for (const FInputDeviceId MappedDevice : MappedInputDevices)
    {
    	if (DeviceMapper.GetInputDeviceConnectionState(MappedDevice) == EInputDeviceConnectionState::Connected)
    	{
    		const FHardwareDeviceIdentifier HardwareInfo = UInputDeviceSubsystem::Get()->GetInputDeviceHardwareIdentifier(MappedDevice);
    		if (HardwareInfo.PrimaryDeviceType == EHardwareDevicePrimaryType::Gamepad)
    		{
    			bHasConnectedController = true;
    		}
    	}			
    }

    // If there are no gamepad input devices mapped to this user, then we want to pop the toast saying to re-connect them
    if (!bHasConnectedController)
    {
    	DisplayControllerDisconnectedMenu();
    }
	// Otherwise we can hide the screen if it is currently being shown
	else if (SpawnedControllerDisconnectScreen)
	{
		HideControllerDisconnectedMenu();
	}
}

void ULyraHUDLayout::DisplayControllerDisconnectedMenu_Implementation()
{
	UE_LOG(LogLyra, Log, TEXT("[%hs] Display controller disconnected menu!"), __func__);

	if (ControllerDisconnectedScreen)
	{
		// Push the "controller disconnected" widget to the menu layer
		SpawnedControllerDisconnectScreen = UCommonUIExtensions::PushContentToLayer_ForPlayer(GetOwningLocalPlayer(), TAG_UI_LAYER_MENU, ControllerDisconnectedScreen);
	}
}

void ULyraHUDLayout::HideControllerDisconnectedMenu_Implementation()
{
	UE_LOG(LogLyra, Log, TEXT("[%hs] Hide controller disconnected menu!"), __func__);
	
	UCommonUIExtensions::PopContentFromLayer(SpawnedControllerDisconnectScreen);
	SpawnedControllerDisconnectScreen = nullptr;
}