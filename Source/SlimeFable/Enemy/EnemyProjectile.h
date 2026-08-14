// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyCombatTypes.h"
#include "EnemyProjectile.generated.h"

class USphereComponent;
class UNiagaraComponent;

UCLASS()
class SLIMEFABLE_API AEnemyProjectile : public AActor
{
	GENERATED_BODY()

public:
	AEnemyProjectile();

	void InitProjectile(AActor* InInstigator, const FEnemySkillDef& InSkill, const FVector& InVelocity);

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ExplodeAndDestroy(bool bSpawnImpact = false);

protected:
	void UpdateHoming(float DeltaSeconds);
	AActor* FindHomingTarget(float MaxRange) const;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UNiagaraComponent> Niagara;

	FEnemySkillDef Skill;
	FVector Velocity = FVector::ZeroVector;
	FVector FallbackAimPoint = FVector::ZeroVector;
	float LifeRemaining = 2.f;
	float WorldCollisionGrace = 0.1f;
	float Age = 0.f;
	bool bHoming = false;
	TSet<TWeakObjectPtr<AActor>> AlreadyHit;
	TWeakObjectPtr<AActor> Source;
};
