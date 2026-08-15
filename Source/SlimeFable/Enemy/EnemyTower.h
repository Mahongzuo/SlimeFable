// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnemyCharacter.h"
#include "EnemyCombatTypes.h"
#include "EnemyTower.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UNiagaraSystem;

UCLASS()
class SLIMEFABLE_API AEnemyTower : public AEnemyCharacter
{
	GENERATED_BODY()

public:
	AEnemyTower();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetEnemyPresence(EEnemyPresence NewPresence) override;
	virtual bool IsInCombat() const override;
	virtual void OnRestoredToSpawn() override;
	virtual void ApplyDifficultyToCombat(float DamageMul, float IntervalMul) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Tower", meta = (ClampMin = "100.0", Units = "cm"))
	float AttackRange = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Tower", meta = (ClampMin = "0.1", Units = "s"))
	float FireInterval = 1.2f;

	/** Delay after attack telegraph notify before beam/projectile actually fires. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Tower", meta = (ClampMin = "0.0", Units = "s"))
	float FireTelegraphTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Tower")
	EEnemyTowerFireMode FireMode = EEnemyTowerFireMode::Beam;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Tower|Beam", meta = (EditCondition = "FireMode == EEnemyTowerFireMode::Beam", EditConditionHides, ClampMin = "0.0"))
	float BeamDamage = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Tower|Beam", meta = (EditCondition = "FireMode == EEnemyTowerFireMode::Beam", EditConditionHides, ClampMin = "0.02", Units = "s"))
	float BeamDuration = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Tower|Beam", meta = (EditCondition = "FireMode == EEnemyTowerFireMode::Beam", EditConditionHides, ClampMin = "1.0", Units = "cm"))
	float BeamThickness = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Tower|Beam", meta = (EditCondition = "FireMode == EEnemyTowerFireMode::Beam", EditConditionHides))
	TSoftObjectPtr<UNiagaraSystem> BeamNiagara;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Tower|Beam", meta = (EditCondition = "FireMode == EEnemyTowerFireMode::Beam", EditConditionHides))
	bool bShowFallbackBeamMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Tower|Beam", meta = (EditCondition = "FireMode == EEnemyTowerFireMode::Beam", EditConditionHides))
	bool bDrawDebugBeam = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Tower|Projectile", meta = (EditCondition = "FireMode == EEnemyTowerFireMode::Projectile", EditConditionHides))
	FEnemySkillDef MissileSkill;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Tower|Projectile", meta = (EditCondition = "FireMode == EEnemyTowerFireMode::Projectile", EditConditionHides))
	TSoftObjectPtr<UNiagaraSystem> MuzzleNiagara;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Tower")
	FName AimSocket = NAME_None;

protected:
	UFUNCTION()
	void HandleAggroBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleAggroEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void SyncRangeSphere();
	void StartFiring();
	void StopFiring();
	void FireAtTarget();
	void CommitFire();
	void FireBeam(AActor* Target);
	void FireProjectile(AActor* Target);
	void HideFallbackBeam();
	FVector GetMuzzleLocation() const;
	APawn* ResolvePlayerTarget() const;
	void FaceTarget(AActor* Target);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<USphereComponent> AggroSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<USphereComponent> EditorRangeVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UStaticMeshComponent> FallbackBeamMesh;

	FTimerHandle FireTimerHandle;
	FTimerHandle FireTelegraphTimerHandle;
	FTimerHandle BeamHideTimerHandle;
	TWeakObjectPtr<APawn> CurrentTarget;
	bool bPlayerInRange = false;
	float DifficultyBaseFireInterval = 1.2f;
	float DifficultyBaseBeamDamage = 12.f;
	float DifficultyBaseMissileDamage = 16.f;
	bool bCombatBasesCaptured = false;
};
