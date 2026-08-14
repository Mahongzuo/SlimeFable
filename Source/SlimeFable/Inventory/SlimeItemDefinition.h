// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SlimeItemTypes.h"
#include "SlimeItemDefinition.generated.h"

class UTexture2D;
class UFileMediaSource;
class UStaticMesh;
class ASlimePlacedActor;

UCLASS(Abstract, BlueprintType)
class SLIMEFABLE_API USlimeItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxStack = 99;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	ESlimeItemCategory Category = ESlimeItemCategory::Consumable;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("SlimeItem"), ItemId.IsNone() ? GetFName() : ItemId);
	}
};

UCLASS(BlueprintType)
class SLIMEFABLE_API USlimeConsumableDefinition : public USlimeItemDefinition
{
	GENERATED_BODY()

public:
	USlimeConsumableDefinition()
	{
		Category = ESlimeItemCategory::Consumable;
		MaxStack = 20;
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable", meta = (ClampMin = "0.0"))
	float HealAmount = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable", meta = (ClampMin = "0.0", Units = "s"))
	float CooldownReduceSeconds = 0.f;

	/** Multiplier applied to outgoing damage while buff is active (e.g. 1.25 = +25%). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable", meta = (ClampMin = "0.0"))
	float DamageBonusMul = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable", meta = (ClampMin = "0.0", Units = "s"))
	float BuffDuration = 0.f;
};

UCLASS(BlueprintType)
class SLIMEFABLE_API USlimePlaceableDefinition : public USlimeItemDefinition
{
	GENERATED_BODY()

public:
	USlimePlaceableDefinition()
	{
		Category = ESlimeItemCategory::Placeable;
		MaxStack = 20;
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placeable")
	TSoftClassPtr<ASlimePlacedActor> PlacedActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placeable")
	TSoftObjectPtr<UStaticMesh> PreviewMesh;

	/** Relative scale applied to the placed mesh (copied from world pickup on collect). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placeable")
	FVector PlacedMeshScale = FVector(0.5f, 0.5f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placeable", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float MaxSlopeDegrees = 12.f;
};

UCLASS(BlueprintType)
class SLIMEFABLE_API USlimeSouvenirDefinition : public USlimeItemDefinition
{
	GENERATED_BODY()

public:
	USlimeSouvenirDefinition()
	{
		Category = ESlimeItemCategory::Souvenir;
		MaxStack = 1;
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souvenir")
	TSoftObjectPtr<UTexture2D> StoryImage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souvenir", meta = (MultiLine = "true"))
	FText StoryText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souvenir")
	TSoftObjectPtr<UFileMediaSource> StoryVideo;
};
