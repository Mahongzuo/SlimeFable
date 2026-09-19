// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "LyraAttributeSet.h"
#include "NativeGameplayTags.h"

#include "LyraHealthSet.generated.h"

#define UE_API LYRAGAME_API

class UObject;
struct FFrame;

LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Gameplay_Damage);
LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Gameplay_DamageImmunity);
LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Gameplay_DamageSelfDestruct);
LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Gameplay_FellOutOfWorld);
LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Lyra_Damage_Message);

struct FGameplayEffectModCallbackData;


/**
 * ULyraHealthSet
 *
 *	Class that defines attributes that are necessary for taking damage.
 *	Attribute examples include: health, shields, and resistances.
 */
UCLASS(MinimalAPI, BlueprintType)
class ULyraHealthSet : public ULyraAttributeSet
{
	GENERATED_BODY()

public:

	UE_API ULyraHealthSet();

	ATTRIBUTE_ACCESSORS(ULyraHealthSet, Health);
	ATTRIBUTE_ACCESSORS(ULyraHealthSet, MaxHealth);
	ATTRIBUTE_ACCESSORS(ULyraHealthSet, Healing);
	ATTRIBUTE_ACCESSORS(ULyraHealthSet, Damage);

	// Delegate when health changes due to damage/healing, some information may be missing on the client
	mutable FLyraAttributeEvent OnHealthChanged;

	// Delegate when max health changes
	mutable FLyraAttributeEvent OnMaxHealthChanged;

	// Delegate to broadcast when the health attribute reaches zero
	mutable FLyraAttributeEvent OnOutOfHealth;

protected:

	UFUNCTION()
	UE_API void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UE_API virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	UE_API virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UE_API virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	UE_API virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	UE_API virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	UE_API void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

private:

	// The current health attribute.  The health will be capped by the max health attribute.  Health is hidden from modifiers so only executions can modify it.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Lyra|Health", Meta = (HideFromModifiers, AllowPrivateAccess = true))
	FGameplayAttributeData Health;

	// The current max health attribute.  Max health is an attribute since gameplay effects can modify it.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Lyra|Health", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxHealth;

	// Used to track when the health reaches 0.
	bool bOutOfHealth;

	// Store the health before any changes 
	float MaxHealthBeforeAttributeChange;
	float HealthBeforeAttributeChange;

	// -------------------------------------------------------------------
	//	Meta Attribute (please keep attributes that aren't 'stateful' below 
	// -------------------------------------------------------------------

	// Incoming healing. This is mapped directly to +Health
	UPROPERTY(BlueprintReadOnly, Category="Lyra|Health", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData Healing;

	// Incoming damage. This is mapped directly to -Health
	UPROPERTY(BlueprintReadOnly, Category="Lyra|Health", Meta=(HideFromModifiers, AllowPrivateAccess=true))
	FGameplayAttributeData Damage;
};

#undef UE_API
