// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SlimeCheatSubsystem.generated.h"

/**
 * Cross-level cheat toggles (god / one-shot / greed). Survives OpenLevel.
 */
UCLASS()
class SLIMEFABLE_API USlimeCheatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Cheat")
	bool IsGodMode() const { return bGodMode; }

	UFUNCTION(BlueprintPure, Category = "Cheat")
	bool IsKillYou() const { return bKillYou; }

	UFUNCTION(BlueprintPure, Category = "Cheat")
	bool IsGreedActive() const { return bGreedActive; }

	/** Toggle by command string (case-insensitive). OutMessage for Toast. Returns false if unknown. */
	bool ExecuteCommand(const FString& RawCommand, FString& OutMessage);

private:
	bool bGodMode = false;
	bool bKillYou = false;
	bool bGreedActive = false;
};
