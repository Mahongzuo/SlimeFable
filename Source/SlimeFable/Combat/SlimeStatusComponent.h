// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeCombatTypes.h"
#include "SlimeStatusComponent.generated.h"

class UNiagaraSystem;
class UWidgetComponent;
class USlimeAuraMarkerWidget;

UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API USlimeStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeStatusComponent();

	virtual void BeginPlay() override;
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

	UPROPERTY(Transient)
	TMap<ESlimeElement, float> AuraRemaining;

	/** After a reaction: blended color flash duration (no single-element aura). */
	UPROPERTY(Transient)
	float ReactionResidueRemaining = 0.f;

	UPROPERTY(Transient)
	FLinearColor ReactionResidueColor = FLinearColor::White;

	UPROPERTY(Transient)
	ESlimeReactionKind ReactionResidueKind = ESlimeReactionKind::Vaporize;

	void RefreshAuraMarker();

private:
	void TriggerReaction(ESlimeElement Incoming, ESlimeElement Existing, AActor* Instigator);
	void ApplyReactionRow(const FSlimeReactionRow& Row, AActor* Instigator);
	void EnsureAuraMarker();
	void ClearReactionResidue();
	void BeginReactionResidue(ESlimeElement A, ESlimeElement B, ESlimeReactionKind Kind, float Duration = 8.f);
	void SyncOwnerAuraFlash();

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> AuraMarker;

	UPROPERTY(EditAnywhere, Category = "0_Config|Combat",
		meta = (ToolTip = "附着标记相对角色原点的高度（厘米）。默认 95。"))
	float AuraMarkerZOffset = 95.f;
};
