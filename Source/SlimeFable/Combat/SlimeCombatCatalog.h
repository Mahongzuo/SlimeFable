// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SlimeCombatTypes.h"
#include "SlimeCombatCatalog.generated.h"

UCLASS(BlueprintType)
class SLIMEFABLE_API USlimeCombatCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USlimeCombatCatalog();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TArray<FSlimeElementKitData> Kits;

	UFUNCTION(BlueprintPure, Category = "Combat")
	FSlimeElementKitData GetKit(ESlimeElement Element) const;

	static const FSlimeElementKitData& GetBuiltinKit(ESlimeElement Element);
};
