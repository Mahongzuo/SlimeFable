// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyCombatTypes.h"
#include "EnemyCombatComponent.generated.h"

class UAnimInstance;
class UNiagaraSystem;
class UEnemySkillAbility;

UCLASS(ClassGroup = (Enemy), meta = (BlueprintSpawnableComponent, PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API UEnemyCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyCombatComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
	bool TryExecute(const FEnemySkillDef& Def);

	/** Called by UEnemySkillAbility after the ASC accepted the activation. */
	bool BeginGasAbility(UEnemySkillAbility* Ability);

	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
	void InterruptCombat();

	UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
	bool IsAttacking() const { return bAttacking; }

	/** When true, Tick polls player combat keys instead of waiting for AI. Set by the morph system. */
	void SetPlayerMorphed(bool bIn) { bPlayerMorphed = bIn; }
	bool IsPlayerMorphed() const { return bPlayerMorphed; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat", meta = (ClampMin = "0.0"))
	float AttackPower = 12.f;

	/** Extra seconds after Windup+HitStart before damage fires (aligns hit with montage contact). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat", meta = (ClampMin = "-1.0", Units = "s",
		ToolTip = "命中相对招表 Windup+HitStart 的偏移秒数（可为负提前）。武士默认约 -0.1。"))
	float GlobalHitDelay = 0.5f;

	/** Melee/AoE will not ApplyDamage if horizontal distance to player exceeds this (safety vs far slash). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat", meta = (ClampMin = "50.0", Units = "cm",
		ToolTip = "近战/AoE 对玩家水平距离超过此值则不结算伤害。默认 200。"))
	float MaxMeleeHitDistance = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Combat")
	FName MuzzleSocket = NAME_None;

protected:
	bool CanStartAction() const;
	bool StartAction(const FEnemySkillDef& Def);
	void TickAction(float DeltaTime);
	void FinishAction();
	void FireHit();
	float GetHitFireTime() const;
	void ExecuteDash(const FEnemySkillDef& Def, const FVector& Forward);
	void ExecuteProjectile(const FEnemySkillDef& Def, const FVector& Forward);
	void SpawnVfx(const TSoftObjectPtr<UNiagaraSystem>& SoftSystem, const FVector& Location) const;
	FVector GetAimForward() const;
	FVector GetMuzzleLocation() const;
	float ResolveDamage(const FEnemySkillDef& Skill) const;
	float GetAuraAttackIntervalMul() const;

	/** Player combat key polling while morphed (mirrors USlimeCombatComponent::PollCombatKeys). */
	void PollPlayerCombatKeys(float DeltaTime);

	FEnemySkillDef ActiveDef;
	FVector ActiveForward = FVector::ForwardVector;
	float ActionElapsed = 0.f;
	bool bAttacking = false;
	bool bHitFired = false;
	bool bActionAnimationStarted = false;
	bool bPlayerMorphed = false;
	TSet<TWeakObjectPtr<AActor>> AlreadyHit;
	FEnemySkillDef PendingGasDef;
	TWeakObjectPtr<UEnemySkillAbility> ActiveGasAbility;
	float AttackLockRemaining = 0.f;
};
