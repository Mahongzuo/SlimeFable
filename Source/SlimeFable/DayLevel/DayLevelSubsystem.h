// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DayLevel/DayLevelTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DayLevelSubsystem.generated.h"

/**
 * Lightweight day-level lookup for "this day in history" entry points.
 * Save slot key convention: DayId string (e.g. "0812").
 * Exploration tags (to hang on Actors later):
 *   Exploration.Objective.*, Exploration.Collectible.*, Exploration.Boss.*
 */
UCLASS()
class SLIMEFABLE_API UDayLevelSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "Day Level")
	void SetRegistry(UDayLevelRegistry* InRegistry);

	UFUNCTION(BlueprintPure, Category = "Day Level")
	UDayLevelRegistry* GetRegistry() const { return Registry; }

	/** Returns today's DayId from local system date (MMDD, includes 0229 on leap days). */
	UFUNCTION(BlueprintPure, Category = "Day Level")
	FDayId GetTodayDayId() const;

	UFUNCTION(BlueprintPure, Category = "Day Level")
	static FDayId MakeDayId(int32 Month, int32 Day);

	UFUNCTION(BlueprintPure, Category = "Day Level")
	static FString MakeDayLevelPackagePath(int32 Month, int32 Day);

	UFUNCTION(BlueprintCallable, Category = "Day Level")
	bool GetLevelForDayId(FName DayId, TSoftObjectPtr<UWorld>& OutLevel) const;

	UFUNCTION(BlueprintCallable, Category = "Day Level")
	bool GetTodayLevel(TSoftObjectPtr<UWorld>& OutLevel) const;

	/** SaveGame slot name for a day level (same as DayId string). */
	UFUNCTION(BlueprintPure, Category = "Day Level")
	static FString GetSaveSlotKeyForDayId(FName DayId);

protected:
	UPROPERTY()
	TObjectPtr<UDayLevelRegistry> Registry;
};
