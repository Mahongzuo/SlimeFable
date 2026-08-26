// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeCombatTypes.h"
#include "NiagaraSystem.h"

namespace
{
	FSlimeSkillDef MakeSkill(
		const TCHAR* Name,
		ESlimeElement Element,
		ESlimeSkillSlot Slot,
		ESlimeSkillExec Exec,
		ESlimeCombatPose Pose,
		ESlimeHitShape Shape,
		float Range,
		float Radius,
		float Damage,
		float Windup,
		float HitStart,
		float HitEnd,
		float Recovery,
		const TCHAR* NiagaraPath = nullptr)
	{
		FSlimeSkillDef Def;
		Def.DisplayName = FText::FromString(Name);
		Def.Element = Element;
		Def.Slot = Slot;
		Def.Exec = Exec;
		Def.Pose = Pose;
		Def.Hit.Shape = Shape;
		Def.Hit.Range = Range;
		Def.Hit.Radius = Radius;
		Def.Damage = Damage;
		Def.Windup = Windup;
		Def.HitStart = HitStart;
		Def.HitEnd = HitEnd;
		Def.Recovery = Recovery;
		Def.bAppliesElementAura = true;
		if (NiagaraPath && NiagaraPath[0] != 0)
		{
			Def.NiagaraSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(NiagaraPath));
		}
		return Def;
	}

	void PushCombos(FSlimeElementKitData& Kit, FSlimeSkillDef A, FSlimeSkillDef B, FSlimeSkillDef C, FSlimeSkillDef D)
	{
		A.Slot = ESlimeSkillSlot::Combo1;
		B.Slot = ESlimeSkillSlot::Combo2;
		C.Slot = ESlimeSkillSlot::Combo3;
		D.Slot = ESlimeSkillSlot::Combo4;
		D.Damage *= 1.3f;
		Kit.Combos = { A, B, C, D };
	}

	FLinearColor ElementVfxColor(ESlimeElement Element)
	{
		switch (Element)
		{
		case ESlimeElement::Water: return FLinearColor(0.08f, 0.58f, 1.f);
		case ESlimeElement::Wind: return FLinearColor(0.18f, 1.f, 0.48f);
		case ESlimeElement::Fire: return FLinearColor(1.f, 0.16f, 0.025f);
		case ESlimeElement::Lightning: return FLinearColor(0.38f, 0.68f, 1.f);
		case ESlimeElement::Dark: return FLinearColor(0.3f, 0.04f, 0.52f);
		case ESlimeElement::Physical:
		default: return FLinearColor(1.f, 0.68f, 0.18f);
		}
	}

	const TCHAR* MagicVfxPath(ESlimeElement Element)
	{
		switch (Element)
		{
		case ESlimeElement::Water: return TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Magic_Big_Bubbles_Explosion.NS_Magic_Big_Bubbles_Explosion");
		case ESlimeElement::Wind: return TEXT("/Game/Characters/Slime/FX/Skills/Wind/NS_Slime_Wind_Impact.NS_Slime_Wind_Impact");
		case ESlimeElement::Dark: return TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Dark_Owner_Cast_Spell.NS_Dark_Owner_Cast_Spell");
		case ESlimeElement::Lightning: return TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Lightning_Owner_Cast.NS_Lightning_Owner_Cast");
		case ESlimeElement::Physical: return TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Dark_Stone_Impact.NS_Dark_Stone_Impact");
		case ESlimeElement::Fire: return TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Gelmir_Wizard_Impact.NS_Gelmir_Wizard_Impact");
		default: return TEXT("");
		}
	}

	const TCHAR* RSkillVfxPath(ESlimeElement Element)
	{
		switch (Element)
		{
		case ESlimeElement::Fire: return TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Magma_Shot.NS_Magma_Shot");
		case ESlimeElement::Water: return TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Crystal_Torrent.NS_Crystal_Torrent");
		case ESlimeElement::Wind: return TEXT("/Game/RPGEffects/ParticlesNiagara/Priest/Beam/NS_Priest_Beam.NS_Priest_Beam");
		case ESlimeElement::Lightning: return TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Lightning_Strike.NS_Lightning_Strike");
		case ESlimeElement::Physical: return TEXT("/Game/RPGEffects/ParticlesNiagara/Archer/ArrowHail/NS_Archer_Arrow_Hail.NS_Archer_Arrow_Hail");
		case ESlimeElement::Dark: return TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Dark_Mist.NS_Dark_Mist");
		default: return TEXT("");
		}
	}

	void ConfigureSkillVfx(FSlimeSkillDef& Skill)
	{
		if (Skill.Slot == ESlimeSkillSlot::Skill1
			|| Skill.Slot == ESlimeSkillSlot::Skill2
			|| Skill.Slot == ESlimeSkillSlot::Skill3)
		{
			Skill.NiagaraSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(MagicVfxPath(Skill.Element)));
			Skill.ImpactNiagaraSystem = Skill.NiagaraSystem;
		}
		if (Skill.Slot == ESlimeSkillSlot::Skill3)
		{
			Skill.NiagaraSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(RSkillVfxPath(Skill.Element)));
			Skill.ImpactNiagaraSystem = Skill.NiagaraSystem;
			if (Skill.Element == ESlimeElement::Water
				|| Skill.Element == ESlimeElement::Physical
				|| Skill.Element == ESlimeElement::Dark)
			{
				// These systems author their effect along local +X; align that axis
				// with the slime's current facing direction.
				Skill.VfxRotationPolicy = ESlimeVfxRotationPolicy::Owner;
			}
		}
		Skill.VfxColor = ElementVfxColor(Skill.Element);
		Skill.VfxMinVisibleTime = 0.5f;
		Skill.VfxLocationOffset = FVector(0.f, 0.f, 10.f);
		Skill.VfxRotationPolicy = ESlimeVfxRotationPolicy::Aim;
		Skill.VfxScale = FVector::OneVector;

		switch (Skill.Exec)
		{
		case ESlimeSkillExec::Projectile:
			Skill.VfxScale = FVector(0.55f);
			Skill.VfxLocationOffset = FVector::ZeroVector;
			break;
		case ESlimeSkillExec::AoE:
			Skill.VfxRotationPolicy = ESlimeVfxRotationPolicy::World;
			Skill.VfxScale = FVector(FMath::Clamp(Skill.Hit.Radius / 180.f, 0.75f, 1.4f));
			break;
		case ESlimeSkillExec::Dash:
			Skill.VfxRotationPolicy = ESlimeVfxRotationPolicy::Owner;
			Skill.VfxScale = FVector(0.9f);
			break;
		case ESlimeSkillExec::Chain:
			Skill.VfxScale = FVector(0.7f);
			break;
		case ESlimeSkillExec::Melee:
		default:
			Skill.VfxScale = FVector(FMath::Clamp(Skill.Hit.Radius / 60.f, 0.75f, 1.2f));
			break;
		}

		Skill.VfxHardLifetime = 4.8f;
	}

	void ConfigureKitVfx(FSlimeElementKitData& Kit)
	{
		for (FSlimeSkillDef& Combo : Kit.Combos)
		{
			ConfigureSkillVfx(Combo);
		}
		ConfigureSkillVfx(Kit.Skill1);
		ConfigureSkillVfx(Kit.Skill2);
		ConfigureSkillVfx(Kit.Skill3);
	}
}

