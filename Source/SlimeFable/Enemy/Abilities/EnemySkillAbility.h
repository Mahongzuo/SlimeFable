#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "EnemySkillAbility.generated.h"

class UEnemyCombatComponent;

/**
 * Thin GAS execution shell for legacy enemy skills. The combat component remains
 * the compatibility timeline, while activation/interrupt/cooldown ownership moves
 * through the enemy ASC so StateTree and future Blueprint abilities share a route.
 */
UCLASS()
class SLIMEFABLE_API UEnemySkillAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UEnemySkillAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	void EndFromCombat();
};
