// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimePlacedActor.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class USlimeItemDefinition;
class APawn;

UCLASS(Blueprintable)
class SLIMEFABLE_API ASlimePlacedActor : public AActor
{
	GENERATED_BODY()

public:
	ASlimePlacedActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Placeable")
	void ConfigureFromItem(FName InItemId, USlimeItemDefinition* InDefinition);

	UFUNCTION(BlueprintCallable, Category = "Placeable")
	bool TryPickup(APawn* Picker);

	UFUNCTION(BlueprintCallable, Category = "Placeable")
	void SetHighlight(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Placeable")
	FName GetItemId() const { return SourceItemId; }

	UFUNCTION(BlueprintPure, Category = "Placeable")
	FVector GetPromptWorldLocation() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placeable")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placeable")
	FName SourceItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placeable")
	TObjectPtr<USlimeItemDefinition> ItemDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placeable", meta = (ClampMin = "100.0", Units = "cm"))
	float HighlightRadius = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placeable")
	TSoftObjectPtr<UMaterialInterface> OutlineOverlayMaterial;

	/** Kept for BP compatibility; highlight no longer scales the mesh (default 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placeable", meta = (ClampMin = "1.0", ClampMax = "1.5"))
	float HighlightScaleMul = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placeable", meta = (ClampMin = "0.0", Units = "cm"))
	float PromptHeightOffset = 40.f;

protected:
	void RefreshHighlightFromNearbyPlayers();
	UMaterialInterface* ResolveOutlineMaterial();

	bool bHighlighted = false;
	FVector RestRelativeScale = FVector::OneVector;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CachedOutlineMaterial;
};
