// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeVehicleComponent.generated.h"

class ASlimeVehiclePickup;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API USlimeVehicleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeVehicleComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Slime|Vehicle")
	bool EnterVehicle(ASlimeVehiclePickup* Pickup);

	/** Exit flight: hide mesh, fall, optionally eject upward, drop a reusable pickup. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Vehicle")
	void ExitVehicle(bool bEjectSlime = true);

	UFUNCTION(BlueprintPure, Category = "Slime|Vehicle")
	bool IsUsingVehicle() const { return bUsingVehicle; }

	void SetVehicleMesh(UStaticMeshComponent* InMesh) { VehicleMesh = InMesh; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Vehicle", meta = (ClampMin = "100.0"))
	float FlySpeed = 720.f;

	/** Upward launch when ejecting with F, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Vehicle", meta = (ClampMin = "100.0"))
	float EjectZVelocity = 780.f;

	/** Vertical fly input strength (held E/Q). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Vehicle", meta = (ClampMin = "0.1"))
	float VerticalInputScale = 1.f;

	/** Spawn drop slightly below the capsule so it clears the slime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Vehicle", meta = (Units = "cm"))
	float DropSpawnZOffset = -30.f;

	/** Lift the slime this many cm when mounting so a large vehicle clears the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Vehicle", meta = (ClampMin = "0.0", Units = "cm"))
	float MountLiftZ = 300.f;

protected:
	void ApplyFlyingMovement(bool bEnable);
	void PollVerticalInput();
	void SetVehicleMeshVisible(bool bVisible);
	void ApplyVehicleMeshCollision(bool bEnable);
	ASlimeVehiclePickup* SpawnDroppedPickup() const;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> VehicleMesh;

	UPROPERTY(Transient)
	TSubclassOf<ASlimeVehiclePickup> CachedPickupClass;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CachedPickupMesh;

	/** World-space scale of the mounted pickup (includes actor scale). */
	FVector CachedPickupWorldScale = FVector(0.55f);

	bool bUsingVehicle = false;
	float SavedMaxFlySpeed = 600.f;
	float SavedBrakingFlying = 0.f;
};
