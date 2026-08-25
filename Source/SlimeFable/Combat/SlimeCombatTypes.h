// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SlimeElementTypes.h"
#include "SlimeTypes.h"
#include "SlimeCombatTypes.generated.h"

class UNiagaraSystem;

UENUM(BlueprintType)
enum class ESlimeTeam : uint8
{
	Player,
	Enemy
};

UENUM(BlueprintType)
enum class ESlimeSkillSlot : uint8
{
	Combo1,
	Combo2,
	Combo3,
	Combo4,
	Skill1,
	Skill2,
	Skill3
};

UENUM(BlueprintType)
enum class ESlimeSkillExec : uint8
{
	Melee,
	Projectile,
	AoE,
	Dash,
	Chain
};

UENUM(BlueprintType)
enum class ESlimeHitShape : uint8
{
	Sphere,
	Capsule,
	Cone,
	ProjectileSweep
};

UENUM(BlueprintType)
enum class ESlimeVfxRotationPolicy : uint8
{
	Aim,
	Owner,
	World
};

UENUM(BlueprintType)
enum class ESlimeCombatPose : uint8
{
	None,
	PunchStretch,
	UpperStretch,
	SlamFlatten,
	Spike,
	WhipSnap,
	Pulse,
	DashRibbon,
	MawOpen,
	Swallow
};

UENUM(BlueprintType)
enum class ESlimeReactionKind : uint8
{
	Vaporize,
	ElectroCharge,
	MistSpread,
	Overload,
	Combustion,
	Cinder,
	LightningSwirl,
	VoidShock,
	MurkTide,
	BreakPoise
};

