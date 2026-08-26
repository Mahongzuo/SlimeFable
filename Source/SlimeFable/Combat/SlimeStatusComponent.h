// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeCombatTypes.h"
#include "SlimeStatusComponent.generated.h"

class UNiagaraSystem;
class UWidgetComponent;
class USlimeAuraMarkerWidget;

UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent, PrioritizeCategories = "0_Config"))
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

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetOutgoingDamageMul() const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetIncomingDamageMul() const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetMoveSpeedMul() const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetAttackIntervalMul() const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetAnnihilateLifestealFraction() const;

	UPROPERTY(Transient)
	TMap<ESlimeElement, float> AuraRemaining;

	/** After a reaction: brief blended body flash (no overhead label). */
	UPROPERTY(Transient)
	float ReactionResidueRemaining = 0.f;

	UPROPERTY(Transient)
	FLinearColor ReactionResidueColor = FLinearColor::White;

	UPROPERTY(Transient)
	ESlimeReactionKind ReactionResidueKind = ESlimeReactionKind::Vaporize;

	void RefreshAuraMarker();

	UPROPERTY(EditAnywhere, Category = "0_Config|Combat",
		meta = (ClampMin = "0.1", ClampMax = "1.0",
			ToolTip = "风蚀：敌人造成伤害倍率。默认 0.7。"))
	float WindOutgoingDamageMul = 0.7f;

	UPROPERTY(EditAnywhere, Category = "0_Config|Combat",
		meta = (ClampMin = "1.0", ClampMax = "3.0",
			ToolTip = "磁暴：敌人受到伤害倍率。默认 1.3。"))
	float LightningIncomingDamageMul = 1.3f;

	UPROPERTY(EditAnywhere, Category = "0_Config|Combat",
		meta = (ClampMin = "1.0", ClampMax = "3.0",
			ToolTip = "潮湿：敌人攻击间隔倍率（冷却/出手变慢）。默认 1.35。"))
	float WaterAttackIntervalMul = 1.35f;

	UPROPERTY(EditAnywhere, Category = "0_Config|Combat",
		meta = (ClampMin = "0.0",
			ToolTip = "灼烧：每秒掉血。默认 4。走血量组件直伤，不触发磁暴/湮灭。"))
	float FireDotPerSecond = 4.f;

	UPROPERTY(EditAnywhere, Category = "0_Config|Combat",
		meta = (ClampMin = "0.1", ClampMax = "1.0",
			ToolTip = "虚弱：移动速度倍率。默认 0.55。"))
	float PhysicalMoveSpeedMul = 0.55f;

	UPROPERTY(EditAnywhere, Category = "0_Config|Combat",
		meta = (ClampMin = "0.0", ClampMax = "1.0",
			ToolTip = "湮灭：史莱姆对该目标造成伤害时，按伤害回复自身的比例。默认 0.15。"))
	float DarkLifestealFraction = 0.15f;

private:
	void TriggerReaction(ESlimeElement Incoming, ESlimeElement Existing, AActor* Instigator);
	void ApplyReactionRow(const FSlimeReactionRow& Row, AActor* Instigator);
	void EnsureAuraMarker();
	void ClearReactionResidue();
	void BeginReactionResidue(ESlimeElement A, ESlimeElement B, ESlimeReactionKind Kind, float Duration = 0.35f);
	void SyncOwnerAuraFlash();
	void OnAurasChanged();
	void TickBurnDot(float DeltaTime);
	void RefreshOwnerMoveSpeed();
	void RefreshOwnerAttackCadence();
	float GetPrimaryAuraRemaining() const;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> AuraMarker;

	UPROPERTY(EditAnywhere, Category = "0_Config|Combat",
		meta = (ToolTip = "附着标记相对角色原点的高度（厘米）。默认 95。"))
	float AuraMarkerZOffset = 95.f;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LastAuraInstigator;

	float BurnDamageCarry = 0.f;
	bool bCapturedBaseWalkSpeed = false;
	float BaseWalkSpeed = 420.f;
};
