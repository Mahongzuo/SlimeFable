// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SuperHeroXinAIController.generated.h"

class ASuperHeroXinEnemy;
class UEnemyCombatComponent;

UENUM()
enum class ESuperHeroXinAIState : uint8
{
	Idle,
	Chase,
	Telegraph,
	Execute,
	Recover
};

/** Ground chase / punch brain. Never starts Superhero flight. */
UCLASS()
class SLIMEFABLE_API ASuperHeroXinAIController : public AAIController
{
	GENERATED_BODY()

public:
	ASuperHeroXinAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	void ReturnToIdle();
	APawn* FindCombatFocus() const;
	void TickIdle(float Dist);
	void TickChase(float Dist2D);
	void TickTelegraph(float DeltaSeconds);
	void TickExecute();
	void TickRecover(float DeltaSeconds);
	void EnterTelegraph(int32 MoveIndex);
	void BeginExecute();
	int32 SelectMove(float Dist2D) const;
	void FacePlayer();
	void DriveTowardPlayer(float Preferred);

	UPROPERTY()
	TObjectPtr<ASuperHeroXinEnemy> Enemy;

	UPROPERTY()
	TObjectPtr<UEnemyCombatComponent> Combat;

	ESuperHeroXinAIState State = ESuperHeroXinAIState::Idle;
	int32 ActiveMoveIndex = INDEX_NONE;
	float StateTime = 0.f;
	TArray<float> MoveCooldowns;
};
