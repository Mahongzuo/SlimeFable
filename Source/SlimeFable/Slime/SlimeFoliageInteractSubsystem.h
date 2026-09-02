// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SlimeFoliageInteractSubsystem.generated.h"

class USlimeFoliageInteractComponent;
class UMaterialParameterCollection;
class UMaterialParameterCollectionInstance;

/**
 * Collects foliage interactors and writes the nearest 4 into MPC_SlimeFoliage.
 * Slot 0 prefers the player-controlled pawn; the rest are nearest by distance.
 * Skips menu worlds without a gameplay player controller.
 */
UCLASS()
class SLIMEFABLE_API USlimeFoliageInteractSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual bool IsTickableWhenPaused() const override { return false; }

	void RegisterInteractor(USlimeFoliageInteractComponent* Component);
	void UnregisterInteractor(USlimeFoliageInteractComponent* Component);

	static constexpr int32 MaxSlots = 4;

protected:
	void EnsureMpcLoaded();
	void ClearAllSlots(UMaterialParameterCollectionInstance* Instance) const;
	void WriteSlot(UMaterialParameterCollectionInstance* Instance, int32 SlotIndex,
		const FVector& Location, const FVector& Velocity, float Radius, float Strength) const;
	void WriteGlobals(UMaterialParameterCollectionInstance* Instance,
		float MaxBend, float TrailSeconds, float IdlePartStrength,
		float GrassHeight, float Flatten) const;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<USlimeFoliageInteractComponent>> Interactors;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> MpcAsset = nullptr;

	static const TCHAR* MpcSoftPath;
};
