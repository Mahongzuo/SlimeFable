// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeWorldPickup.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UMaterialInterface;
class USlimeItemDefinition;
class USlimeInventorySubsystem;
class APawn;

UCLASS(Blueprintable)
class SLIMEFABLE_API ASlimeWorldPickup : public AActor
{
	GENERATED_BODY()

public:
	ASlimeWorldPickup();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TryPickup(APawn* Picker);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetHighlight(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FName GetItemId() const;

	/** World point above the mesh bounds top (for HUD interact prompt). */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	FVector GetPromptWorldLocation() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<USphereComponent> HighlightSphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TObjectPtr<USlimeItemDefinition> ItemDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FName ItemIdOverride = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "100.0", Units = "cm"))
	float HighlightRadius = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TSoftObjectPtr<UMaterialInterface> OutlineOverlayMaterial;

	/** Extra scale applied while highlighted so the pickup stays readable without a perfect outline MI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "1.0", ClampMax = "1.5"))
	float HighlightScaleMul = 1.18f;

	/** Extra cm above mesh bounds top for the interact prompt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "0.0", Units = "cm"))
	float PromptHeightOffset = 40.f;

protected:
	/**
	 * Called during TryPickup before the item id is resolved. Subclasses can
	 * register per-instance definitions (e.g. souvenir with a custom image).
	 */
	virtual void PrepareDefinition(USlimeInventorySubsystem& Inventory) {}

	void RefreshHighlightFromNearbyPlayers();
	UMaterialInterface* ResolveOutlineMaterial();

	bool bHighlighted = false;
	FVector RestRelativeScale = FVector::OneVector;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CachedOutlineMaterial;
};
