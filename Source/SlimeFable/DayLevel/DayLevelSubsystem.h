// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DayLevel/DayLevelTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DayLevelSubsystem.generated.h"

/**
 * Day-level lookup and travel for "this day in history".
 * Save slot key convention: DayId string (e.g. "0812").
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

	UFUNCTION(BlueprintPure, Category = "Day Level")
	bool HasRegistry() const { return Registry != nullptr; }

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

	UFUNCTION(BlueprintPure, Category = "Day Level")
	static FString GetSaveSlotKeyForDayId(FName DayId);

	/** e.g. "8月12日 (0812)" */
	UFUNCTION(BlueprintPure, Category = "Day Level")
	FString GetTodayDisplayString() const;

	UFUNCTION(BlueprintCallable, Category = "Day Level")
	void GetEntriesForMonth(int32 Month, TArray<FDayLevelEntry>& OutEntries) const;

	UFUNCTION(BlueprintCallable, Category = "Day Level", meta = (WorldContext = "WorldContextObject"))
	bool TravelToDayId(const UObject* WorldContextObject, FName DayId);

	UFUNCTION(BlueprintCallable, Category = "Day Level", meta = (WorldContext = "WorldContextObject"))
	bool TravelToToday(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Day Level", meta = (WorldContext = "WorldContextObject"))
	void TravelToMainMenu(const UObject* WorldContextObject);

	/** Hard-travel to Main and open the level-select calendar overlay. */
	UFUNCTION(BlueprintCallable, Category = "Day Level", meta = (WorldContext = "WorldContextObject"))
	void TravelToLevelSelect(const UObject* WorldContextObject);

	static const TCHAR* OpenLevelSelectOption;

protected:
	void LoadDefaultRegistry();

	UPROPERTY()
	TObjectPtr<UDayLevelRegistry> Registry;

	UPROPERTY()
	TSoftObjectPtr<UDayLevelRegistry> DefaultRegistryPath;
};
