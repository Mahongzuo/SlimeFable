// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeCombatTypes.h"
#include "GameplayTagContainer.h"
#include "EnemyCombatTypes.generated.h"

class AActor;
class UAnimationAsset;
class UAnimMontage;
class UCapsuleComponent;
class UNiagaraSystem;
class USkeletalMesh;
class USkeletalMeshComponent;
class USoundBase;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMeshComponent;
class UGameplayEffect;

UENUM(BlueprintType)
enum class EEnemyMeshPartKind : uint8
{
	Skeletal,
	Static
};

UENUM(BlueprintType)
enum class EEnemySkillExec : uint8
{
	Melee,
	Projectile,
	AoE,
	Dash
};

UENUM(BlueprintType)
enum class EEnemyPresence : uint8
{
	Active,
	Idle,
	Sleep,
	Despawned
};

UENUM(BlueprintType)
enum class EEnemyTowerFireMode : uint8
{
	Beam UMETA(DisplayName = "Beam"),
	Projectile UMETA(DisplayName = "Projectile")
};

/** One attachable mesh piece on AEnemyCharacter (skeletal or static). */
USTRUCT(BlueprintType)
struct SLIMEFABLE_API FEnemyMeshPart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	FName PartName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	EEnemyMeshPartKind Kind = EEnemyMeshPartKind::Static;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	/** Socket on the character Mesh (or parent part). Empty = attach to capsule / root. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	FName AttachSocket = NAME_None;

	/** If set, attach to another MeshParts entry by PartName instead of the primary Mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	FName AttachToPart = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	FTransform RelativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TSoftObjectPtr<UMaterialInterface> MaterialOverride;
};

/** Single attack / skill for mesh enemies (Details-configurable). */
USTRUCT(BlueprintType)
struct SLIMEFABLE_API FEnemySkillDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	EEnemySkillExec Exec = EEnemySkillExec::Melee;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FSlimeHitSpec Hit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float Windup = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float HitStart = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float HitEnd = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float Recovery = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Damage", meta = (ClampMin = "0.0"))
	float Damage = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Damage", meta = (ClampMin = "0.0", Units = "cm/s"))
	float Knockback = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Damage", meta = (ClampMin = "0.0", Units = "cm/s"))
	float LaunchZ = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	TSoftObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|VFX")
	TSoftObjectPtr<UNiagaraSystem> CastNiagara;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|VFX")
	TSoftObjectPtr<UNiagaraSystem> HitNiagara;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Move", meta = (ClampMin = "0.0", Units = "cm"))
	float DashDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Move",
		meta = (ToolTip = "勾选后冲刺允许离地（LaunchCharacter，不覆盖竖直速度）。默认关：地面冲刺保持走路模式，不会飘起来。"))
	bool bAirDash = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Projectile", meta = (ClampMin = "0.0", Units = "cm/s"))
	float ProjectileSpeed = 1600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Projectile", meta = (ClampMin = "0.1", Units = "s"))
	float ProjectileLife = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Projectile", meta = (ClampMin = "0.0", Units = "cm"))
	float HomingRange = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Projectile", meta = (ClampMin = "0.0"))
	float HomingTurnRate = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Projectile")
	TSoftObjectPtr<UNiagaraSystem> ProjectileNiagara;

	float GetTotalDuration() const { return Windup + Recovery; }
};

