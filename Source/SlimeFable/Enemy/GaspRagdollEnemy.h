// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GaspSandboxPawn.h"
#include "GaspRagdollEnemy.generated.h"

struct FCharacterDefaultInputs;

/**
 * GASP enemy whose Blueprint derives from a copy of the official SandboxCharacter_Mover_Ragdoll.
 *
 * Everything slime (devour / morph / traversal / smart objects / combat / HUD) is inherited from
 * AGaspSandboxPawn. Hits still go through the official ragdoll kit. Death prefers official
 * Ragdoll mode; if that never activates it falls back to Echo mesh physics.
 */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API AGaspRagdollEnemy : public AGaspSandboxPawn
{
	GENERATED_BODY()

public:
	AGaspRagdollEnemy();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;
	virtual void TriggerSandboxRagdoll() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Ragdoll|Official",
		meta = (ToolTip = "死亡时写入 BP 变量 Ragdoll_InjuryState 的枚举项名。默认 Limp（全身瘫软，不爬起）。可填 None/Limp/Stunned/Head_Face/Head_Back/Body_Front/Groin。留空则不改。"))
	FName DeathInjuryState = TEXT("Limp");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Ragdoll|Official",
		meta = (ToolTip = "累计伤害倒地时写入 Ragdoll_InjuryState 的枚举项名。默认 Stunned（能被官方爬起接管）。留空则不改。"))
	FName KnockdownInjuryState = TEXT("Stunned");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Ragdoll|Official",
		meta = (ToolTip = "受击时传给官方 SetPhysicsProfile 的 profile 名。默认空=不调用。要用先看日志里 dump 出来的参数表。"))
	FName HitPhysicsProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Ragdoll|Official",
		meta = (ClampMin = "0.0",
			ToolTip = "轻击后退倍率。默认 1。官方 OnRagdollHit 已经把人推开时不会再叠。调大=退得更远，0=绝不补击退。"))
	float HitKnockbackScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Ragdoll|Official",
		meta = (ClampMin = "0.0",
			ToolTip = "伤害冲量为 0 时传给 OnRagdollHit 的默认冲量大小（厘米/秒）。默认 400。"))
	float HitDefaultImpulse = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Ragdoll|Official",
		meta = (ToolTip = "倒地后是否调官方 Ragdoll_PlayRollingGetups 爬起。默认开。关掉就一直躺着直到超时。"))
	bool bOfficialGetUpAfterKnockdown = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Ragdoll|Official",
		meta = (ToolTip = "受击是否额外播四向受击蒙太奇。默认关：只走官方物理受击 + 闪红，避免和 Motion Matching 打架。"))
	bool bPlayHitReactMontage = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Ragdoll|Official",
		meta = (ToolTip = "BeginPlay 时把官方 ragdoll 函数的参数表打进日志（排签名用）。默认开，稳定后可关。"))
	bool bLogRagdollSignatures = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Death",
		meta = (ClampMin = "1.0", Units = "cm/s",
			ToolTip = "布娃娃速度低于这个值算静止，开始进入溶解。默认 40。调大=更早消失。"))
	float DeathRagdollSettleSpeed = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Death",
		meta = (ClampMin = "0.1", Units = "s",
			ToolTip = "需要连续静止多久才认定倒地完成。默认 0.35。"))
	float DeathRagdollSettleHold = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Death",
		meta = (ClampMin = "0.5", Units = "s",
			ToolTip = "等静止的最长秒数，超时也强行进入溶解（防止卡在斜坡上一直滑）。默认 6。"))
	float DeathRagdollMaxSettleSeconds = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Death",
		meta = (ClampMin = "0.0", Units = "s",
			ToolTip = "静止后再停留多久才开始溶解。默认 0.8。和 DeathDissolveSeconds 串联成总消失时长。"))
	float DeathDissolveDelaySeconds = 0.8f;

protected:
	virtual void BeginKnockdownDeath() override;
	virtual void BeginCombatKnockdown() override;
	virtual void PlayCombatGetUp() override;
	virtual void StartDeathDissolve() override;

	/** Official BP_MovementMode_Ragdoll owns the physics — kill every manual-physics fallback. */
	virtual void KeepDeathRagdollPhysics() override {}
	virtual void RestoreUnexpectedRagdoll() override {}
	virtual void ConfirmDeathRagdollThenStopAI() override {}

	virtual bool ApplyPendingRagdollInput(FCharacterDefaultInputs& Inputs) override;

	/**
	 * Enter the official ragdoll with an injury state.
	 * The BP event is TriggerRagdoll(bool StopActiveMontages, FMontageBlendSettings, injury enum),
	 * so the injury is passed as an argument rather than only written to the member variable.
	 */
	void TriggerOfficialRagdoll(FName InjuryEntry);
	bool CallOfficialTriggerRagdoll(FName InjuryEntry);
	bool CallOfficialOnRagdollHit(AActor* DamageCauser, const FVector& HitLocation, const FVector& Impulse);
	void ApplyHitKnockbackIfNeeded(const FVector& Impulse);

	void BeginRagdollDeath();
	void ApplyManualDeathPhysics();
	void TickRagdollDeath(float DeltaSeconds);
	bool IsInOfficialRagdollMode() const;
	bool IsSourceMeshSimulating() const;
	bool IsAnyDeathMeshSimulating() const;
	void RequestOfficialRagdollMode();
	void EnsureEchoFollowsSource();
	void LogRagdollRuntimeState(const TCHAR* Reason) const;
	bool CallOfficialGetUp();
	void ApplyOfficialHitFeedback(AActor* DamageCauser, const FVector& HitLocation, const FVector& Impulse);
	/** FName(TEXT("None")) is NAME_None, so the enum's own "None" entry needs bAllowNoneEntry. */
	bool SetInjuryState(FName EntryName, bool bAllowNoneEntry = false);
	bool CallBpFunctionWithName(FName FunctionName, FName Value);
	void LogOfficialRagdollSignatures() const;
	float MeasureRagdollSpeed(float DeltaSeconds);

	bool bRagdollDeathActive = false;
	bool bDeathDissolveScheduled = false;
	bool bRagdollSpeedPrimed = false;
	bool bDeathRagdollRetried = false;
	float DeathRagdollStartTime = 0.f;
	float DeathSettleAccum = 0.f;
	FVector PrevRagdollSample = FVector::ZeroVector;
};
