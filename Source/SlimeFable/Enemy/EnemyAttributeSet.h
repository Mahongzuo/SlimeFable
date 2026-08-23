// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "EnemyAttributeSet.generated.h"

#define ENEMY_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 *  Authoritative combat stats for every enemy. Health mirrors into the legacy
 *  USlimeHealthComponent facade so health bars, lock-on, devour, quests and old
 *  Blueprint calls keep working.
 */
UCLASS()
class SLIMEFABLE_API UEnemyAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	ENEMY_ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, Health);
	ENEMY_ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, MaxHealth);
	ENEMY_ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, Poise);
	ENEMY_ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, MaxPoise);
	ENEMY_ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, Guard);
	ENEMY_ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, MoveSpeed);
	ENEMY_ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, DamagePower);
	ENEMY_ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, PhaseIndex);
	ENEMY_ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, IncomingDamage);
	ENEMY_ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, IncomingPoiseDamage);
	ENEMY_ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, IncomingHealing);

	UPROPERTY(BlueprintReadOnly, Category = "Attributes") FGameplayAttributeData Health;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes") FGameplayAttributeData MaxHealth;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes") FGameplayAttributeData Poise;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes") FGameplayAttributeData MaxPoise;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes") FGameplayAttributeData Guard;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes") FGameplayAttributeData MoveSpeed;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes") FGameplayAttributeData DamagePower;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes") FGameplayAttributeData PhaseIndex;

	/** Meta attributes: written by GameplayEffects, consumed in PostGameplayEffectExecute. */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes") FGameplayAttributeData IncomingDamage;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes") FGameplayAttributeData IncomingPoiseDamage;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes") FGameplayAttributeData IncomingHealing;
};

#undef ENEMY_ATTRIBUTE_ACCESSORS
