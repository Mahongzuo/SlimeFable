// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeItemTypes.generated.h"

UENUM(BlueprintType)
enum class ESlimeItemCategory : uint8
{
	Consumable UMETA(DisplayName = "消耗品"),
	Placeable UMETA(DisplayName = "放置品"),
	Souvenir UMETA(DisplayName = "纪念品")
};

USTRUCT(BlueprintType)
struct SLIMEFABLE_API FSlimeInventoryEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "0"))
	int32 Count = 0;
};

/** Hotbar has 6 slots: 0-2 consumable, 3-5 placeable. */
static constexpr int32 SlimeHotbarSlotCount = 6;
