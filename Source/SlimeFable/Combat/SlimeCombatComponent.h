// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeCombatTypes.h"
#include "SlimeCombatComponent.generated.h"

class APlayerController;
class UEnhancedInputComponent;
class UInputAction;
class USlimeAbilityComponent;
class USlimeBodyComponent;
class USlimeCombatCatalog;
class USlimeCombatHUDWidget;
class USlimeElementComponent;
class USlimeHealthComponent;
class USlimeLockOnComponent;
class UNiagaraSystem;

UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API USlimeCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeCombatComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void BindInput(UEnhancedInputComponent* EnhancedInput);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool TryComboAttack();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool TrySkill(ESlimeSkillSlot Slot);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void InterruptCombat();

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsAttacking() const { return bAttacking; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetComboIndex() const { return ComboIndex; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetResonance() const { return Resonance; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetUltimate() const { return Ultimate; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetSkillCooldownRemaining(ESlimeSkillSlot Slot) const;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ReduceSkillCooldowns(float Seconds);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ReduceSkillCooldownsPercent(float Percent);

	UFUNCTION(BlueprintPure, Category = "Combat")
	float ResolveOutgoingDamage(const FSlimeSkillDef& Skill) const;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ApplyOutgoingDamageMul(float Mul, float DurationSeconds);

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetAttackPower() const { return AttackPower; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetOutgoingDamageMul() const { return OutgoingDamageMul; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	FSlimeElementKitData GetCurrentKit() const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	FVector GetAimForward() const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	FVector GetAimDirection() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float AttackPower = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	TObjectPtr<USlimeCombatCatalog> Catalog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.1", Units = "s"))
	float ComboResetDelay = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	TSubclassOf<USlimeCombatHUDWidget> HUDWidgetClass;

	USlimeCombatHUDWidget* GetCombatHUD() const { return HUDWidget; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> AttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> Skill1Action;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> Skill2Action;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> Skill3Action;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bPollCombatKeys = true;

private:
	void HandleAttack();
	void HandleSkill1();
	void HandleSkill2();
	void HandleSkill3();
	bool CanStartAction() const;
	bool StartAction(const FSlimeSkillDef& Def, bool bFromCombo);
	void TickAction(float DeltaTime);
	void FinishAction();
	void FireHit();
	void ExecuteDash(const FSlimeSkillDef& Def, const FVector& Forward);
	void ExecuteProjectile(const FSlimeSkillDef& Def, const FVector& Forward);
	void ExecuteChain(const FSlimeSkillDef& Def, const FVector& Origin, const FVector& Forward);
	void SpawnVfx(const FSlimeSkillDef& Def, const FVector& Location) const;
	void AwardResources(const FSlimeSkillDef& Def, int32 HitCount);
	void TickCooldowns(float DeltaTime);
	void TickDamageBuff(float DeltaTime);
	void PollCombatKeys();
	int32 SkillCdIndex(ESlimeElement InElement, ESlimeSkillSlot Slot) const;
	APlayerController* GetPlayerController() const;
	FVector GetBlobOrigin() const;
	AActor* FindNearestHostile(float MaxRange) const;
	FVector ResolveGroundPoint(float ForwardCm) const;
	FVector ResolveFinisherLocation(float SeekRange = 1000.f) const;
	FVector ResolveSkillHitOrigin(const FSlimeSkillDef& Def) const;
	void ApplyComboLunge();
	void TickComboReturn(float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<USlimeBodyComponent> Body;

	UPROPERTY(Transient)
	TObjectPtr<USlimeElementComponent> Element;

	UPROPERTY(Transient)
	TObjectPtr<USlimeAbilityComponent> Abilities;

	UPROPERTY(Transient)
	TObjectPtr<USlimeLockOnComponent> LockOn;

	UPROPERTY(Transient)
	TObjectPtr<USlimeCombatHUDWidget> HUDWidget;

	FSlimeSkillDef ActiveDef;
	FVector ActiveForward = FVector::ForwardVector;
	FVector ActiveAim = FVector::ForwardVector;
	FVector ActiveHitOrigin = FVector::ZeroVector;
	bool bUseExplicitHitOrigin = false;
	FVector ComboHomeLocation = FVector::ZeroVector;
	bool bComboReturnHome = false;
	float ComboLungeDistance = 100.f;
	TSet<TWeakObjectPtr<AActor>> AlreadyHit;
	float ActionElapsed = 0.f;
	bool bAttacking = false;
	bool bHitFired = false;
	bool bComboQueued = false;

	int32 ComboIndex = 0;
	float ComboResetRemaining = 0.f;
	bool bComboOpen = false;

	float Resonance = 0.f;
	float Ultimate = 0.f;
	float SkillCd[18];

	float OutgoingDamageMul = 1.f;
	float DamageBuffRemaining = 0.f;

	bool bPollAttackDown = false;
	bool bPollSkill1Down = false;
	bool bPollSkill2Down = false;
	bool bPollSkill3Down = false;
};
