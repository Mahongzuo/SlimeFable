// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyCombatTypes.h"
#include "EnemyCombatComponent.generated.h"

class UAnimInstance;
class UNiagaraSystem;

UCLASS(ClassGroup = (Enemy), meta = (BlueprintSpawnableComponent, PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API UEnemyCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyCombatComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
	bool TryExecute(const FEnemySkillDef& Def);

	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
	void InterruptCombat();

	UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
	bool IsAttacking() const { return bAttacking; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat", meta = (ClampMin = "0.0"))
	float AttackPower = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat")
	FName MuzzleSocket = NAME_None;

protected:
	bool CanStartAction() const;
	bool StartAction(const FEnemySkillDef& Def);
	void TickAction(float DeltaTime);
	void FinishAction();
	void FireHit();
	void ExecuteDash(const FEnemySkillDef& Def, const FVector& Forward);
	void ExecuteProjectile(const FEnemySkillDef& Def, const FVector& Forward);
	void SpawnVfx(const TSoftObjectPtr<UNiagaraSystem>& SoftSystem, const FVector& Location) const;
	FVector GetAimForward() const;
	FVector GetMuzzleLocation() const;
	float ResolveDamage(const FEnemySkillDef& Skill) const;

	FEnemySkillDef ActiveDef;
	FVector ActiveForward = FVector::ForwardVector;
	float ActionElapsed = 0.f;
	bool bAttacking = false;
	bool bHitFired = false;
	TSet<TWeakObjectPtr<AActor>> AlreadyHit;
};
