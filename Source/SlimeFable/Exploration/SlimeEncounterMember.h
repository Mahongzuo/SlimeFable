#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeEncounterMember.generated.h"

class ASlimeEncounterDirector;

/** Runtime-only ownership: copies created by devour/morph never inherit this component. */
UCLASS()
class SLIMEFABLE_API USlimeEncounterMember : public UActorComponent
{
	GENERATED_BODY()
public:
	USlimeEncounterMember();
	void Initialize(ASlimeEncounterDirector* InDirector, FGuid InId, float HealthScale, float DamageScale, float HealthFraction, int32 HealthPhase = 1);
	void SetSleeping(bool bSleep);
	bool IsEngaged() const;
	bool IsSleeping() const { return bSleeping; }
	float GetDamageMultiplier() const;
	FGuid GetRecordId() const { return RecordId; }
	static float DamageMultiplier(const AActor* Actor);
	UFUNCTION() void HandleDied();
	UFUNCTION() void HandleHealth(float Current, float Max);
private:
	TWeakObjectPtr<ASlimeEncounterDirector> Director;
	FGuid RecordId;
	float DamageMultiplierValue = 1.f;
	float LastHP = 0.f;
	float LastHitTime = -100.f;
	bool bSleeping = false;
	bool bDefeatReported = false;
	struct FActorState { TWeakObjectPtr<AActor> Actor; bool bHidden; bool bTick; bool bCollision; };
	struct FComponentState { TWeakObjectPtr<UActorComponent> Component; bool bTick; };
	TArray<FActorState> ActorStates;
	TArray<FComponentState> ComponentStates;
};