/** GAS-facing ability contract used by encounter data and StateTree selection. */
USTRUCT(BlueprintType)
struct SLIMEFABLE_API FEnemyAbilityDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability",
		meta = (ToolTip = "Ability 唯一标识；用于 StateTree 选招与连段，不要和同一敌人的其他招重复。"))
	FName AbilityId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability",
		meta = (ToolTip = "兼容旧 CombatComponent 的技能数据；迁移到 GAS 后仍由此定义命中、伤害和动画。"))
	FEnemySkillDef Skill;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability|Rules",
		meta = (ToolTip = "允许使用该 Ability 的最低遭遇阶段，1/2/3 对应 Boss 血量阶段。"))
	int32 MinPhase = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability|Rules",
		meta = (ToolTip = "允许使用该 Ability 的最高遭遇阶段；0 表示不限制。"))
	int32 MaxPhase = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability|Rules",
		meta = (ToolTip = "勾选后必须持有近战攻击权才能激活；远程压制通常关闭。"))
	bool bRequiresAttackSlot = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability|Rules",
		meta = (ToolTip = "距离下限（厘米）。目标更近时不选该招。"))
	float MinRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability|Rules",
		meta = (ToolTip = "距离上限（厘米）。目标更远时不选该招。"))
	float MaxRange = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability|Rules",
		meta = (ToolTip = "Ability 冷却秒数；由遭遇 Director/StateTree 选择层消费。"))
	float Cooldown = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability|Rules",
		meta = (ToolTip = "可选冷却 GameplayEffect。为空时使用 Cooldown 数值，由兼容门面计时。"))
	TSubclassOf<UGameplayEffect> CooldownEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability|Tags",
		meta = (ToolTip = "激活时附加的标签，例如 Combat.Ability.Skill；用于 StateTree/Director 查询。"))
	FGameplayTagContainer AbilityTags;

	bool IsUsableInPhase(int32 Phase) const
	{
		return Phase >= MinPhase && (MaxPhase <= 0 || Phase <= MaxPhase);
	}
};

/** Fighter move table row: skill + AI gates / chaining. */
USTRUCT(BlueprintType)
struct SLIMEFABLE_API FEnemyMoveDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	FName MoveId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	FEnemySkillDef Skill;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", Units = "cm"))
	float MinRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxRange = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", Units = "s"))
	float TelegraphTime = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move|VFX")
	TSoftObjectPtr<UNiagaraSystem> TelegraphNiagara;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", Units = "s"))
	float Cooldown = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	bool bGapCloser = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	FName NextMoveId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	bool bInterruptible = true;
};

