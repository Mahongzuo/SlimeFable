// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Farm/SlimeCropTypes.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SlimeFarmSubsystem.generated.h"

class USlimeInventorySubsystem;

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
};

UCLASS()
class SLIMEFABLE_API USlimeFarmSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	USlimeCropDefinition* FindCrop(FName CropId) const;
	USlimeSeedDefinition* FindSeedForCrop(FName CropId) const;
	void GetSeedsInBag(const USlimeInventorySubsystem* Inventory, TArray<USlimeSeedDefinition*>& OutSeeds) const;

	bool GetPlotRecord(FName PlotId, FSlimeFarmPlotRecord& OutRecord) const;
	void WritePlotRecord(const FSlimeFarmPlotRecord& Record);
	void Flush();

	int32 GetLastSeedCrateDayKey() const;
	void SetLastSeedCrateDayKey(int32 DayKey);

	static int32 MakeTodayKey();

private:
	void ScanHubAssets(USlimeInventorySubsystem* Inventory);
	void EnsureBuiltins(USlimeInventorySubsystem* Inventory);
	void Load();

	UPROPERTY()
	TObjectPtr<USlimeFarmSaveGame> Save;

	UPROPERTY()
	TMap<FName, TObjectPtr<USlimeCropDefinition>> Crops;

	UPROPERTY()
	TArray<TObjectPtr<USlimeSeedDefinition>> Seeds;

	static const TCHAR* SaveSlot;
};
