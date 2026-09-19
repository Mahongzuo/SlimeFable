// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "SlimeLyraWeaponPickup.generated.h"

class ULyraWeaponPickupDefinition;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Lab weapon / ammo pad for Lyra bodies. Reads ShooterCore's WeaponPickupData_* (mesh, item def,
 * cooldown) but gives the item to the pawn's own inventory (ALyraShooterEnemy::GiveWeaponItem) instead of
 * the controller-side QuickBar that B_WeaponSpawner expects. Only a player-controlled Lyra body (the slime
 * morphed into a shooter) can pick it up; AI bodies and the plain slime walk through.
 * Already own that gun → the pad works as an ammo box.
 */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeLyraWeaponPickup : public AActor
{
	GENERATED_BODY()

public:
	ASlimeLyraWeaponPickup();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config",
		meta = (ToolTip = "ShooterCore 的 WeaponPickupData_Rifle / Pistol / Shotgun。决定模型、物品、冷却。"))
	TObjectPtr<ULyraWeaponPickupDefinition> PickupDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config",
		meta = (ClampMin = "0", ClampMax = "20",
			ToolTip = "已持有同款枪时补几个弹匣的备弹。默认 2。"))
	int32 AmmoMagazines = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config",
		meta = (ClampMin = "0.0", ClampMax = "120.0", Units = "s",
			ToolTip = "覆盖冷却秒数；<=0 用 PickupDefinition.SpawnCoolDownSeconds。默认 0。"))
	float CoolDownOverride = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config",
		meta = (ClampMin = "0.0", ClampMax = "720.0",
			ToolTip = "展示模型自转速度（度/秒）。默认 60。"))
	float MeshSpinDegPerSec = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config",
		meta = (ToolTip = "允许 AI 控制的 Lyra 身体拾取。默认关，否则 AI 会把枪都吸走。"))
	bool bAllowAIPickup = false;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<UStaticMeshComponent> PadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	UPROPERTY(ReplicatedUsing = OnRep_Available)
	bool bAvailable = true;

	UFUNCTION()
	void OnRep_Available();

	UFUNCTION()
	void OnTriggerBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void ApplyDefinitionVisuals();
	bool TryGiveTo(AActor* Other);
	void StartCoolDown();
	void OnCoolDownFinished();
	void RefreshVisibility();

	FTimerHandle CoolDownHandle;
};
