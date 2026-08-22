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

	/** When true, Tick polls player combat keys instead of waiting for AI. Set by the morph system. */
	void SetPlayerMorphed(bool bIn) { bPlayerMorphed = bIn; }
	bool IsPlayerMorphed() const { return bPlayerMorphed; }

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

	/** Player combat key polling while morphed (mirrors USlimeCombatComponent::PollCombatKeys). */
	void PollPlayerCombatKeys(float DeltaTime);

	FEnemySkillDef ActiveDef;
	FVector ActiveForward = FVector::ForwardVector;
	float ActionElapsed = 0.f;
	bool bAttacking = false;
	bool bHitFired = false;
	bool bPlayerMorphed = false;
	TSet<TWeakObjectPtr<AActor>> AlreadyHit;
};
