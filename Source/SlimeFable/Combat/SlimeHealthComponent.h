// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeCombatTypes.h"
#include "SlimeHealthComponent.generated.h"

class USlimeElementComponent;
class USlimeBodyComponent;
class USlimeCombatComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlimeHealthChanged, float, CurrentHP, float, MaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSlimeDied);

UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API USlimeHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeHealthComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	ESlimeTeam Team = ESlimeTeam::Player;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "1.0"))
	float MaxHP = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	float CurrentHP = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool bDestroyOnDeath = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool bRegenOnDeath = true;

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnSlimeHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnSlimeDied OnDied;

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsAlive() const { return CurrentHP > 0.f; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetHealthPercent() const { return MaxHP > 0.f ? CurrentHP / MaxHP : 0.f; }

	UFUNCTION(BlueprintCallable, Category = "Combat")
	float ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ApplyHealing(float Healing);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ResetHP();

private:
	void HandleDeath(AActor* DamageCauser);
	void BeginDeathDissolve();
	void TickDeathDissolve();
	void FinishDeathDissolve();
	void ApplyDeathVisual(float Alpha) const;

	bool bDissolving = false;
	float DissolveElapsed = 0.f;
	float DissolveDuration = 0.7f;
	FTimerHandle DissolveTimerHandle;
};
