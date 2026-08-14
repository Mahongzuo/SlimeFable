// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimePlacementComponent.generated.h"

class USlimePlaceableDefinition;
class ASlimePlacePreview;

UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API USlimePlacementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimePlacementComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool BeginPlacement(USlimePlaceableDefinition* Definition);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void CancelPlacement();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool ConfirmPlacement();

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsPlacing() const { return ActiveDefinition != nullptr; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "100.0", Units = "cm"))
	float TraceDistance = 1200.f;

protected:
	void UpdatePreview();
	bool IsFlatGround(const FHitResult& Hit, float MaxSlopeDegrees) const;

	UPROPERTY(Transient)
	TObjectPtr<USlimePlaceableDefinition> ActiveDefinition;

	UPROPERTY(Transient)
	TObjectPtr<ASlimePlacePreview> PreviewActor;

	bool bPlacementValid = false;
	FHitResult LastHit;
};
