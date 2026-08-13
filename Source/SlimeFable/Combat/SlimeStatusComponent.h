// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeCombatTypes.h"
#include "SlimeStatusComponent.generated.h"

class UNiagaraSystem;

UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API USlimeStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeStatusComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ApplyAura(ESlimeElement Element, AActor* Instigator, float Duration = 8.f);

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool HasAura(ESlimeElement Element) const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	ESlimeElement GetPrimaryAura(bool& bHasAura) const;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ClearAura(ESlimeElement Element);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ClearAllAuras();

private:
	void TriggerReaction(ESlimeElement Incoming, ESlimeElement Existing, AActor* Instigator);
	void ApplyReactionRow(const FSlimeReactionRow& Row, AActor* Instigator);

	UPROPERTY(Transient)
	TMap<ESlimeElement, float> AuraRemaining;
};
