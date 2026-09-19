// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Character/LyraCharacter.h"

#include "LyraCharacterWithAbilities.generated.h"

#define UE_API LYRAGAME_API

class UAbilitySystemComponent;
class ULyraAbilitySystemComponent;
class UObject;

// ALyraCharacter typically gets the ability system component from the possessing player state
// This represents a character with a self-contained ability system component.
UCLASS(MinimalAPI, Blueprintable)
class ALyraCharacterWithAbilities : public ALyraCharacter
{
	GENERATED_BODY()

public:
	UE_API ALyraCharacterWithAbilities(const FObjectInitializer& ObjectInitializer);

	UE_API virtual void PostInitializeComponents() override;

	UE_API virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

private:

	// The ability system component sub-object used by player characters.
	UPROPERTY(VisibleAnywhere, Category = "Lyra|PlayerState")
	TObjectPtr<ULyraAbilitySystemComponent> AbilitySystemComponent;
	
	// Health attribute set used by this actor.
	UPROPERTY()
	TObjectPtr<const class ULyraHealthSet> HealthSet;
	// Combat attribute set used by this actor.
	UPROPERTY()
	TObjectPtr<const class ULyraCombatSet> CombatSet;
};

#undef UE_API