FSlimeCombatPoseState SlimeCombat::MakePose(ESlimeCombatPose Pose, const FVector& Forward, float Strength)
{
	FSlimeCombatPoseState State;
	State.bActive = Pose != ESlimeCombatPose::None && Strength > KINDA_SMALL_NUMBER;
	FVector PoseForward = Forward;
	PoseForward.Z = 0.f;
	State.Forward = PoseForward.GetSafeNormal();
	if (State.Forward.IsNearlyZero())
	{
		State.Forward = FVector::ForwardVector;
	}
	const float S = FMath::Clamp(Strength, 0.f, 1.5f);

	switch (Pose)
	{
	case ESlimeCombatPose::PunchStretch:
		State.StretchForward = FMath::Lerp(1.f, 2.7f, S);
		State.StretchSide = FMath::Lerp(1.f, 0.48f, S);
		State.StretchUp = FMath::Lerp(1.f, 0.7f, S);
		break;
	case ESlimeCombatPose::UpperStretch:
		State.StretchForward = FMath::Lerp(1.f, 0.8f, S);
		State.StretchSide = FMath::Lerp(1.f, 0.72f, S);
		State.StretchUp = FMath::Lerp(1.f, 2.35f, S);
		break;
	case ESlimeCombatPose::SlamFlatten:
		State.Flatten = S;
		State.StretchForward = FMath::Lerp(1.f, 1.45f, S);
		State.StretchSide = FMath::Lerp(1.f, 1.45f, S);
		State.StretchUp = FMath::Lerp(1.f, 0.4f, S);
		break;
	case ESlimeCombatPose::Spike:
		State.StretchForward = FMath::Lerp(1.f, 2.9f, S);
		State.StretchSide = FMath::Lerp(1.f, 0.42f, S);
		State.StretchUp = FMath::Lerp(1.f, 0.62f, S);
		break;
	case ESlimeCombatPose::WhipSnap:
		State.StretchForward = FMath::Lerp(1.f, 2.75f, S);
		State.StretchSide = FMath::Lerp(1.f, 0.46f, S);
		State.StretchUp = FMath::Lerp(1.f, 0.68f, S);
		break;
	case ESlimeCombatPose::Pulse:
		State.Pulse = 0.7f * S;
		break;
	case ESlimeCombatPose::DashRibbon:
		State.StretchForward = FMath::Lerp(1.f, 2.6f, S);
		State.StretchSide = FMath::Lerp(1.f, 0.45f, S);
		State.StretchUp = FMath::Lerp(1.f, 0.6f, S);
		break;
	case ESlimeCombatPose::MawOpen:
		State.StretchForward = FMath::Lerp(1.f, 1.6f, S);
		State.StretchSide = FMath::Lerp(1.f, 0.8f, S);
		State.StretchUp = FMath::Lerp(1.f, 1.15f, S);
		break;
	case ESlimeCombatPose::Swallow:
		State.StretchForward = FMath::Lerp(1.f, 0.9f, S);
		State.StretchSide = FMath::Lerp(1.f, 1.05f, S);
		State.StretchUp = FMath::Lerp(1.f, 1.1f, S);
		State.Pulse = 0.25f * S;
		break;
	default:
		State.bActive = false;
		break;
	}
	return State;
}

