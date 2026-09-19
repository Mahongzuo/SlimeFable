// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cosmetics/LyraCosmeticAnimationTypes.h"
#include "Equipment/LyraEquipmentInstance.h"
#include "GameFramework/InputDevicePropertyHandle.h"

#include "LyraWeaponInstance.generated.h"

#define UE_API LYRAGAME_API

class UAnimInstance;
class UObject;
struct FFrame;
struct FGameplayTagContainer;
class UInputDeviceProperty;

/**
 * ULyraWeaponInstance
 *
 * A piece of equipment representing a weapon spawned and applied to a pawn
 */
UCLASS(MinimalAPI)
class ULyraWeaponInstance : public ULyraEquipmentInstance
{
	GENERATED_BODY()

public:
	UE_API ULyraWeaponInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ULyraEquipmentInstance interface
	UE_API virtual void OnEquipped() override;
	UE_API virtual void OnUnequipped() override;
	//~End of ULyraEquipmentInstance interface

	UFUNCTION(BlueprintCallable)
	UE_API void UpdateFiringTime();

	// Returns how long it's been since the weapon was interacted with (fired or equipped)
	UFUNCTION(BlueprintPure)
	UE_API float GetTimeSinceLastInteractedWith() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Animation)
	FLyraAnimLayerSelectionSet EquippedAnimSet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Animation)
	FLyraAnimLayerSelectionSet UneuippedAnimSet;

	/**
	 * Device properties that should be applied while this weapon is equipped.
	 * These properties will be played in with the "Looping" flag enabled, so they will
	 * play continuously until this weapon is unequipped! 
	 */
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "Input Devices")
	TArray<TObjectPtr<UInputDeviceProperty>> ApplicableDeviceProperties;
	
public:
	// Choose the best layer from EquippedAnimSet or UneuippedAnimSet based on the specified gameplay tags
	// (public for SlimeFable: ALyraShooterEnemy relinks layers from C++ without the cosmetics component)
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=Animation)
	UE_API TSubclassOf<UAnimInstance> PickBestAnimLayer(bool bEquipped, const FGameplayTagContainer& CosmeticTags) const;

protected:
	/** Returns the owning Pawn's Platform User ID */
	UFUNCTION(BlueprintCallable)
	UE_API const FPlatformUserId GetOwningUserId() const;

	/** Callback for when the owning pawn of this weapon dies. Removes all spawned device properties. */
	UFUNCTION()
	UE_API void OnDeathStarted(AActor* OwningActor);

	/**
	 * Apply the ApplicableDeviceProperties to the owning pawn of this weapon.
	 * Populate the DevicePropertyHandles so that they can be removed later. This will
	 * Play the device properties in Looping mode so that they will share the lifetime of the
	 * weapon being Equipped.
	 */
	UE_API void ApplyDeviceProperties();

	/** Remove any device proeprties that were activated in ApplyDeviceProperties. */
	UE_API void RemoveDeviceProperties();

private:

	/** Set of device properties activated by this weapon. Populated by ApplyDeviceProperties */
	UPROPERTY(Transient)
	TSet<FInputDevicePropertyHandle> DevicePropertyHandles;

	double TimeLastEquipped = 0.0;
	double TimeLastFired = 0.0;
};

#undef UE_API
