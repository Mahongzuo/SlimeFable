// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyCombatTypes.h"
#include "EnemyFighterAIController.generated.h"

class AEnemyFighter;
class UAnimMontage;
class UEnemyCombatComponent;
class UNiagaraComponent;
class UStateTree;
class UStateTreeComponent;

UENUM(BlueprintType)
enum class EEnemyFighterState : uint8
{
	Idle,
	Combat,
	Choose,
	Telegraph,
	Execute,
	Recover
};

UCLASS()
class SLIMEFABLE_API AEnemyFighterAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyFighterAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

	bool IsEngaged() const { return State != EEnemyFighterState::Idle; }
	void ReturnToIdle();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.05", Units = "s"))
	float PathRefreshInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|StateTree",
		meta = (ToolTip = "可选共享敌人 StateTree。加载成功后由 StateTree 决策，未配置或加载失败自动回退旧状态机。"))
	TSoftObjectPtr<UStateTree> DecisionStateTree;

protected:
	virtual APawn* FindCombatFocus() const;
	virtual void TickIdle(float DeltaSeconds, float Dist);
	void TickWander(float DeltaSeconds);
	void StartWanderTo(const FVector& Dest);
	void TickDirectWander();
	void TryPlayRandomIdleMontage();
	void StopIdleMontage();
	void PlayWalkAnim();
	void PlayRunAnim();
	void StopLocomotionAnim();
	void ClearDirectWander();
	void ApplyLocomotionMaxSpeed(bool bChasing);
	void UpdateWalkPlayRate();
	void TickCombat(float DeltaSeconds, float Dist);
	void EnterTelegraph(int32 MoveIndex);
	void TickTelegraph(float DeltaSeconds);
	void BeginExecute();
	void TickExecute();
	void TickRecover(float DeltaSeconds);
	int32 SelectMove(float Dist) const;
	int32 FindMoveIndexById(FName MoveId) const;
	void FacePlayer();
	void RequestMoveToPreferred(float Dist);
	void ResetChaseFallback();
	bool TryMoveToNavLocation(const FVector& Dest);
	/** Sample points toward focus (fwd / fwd-side / side); MoveTo first navigable. */
	bool RequestNavDetourTowardFocus();
	void PlayChaseLocomotionIfMoving();
	void ClearTelegraphFx();
	bool IsMeleeEngageMove(const FEnemyMoveDef& Move) const;
	bool IsRangedProjectileMove(const FEnemyMoveDef& Move) const;
	bool IsGapCloserDashMove(const FEnemyMoveDef& Move) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat", meta = (ClampMin = "50.0", Units = "cm",
		ToolTip = "水平距离 ≤ 此值才可选近战/近距 AoE；更远仅远程（CD 好），否则只追击。默认 200。"))
	float MeleeEngageDistance = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "50.0", Units = "cm",
		ToolTip = "卡住时朝玩家方向绕障采样距离。默认 160。"))
	float SideStepOffset = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.1", Units = "s",
		ToolTip = "Nav 全失败后短直推最长秒数，超时改再采 Nav。默认 0.75。"))
	float DirectChaseMaxSeconds = 0.75f;

	UPROPERTY(Transient)
	TObjectPtr<AEnemyFighter> Fighter;

	UPROPERTY(Transient)
	TObjectPtr<UEnemyCombatComponent> Combat;

	EEnemyFighterState State = EEnemyFighterState::Idle;
	int32 ActiveMoveIndex = INDEX_NONE;
	float StateTime = 0.f;
	float PathRefreshRemaining = 0.f;
	float WanderPauseRemaining = 0.f;
	bool bWasWanderMoving = false;
	bool bDirectWander = false;
	bool bPlayingWalk = false;
	bool bPlayingRun = false;
	FVector WanderDirectDest = FVector::ZeroVector;
	float WanderStuckTime = 0.f;
	FVector WanderLastPos = FVector::ZeroVector;
	float ChaseStalledSeconds = 0.f;
	FVector ChaseLastPos = FVector::ZeroVector;
	bool bDirectChaseFallback = false;
	float DirectChaseActiveSeconds = 0.f;
	int32 DetourSampleIndex = 0;
	TArray<float> MoveCooldowns;
	TWeakObjectPtr<UNiagaraComponent> TelegraphFx;
	TWeakObjectPtr<UAnimMontage> PlayingIdleMontage;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|StateTree")
	TObjectPtr<UStateTreeComponent> StateTreeComponent;

	bool bUsingStateTree = false;
};
