// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeCombatTypes.h"
#include "EnemyCombatTypes.generated.h"

class UAnimMontage;
class UNiagaraSystem;
class USkeletalMesh;
class UStaticMesh;
class UMaterialInterface;

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
	SLIMEFABLE_API FEnemySkillDef MakeDefaultMissileSkill();
}
