#include "EnemySkillAbility.h"

#include "AbilitySystemComponent.h"
#include "EnemyCharacter.h"
#include "EnemyCombatComponent.h"
#include "SlimeEnemyGameplayTags.h"

UEnemySkillAbility::UEnemySkillAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	FGameplayTagContainer DefaultAbilityTags;
	DefaultAbilityTags.AddTag(SlimeEnemyTags::Ability_Skill);
	SetAssetTags(DefaultAbilityTags);
	ActivationOwnedTags.AddTag(SlimeEnemyTags::State_Attacking);
}

void UEnemySkillAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AEnemyCharacter* Enemy = ActorInfo ? Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UEnemyCombatComponent* Combat = Enemy ? Enemy->GetEnemyCombat() : nullptr;
	if (!Combat || !Combat->BeginGasAbility(this))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		Combat->InterruptCombat();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UEnemySkillAbility::EndFromCombat()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
