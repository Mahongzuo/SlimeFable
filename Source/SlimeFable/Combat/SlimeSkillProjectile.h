// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeCombatTypes.h"
#include "SlimeSkillProjectile.generated.h"

class USphereComponent;
class UNiagaraComponent;

UCLASS()
class SLIMEFABLE_API ASlimeSkillProjectile : public AActor
{
	GENERATED_BODY()

public:
	ASlimeSkillProjectile();

	void InitProjectile(AActor* InInstigator, const FSlimeSkillDef& InSkill, const FVector& InVelocity);

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

	FSlimeSkillDef Skill;
	FVector Velocity = FVector::ZeroVector;
	FVector FallbackAimPoint = FVector::ZeroVector;
	float LifeRemaining = 1.4f;
	float WorldCollisionGrace = 0.12f;
	float Age = 0.f;
	float HomingRange = 1200.f;
	float HomingTurnRate = 6.f;
	bool bHoming = false;
	TSet<TWeakObjectPtr<AActor>> AlreadyHit;
	TWeakObjectPtr<AActor> Source;
};
