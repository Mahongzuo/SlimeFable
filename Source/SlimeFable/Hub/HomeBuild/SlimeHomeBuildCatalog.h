// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Hub/HomeBuild/SlimeHomeBuildTypes.h"
#include "SlimeHomeBuildCatalog.generated.h"

UCLASS(BlueprintType)
class SLIMEFABLE_API USlimeHomeBuildCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build")
	TArray<FSlimeHomeBuildEntry> Entries;

	/** World centimeters of one grid cell. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build")
	float CellSize = 50.f;

	const FSlimeHomeBuildEntry* FindEntry(FName EntryId) const;

	static USlimeHomeBuildCatalog* LoadCatalog();
};
