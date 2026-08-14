// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InputCoreTypes.h"
#include "SlimeInputTypes.h"
#include "SlimeInputSettings.generated.h"

class APlayerController;

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

protected:
	void FillDefaults();
	void MigrateBindSchemeIfNeeded();
	FString ActionConfigName(ESlimeInputAction Action) const;

	UPROPERTY()
	TMap<ESlimeInputAction, FKey> Keys;

	/** Keys last written into Enhanced Input contexts (for remap chase). */
	TMap<ESlimeInputAction, FKey> AppliedMovementKeys;
};
