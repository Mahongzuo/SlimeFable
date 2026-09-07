// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InputCoreTypes.h"
#include "SlimeInputTypes.h"
#include "SlimeInputSettings.generated.h"

class APlayerController;

DECLARE_MULTICAST_DELEGATE(FOnSlimePlayInputModeChanged);

/**
 *  Remappable slime gameplay keys. Poll paths and UI both read from here.
 *  Persists to GameUserSettings.ini section [SlimeInput].
 */
UCLASS()
class SLIMEFABLE_API USlimeInputSettings : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Input")
	FKey GetKey(ESlimeInputAction Action) const;

	UFUNCTION(BlueprintPure, Category = "Input")
	FText GetActionDisplayName(ESlimeInputAction Action) const;

	UFUNCTION(BlueprintPure, Category = "Input")
	FText GetKeyDisplayName(ESlimeInputAction Action) const;

	/** Returns false if NewKey is already bound to another action (conflict rejected). */
	UFUNCTION(BlueprintCallable, Category = "Input")
	bool TrySetKey(ESlimeInputAction Action, FKey NewKey, FText& OutError);

	UFUNCTION(BlueprintCallable, Category = "Input")
	void ResetToDefaults();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void Save();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void Load();

	bool IsKeyDown(const APlayerController* PC, ESlimeInputAction Action) const;
	bool WasKeyPressed(const APlayerController* PC, ESlimeInputAction Action) const;

	/** True when any move/jump key differs from WASD / Space defaults. */
	bool UsesCustomMovementKeys() const;

	/**
	 *  When move/jump keys are customized, removes IMC_Default so Character poll can drive
	 *  movement without double input. Restores IMC_Default when back on defaults.
	 */
	void ApplyEnhancedInputRemaps(APlayerController* PC);

	static FKey GetDefaultKey(ESlimeInputAction Action);
	static TArray<ESlimeInputAction> GetAllActions();

	UFUNCTION(BlueprintPure, Category = "Input")
	ESlimePlayInputMode GetPlayInputMode() const { return PlayInputMode; }

	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetPlayInputMode(ESlimePlayInputMode Mode);

	UFUNCTION(BlueprintPure, Category = "Input")
	ESlimeResolvedInputMode ResolvePlayInputMode() const;

	UFUNCTION(BlueprintPure, Category = "Input")
	FText GetPlayInputModeDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Input")
	ESlimeTouchHandedness GetTouchHandedness() const { return TouchHandedness; }

	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetTouchHandedness(ESlimeTouchHandedness Hand);

	UFUNCTION(BlueprintPure, Category = "Input")
	bool ShouldReadGamepadAbilityKeys() const;

	UFUNCTION(BlueprintPure, Category = "Input")
	bool ShouldUseTouchHud() const;

	UFUNCTION(BlueprintPure, Category = "Input")
	bool ShouldShowPixelStreamPlayHint() const;

	void NotifyPixelStreamingChanged();

	void NoteLastInputDevice(ESlimeLastInputDevice Device);
	ESlimeLastInputDevice GetLastInputDevice() const { return LastInputDevice; }

	static FKey GetDefaultGamepadKey(ESlimeInputAction Action);
	static FText GetGamepadGuideText();
	static bool IsGamepadDismissKey(const FKey& Key);

	void SetVirtualActionDown(ESlimeInputAction Action, bool bDown);
	void ClearVirtualActions();
	void SetVirtualMoveAxis(FVector2D Axis);
	FVector2D GetVirtualMoveAxis() const { return VirtualMoveAxis; }
	void AddVirtualLookDelta(FVector2D Delta);
	FVector2D ConsumeVirtualLookDelta();
	void SetTouchPointerBusy(bool bBusy);
	bool IsTouchPointerBusy() const { return bTouchPointerBusy; }

	FOnSlimePlayInputModeChanged OnPlayInputModeChanged;

protected:
	void FillDefaults();
	void MigrateBindSchemeIfNeeded();
	FString ActionConfigName(ESlimeInputAction Action) const;
	void SaveDevicePrefs();
	void LoadDevicePrefs();
	bool IsVirtualActionDown(ESlimeInputAction Action) const;
	bool WasVirtualActionPressed(ESlimeInputAction Action) const;

	UPROPERTY()
	TMap<ESlimeInputAction, FKey> Keys;

	/** Keys last written into Enhanced Input contexts (for remap chase). */
	TMap<ESlimeInputAction, FKey> AppliedMovementKeys;

	ESlimePlayInputMode PlayInputMode = ESlimePlayInputMode::KeyboardMouse;
	ESlimeTouchHandedness TouchHandedness = ESlimeTouchHandedness::Right;
	ESlimeLastInputDevice LastInputDevice = ESlimeLastInputDevice::None;

	TSet<ESlimeInputAction> VirtualDown;
	TMap<ESlimeInputAction, uint64> VirtualPressFrame;
	FVector2D VirtualMoveAxis = FVector2D::ZeroVector;
	FVector2D VirtualLookPending = FVector2D::ZeroVector;
	bool bTouchPointerBusy = false;
};
