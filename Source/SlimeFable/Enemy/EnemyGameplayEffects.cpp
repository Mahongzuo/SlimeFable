// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyGameplayEffects.h"

#include "EnemyAttributeSet.h"
#include "SlimeEnemyGameplayTags.h"

namespace
{
	FSetByCallerFloat MakeSetByCaller(const FGameplayTag& DataTag)
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = DataTag;
		return SetByCaller;
	}

	FGameplayModifierInfo MakeSetByCallerModifier(
		const FGameplayAttribute& Attribute,
		const FGameplayTag& DataTag,
		EGameplayModOp::Type Op = EGameplayModOp::AddBase)
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = Op;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(MakeSetByCaller(DataTag));
		return Modifier;
	}
}

UGE_EnemyDamage::UGE_EnemyDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeSetByCallerModifier(
		UEnemyAttributeSet::GetIncomingDamageAttribute(), SlimeEnemyTags::Data_Damage));
	Modifiers.Add(MakeSetByCallerModifier(
		UEnemyAttributeSet::GetIncomingPoiseDamageAttribute(), SlimeEnemyTags::Data_PoiseDamage));
}

UGE_EnemyHealing::UGE_EnemyHealing()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeSetByCallerModifier(
		UEnemyAttributeSet::GetIncomingHealingAttribute(), SlimeEnemyTags::Data_Healing));
}

UGE_EnemyTimedState::UGE_EnemyTimedState()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(MakeSetByCaller(SlimeEnemyTags::Data_Duration));
}

void UGE_EnemyTimedState::GrantStateTag(const FGameplayTag& Tag)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(Tag);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

UGE_EnemyStagger::UGE_EnemyStagger()
{
	GrantStateTag(SlimeEnemyTags::State_Staggered);
}

UGE_EnemyInvulnerable::UGE_EnemyInvulnerable()
{
	GrantStateTag(SlimeEnemyTags::State_Invulnerable);
}

UGE_EnemyGuard::UGE_EnemyGuard()
{
	GrantStateTag(SlimeEnemyTags::State_Guarding);
}

UGE_EnemySuperArmor::UGE_EnemySuperArmor()
{
	GrantStateTag(SlimeEnemyTags::State_SuperArmor);
}

UGE_EnemyEmpower::UGE_EnemyEmpower()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(MakeSetByCaller(SlimeEnemyTags::Data_Duration));
	Modifiers.Add(MakeSetByCallerModifier(
		UEnemyAttributeSet::GetDamagePowerAttribute(),
		SlimeEnemyTags::Data_Power,
		EGameplayModOp::MultiplyCompound));
}