FSlimeElementKitData SlimeCombat::MakeDefaultKit(ESlimeElement Element)
{
	FSlimeElementKitData Kit;
	Kit.Element = Element;

	switch (Element)
	{
	case ESlimeElement::Water:
		PushCombos(Kit,
			MakeSkill(TEXT("水戳"), Element, ESlimeSkillSlot::Combo1, ESlimeSkillExec::Melee, ESlimeCombatPose::WhipSnap, ESlimeHitShape::Capsule, 160.f, 28.f, 10.f, 0.1f, 0.12f, 0.2f, 0.28f),
			MakeSkill(TEXT("水扫"), Element, ESlimeSkillSlot::Combo2, ESlimeSkillExec::Melee, ESlimeCombatPose::PunchStretch, ESlimeHitShape::Capsule, 170.f, 32.f, 11.f, 0.1f, 0.12f, 0.22f, 0.3f),
			MakeSkill(TEXT("水挑"), Element, ESlimeSkillSlot::Combo3, ESlimeSkillExec::Melee, ESlimeCombatPose::UpperStretch, ESlimeHitShape::Capsule, 165.f, 30.f, 12.f, 0.12f, 0.14f, 0.24f, 0.32f),
			MakeSkill(TEXT("浪砸"), Element, ESlimeSkillSlot::Combo4, ESlimeSkillExec::Melee, ESlimeCombatPose::SlamFlatten, ESlimeHitShape::Cone, 200.f, 50.f, 18.f, 0.14f, 0.18f, 0.32f, 0.42f));
		Kit.Combos[0].DashDistance = 100.f;
		Kit.Combos[1].DashDistance = 100.f;
		Kit.Combos[2].DashDistance = 110.f;
		Kit.Combos[3].Hit.ConeHalfAngle = 70.f;
		Kit.Skill1 = MakeSkill(TEXT("水泡弹"), Element, ESlimeSkillSlot::Skill1, ESlimeSkillExec::Projectile, ESlimeCombatPose::PunchStretch, ESlimeHitShape::ProjectileSweep, 1200.f, 22.f, 16.f, 0.12f, 0.12f, 0.2f, 0.28f, TEXT("/Game/Characters/Slime/FX/Skills/Water/NS_Slime_Water_Skill1.NS_Slime_Water_Skill1"));
		Kit.Skill1.ImpactNiagaraSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Characters/Slime/FX/Skills/Water/NS_Slime_Water_Impact.NS_Slime_Water_Impact")));
		Kit.Skill1.Cooldown = 7.f;
		Kit.Skill1.ProjectileSpeed = 1200.f;
		Kit.Skill2 = MakeSkill(TEXT("水鞭扫"), Element, ESlimeSkillSlot::Skill2, ESlimeSkillExec::Melee, ESlimeCombatPose::WhipSnap, ESlimeHitShape::Capsule, 220.f, 36.f, 24.f, 0.14f, 0.16f, 0.3f, 0.36f, TEXT("/Game/Characters/Slime/FX/Skills/Water/NS_Slime_Water_Skill2.NS_Slime_Water_Skill2"));
		Kit.Skill2.Cooldown = 10.f;
		Kit.Skill3 = MakeSkill(TEXT("泡泡爆"), Element, ESlimeSkillSlot::Skill3, ESlimeSkillExec::AoE, ESlimeCombatPose::Pulse, ESlimeHitShape::Sphere, 0.f, 280.f, 42.f, 0.2f, 0.22f, 0.4f, 0.55f, TEXT("/Game/Characters/Slime/FX/Skills/Water/NS_Slime_Water_Skill3.NS_Slime_Water_Skill3"));
		Kit.Skill3.Cooldown = 18.f;
		break;

	case ESlimeElement::Wind:
		PushCombos(Kit,
			MakeSkill(TEXT("风戳"), Element, ESlimeSkillSlot::Combo1, ESlimeSkillExec::Melee, ESlimeCombatPose::PunchStretch, ESlimeHitShape::Sphere, 140.f, 42.f, 8.f, 0.05f, 0.06f, 0.12f, 0.16f),
			MakeSkill(TEXT("风扫"), Element, ESlimeSkillSlot::Combo2, ESlimeSkillExec::Melee, ESlimeCombatPose::WhipSnap, ESlimeHitShape::Sphere, 145.f, 44.f, 9.f, 0.05f, 0.06f, 0.12f, 0.16f),
			MakeSkill(TEXT("风挑"), Element, ESlimeSkillSlot::Combo3, ESlimeSkillExec::Melee, ESlimeCombatPose::UpperStretch, ESlimeHitShape::Sphere, 150.f, 46.f, 10.f, 0.06f, 0.07f, 0.14f, 0.18f),
			MakeSkill(TEXT("风砸"), Element, ESlimeSkillSlot::Combo4, ESlimeSkillExec::Dash, ESlimeCombatPose::DashRibbon, ESlimeHitShape::Sphere, 150.f, 48.f, 14.f, 0.08f, 0.08f, 0.2f, 0.24f));
		Kit.Combos[0].DashDistance = 100.f;
		Kit.Combos[1].DashDistance = 100.f;
		Kit.Combos[2].DashDistance = 110.f;
		Kit.Combos[2].LaunchZ = 220.f;
		Kit.Combos[3].DashDistance = 140.f;
		Kit.Skill1 = MakeSkill(TEXT("短突进"), Element, ESlimeSkillSlot::Skill1, ESlimeSkillExec::Dash, ESlimeCombatPose::DashRibbon, ESlimeHitShape::Capsule, 300.f, 40.f, 14.f, 0.06f, 0.06f, 0.2f, 0.22f, TEXT("/Game/Characters/Slime/FX/Skills/Wind/NS_Slime_Wind_Skill1.NS_Slime_Wind_Skill1"));
		Kit.Skill1.Cooldown = 6.f;
		Kit.Skill1.DashDistance = 300.f;
		Kit.Skill2 = MakeSkill(TEXT("风刃回旋"), Element, ESlimeSkillSlot::Skill2, ESlimeSkillExec::AoE, ESlimeCombatPose::Pulse, ESlimeHitShape::Sphere, 0.f, 180.f, 22.f, 0.1f, 0.12f, 0.28f, 0.3f, TEXT("/Game/Characters/Slime/FX/Skills/Wind/NS_Slime_Wind_Skill2.NS_Slime_Wind_Skill2"));
		Kit.Skill2.Cooldown = 9.f;
		Kit.Skill3 = MakeSkill(TEXT("绿啸冲"), Element, ESlimeSkillSlot::Skill3, ESlimeSkillExec::Dash, ESlimeCombatPose::DashRibbon, ESlimeHitShape::Capsule, 450.f, 48.f, 38.f, 0.12f, 0.12f, 0.32f, 0.4f, TEXT("/Game/Characters/Slime/FX/Skills/Wind/NS_Slime_Wind_Skill3.NS_Slime_Wind_Skill3"));
		Kit.Skill3.Cooldown = 16.f;
		Kit.Skill3.DashDistance = 450.f;
		break;

	case ESlimeElement::Fire:
		PushCombos(Kit,
			MakeSkill(TEXT("火戳"), Element, ESlimeSkillSlot::Combo1, ESlimeSkillExec::Melee, ESlimeCombatPose::PunchStretch, ESlimeHitShape::Sphere, 130.f, 42.f, 11.f, 0.08f, 0.1f, 0.18f, 0.22f),
			MakeSkill(TEXT("火扫"), Element, ESlimeSkillSlot::Combo2, ESlimeSkillExec::Melee, ESlimeCombatPose::PunchStretch, ESlimeHitShape::Sphere, 135.f, 44.f, 12.f, 0.08f, 0.1f, 0.18f, 0.24f),
			MakeSkill(TEXT("火挑"), Element, ESlimeSkillSlot::Combo3, ESlimeSkillExec::Melee, ESlimeCombatPose::UpperStretch, ESlimeHitShape::Sphere, 140.f, 46.f, 13.f, 0.1f, 0.12f, 0.2f, 0.26f),
			MakeSkill(TEXT("焰砸"), Element, ESlimeSkillSlot::Combo4, ESlimeSkillExec::AoE, ESlimeCombatPose::Pulse, ESlimeHitShape::Sphere, 0.f, 160.f, 20.f, 0.12f, 0.16f, 0.3f, 0.38f));
		Kit.Combos[0].DashDistance = 100.f;
		Kit.Combos[1].DashDistance = 100.f;
		Kit.Combos[2].DashDistance = 110.f;
		Kit.Skill1 = MakeSkill(TEXT("岩浆弹"), Element, ESlimeSkillSlot::Skill1, ESlimeSkillExec::Projectile, ESlimeCombatPose::PunchStretch, ESlimeHitShape::ProjectileSweep, 1000.f, 24.f, 18.f, 0.12f, 0.12f, 0.22f, 0.28f, TEXT("/Game/Characters/Slime/FX/Skills/Fire/NS_Slime_Fire_Skill1.NS_Slime_Fire_Skill1"));
		Kit.Skill1.ImpactNiagaraSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Characters/Slime/FX/Skills/Fire/NS_Slime_Fire_Impact.NS_Slime_Fire_Impact")));
		Kit.Skill1.Cooldown = 7.f;
		Kit.Skill1.ProjectileSpeed = 1300.f;
		Kit.Skill2 = MakeSkill(TEXT("焰突"), Element, ESlimeSkillSlot::Skill2, ESlimeSkillExec::Dash, ESlimeCombatPose::DashRibbon, ESlimeHitShape::Capsule, 280.f, 42.f, 26.f, 0.1f, 0.1f, 0.24f, 0.3f, TEXT("/Game/Characters/Slime/FX/Skills/Fire/NS_Slime_Fire_Skill2.NS_Slime_Fire_Skill2"));
		Kit.Skill2.Cooldown = 10.f;
		Kit.Skill2.DashDistance = 280.f;
		Kit.Skill3 = MakeSkill(TEXT("岩浆怒"), Element, ESlimeSkillSlot::Skill3, ESlimeSkillExec::AoE, ESlimeCombatPose::Pulse, ESlimeHitShape::Sphere, 0.f, 320.f, 46.f, 0.22f, 0.24f, 0.42f, 0.55f, TEXT("/Game/Characters/Slime/FX/Skills/Fire/NS_Slime_Fire_Skill3.NS_Slime_Fire_Skill3"));
		Kit.Skill3.Cooldown = 18.f;
		break;

	case ESlimeElement::Lightning:
		PushCombos(Kit,
			MakeSkill(TEXT("雷戳"), Element, ESlimeSkillSlot::Combo1, ESlimeSkillExec::Melee, ESlimeCombatPose::Spike, ESlimeHitShape::Capsule, 140.f, 22.f, 11.f, 0.06f, 0.07f, 0.12f, 0.2f),
			MakeSkill(TEXT("雷扫"), Element, ESlimeSkillSlot::Combo2, ESlimeSkillExec::Melee, ESlimeCombatPose::Spike, ESlimeHitShape::Capsule, 145.f, 24.f, 12.f, 0.06f, 0.07f, 0.12f, 0.2f),
			MakeSkill(TEXT("雷挑"), Element, ESlimeSkillSlot::Combo3, ESlimeSkillExec::Melee, ESlimeCombatPose::UpperStretch, ESlimeHitShape::Capsule, 140.f, 24.f, 13.f, 0.07f, 0.08f, 0.14f, 0.22f),
			MakeSkill(TEXT("雷炸"), Element, ESlimeSkillSlot::Combo4, ESlimeSkillExec::AoE, ESlimeCombatPose::Pulse, ESlimeHitShape::Sphere, 0.f, 120.f, 18.f, 0.1f, 0.1f, 0.2f, 0.3f));
		Kit.Combos[0].DashDistance = 100.f;
		Kit.Combos[1].DashDistance = 100.f;
		Kit.Combos[2].DashDistance = 110.f;
		Kit.Skill1 = MakeSkill(TEXT("落雷点"), Element, ESlimeSkillSlot::Skill1, ESlimeSkillExec::AoE, ESlimeCombatPose::Spike, ESlimeHitShape::Sphere, 0.f, 150.f, 20.f, 0.1f, 0.12f, 0.22f, 0.26f, TEXT("/Game/Characters/Slime/FX/Skills/Lightning/NS_Slime_Lightning_Skill1.NS_Slime_Lightning_Skill1"));
		Kit.Skill1.Cooldown = 7.f;
		Kit.Skill1.Hit.OriginForwardOffset = 0.f;
		Kit.Skill2 = MakeSkill(TEXT("链式电弧"), Element, ESlimeSkillSlot::Skill2, ESlimeSkillExec::Chain, ESlimeCombatPose::Spike, ESlimeHitShape::Capsule, 160.f, 28.f, 16.f, 0.08f, 0.1f, 0.24f, 0.28f, TEXT("/Game/Characters/Slime/FX/Skills/Lightning/NS_Slime_Lightning_Skill2.NS_Slime_Lightning_Skill2"));
		Kit.Skill2.Cooldown = 10.f;
		Kit.Skill2.ChainCount = 3;
		Kit.Skill2.ChainRange = 400.f;
		Kit.Skill3 = MakeSkill(TEXT("大落雷"), Element, ESlimeSkillSlot::Skill3, ESlimeSkillExec::AoE, ESlimeCombatPose::Pulse, ESlimeHitShape::Sphere, 0.f, 350.f, 48.f, 0.18f, 0.2f, 0.4f, 0.5f, TEXT("/Game/Characters/Slime/FX/Skills/Lightning/NS_Slime_Lightning_Skill3.NS_Slime_Lightning_Skill3"));
		Kit.Skill3.Cooldown = 18.f;
		Kit.Skill3.Hit.OriginForwardOffset = 0.f;
		break;

	case ESlimeElement::Dark:
		PushCombos(Kit,
			MakeSkill(TEXT("暗戳"), Element, ESlimeSkillSlot::Combo1, ESlimeSkillExec::Melee, ESlimeCombatPose::PunchStretch, ESlimeHitShape::Sphere, 130.f, 42.f, 12.f, 0.12f, 0.14f, 0.22f, 0.3f),
			MakeSkill(TEXT("暗扫"), Element, ESlimeSkillSlot::Combo2, ESlimeSkillExec::Melee, ESlimeCombatPose::WhipSnap, ESlimeHitShape::Sphere, 135.f, 44.f, 13.f, 0.12f, 0.14f, 0.22f, 0.32f),
			MakeSkill(TEXT("暗挑"), Element, ESlimeSkillSlot::Combo3, ESlimeSkillExec::Melee, ESlimeCombatPose::UpperStretch, ESlimeHitShape::Sphere, 140.f, 46.f, 14.f, 0.14f, 0.16f, 0.26f, 0.34f),
			MakeSkill(TEXT("雾砸"), Element, ESlimeSkillSlot::Combo4, ESlimeSkillExec::AoE, ESlimeCombatPose::Pulse, ESlimeHitShape::Sphere, 0.f, 180.f, 20.f, 0.16f, 0.2f, 0.36f, 0.45f));
		Kit.Combos[0].DashDistance = 100.f;
		Kit.Combos[1].DashDistance = 100.f;
		Kit.Combos[2].DashDistance = 110.f;
		Kit.Skill1 = MakeSkill(TEXT("暗弹"), Element, ESlimeSkillSlot::Skill1, ESlimeSkillExec::Projectile, ESlimeCombatPose::PunchStretch, ESlimeHitShape::ProjectileSweep, 900.f, 22.f, 18.f, 0.14f, 0.14f, 0.24f, 0.32f, TEXT("/Game/Characters/Slime/FX/Skills/Dark/NS_Slime_Dark_Skill1.NS_Slime_Dark_Skill1"));
		Kit.Skill1.ImpactNiagaraSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Characters/Slime/FX/Skills/Dark/NS_Slime_Dark_Impact.NS_Slime_Dark_Impact")));
		Kit.Skill1.Cooldown = 8.f;
		Kit.Skill1.ProjectileSpeed = 1100.f;
		Kit.Skill1.ProjectileLife = 2.2f;
		Kit.Skill2 = MakeSkill(TEXT("暗雾区"), Element, ESlimeSkillSlot::Skill2, ESlimeSkillExec::AoE, ESlimeCombatPose::Pulse, ESlimeHitShape::Sphere, 0.f, 200.f, 22.f, 0.16f, 0.18f, 0.36f, 0.4f, TEXT("/Game/Characters/Slime/FX/Skills/Dark/NS_Slime_Dark_Skill2.NS_Slime_Dark_Skill2"));
		Kit.Skill2.Cooldown = 11.f;
		Kit.Skill3 = MakeSkill(TEXT("暗石砸"), Element, ESlimeSkillSlot::Skill3, ESlimeSkillExec::AoE, ESlimeCombatPose::SlamFlatten, ESlimeHitShape::Sphere, 0.f, 90.f, 44.f, 0.22f, 0.26f, 0.42f, 0.55f, TEXT("/Game/Characters/Slime/FX/Skills/Dark/NS_Slime_Dark_Skill3.NS_Slime_Dark_Skill3"));
		Kit.Skill3.Cooldown = 18.f;
		Kit.Skill3.Hit.OriginForwardOffset = 0.f;
		break;

	case ESlimeElement::Physical:
	default:
		PushCombos(Kit,
			MakeSkill(TEXT("锤戳"), Element, ESlimeSkillSlot::Combo1, ESlimeSkillExec::Melee, ESlimeCombatPose::PunchStretch, ESlimeHitShape::Sphere, 130.f, 42.f, 14.f, 0.1f, 0.12f, 0.2f, 0.24f),
			MakeSkill(TEXT("锤扫"), Element, ESlimeSkillSlot::Combo2, ESlimeSkillExec::Melee, ESlimeCombatPose::PunchStretch, ESlimeHitShape::Sphere, 135.f, 44.f, 15.f, 0.1f, 0.12f, 0.22f, 0.26f),
			MakeSkill(TEXT("上挑"), Element, ESlimeSkillSlot::Combo3, ESlimeSkillExec::Melee, ESlimeCombatPose::UpperStretch, ESlimeHitShape::Sphere, 140.f, 46.f, 16.f, 0.12f, 0.14f, 0.24f, 0.3f),
			MakeSkill(TEXT("砸地"), Element, ESlimeSkillSlot::Combo4, ESlimeSkillExec::Melee, ESlimeCombatPose::SlamFlatten, ESlimeHitShape::Sphere, 145.f, 60.f, 24.f, 0.14f, 0.18f, 0.32f, 0.4f));
		Kit.Combos[0].DashDistance = 100.f;
		Kit.Combos[1].DashDistance = 100.f;
		Kit.Combos[2].DashDistance = 110.f;
		Kit.Combos[0].Knockback = 360.f;
		Kit.Combos[1].Knockback = 380.f;
		Kit.Combos[2].Knockback = 400.f;
		Kit.Combos[3].Knockback = 520.f;
		Kit.Skill1 = MakeSkill(TEXT("锤击"), Element, ESlimeSkillSlot::Skill1, ESlimeSkillExec::Melee, ESlimeCombatPose::PunchStretch, ESlimeHitShape::Sphere, 120.f, 50.f, 22.f, 0.12f, 0.14f, 0.24f, 0.3f, TEXT("/Game/Characters/Slime/FX/Skills/Physical/NS_Slime_Physical_Skill1.NS_Slime_Physical_Skill1"));
		Kit.Skill1.Cooldown = 7.f;
		Kit.Skill1.Knockback = 450.f;
		Kit.Skill2 = MakeSkill(TEXT("剑气劈"), Element, ESlimeSkillSlot::Skill2, ESlimeSkillExec::Melee, ESlimeCombatPose::WhipSnap, ESlimeHitShape::Capsule, 200.f, 34.f, 28.f, 0.14f, 0.16f, 0.3f, 0.34f, TEXT("/Game/Characters/Slime/FX/Skills/Physical/NS_Slime_Physical_Skill2.NS_Slime_Physical_Skill2"));
		Kit.Skill2.Cooldown = 10.f;
		Kit.Skill3 = MakeSkill(TEXT("巨锤砸"), Element, ESlimeSkillSlot::Skill3, ESlimeSkillExec::Melee, ESlimeCombatPose::SlamFlatten, ESlimeHitShape::Sphere, 220.f, 80.f, 50.f, 0.2f, 0.24f, 0.4f, 0.5f, TEXT("/Game/Characters/Slime/FX/Skills/Physical/NS_Slime_Physical_Skill3.NS_Slime_Physical_Skill3"));
		Kit.Skill3.Cooldown = 18.f;
		Kit.Skill3.Knockback = 700.f;
		break;
	}

	ConfigureKitVfx(Kit);
	return Kit;
}

