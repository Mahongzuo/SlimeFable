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
	void ClearTelegraphFx();

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
	TArray<float> MoveCooldowns;
	TWeakObjectPtr<UNiagaraComponent> TelegraphFx;
	TWeakObjectPtr<UAnimMontage> PlayingIdleMontage;
};
