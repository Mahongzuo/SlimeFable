// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Farm/SlimeCropTypes.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "SlimeFarmSubsystem.generated.h"

class USlimeInventorySubsystem;
class UWorld;

UCLASS()
class SLIMEFABLE_API USlimeFarmSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FSlimeFarmPlotRecord> Plots;

	/** Year*10000 + Month*100 + Day of the last seed-crate claim. 0 = never. */
	UPROPERTY()
	int32 LastSeedCrateDayKey = 0;

	/** Crop chosen last time the planting panel was confirmed. */
	UPROPERTY()
	FName LastPlantedCropId;
};

UCLASS()
class SLIMEFABLE_API USlimeFarmSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	USlimeCropDefinition* FindCrop(FName CropId) const;
	void GetAllCrops(TArray<USlimeCropDefinition*>& OutCrops) const;
	USlimeSeedDefinition* FindSeedForCrop(FName CropId) const;
	void GetSeedsInBag(const USlimeInventorySubsystem* Inventory, TArray<USlimeSeedDefinition*>& OutSeeds) const;

	bool GetPlotRecord(FName PlotId, FSlimeFarmPlotRecord& OutRecord) const;
	void WritePlotRecord(const FSlimeFarmPlotRecord& Record);
	void RemovePlotRecord(FName PlotId);
	/** Drop HomePlot_* rows whose id is not in the live home-build set. */
	void PurgeOrphanHomePlots(const TSet<FName>& LiveHomePlotIds);
	/** Write the save now. Periodic growth only marks the save dirty. */
	void RequestFlush();
	void Flush();

	FName GetLastPlantedCrop() const;
	void SetLastPlantedCrop(FName CropId);

	int32 GetLastSeedCrateDayKey() const;
	void SetLastSeedCrateDayKey(int32 DayKey);

	static int32 MakeTodayKey();

private:
	void ScanHubAssets(USlimeInventorySubsystem* Inventory);
	void EnsureBuiltins(USlimeInventorySubsystem* Inventory);
	void Load();
	void RebuildPlotIndex();
	void ArmFlushTimer(UWorld* World);
	void DisarmFlushTimer(UWorld* World);
	void FlushIfDirty();

	bool bDirty = false;
	FTimerHandle FlushTimer;
	TWeakObjectPtr<UWorld> FlushWorld;
	FDelegateHandle WorldInitHandle;
	FDelegateHandle WorldCleanupHandle;
	TMap<FName, int32> PlotIndex;

	UPROPERTY()
	TObjectPtr<USlimeFarmSaveGame> Save;

	UPROPERTY()
	TMap<FName, TObjectPtr<USlimeCropDefinition>> Crops;

	UPROPERTY()
	TArray<TObjectPtr<USlimeSeedDefinition>> Seeds;

	static const TCHAR* SaveSlot;
};
