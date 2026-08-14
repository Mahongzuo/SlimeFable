// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatDamageable.h"
#include "GameFramework/Character.h"
#include "EnemyCombatTypes.h"
#include "SlimeLockTarget.h"
#include "EnemyCharacter.generated.h"

class UEnemyCombatComponent;
class USlimeHealthComponent;
class UWidgetComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UAnimMontage;
class USkeletalMesh;

UCLASS()
class SLIMEFABLE_API AEnemyCharacter : public ACharacter, public ISlimeLockTarget, public ICombatDamageable
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	USlimeHealthComponent* GetEnemyHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UEnemyCombatComponent* GetEnemyCombat() const { return Combat; }

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual bool CanBeLockedOn() const override;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual FVector GetLockOnLocation() const override;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void HandleDeath() override;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void ApplyHealing(float Healing, AActor* Healer) override;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override;

	/** Soft / skeletal mesh kit assembled in Construction Script. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Mesh")
	TArray<FEnemyMeshPart> MeshParts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Mesh")
	TSoftObjectPtr<USkeletalMesh> PrimarySkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Stats", meta = (ClampMin = "1.0"))
	float MaxHP = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Presence")
	bool bAllowDespawn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Presence", meta = (ClampMin = "100.0", Units = "cm"))
	float ActiveRangeOverride = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Presence", meta = (ClampMin = "100.0", Units = "cm"))
	float SleepRangeOverride = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Presence", meta = (ClampMin = "100.0", Units = "cm"))
	float DespawnRangeOverride = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Stats")
	TSoftObjectPtr<UAnimMontage> DeathMontage;

	UFUNCTION(BlueprintCallable, Category = "Enemy|Presence")
	virtual void SetEnemyPresence(EEnemyPresence NewPresence);

	UFUNCTION(BlueprintPure, Category = "Enemy|Presence")
	EEnemyPresence GetEnemyPresence() const { return Presence; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Presence")
	bool WantsCombatBudget() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Presence")
	FTransform GetSpawnTransform() const { return SpawnTransform; }

	float GetSavedHP() const { return SavedHP; }
	void SetSavedHP(float InHP) { SavedHP = InHP; }

	/** World-space center of visible mesh parts (primary / MeshParts / placeholder). */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	FVector GetVisualBoundsCenter() const;

protected:
	void RebuildMeshParts();
	void ClearGeneratedParts();
	USceneComponent* ResolveAttachParent(const FEnemyMeshPart& Part) const;
	void ApplyPlaceholderVisual();

	UFUNCTION()
	void HandleDied();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<USlimeHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UEnemyCombatComponent> Combat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UWidgetComponent> HealthBar;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UStaticMeshComponent> PlaceholderMesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> GeneratedParts;

	UPROPERTY(Transient)
	EEnemyPresence Presence = EEnemyPresence::Active;

	FTransform SpawnTransform = FTransform::Identity;
	float SavedHP = -1.f;
	bool bPresenceRegistered = false;
};
