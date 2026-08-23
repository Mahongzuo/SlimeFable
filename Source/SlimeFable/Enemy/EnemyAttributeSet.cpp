// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyAttributeSet.h"

#include "EnemyCharacter.h"
#include "GameplayEffectExtension.h"

void UEnemyAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetPoiseAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxPoise());
	}
	else if (Attribute == GetGuardAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void UEnemyAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Data.Target.GetAvatarActor());
	AActor* Instigator = Data.EffectSpec.GetEffectContext().GetOriginalInstigator();

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = GetIncomingDamage();
		SetIncomingDamage(0.f);
		if (Damage > 0.f)
		{
			SetHealth(FMath::Clamp(GetHealth() - Damage, 0.f, GetMaxHealth()));
			if (Enemy)
			{
				Enemy->OnGasDamageApplied(Damage, Instigator);
			}
		}
		return;
	}

	if (Data.EvaluatedData.Attribute == GetIncomingHealingAttribute())
	{
		const float Healing = GetIncomingHealing();
		SetIncomingHealing(0.f);
		if (Healing > 0.f)
		{
			SetHealth(FMath::Clamp(GetHealth() + Healing, 0.f, GetMaxHealth()));
			if (Enemy)
			{
				Enemy->OnGasHealingApplied(Healing, Instigator);
			}
		}
		return;
	}

	if (Data.EvaluatedData.Attribute == GetIncomingPoiseDamageAttribute())
	{
		const float PoiseDamage = GetIncomingPoiseDamage();
		SetIncomingPoiseDamage(0.f);
		if (PoiseDamage <= 0.f)
		{
			return;
		}
		SetPoise(FMath::Clamp(GetPoise() - PoiseDamage, 0.f, GetMaxPoise()));
		if (Enemy && GetPoise() <= 0.f)
		{
			Enemy->OnPoiseBroken(Instigator);
		}
		return;
	}

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
}
