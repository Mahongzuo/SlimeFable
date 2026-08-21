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
		Def.bAppliesElementAura = Element != ESlimeElement::Physical;
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
		Kit.Combos = { A, B, C, D };
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
			MakeSkill(TEXT("浪砸"), Element, ESlimeSkillSlot::Combo4, ESlimeSkillExec::Melee, ESlimeCombatPose::SlamFlatten, ESlimeHitShape::Cone, 200.f, 50.f, 18.f, 0.14f, 0.18f, 0.32f, 0.42f, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Magic_Big_Bubbles_Explosion.NS_Magic_Big_Bubbles_Explosion")));
		Kit.Combos[0].DashDistance = 100.f;
		Kit.Combos[1].DashDistance = 100.f;
		Kit.Combos[2].DashDistance = 110.f;
		Kit.Combos[3].Hit.ConeHalfAngle = 70.f;
		Kit.Skill1 = MakeSkill(TEXT("水泡弹"), Element, ESlimeSkillSlot::Skill1, ESlimeSkillExec::Projectile, ESlimeCombatPose::PunchStretch, ESlimeHitShape::ProjectileSweep, 1200.f, 22.f, 16.f, 0.12f, 0.12f, 0.2f, 0.28f, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Magic_Bubbles_Owner_Cast_Spell.NS_Magic_Bubbles_Owner_Cast_Spell"));
		Kit.Skill1.Cooldown = 7.f;
		Kit.Skill1.ProjectileSpeed = 1200.f;
		Kit.Skill2 = MakeSkill(TEXT("水鞭扫"), Element, ESlimeSkillSlot::Skill2, ESlimeSkillExec::Melee, ESlimeCombatPose::WhipSnap, ESlimeHitShape::Capsule, 220.f, 36.f, 24.f, 0.14f, 0.16f, 0.3f, 0.36f, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Ice_Mist.NS_Ice_Mist"));
		Kit.Skill2.Cooldown = 10.f;
		Kit.Skill3 = MakeSkill(TEXT("泡泡爆"), Element, ESlimeSkillSlot::Skill3, ESlimeSkillExec::AoE, ESlimeCombatPose::Pulse, ESlimeHitShape::Sphere, 0.f, 280.f, 42.f, 0.2f, 0.22f, 0.4f, 0.55f, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Magic_Big_Bubbles_Explosion.NS_Magic_Big_Bubbles_Explosion"));
		Kit.Skill3.Cooldown = 18.f;
		break;

	case ESlimeElement::Wind:
		PushCombos(Kit,
			MakeSkill(TEXT("风戳"), Element, ESlimeSkillSlot::Combo1, ESlimeSkillExec::Melee, ESlimeCombatPose::PunchStretch, ESlimeHitShape::Sphere, 90.f, 36.f, 8.f, 0.05f, 0.06f, 0.12f, 0.16f),
			MakeSkill(TEXT("风扫"), Element, ESlimeSkillSlot::Combo2, ESlimeSkillExec::Melee, ESlimeCombatPose::WhipSnap, ESlimeHitShape::Sphere, 95.f, 38.f, 9.f, 0.05f, 0.06f, 0.12f, 0.16f),
			MakeSkill(TEXT("风挑"), Element, ESlimeSkillSlot::Combo3, ESlimeSkillExec::Melee, ESlimeCombatPose::UpperStretch, ESlimeHitShape::Sphere, 100.f, 40.f, 10.f, 0.06f, 0.07f, 0.14f, 0.18f),
			MakeSkill(TEXT("风砸"), Element, ESlimeSkillSlot::Combo4, ESlimeSkillExec::Dash, ESlimeCombatPose::DashRibbon, ESlimeHitShape::Sphere, 110.f, 44.f, 14.f, 0.08f, 0.08f, 0.2f, 0.24f, TEXT("/Game/BlinkAndDashVFX/VFX_Niagara/NS_Dash_Wind.NS_Dash_Wind")));
		Kit.Combos[0].DashDistance = 100.f;
		Kit.Combos[1].DashDistance = 100.f;
		Kit.Combos[2].DashDistance = 110.f;
		Kit.Combos[2].LaunchZ = 220.f;
		Kit.Combos[3].DashDistance = 140.f;
		Kit.Skill1 = MakeSkill(TEXT("短突进"), Element, ESlimeSkillSlot::Skill1, ESlimeSkillExec::Dash, ESlimeCombatPose::DashRibbon, ESlimeHitShape::Capsule, 300.f, 40.f, 14.f, 0.06f, 0.06f, 0.2f, 0.22f, TEXT("/Game/BlinkAndDashVFX/VFX_Niagara/NS_Dash_Wind.NS_Dash_Wind"));
		Kit.Skill1.Cooldown = 6.f;
		Kit.Skill1.DashDistance = 300.f;
		Kit.Skill2 = MakeSkill(TEXT("风刃回旋"), Element, ESlimeSkillSlot::Skill2, ESlimeSkillExec::AoE, ESlimeCombatPose::Pulse, ESlimeHitShape::Sphere, 0.f, 180.f, 22.f, 0.1f, 0.12f, 0.28f, 0.3f, TEXT("/Game/BlinkAndDashVFX/VFX_Niagara/NS_Dash_Wind.NS_Dash_Wind"));
		Kit.Skill2.Cooldown = 9.f;
		Kit.Skill3 = MakeSkill(TEXT("绿啸冲"), Element, ESlimeSkillSlot::Skill3, ESlimeSkillExec::Dash, ESlimeCombatPose::DashRibbon, ESlimeHitShape::Capsule, 450.f, 48.f, 38.f, 0.12f, 0.12f, 0.32f, 0.4f, TEXT("/Game/BlinkAndDashVFX/VFX_Niagara/NS_Dash_Wind.NS_Dash_Wind"));
		Kit.Skill3.Cooldown = 16.f;
		Kit.Skill3.DashDistance = 450.f;
		break;

	case ESlimeElement::Fire:
		PushCombos(Kit,
			MakeSkill(TEXT("火戳"), Element, ESlimeSkillSlot::Combo1, ESlimeSkillExec::Melee, ESlimeCombatPose::PunchStretch, ESlimeHitShape::Sphere, 110.f, 40.f, 11.f, 0.08f, 0.1f, 0.18f, 0.22f),
			MakeSkill(TEXT("火扫"), Element, ESlimeSkillSlot::Combo2, ESlimeSkillExec::Melee, ESlimeCombatPose::PunchStretch, ESlimeHitShape::Sphere, 115.f, 42.f, 12.f, 0.08f, 0.1f, 0.18f, 0.24f),
			MakeSkill(TEXT("火挑"), Element, ESlimeSkillSlot::Combo3, ESlimeSkillExec::Melee, ESlimeCombatPose::UpperStretch, ESlimeHitShape::Sphere, 120.f, 44.f, 13.f, 0.1f, 0.12f, 0.2f, 0.26f),
			MakeSkill(TEXT("焰砸"), Element, ESlimeSkillSlot::Combo4, ESlimeSkillExec::AoE, ESlimeCombatPose::Pulse, ESlimeHitShape::Sphere, 0.f, 160.f, 20.f, 0.12f, 0.16f, 0.3f, 0.38f, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Gelmir_Fury.NS_Gelmir_Fury")));
		Kit.Combos[0].DashDistance = 100.f;
		Kit.Combos[1].DashDistance = 100.f;
		Kit.Combos[2].DashDistance = 110.f;
		Kit.Skill1 = MakeSkill(TEXT("岩浆弹"), Element, ESlimeSkillSlot::Skill1, ESlimeSkillExec::Projectile, ESlimeCombatPose::PunchStretch, ESlimeHitShape::ProjectileSweep, 1000.f, 24.f, 18.f, 0.12f, 0.12f, 0.22f, 0.28f, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Magma_Shot.NS_Magma_Shot"));
		Kit.Skill1.Cooldown = 7.f;
		Kit.Skill1.ProjectileSpeed = 1300.f;
		Kit.Skill2 = MakeSkill(TEXT("焰突"), Element, ESlimeSkillSlot::Skill2, ESlimeSkillExec::Dash, ESlimeCombatPose::DashRibbon, ESlimeHitShape::Capsule, 280.f, 42.f, 26.f, 0.1f, 0.1f, 0.24f, 0.3f, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Gelmir_Fury.NS_Gelmir_Fury"));
		Kit.Skill2.Cooldown = 10.f;
		Kit.Skill2.DashDistance = 280.f;
		Kit.Skill3 = MakeSkill(TEXT("岩浆怒"), Element, ESlimeSkillSlot::Skill3, ESlimeSkillExec::AoE, ESlimeCombatPose::Pulse, ESlimeHitShape::Sphere, 0.f, 320.f, 46.f, 0.22f, 0.24f, 0.42f, 0.55f, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Gelmir_Fury.NS_Gelmir_Fury"));
		Kit.Skill3.Cooldown = 18.f;
		break;

	case ESlimeElement::Lightning:
		PushCombos(Kit,
			MakeSkill(TEXT("雷戳"), Element, ESlimeSkillSlot::Combo1, ESlimeSkillExec::Melee, ESlimeCombatPose::Spike, ESlimeHitShape::Capsule, 140.f, 22.f, 11.f, 0.06f, 0.07f, 0.12f, 0.2f),
			MakeSkill(TEXT("雷扫"), Element, ESlimeSkillSlot::Combo2, ESlimeSkillExec::Melee, ESlimeCombatPose::Spike, ESlimeHitShape::Capsule, 145.f, 24.f, 12.f, 0.06f, 0.07f, 0.12f, 0.2f),
			MakeSkill(TEXT("雷挑"), Element, ESlimeSkillSlot::Combo3, ESlimeSkillExec::Melee, ESlimeCombatPose::UpperStretch, ESlimeHitShape::Capsule, 140.f, 24.f, 13.f, 0.07f, 0.08f, 0.14f, 0.22f),
			MakeSkill(TEXT("雷炸"), Element, ESlimeSkillSlot::Combo4, ESlimeSkillExec::AoE, ESlimeCombatPose::Pulse, ESlimeHitShape::Sphere, 0.f, 120.f, 18.f, 0.1f, 0.1f, 0.2f, 0.3f, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Lightning_Strike.NS_Lightning_Strike")));
		Kit.Combos[0].DashDistance = 100.f;
		Kit.Combos[1].DashDistance = 100.f;
		Kit.Combos[2].DashDistance = 110.f;
		Kit.Skill1 = MakeSkill(TEXT("落雷点"), Element, ESlimeSkillSlot::Skill1, ESlimeSkillExec::AoE, ESlimeCombatPose::Spike, ESlimeHitShape::Sphere, 0.f, 150.f, 20.f, 0.1f, 0.12f, 0.22f, 0.26f, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Lightning_Strike.NS_Lightning_Strike"));
		Kit.Skill1.Cooldown = 7.f;
		Kit.Skill1.Hit.OriginForwardOffset = 0.f;
		Kit.Skill2 = MakeSkill(TEXT("链式电弧"), Element, ESlimeSkillSlot::Skill2, ESlimeSkillExec::Chain, ESlimeCombatPose::Spike, ESlimeHitShape::Capsule, 160.f, 28.f, 16.f, 0.08f, 0.1f, 0.24f, 0.28f, TEXT("/Game/Characters/Slime/FX/NS_SlimeTeslaArc.NS_SlimeTeslaArc"));
		Kit.Skill2.Cooldown = 10.f;
		Kit.Skill2.ChainCount = 3;
		Kit.Skill2.ChainRange = 400.f;
		Kit.Skill3 = MakeSkill(TEXT("大落雷"), Element, ESlimeSkillSlot::Skill3, ESlimeSkillExec::AoE, ESlimeCombatPose::Pulse, ESlimeHitShape::Sphere, 0.f, 350.f, 48.f, 0.18f, 0.2f, 0.4f, 0.5f, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Lightning_Strike.NS_Lightning_Strike"));
		Kit.Skill3.Cooldown = 18.f;
		Kit.Skill3.Hit.OriginForwardOffset = 0.f;
		break;

	case ESlimeElement::Dark:
		PushCombos(Kit,
			MakeSkill(TEXT("暗戳"), Element, ESlimeSkillSlot::Combo1, ESlimeSkillExec::Melee, ESlimeCombatPose::PunchStretch, ESlimeHitShape::Sphere, 100.f, 40.f, 12.f, 0.12f, 0.14f, 0.22f, 0.3f),
			MakeSkill(TEXT("暗扫"), Element, ESlimeSkillSlot::Combo2, ESlimeSkillExec::Melee, ESlimeCombatPose::WhipSnap, ESlimeHitShape::Sphere, 105.f, 42.f, 13.f, 0.12f, 0.14f, 0.22f, 0.32f),
			MakeSkill(TEXT("暗挑"), Element, ESlimeSkillSlot::Combo3, ESlimeSkillExec::Melee, ESlimeCombatPose::UpperStretch, ESlimeHitShape::Sphere, 110.f, 44.f, 14.f, 0.14f, 0.16f, 0.26f, 0.34f),
			MakeSkill(TEXT("雾砸"), Element, ESlimeSkillSlot::Combo4, ESlimeSkillExec::AoE, ESlimeCombatPose::Pulse, ESlimeHitShape::Sphere, 0.f, 180.f, 20.f, 0.16f, 0.2f, 0.36f, 0.45f, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Dark_Mist.NS_Dark_Mist")));
		Kit.Combos[0].DashDistance = 100.f;
		Kit.Combos[1].DashDistance = 100.f;
		Kit.Combos[2].DashDistance = 110.f;
		Kit.Skill1 = MakeSkill(TEXT("暗弹"), Element, ESlimeSkillSlot::Skill1, ESlimeSkillExec::Projectile, ESlimeCombatPose::PunchStretch, ESlimeHitShape::ProjectileSweep, 900.f, 22.f, 18.f, 0.14f, 0.14f, 0.24f, 0.32f, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Dark_Solo_Projectile.NS_Dark_Solo_Projectile"));
		Kit.Skill1.Cooldown = 8.f;
		Kit.Skill1.ProjectileSpeed = 1100.f;
		Kit.Skill1.ProjectileLife = 2.2f;
		Kit.Skill2 = MakeSkill(TEXT("暗雾区"), Element, ESlimeSkillSlot::Skill2, ESlimeSkillExec::AoE, ESlimeCombatPose::Pulse, ESlimeHitShape::Sphere, 0.f, 200.f, 22.f, 0.16f, 0.18f, 0.36f, 0.4f, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Dark_Mist.NS_Dark_Mist"));
		Kit.Skill2.Cooldown = 11.f;
		Kit.Skill3 = MakeSkill(TEXT("暗石砸"), Element, ESlimeSkillSlot::Skill3, ESlimeSkillExec::AoE, ESlimeCombatPose::SlamFlatten, ESlimeHitShape::Sphere, 0.f, 90.f, 44.f, 0.22f, 0.26f, 0.42f, 0.55f, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Dark_Stone_Impact.NS_Dark_Stone_Impact"));
		Kit.Skill3.Cooldown = 18.f;
		Kit.Skill3.Hit.OriginForwardOffset = 0.f;
		break;

	case ESlimeElement::Physical:
	default:
		PushCombos(Kit,
			MakeSkill(TEXT("锤戳"), Element, ESlimeSkillSlot::Combo1, ESlimeSkillExec::Melee, ESlimeCombatPose::PunchStretch, ESlimeHitShape::Sphere, 80.f, 36.f, 14.f, 0.1f, 0.12f, 0.2f, 0.24f),
			MakeSkill(TEXT("锤扫"), Element, ESlimeSkillSlot::Combo2, ESlimeSkillExec::Melee, ESlimeCombatPose::PunchStretch, ESlimeHitShape::Sphere, 85.f, 38.f, 15.f, 0.1f, 0.12f, 0.22f, 0.26f),
			MakeSkill(TEXT("上挑"), Element, ESlimeSkillSlot::Combo3, ESlimeSkillExec::Melee, ESlimeCombatPose::UpperStretch, ESlimeHitShape::Sphere, 90.f, 40.f, 16.f, 0.12f, 0.14f, 0.24f, 0.3f),
			MakeSkill(TEXT("砸地"), Element, ESlimeSkillSlot::Combo4, ESlimeSkillExec::Melee, ESlimeCombatPose::SlamFlatten, ESlimeHitShape::Sphere, 140.f, 60.f, 24.f, 0.14f, 0.18f, 0.32f, 0.4f, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Big_Hammer_Impact.NS_Big_Hammer_Impact")));
		Kit.Combos[0].DashDistance = 100.f;
		Kit.Combos[1].DashDistance = 100.f;
		Kit.Combos[2].DashDistance = 110.f;
		Kit.Combos[0].Knockback = 360.f;
		Kit.Combos[1].Knockback = 380.f;
		Kit.Combos[2].Knockback = 400.f;
		Kit.Combos[3].Knockback = 520.f;
		Kit.Skill1 = MakeSkill(TEXT("锤击"), Element, ESlimeSkillSlot::Skill1, ESlimeSkillExec::Melee, ESlimeCombatPose::PunchStretch, ESlimeHitShape::Sphere, 120.f, 50.f, 22.f, 0.12f, 0.14f, 0.24f, 0.3f, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Big_Hammer.NS_Big_Hammer"));
		Kit.Skill1.Cooldown = 7.f;
		Kit.Skill1.Knockback = 450.f;
		Kit.Skill1.bAppliesElementAura = false;
		Kit.Skill2 = MakeSkill(TEXT("剑气劈"), Element, ESlimeSkillSlot::Skill2, ESlimeSkillExec::Melee, ESlimeCombatPose::WhipSnap, ESlimeHitShape::Capsule, 200.f, 34.f, 28.f, 0.14f, 0.16f, 0.3f, 0.34f, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Big_Sword.NS_Big_Sword"));
		Kit.Skill2.Cooldown = 10.f;
		Kit.Skill2.bAppliesElementAura = false;
		Kit.Skill3 = MakeSkill(TEXT("巨锤砸"), Element, ESlimeSkillSlot::Skill3, ESlimeSkillExec::Melee, ESlimeCombatPose::SlamFlatten, ESlimeHitShape::Sphere, 220.f, 80.f, 50.f, 0.2f, 0.24f, 0.4f, 0.5f, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Big_Hammer_Impact.NS_Big_Hammer_Impact"));
		Kit.Skill3.Cooldown = 18.f;
		Kit.Skill3.Knockback = 700.f;
		Kit.Skill3.bAppliesElementAura = false;
		break;
	}

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

	Add(ESlimeElement::Water, ESlimeElement::Fire, ESlimeReactionKind::Vaporize, 22.f, 0.f, 0.f, true, true, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Magic_Big_Bubbles_Explosion.NS_Magic_Big_Bubbles_Explosion"));
	Add(ESlimeElement::Water, ESlimeElement::Lightning, ESlimeReactionKind::ElectroCharge, 14.f, 180.f, 1.2f, false, true, TEXT("/Game/Characters/Slime/FX/NS_SlimeTeslaArc.NS_SlimeTeslaArc"));
	Add(ESlimeElement::Water, ESlimeElement::Wind, ESlimeReactionKind::MistSpread, 8.f, 220.f, 0.f, false, true, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Ice_Mist.NS_Ice_Mist"));
	Add(ESlimeElement::Fire, ESlimeElement::Lightning, ESlimeReactionKind::Overload, 26.f, 200.f, 0.f, true, true, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Gelmir_Fury.NS_Gelmir_Fury"));
	Add(ESlimeElement::Fire, ESlimeElement::Wind, ESlimeReactionKind::Combustion, 12.f, 240.f, 0.f, false, true, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Magma_Shot.NS_Magma_Shot"));
	Add(ESlimeElement::Fire, ESlimeElement::Dark, ESlimeReactionKind::Cinder, 10.f, 0.f, 3.f, true, true, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Dark_Stone_Impact.NS_Dark_Stone_Impact"));
	Add(ESlimeElement::Lightning, ESlimeElement::Wind, ESlimeReactionKind::LightningSwirl, 16.f, 200.f, 0.f, true, true, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Lightning_Strike.NS_Lightning_Strike"));
	Add(ESlimeElement::Lightning, ESlimeElement::Dark, ESlimeReactionKind::VoidShock, 12.f, 0.f, 0.6f, true, true, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Dark_Solo_Impact.NS_Dark_Solo_Impact"));
	Add(ESlimeElement::Dark, ESlimeElement::Water, ESlimeReactionKind::MurkTide, 8.f, 160.f, 2.5f, true, true, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Dark_Mist.NS_Dark_Mist"));
	Add(ESlimeElement::Physical, ESlimeElement::Water, ESlimeReactionKind::BreakPoise, 8.f, 0.f, 0.f, false, false, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Big_Hammer_Impact.NS_Big_Hammer_Impact"));
	Add(ESlimeElement::Physical, ESlimeElement::Fire, ESlimeReactionKind::BreakPoise, 8.f, 0.f, 0.f, false, false, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Big_Hammer_Impact.NS_Big_Hammer_Impact"));
	Add(ESlimeElement::Physical, ESlimeElement::Lightning, ESlimeReactionKind::BreakPoise, 8.f, 0.f, 0.f, false, false, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Big_Hammer_Impact.NS_Big_Hammer_Impact"));
	Add(ESlimeElement::Physical, ESlimeElement::Dark, ESlimeReactionKind::BreakPoise, 8.f, 0.f, 0.f, false, false, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Big_Hammer_Impact.NS_Big_Hammer_Impact"));
	Add(ESlimeElement::Physical, ESlimeElement::Wind, ESlimeReactionKind::BreakPoise, 8.f, 0.f, 0.f, false, false, TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Big_Hammer_Impact.NS_Big_Hammer_Impact"));
}
