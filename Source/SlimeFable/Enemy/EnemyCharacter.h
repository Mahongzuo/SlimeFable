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
class UAnimInstance;
class UAnimationAsset;
class USkeletalMesh;
class UNiagaraSystem;
class UMaterialInterface;
class USlimeSouvenirDefinition;
class UQuestObjectiveComponent;

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

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UStaticMeshComponent* GetPlaceholderMesh() const { return PlaceholderMesh; }

	const TArray<TObjectPtr<USceneComponent>>& GetGeneratedParts() const { return GeneratedParts; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsInDeathSequence() const { return bDeathSequence; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsPhantomInstance() const { return bPhantomInstance; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsDevouredDeath() const { return bDevouredDeath; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsDevourLocked() const { return bDevourLocked; }

	void SetDevourLocked(bool bLocked);

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsDevourableNow() const;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	AActor* GetPhantomMaster() const { return PhantomMaster.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void InitAsPhantom(float LifeSeconds, AActor* Master);

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void BeginDevouredDeath(AActor* Devourer);

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void BeginPhantomExpire();

	FLinearColor ResolveDevourWheelTint() const;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Mesh",
		meta = (ToolTip = "主骨骼网格。空则编辑器/游戏显示占位立方体。按日 BP 用脚本绑，不要写死在 C++。"))
	TSoftObjectPtr<USkeletalMesh> PrimarySkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Mesh",
		meta = (ToolTip = "主骨骼用的 AnimBP。填了才会站姿/Idle；空则 T-pose。和 PrimarySkeletalMesh 一起绑。"))
	TSoftClassPtr<UAnimInstance> PrimaryAnimClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Stats", meta = (ClampMin = "1.0"))
	float MaxHP = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Devour",
		meta = (ToolTip = "能否被史莱姆吞噬。塔默认关。幻形实例运行时会关掉。"))
	bool bDevourable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Debug",
		meta = (ToolTip = "勾选后攻击伤害归零（含近战/投射）。SlimeLab 测试桩勾上。"))
	bool bHarmless = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Debug", meta = (ClampMin = "0.0", ClampMax = "1.0",
		ToolTip = "出生时把 HP 设为 MaxHP 的这个比例。0 表示关闭。设 0.08 可直接测吞噬。脱战复位也会再套一次。"))
	float DebugStartHealthPercent = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Debug",
		meta = (ToolTip = "勾选后关掉脱战/卡住回出生点（掉虚空仍会拉回）。SlimeLab 测试桩运行时会自动勾。"))
	bool bSuppressOutOfCombatReset = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD",
		meta = (ToolTip = "锁定顶栏显示的名字。每个拖进关卡的实例都可以改。空则显示「敌人」，不会用 BP_ 内部名。"))
	FText DisplayName;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	FText GetResolvedDisplayName() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD", meta = (ClampMin = "-200.0", ClampMax = "800.0", Units = "cm",
		ToolTip = "血条在网格顶上方的额外厘米。默认 12。网格用参考姿势包围盒，不跟动画抖。没网格时退回胶囊顶。"))
	float HealthBarZOffset = 12.f;

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

	/** Reference-pose mesh box in world space (no animation jitter). */
	bool GetStableMeshBounds(FBox& OutBox) const;

	/** Actor XY + cached mesh top Z. Ignores animation jitter; follows the capsule. */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	FVector GetHudAnchorLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Enemy|Presence")
	void RestoreToSpawn();

	UFUNCTION(BlueprintPure, Category = "Enemy")
	virtual bool UsesSingleNodeAnims() const { return false; }

	void PlayMeshAnimation(UAnimationAsset* Asset, bool bLoop);
	void StopMeshAnimation();

	UFUNCTION(BlueprintPure, Category = "Enemy")
	virtual bool IsInCombat() const;

protected:
	virtual void OnRestoredToSpawn();
	void TickOutOfCombatReset(float DeltaSeconds);
	void ApplyHealthBarOffset();
	void RefreshWorldHealthBarVisibility();
	void UpdateHudAnchorCache() const;
	void RebuildMeshParts();
	void ApplySingleNodeAnimModeIfNeeded();
	void ClearGeneratedParts();
	USceneComponent* ResolveAttachParent(const FEnemyMeshPart& Part) const;
	void ApplyPlaceholderVisual();
	void EnsureDefaultQuestIds();
	void EnsureDefaultDisplayName();

	virtual void ApplyDifficultyToCombat(float DamageMul, float IntervalMul);
	void CaptureDifficultyBases();
	void DropSouvenirReward();
	void PlayDeathMontageThenDissolve();
	void StartDeathDissolve();
	void TickDeathDissolve();
	void FinishDeathSequence();
	void ApplyDeathDissolveVisual(float Alpha);
	void ApplyPhantomVisuals();
	void ApplyLabDummyFlags();
	void ApplyHealthOverridesAfterBeginPlay();

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UQuestObjectiveComponent> Objective;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> GeneratedParts;

	UPROPERTY(Transient)
	EEnemyPresence Presence = EEnemyPresence::Active;

	FTransform SpawnTransform = FTransform::Identity;
	float SavedHP = -1.f;
	bool bPresenceRegistered = false;
	float OutOfCombatSeconds = 0.f;
	bool bDeathSequence = false;
	bool bDevouredDeath = false;
	bool bDevourLocked = false;
	mutable bool bHudAnchorCached = false;
	mutable float HudAnchorRelZ = 0.f;
	bool bPhantomInstance = false;
	float PhantomLifeSeconds = 5.f;
	TWeakObjectPtr<AActor> PhantomMaster;
	FTimerHandle PhantomLifeTimer;
	bool bDifficultyBasesCaptured = false;
	float DifficultyBaseMaxHP = 200.f;
	float DeathDissolveElapsed = 0.f;
	FTimerHandle DeathDissolveTimer;
	FTimerHandle DeathMontageFallbackTimer;
};
