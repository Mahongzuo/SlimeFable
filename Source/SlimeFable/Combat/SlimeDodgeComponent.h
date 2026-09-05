// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeDodgeComponent.generated.h"

class USlimeHealthComponent;
class ASlimeDodgeAfterimage;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBlinkDashRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPerfectDodge);

UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API USlimeDodgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeDodgeComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Called by enemies when they commit an attack against the player. */
	UFUNCTION(BlueprintCallable, Category = "Dodge")
	void NotifyIncomingAttack(AActor* Attacker);

	/** Helper for enemy code: notify the local player dodge component. */
	static void NotifyPlayerIncomingAttack(UObject* WorldContext, AActor* Attacker);

	UFUNCTION(BlueprintPure, Category = "Dodge")
	bool IsInEnemyThreatRange() const;

	UFUNCTION(BlueprintPure, Category = "Dodge")
	bool IsInPerfectWindow() const;

	/** Safe-zone RMB: Blueprint should bind Blink Dash FX here. */
	UPROPERTY(BlueprintAssignable, Category = "Dodge")
	FOnBlinkDashRequested OnBlinkDashRequested;

	UPROPERTY(BlueprintAssignable, Category = "Dodge")
	FOnPerfectDodge OnPerfectDodge;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Audio",
		meta = (ToolTip = "安全区右键闪现。空则 /Game/Audio/SFX/Movement/sfx_blink_01。"))
	TSoftObjectPtr<USoundBase> BlinkDashSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Audio",
		meta = (ToolTip = "完美闪避。空则 /Game/Audio/SFX/Combat/sfx_perfect_dodge_01。"))
	TSoftObjectPtr<USoundBase> PerfectDodgeSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge", meta = (ClampMin = "1.0", Units = "cm"))
	float RollDistance = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge", meta = (ClampMin = "0.05", Units = "s"))
	float RollDuration = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge", meta = (ClampMin = "0.0", Units = "s"))
	float RollCooldown = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge", meta = (ClampMin = "0.05", Units = "s"))
	float PerfectWindow = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge", meta = (ClampMin = "0.05", Units = "s"))
	float PerfectInvulnDuration = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge", meta = (ClampMin = "0.05", Units = "s"))
	float AfterimageLife = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
	bool bPollRightMouse = true;

protected:
	void PollRightMouse();
	void TryHandleRightClick();
	void PerformCombatRoll(bool bSpawnRollAfterimage = true);
	void PerformPerfectDodge();
	void SpawnAfterimage();
	FVector ResolveRollDirection() const;

	UPROPERTY(Transient)
	TObjectPtr<USlimeHealthComponent> Health;

	float CooldownRemaining = 0.f;
	float LastIncomingAttackTime = -1000.f;
	bool bRightMouseDown = false;
};
