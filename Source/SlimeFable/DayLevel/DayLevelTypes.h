// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/World.h"
#include "DayLevelTypes.generated.h"

/** Calendar day identifier in MMDD form, e.g. "0812" or "0229". */
USTRUCT(BlueprintType)
struct FDayId
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day Level")
	FName Id;

	FDayId() = default;

	explicit FDayId(FName InId)
		: Id(InId)
	{
	}

	explicit FDayId(const FString& InId)
		: Id(FName(*InId))
	{
	}

	bool IsValid() const
	{
		return !Id.IsNone();
	}

	FString ToString() const
	{
		return Id.ToString();
	}

	bool operator==(const FDayId& Other) const
	{
		return Id == Other.Id;
	}

	friend uint32 GetTypeHash(const FDayId& DayId)
	{
		return GetTypeHash(DayId.Id);
	}
};

/** One calendar-day level entry used by the day-level registry. */
USTRUCT(BlueprintType)
struct FDayLevelEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day Level")
	FName DayId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day Level")
	int32 Month = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day Level")
	int32 Day = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day Level", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath Level;

	/** Optional year/chapter sub-levels, keyed by ChapterId (e.g. "1920"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day Level")
	TMap<FName, TSoftObjectPtr<UWorld>> SubLevels;

	/** Convention: SaveGame slot / exploration progress key uses this DayId string. */
	FString GetSaveSlotKey() const
	{
		return DayId.ToString();
	}

	TSoftObjectPtr<UWorld> GetLevelSoftPtr() const
	{
		return TSoftObjectPtr<UWorld>(Level);
	}
};

/**
 * Primary data asset listing all 366 day levels.
 * Soft path convention: /Game/Maps/Days/MM/MMDD.MMDD
 */
UCLASS(BlueprintType)
class SLIMEFABLE_API UDayLevelRegistry : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day Level")
	TArray<FDayLevelEntry> Entries;

	UFUNCTION(BlueprintCallable, Category = "Day Level")
	bool FindEntry(FName InDayId, FDayLevelEntry& OutEntry) const;

	UFUNCTION(BlueprintCallable, Category = "Day Level")
	bool FindEntryForMonthDay(int32 Month, int32 Day, FDayLevelEntry& OutEntry) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("DayLevelRegistry"), GetFName());
	}
};