void SlimeCombat::FillDefaultReactions(TArray<FSlimeReactionRow>& OutRows)
{
	OutRows.Reset();

	auto Add = [&OutRows](ESlimeElement A, ESlimeElement B, ESlimeReactionKind Kind, float Damage, float Radius, float Duration, bool ConsumeA, bool ConsumeB, const TCHAR* Path)
	{
		FSlimeReactionRow Row;
		Row.First = A;
		Row.Second = B;
		Row.Kind = Kind;
		Row.ExtraDamage = Damage;
		Row.AoERadius = Radius;
		Row.Duration = Duration;
		Row.bConsumeFirst = ConsumeA;
		Row.bConsumeSecond = ConsumeB;
		if (Path && Path[0] != 0)
		{
			Row.NiagaraSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(Path));
		}
		OutRows.Add(Row);
	};

	Add(ESlimeElement::Water, ESlimeElement::Fire, ESlimeReactionKind::Vaporize, 22.f, 0.f, 0.f, true, true, TEXT("/Game/Characters/Slime/FX/Skills/Water/NS_Slime_Water_Impact.NS_Slime_Water_Impact"));
	Add(ESlimeElement::Water, ESlimeElement::Lightning, ESlimeReactionKind::ElectroCharge, 14.f, 180.f, 1.2f, false, true, TEXT("/Game/Characters/Slime/FX/Skills/Lightning/NS_Slime_Lightning_Impact.NS_Slime_Lightning_Impact"));
	Add(ESlimeElement::Water, ESlimeElement::Wind, ESlimeReactionKind::MistSpread, 8.f, 220.f, 0.f, false, true, TEXT("/Game/Characters/Slime/FX/Skills/Wind/NS_Slime_Wind_Impact.NS_Slime_Wind_Impact"));
	Add(ESlimeElement::Fire, ESlimeElement::Lightning, ESlimeReactionKind::Overload, 26.f, 200.f, 0.f, true, true, TEXT("/Game/Characters/Slime/FX/Skills/Fire/NS_Slime_Fire_Impact.NS_Slime_Fire_Impact"));
	Add(ESlimeElement::Fire, ESlimeElement::Wind, ESlimeReactionKind::Combustion, 12.f, 240.f, 0.f, false, true, TEXT("/Game/Characters/Slime/FX/Skills/Fire/NS_Slime_Fire_Impact.NS_Slime_Fire_Impact"));
	Add(ESlimeElement::Fire, ESlimeElement::Dark, ESlimeReactionKind::Cinder, 10.f, 0.f, 3.f, true, true, TEXT("/Game/Characters/Slime/FX/Skills/Dark/NS_Slime_Dark_Impact.NS_Slime_Dark_Impact"));
	Add(ESlimeElement::Lightning, ESlimeElement::Wind, ESlimeReactionKind::LightningSwirl, 16.f, 200.f, 0.f, true, true, TEXT("/Game/Characters/Slime/FX/Skills/Lightning/NS_Slime_Lightning_Impact.NS_Slime_Lightning_Impact"));
	Add(ESlimeElement::Lightning, ESlimeElement::Dark, ESlimeReactionKind::VoidShock, 12.f, 0.f, 0.6f, true, true, TEXT("/Game/Characters/Slime/FX/Skills/Dark/NS_Slime_Dark_Impact.NS_Slime_Dark_Impact"));
	Add(ESlimeElement::Dark, ESlimeElement::Water, ESlimeReactionKind::MurkTide, 8.f, 160.f, 2.5f, true, true, TEXT("/Game/Characters/Slime/FX/Skills/Dark/NS_Slime_Dark_Impact.NS_Slime_Dark_Impact"));
	Add(ESlimeElement::Dark, ESlimeElement::Wind, ESlimeReactionKind::DarkSwirl, 15.f, 180.f, 0.f, true, true, TEXT("/Game/Characters/Slime/FX/Skills/Dark/NS_Slime_Dark_Impact.NS_Slime_Dark_Impact"));
	Add(ESlimeElement::Physical, ESlimeElement::Water, ESlimeReactionKind::BreakPoise, 8.f, 0.f, 0.f, false, false, TEXT("/Game/Characters/Slime/FX/Skills/Physical/NS_Slime_Physical_Impact.NS_Slime_Physical_Impact"));
	Add(ESlimeElement::Physical, ESlimeElement::Fire, ESlimeReactionKind::BreakPoise, 8.f, 0.f, 0.f, false, false, TEXT("/Game/Characters/Slime/FX/Skills/Physical/NS_Slime_Physical_Impact.NS_Slime_Physical_Impact"));
	Add(ESlimeElement::Physical, ESlimeElement::Lightning, ESlimeReactionKind::BreakPoise, 8.f, 0.f, 0.f, false, false, TEXT("/Game/Characters/Slime/FX/Skills/Physical/NS_Slime_Physical_Impact.NS_Slime_Physical_Impact"));
	Add(ESlimeElement::Physical, ESlimeElement::Dark, ESlimeReactionKind::BreakPoise, 8.f, 0.f, 0.f, false, false, TEXT("/Game/Characters/Slime/FX/Skills/Physical/NS_Slime_Physical_Impact.NS_Slime_Physical_Impact"));
	Add(ESlimeElement::Physical, ESlimeElement::Wind, ESlimeReactionKind::BreakPoise, 8.f, 0.f, 0.f, false, false, TEXT("/Game/Characters/Slime/FX/Skills/Physical/NS_Slime_Physical_Impact.NS_Slime_Physical_Impact"));
}