USTRUCT(BlueprintType)
struct SLIMEFABLE_API FSlimeHitSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	ESlimeHitShape Shape = ESlimeHitShape::Sphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit", meta = (ClampMin = "0.0", Units = "cm"))
	float Range = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit", meta = (ClampMin = "1.0", Units = "cm"))
	float Radius = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ConeHalfAngle = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit", meta = (ClampMin = "0.0", Units = "cm"))
	float OriginForwardOffset = 20.f;

	/** Extra Z on hit origin (cm). Negative lowers the query toward short targets like the slime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit", meta = (Units = "cm",
		ToolTip = "命中取点额外高度（厘米）。负数往下瞄，方便打到矮史莱姆。"))
	float OriginZOffset = 0.f;
};

USTRUCT(BlueprintType)
struct SLIMEFABLE_API FSlimeSkillDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	ESlimeElement Element = ESlimeElement::Water;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	ESlimeSkillSlot Slot = ESlimeSkillSlot::Combo1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	ESlimeSkillExec Exec = ESlimeSkillExec::Melee;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	ESlimeCombatPose Pose = ESlimeCombatPose::PunchStretch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FSlimeHitSpec Hit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float Windup = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float HitStart = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float HitEnd = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float Recovery = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Damage", meta = (ClampMin = "0.0"))
	float Damage = 12.f;

	/** Scales AttackPower into final damage. Default 0 keeps legacy flat Damage tables. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Damage", meta = (ClampMin = "0.0"))
	float AtkScale = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Damage", meta = (ClampMin = "0.0", Units = "cm/s"))
	float Knockback = 280.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Damage", meta = (ClampMin = "0.0", Units = "cm/s"))
	float LaunchZ = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill", meta = (ClampMin = "0.0", Units = "s"))
	float Cooldown = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill", meta = (ClampMin = "0.0"))
	float ResonanceCost = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill", meta = (ClampMin = "0.0"))
	float UltimateCost = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	bool bAppliesElementAura = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Move", meta = (ClampMin = "0.0", Units = "cm"))
	float DashDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Projectile", meta = (ClampMin = "0.0", Units = "cm/s"))
	float ProjectileSpeed = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Projectile", meta = (ClampMin = "0.1", Units = "s"))
	float ProjectileLife = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Chain", meta = (ClampMin = "1"))
	int32 ChainCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Chain", meta = (ClampMin = "0.0", Units = "cm"))
	float ChainRange = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|VFX")
	TSoftObjectPtr<UNiagaraSystem> NiagaraSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|VFX",
		meta = (ToolTip = "投射物命中或技能落点使用的轻量 Niagara；为空时不生成命中特效。"))
	TSoftObjectPtr<UNiagaraSystem> ImpactNiagaraSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|VFX",
		meta = (ToolTip = "技能主特效的独立缩放，默认 1。"))
	FVector VfxScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|VFX",
		meta = (ToolTip = "相对所选特效朝向的生成位置偏移，单位厘米。"))
	FVector VfxLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|VFX",
		meta = (ToolTip = "Aim 跟随瞄准方向，Owner 跟随角色朝向，World 使用世界零旋转。"))
	ESlimeVfxRotationPolicy VfxRotationPolicy = ESlimeVfxRotationPolicy::Aim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|VFX",
		meta = (ToolTip = "尝试写入 Niagara 的 User.Color/User.Tint/User.ElementColor，用于统一属性辨识色。"))
	FLinearColor VfxColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|VFX",
		meta = (ClampMin = "0.0", Units = "s", ToolTip = "代码不会在此时间前强制回收特效；默认 0.5 秒。"))
	float VfxMinVisibleTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|VFX",
		meta = (ClampMin = "0.0", Units = "s", ToolTip = "循环特效的强制回收上限；0 表示完全由 Niagara Auto Destroy 管理。"))
	float VfxHardLifetime = 0.f;

	float GetTotalDuration() const { return FMath::Max(HitEnd, Windup) + Recovery; }
};

USTRUCT(BlueprintType)
struct SLIMEFABLE_API FSlimeElementKitData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit")
	ESlimeElement Element = ESlimeElement::Water;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit")
	TArray<FSlimeSkillDef> Combos;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit")
	FSlimeSkillDef Skill1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit")
	FSlimeSkillDef Skill2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit")
	FSlimeSkillDef Skill3;

	const FSlimeSkillDef* GetCombo(int32 Index) const
	{
		return Combos.IsValidIndex(Index) ? &Combos[Index] : nullptr;
	}

	const FSlimeSkillDef* GetSkillSlot(ESlimeSkillSlot Slot) const
	{
		switch (Slot)
		{
		case ESlimeSkillSlot::Skill1: return &Skill1;
		case ESlimeSkillSlot::Skill2: return &Skill2;
		case ESlimeSkillSlot::Skill3: return &Skill3;
		default:
			{
				const int32 ComboIndex = static_cast<int32>(Slot);
				return GetCombo(ComboIndex);
			}
		}
	}
};

USTRUCT(BlueprintType)
struct SLIMEFABLE_API FSlimeReactionRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	ESlimeElement First = ESlimeElement::Water;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	ESlimeElement Second = ESlimeElement::Fire;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	ESlimeReactionKind Kind = ESlimeReactionKind::Vaporize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	float ExtraDamage = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	float AoERadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	float Duration = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	bool bConsumeFirst = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	bool bConsumeSecond = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	TSoftObjectPtr<UNiagaraSystem> NiagaraSystem;
};

namespace SlimeCombat
{
	FORCEINLINE bool IsComboSlot(ESlimeSkillSlot Slot)
	{
		return Slot <= ESlimeSkillSlot::Combo4;
	}

	FORCEINLINE int32 ComboIndex(ESlimeSkillSlot Slot)
	{
		return static_cast<int32>(Slot);
	}

	SLIMEFABLE_API FSlimeCombatPoseState MakePose(ESlimeCombatPose Pose, const FVector& Forward, float Strength = 1.f);
	SLIMEFABLE_API FSlimeElementKitData MakeDefaultKit(ESlimeElement Element);
	SLIMEFABLE_API void FillDefaultReactions(TArray<FSlimeReactionRow>& OutRows);
	SLIMEFABLE_API FLinearColor GetElementVfxColor(ESlimeElement Element);
	SLIMEFABLE_API FText GetReactionDisplayName(ESlimeReactionKind Kind);
}
