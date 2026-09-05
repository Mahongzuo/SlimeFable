// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GaspEnemyAIController.generated.h"

class AGaspSandboxPawn;
class UEnemyCombatComponent;

UENUM()
enum class EGaspEnemyAIState : uint8
{
	Idle,
	Chase,
	Telegraph,
	Execute,
	Recover
};

/**
 * Lightweight chase / attack brain for AGaspSandboxPawn (official Mover + NavMover).
 * Does not use EnemyEncounterSubsystem attack slots.
 */
UCLASS()
class SLIMEFABLE_API AGaspEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AGaspEnemyAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Run one AI decision frame (also callable from pawn if controller tick is dormant). */
	void DriveCombatAI(float DeltaSeconds);

protected:
	void ReturnToIdle();
	APawn* FindCombatFocus() const;
	void TickIdle(float DeltaSeconds, float Dist);
	void TickWander(float DeltaSeconds);
	void StartWanderTo(const FVector& Dest);
	void TickChase(float DeltaSeconds, float Dist2D);
	void TickTelegraph(float DeltaSeconds);
	void TickExecute();
	void TickRecover(float DeltaSeconds);
	void EnterTelegraph(int32 MoveIndex);
	void BeginExecute();
	int32 SelectMove(float Dist2D) const;
	void FacePlayer();
	void DriveTowardPlayer(float Preferred);
	bool HasRecastNavMesh() const;
	void StopPathIfMoving();

	UPROPERTY()
	TObjectPtr<AGaspSandboxPawn> Enemy;

	UPROPERTY()
	TObjectPtr<UEnemyCombatComponent> Combat;

	EGaspEnemyAIState State = EGaspEnemyAIState::Idle;
	int32 ActiveMoveIndex = INDEX_NONE;
	float StateTime = 0.f;
	uint64 LastDriveFrame = 0;
	TArray<float> MoveCooldowns;

	float WanderPauseRemaining = 0.f;
	float WanderStuckTime = 0.f;
	bool bWasWanderMoving = false;
	FVector WanderLastPos = FVector::ZeroVector;
	FVector WanderDest = FVector::ZeroVector;
	float LastNavRequestTime = -1000.f;
	FVector LastNavGoal = FVector::ZeroVector;
};