FLinearColor SlimeCombat::GetElementVfxColor(ESlimeElement Element)
{
	return ElementVfxColor(Element);
}

FText SlimeCombat::GetReactionDisplayName(ESlimeReactionKind Kind)
{
	switch (Kind)
	{
	case ESlimeReactionKind::Vaporize: return FText::FromString(TEXT("蒸发"));
	case ESlimeReactionKind::ElectroCharge: return FText::FromString(TEXT("感电"));
	case ESlimeReactionKind::MistSpread: return FText::FromString(TEXT("雾散"));
	case ESlimeReactionKind::Overload: return FText::FromString(TEXT("超载"));
	case ESlimeReactionKind::Combustion: return FText::FromString(TEXT("燃烧"));
	case ESlimeReactionKind::Cinder: return FText::FromString(TEXT("余烬"));
	case ESlimeReactionKind::LightningSwirl: return FText::FromString(TEXT("雷涡"));
	case ESlimeReactionKind::VoidShock: return FText::FromString(TEXT("虚空震"));
	case ESlimeReactionKind::MurkTide: return FText::FromString(TEXT("浑潮"));
	case ESlimeReactionKind::DarkSwirl: return FText::FromString(TEXT("暗涡"));
	case ESlimeReactionKind::BreakPoise: return FText::FromString(TEXT("破势"));
	default: return FText::FromString(TEXT("反应"));
	}
}

FText SlimeCombat::GetAuraStatusDisplayName(ESlimeElement Element)
{
	switch (Element)
	{
	case ESlimeElement::Wind: return FText::FromString(TEXT("风蚀"));
	case ESlimeElement::Lightning: return FText::FromString(TEXT("磁暴"));
	case ESlimeElement::Water: return FText::FromString(TEXT("潮湿"));
	case ESlimeElement::Fire: return FText::FromString(TEXT("灼烧"));
	case ESlimeElement::Physical: return FText::FromString(TEXT("虚弱"));
	case ESlimeElement::Dark: return FText::FromString(TEXT("湮灭"));
	default: return FText::FromString(TEXT("状态"));
	}
}

