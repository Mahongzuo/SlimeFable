// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserWidget.h"
#include "LyraSimulatedInputWidget.generated.h"

#define UE_API LYRAGAME_API

class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UCommonHardwareVisibilityBorder;
class UEnhancedPlayerInput;

/**
 *  A UMG widget with base functionality to inject input (keys or input actions)
 *  to the enhanced input subsystem.
 */
UCLASS(MinimalAPI, meta=( DisplayName="Lyra Simulated Input Widget" ))
class ULyraSimulatedInputWidget : public UCommonUserWidget
{
	GENERATED_BODY()
	
public:
	
	UE_API ULyraSimulatedInputWidget(const FObjectInitializer& ObjectInitializer);
	
	//~ Begin UWidget
#if WITH_EDITOR
	UE_API virtual const FText GetPaletteCategory() override;
#endif
	//~ End UWidget interface

	//~ Begin UUserWidget
	UE_API virtual void NativeConstruct() override;
	UE_API virtual void NativeDestruct() override;
	UE_API virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	//~ End UUserWidget interface
	
	/** Get the enhanced input subsystem based on the owning local player of this widget. Will return null if there is no owning player */
	UFUNCTION(BlueprintCallable)
	UE_API UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;

	/** Get the current player input from the current input subsystem */
	UE_API UEnhancedPlayerInput* GetPlayerInput() const;

	/**  */
	UFUNCTION(BlueprintCallable)
	const UInputAction* GetAssociatedAction() const { return AssociatedAction; }

	/** Returns the current key that will be used to input any values. */
	UFUNCTION(BlueprintCallable)
	FKey GetSimulatedKey() const { return KeyToSimulate; }

	/**
	 * Injects the given vector as an input to the current simulated key.
	 * This calls "InputKey" on the current player.
	 */
	UFUNCTION(BlueprintCallable)
	UE_API void InputKeyValue(const FVector& Value);

	/**
	 * Injects the given vector as an input to the current simulated key.
	 * This calls "InputKey" on the current player.
	 */
	UFUNCTION(BlueprintCallable)
	UE_API void InputKeyValue2D(const FVector2D& Value);

	UFUNCTION(BlueprintCallable)
	UE_API void FlushSimulatedInput();
	
protected:

	/** Set the KeyToSimulate based on a query from enhanced input about what keys are mapped to the associated action */
	UE_API void QueryKeyToSimulate();

	/** Called whenever control mappings change, so we have a chance to adapt our own keys */
	UFUNCTION()
	UE_API void OnControlMappingsRebuilt();

	/** The common visibility border will allow you to specify UI for only specific platforms if desired */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonHardwareVisibilityBorder> CommonVisibilityBorder = nullptr;
	
	/** The associated input action that we should simulate input for */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<const UInputAction> AssociatedAction = nullptr;

	/** The Key to simulate input for in the case where none are currently bound to the associated action */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FKey FallbackBindingKey = EKeys::Gamepad_Right2D;

	/** The key that should be input via InputKey on the player input */
	FKey KeyToSimulate;
};

#undef UE_API
