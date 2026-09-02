// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnemyFighter.h"
#include "PhoebeEnemy.generated.h"

class UAnimMontage;
class UCharacterTrajectoryComponent;
class UPhoebeClimbComponent;

/** Devourable / morphable Phoebe fighter with capsule wall-climb. */
UCLASS()
class SLIMEFABLE_API APhoebeEnemy : public AEnemyFighter
{
	GENERATED_BODY()

public:
	APhoebeEnemy();

	UFUNCTION(BlueprintPure, Category = "Phoebe")
	UPhoebeClimbComponent* GetClimbComponent() const { return Climb; }

	/** LMB while falling/climbing: start the two-gravity plunge attack. */
	bool TryStartAirAttack();

	/** RMB dodge only while an enemy attack is incoming; otherwise RMB is sprint/climb-dash. */
	bool WantsCombatDodge() const;

	virtual void Tick(float DeltaSeconds) override;
	virtual void EnterStagger(float Duration, AActor* StaggerInstigator) override;
	virtual void InitAsMorphTarget(AActor* Master) override;
	virtual void Landed(const FHitResult& Hit) override;

	/** Moves[0] uses LightAttack lock; Q/E/R skills use Skill lock. */
	float ResolveMoveLockSeconds(const FEnemySkillDef& Def) const;
	float GetAirAttackLandMoveLockSeconds() const { return AirAttackLandMoveLockSeconds; }

protected:
	virtual void MorphMove(const FInputActionValue& Value) override;
	virtual bool ShouldInterruptCombatOnMove() const override { return true; }
	virtual void MorphJump() override;
	virtual void UpdateMorphSprintSpeed() override;

	void ApplyThirdPersonLocomotionDefaults();
	void UpdateFaceHeadForward();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<UPhoebeClimbComponent> Climb;

	/** GASP CMC-style trajectory for Motion Matching (world-space history + prediction). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<UCharacterTrajectoryComponent> CharacterTrajectory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Phoebe",
		meta = (ToolTip = "破韧受击 Montage（DefaultSlot）。默认 AM_Phoebe_Behit_S_L。空则只走通用 stagger。"))
	TSoftObjectPtr<UAnimMontage> HitReactMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Phoebe|AirAttack",
		meta = (ToolTip = "空中左键下落攻击的起手 Montage。绑定 AirAttack_Start，DefaultSlot。"))
	TSoftObjectPtr<UAnimMontage> AirAttackStartMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Phoebe|AirAttack",
		meta = (ToolTip = "高处下落期间反复播放的 Montage。绑定 AirAttack_Loop，DefaultSlot。"))
	TSoftObjectPtr<UAnimMontage> AirAttackLoopMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Phoebe|AirAttack",
		meta = (ToolTip = "下落攻击落地时播放的 Montage。绑定 AirAttack_End，DefaultSlot。"))
	TSoftObjectPtr<UAnimMontage> AirAttackEndMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Phoebe|AirAttack",
		meta = (ClampMin = "1.0", ToolTip = "下落攻击期间的重力倍率。默认 2，结束或取消后恢复原值。"))
	float AirAttackGravityMultiplier = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Phoebe|AirAttack",
		meta = (ClampMin = "1.0", Units = "cm/s", ToolTip = "触发下落攻击时保证的最低向下速度。默认 220。"))
	float AirAttackInitialDownSpeed = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Phoebe|AirAttack",
		meta = (ClampMin = "0.0", ToolTip = "下落攻击落地范围伤害。默认 28。"))
	float AirAttackDamage = 28.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Phoebe|Combat",
		meta = (ClampMin = "0.0", Units = "s",
			ToolTip = "普攻（左键 / Moves[0]）锁定移动的秒数。超时后 WASD 平滑掐 Montage。默认 0.5。"))
	float LightAttackMoveLockSeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Phoebe|Combat",
		meta = (ClampMin = "0.0", Units = "s",
			ToolTip = "技能（Q/E/R）锁定移动的秒数。超时后 WASD 平滑掐 Montage。默认 2。"))
	float SkillMoveLockSeconds = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Phoebe|Combat",
		meta = (ClampMin = "0.0", Units = "s",
			ToolTip = "下落攻击检测到落地后再锁定移动的秒数。超时后 WASD 掐 End Montage。默认 1。"))
	float AirAttackLandMoveLockSeconds = 1.f;
};