namespace EnemyCombat
{
	SLIMEFABLE_API FSlimeSkillDef ToSlimeHitSkill(const FEnemySkillDef& Def);
	SLIMEFABLE_API void FillDefaultFighterMoves(TArray<FEnemyMoveDef>& OutMoves);
	SLIMEFABLE_API void FillWatchdogBiteMoves(TArray<FEnemyMoveDef>& OutMoves);
	/** GASP / Mover enemies: melee-only kit (no Dash — CMC LaunchCharacter unavailable). */
	SLIMEFABLE_API void FillDefaultGaspMoves(TArray<FEnemyMoveDef>& OutMoves);
	/** Strip ragdoll / movement-mode notifies from our GASP combat montage copies. */
	SLIMEFABLE_API void SanitizeGaspCombatMontage(UAnimMontage* Montage);
	/** Load a hit-react montage, or wrap the Mannequin sequence as a dynamic montage. */
	SLIMEFABLE_API UAnimMontage* LoadGaspHitReactMontage(const TSoftObjectPtr<UAnimMontage>& SoftMontage, const TCHAR* SequencePath);
	/** 0 front, 1 back, 2 left, 3 right. */
	SLIMEFABLE_API int32 ResolveGaspHitCardinal(const AActor* Actor, const FVector& HitLocation);
	/** Official Interaction victim knockdown. Does not sanitize ragdoll notifies. */
	SLIMEFABLE_API UAnimMontage* LoadGaspDeathKnockdownMontage(int32 Cardinal);
	/** Montage source sequence when available so death can play without an AnimBP. */
	SLIMEFABLE_API UAnimationAsset* LoadGaspDeathKnockdownAnim(int32 Cardinal);
	/** Clear source AnimBP and play SingleNode death. Returns play length, or 0 if failed. */
	SLIMEFABLE_API float PlayGaspDeathSingleNode(USkeletalMeshComponent* Mesh, UAnimationAsset* Anim);
	/** Drop capsule physics and stop ragdoll so the death pose can leave the capsule. */
	SLIMEFABLE_API void PrepareGaspDeathPhysics(
		UCapsuleComponent* Capsule,
		USkeletalMeshComponent* SourceMesh,
		USkeletalMeshComponent* VisualMesh);
	SLIMEFABLE_API void ApplyGaspDeathDissolveOverlay(UMeshComponent* Mesh, UMaterialInterface* Material, UMaterialInstanceDynamic*& OutMID);
	SLIMEFABLE_API void SetGaspDeathDissolveAmount(UMaterialInstanceDynamic* MID, float Amount);
	SLIMEFABLE_API UMaterialInterface* LoadDefaultHitFlashMaterial();
	SLIMEFABLE_API UMaterialInterface* LoadDefaultLightningHitOverlay();
	SLIMEFABLE_API UMaterialInterface* LoadDefaultWindHitOverlay();
	SLIMEFABLE_API UMaterialInterface* ResolveGaspElementHitOverlay(
		ESlimeElement Element,
		UMaterialInterface* HitFlashFallback,
		UMaterialInterface* LightningOverlay,
		UMaterialInterface* WindOverlay);
	SLIMEFABLE_API bool GaspElementOverlayUsesHitFlashParams(
		ESlimeElement Element,
		UMaterialInterface* FlashMat,
		UMaterialInterface* HitFlashFallback);
	SLIMEFABLE_API void DriveGaspOverlayIntensity(
		UMaterialInstanceDynamic* MID,
		bool bUsesHitFlashParams,
		float Pulse,
		float OpacityMul,
		float HitTime);
	/** Overlay only visible devour/visual meshes (Echo). Hidden UEFN source is skipped. */
	SLIMEFABLE_API void ApplyGaspVisualOverlay(AActor* Actor, UMaterialInstanceDynamic* MID);
	SLIMEFABLE_API void ClearGaspVisualOverlay(AActor* Actor);
	/** @deprecated Use ApplyGaspVisualOverlay. */
	SLIMEFABLE_API void ApplyGaspHitFlashOverlay(AActor* Actor, UMaterialInstanceDynamic* MID);
	SLIMEFABLE_API void ClearGaspHitFlashOverlay(AActor* Actor);
	SLIMEFABLE_API void PlayGaspSfxAt(
		const UObject* WorldContext,
		const TSoftObjectPtr<USoundBase>& Soft,
		const TCHAR* FallbackPath,
		const FVector& Location);
	SLIMEFABLE_API FEnemySkillDef MakeDefaultMissileSkill();

	inline const TCHAR* DefaultHitFlashPath = TEXT("/Game/_Slime/FX/M_EnemyHitFlash.M_EnemyHitFlash");
	inline const TCHAR* DefaultLightningOverlayPath =
		TEXT("/Game/NiagaraExamples/Materials/MI_Mesh_Overlay_TeslaCoil_Player.MI_Mesh_Overlay_TeslaCoil_Player");
	inline const TCHAR* DefaultWindOverlayPath = TEXT("/Game/_Slime/FX/MI_EnemyHitOverlay_Wind.MI_EnemyHitOverlay_Wind");
	inline const TCHAR* DefaultGaspAttackSound = TEXT("/Game/Audio/SFX/Combat/sfx_attack_01.sfx_attack_01");
	inline const TCHAR* DefaultGaspHitTakenSound = TEXT("/Game/Audio/SFX/Combat/sfx_hit_01.sfx_hit_01");
	inline const TCHAR* DefaultGaspAttackImpactSound = TEXT("/Game/Audio/SFX/Combat/sfx_hit_01.sfx_hit_01");

	struct FGaspRagdollHitArgs
	{
		FVector HitLocation = FVector::ZeroVector;
		FVector HitNormal = FVector::UpVector;
		FVector Impulse = FVector::ZeroVector;
	};

	/** ProcessEvent official Sandbox ragdoll graphs (OnHit / UpdateImpactDirection / PlayRollingGetups). */
	SLIMEFABLE_API bool CallGaspRagdollFunction(AActor* Actor, FName FunctionName, const FGaspRagdollHitArgs* Args = nullptr);
	SLIMEFABLE_API void CallGaspRagdollOnHit(AActor* Actor, const FVector& HitLocation, const FVector& Impulse);
}
