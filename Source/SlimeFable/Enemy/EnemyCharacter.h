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
class UNiagaraSystem;
class UMaterialInterface;
class USlimeSouvenirDefinition;

UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API AEnemyCharacter : public ACharacter, public ISlimeLockTarget, public ICombatDamageable
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD", meta = (ClampMin = "-200.0", ClampMax = "800.0", Units = "cm"))
	float HealthBarZOffset = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD", meta = (ClampMin = "100.0", Units = "cm"))
	float HealthBarVisibleRange = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Presence")
	bool bAllowDespawn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Presence", meta = (ClampMin = "100.0", Units = "cm"))
	float ActiveRangeOverride = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Presence", meta = (ClampMin = "100.0", Units = "cm"))
	float SleepRangeOverride = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Presence", meta = (ClampMin = "100.0", Units = "cm"))
	float DespawnRangeOverride = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Presence", meta = (ClampMin = "0.0", Units = "s"))
	float OutOfCombatResetSeconds = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Presence", meta = (ClampMin = "100.0", Units = "cm"))
	float VoidResetDepth = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Presence", meta = (ClampMin = "50.0", Units = "cm"))
	float StuckResetDistance = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Stats")
	TSoftObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Death", meta = (ClampMin = "0.2", Units = "s"))
	float DeathDissolveSeconds = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Death")
	TSoftObjectPtr<UNiagaraSystem> DeathDissolveNiagara;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Death")
	TSoftObjectPtr<UMaterialInterface> DeathDissolveMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Souvenir")
	TSoftObjectPtr<USlimeSouvenirDefinition> SouvenirReward;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Souvenir")
	TSoftClassPtr<AActor> SouvenirDropClass;

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void ApplyWeekDifficulty(int32 InWeekIndex);

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

	UFUNCTION(BlueprintCallable, Category = "Enemy|Presence")
	void RestoreToSpawn();

protected:
	virtual bool IsInCombat() const;
	virtual void OnRestoredToSpawn();
	void TickOutOfCombatReset(float DeltaSeconds);
	void ApplyHealthBarOffset();
	void RefreshWorldHealthBarVisibility();
	void RebuildMeshParts();
	void ClearGeneratedParts();
	USceneComponent* ResolveAttachParent(const FEnemyMeshPart& Part) const;
	void ApplyPlaceholderVisual();

	virtual void ApplyDifficultyToCombat(float DamageMul, float IntervalMul);
	void CaptureDifficultyBases();
	void DropSouvenirReward();
	void PlayDeathMontageThenDissolve();
	void StartDeathDissolve();
	void TickDeathDissolve();
	void FinishDeathSequence();
	void ApplyDeathDissolveVisual(float Alpha);

	UFUNCTION()
	void HandleDied();

	UFUNCTION()
	void HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted);

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
	float OutOfCombatSeconds = 0.f;
	bool bDeathSequence = false;
	bool bDifficultyBasesCaptured = false;
	float DifficultyBaseMaxHP = 200.f;
	float DeathDissolveElapsed = 0.f;
	FTimerHandle DeathDissolveTimer;
	FTimerHandle DeathMontageFallbackTimer;
};
