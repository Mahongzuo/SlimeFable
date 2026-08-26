// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyCombatTypes.h"

FSlimeSkillDef EnemyCombat::ToSlimeHitSkill(const FEnemySkillDef& Def)
{
	FSlimeSkillDef Out;
	Out.DisplayName = Def.DisplayName;
	Out.Hit = Def.Hit;
	Out.Damage = Def.Damage;
	Out.Knockback = Def.Knockback;
	Out.LaunchZ = Def.LaunchZ;
	Out.Windup = Def.Windup;
	Out.HitStart = Def.HitStart;
	Out.HitEnd = Def.HitEnd;
	Out.Recovery = Def.Recovery;
	Out.DashDistance = Def.DashDistance;
	Out.ProjectileSpeed = Def.ProjectileSpeed;
	Out.ProjectileLife = Def.ProjectileLife;
	Out.bAppliesElementAura = false;
	Out.NiagaraSystem = Def.HitNiagara;

	switch (Def.Exec)
	{
	case EEnemySkillExec::Projectile:
		Out.Exec = ESlimeSkillExec::Projectile;
		break;
	case EEnemySkillExec::AoE:
		Out.Exec = ESlimeSkillExec::AoE;
		break;
	case EEnemySkillExec::Dash:
		Out.Exec = ESlimeSkillExec::Dash;
		break;
	case EEnemySkillExec::Melee:
	default:
		Out.Exec = ESlimeSkillExec::Melee;
		break;
	}
	return Out;
}

FEnemySkillDef EnemyCombat::MakeDefaultMissileSkill()
{
	FEnemySkillDef Skill;
	Skill.DisplayName = FText::FromString(TEXT("Missile"));
	Skill.Exec = EEnemySkillExec::Projectile;
	Skill.Windup = 0.1f;
	Skill.HitStart = 0.1f;
	Skill.HitEnd = 0.12f;
	Skill.Recovery = 0.2f;
	Skill.Damage = 16.f;
	Skill.Knockback = 200.f;
	Skill.LaunchZ = 0.f;
	Skill.ProjectileSpeed = 1800.f;
	Skill.ProjectileLife = 2.2f;
	Skill.HomingRange = 1600.f;
	Skill.HomingTurnRate = 4.5f;
	Skill.Hit.Shape = ESlimeHitShape::Sphere;
	Skill.Hit.Radius = 22.f;
	Skill.Hit.Range = 0.f;
	return Skill;
}

void EnemyCombat::FillDefaultFighterMoves(TArray<FEnemyMoveDef>& OutMoves)
{
	OutMoves.Reset();

	{
		FEnemyMoveDef Move;
		Move.MoveId = TEXT("MeleeSlash");
		Move.Skill.DisplayName = FText::FromString(TEXT("Slash"));
		Move.Skill.Exec = EEnemySkillExec::Melee;
		Move.Skill.Windup = 0.12f;
		Move.Skill.HitStart = 0.18f;
		Move.Skill.HitEnd = 0.28f;
		Move.Skill.Recovery = 0.35f;
		Move.Skill.Damage = 18.f;
		Move.Skill.Knockback = 380.f;
		Move.Skill.Hit.Shape = ESlimeHitShape::Sphere;
		Move.Skill.Hit.Radius = 70.f;
		Move.Skill.Hit.Range = 120.f;
		Move.Skill.Hit.OriginForwardOffset = 40.f;
		Move.Skill.Hit.OriginZOffset = -55.f;
		Move.MinRange = 0.f;
		Move.MaxRange = 200.f;
		Move.Weight = 1.4f;
		Move.TelegraphTime = 0.2f;
		Move.Cooldown = 0.8f;
		OutMoves.Add(Move);
	}

	{
		FEnemyMoveDef Move;
		Move.MoveId = TEXT("DashStrike");
		Move.Skill.DisplayName = FText::FromString(TEXT("Dash Strike"));
		Move.Skill.Exec = EEnemySkillExec::Dash;
		Move.Skill.Windup = 0.15f;
		Move.Skill.HitStart = 0.2f;
		Move.Skill.HitEnd = 0.4f;
		Move.Skill.Recovery = 0.4f;
		Move.Skill.Damage = 22.f;
		Move.Skill.DashDistance = 450.f;
		Move.Skill.Knockback = 420.f;
		Move.Skill.Hit.Shape = ESlimeHitShape::Capsule;
		Move.Skill.Hit.Radius = 65.f;
		Move.Skill.Hit.Range = 280.f;
		Move.Skill.Hit.OriginZOffset = -40.f;
		Move.MinRange = 250.f;
		Move.MaxRange = 900.f;
		Move.Weight = 1.f;
		Move.TelegraphTime = 0.3f;
		Move.Cooldown = 2.5f;
		Move.bGapCloser = true;
		OutMoves.Add(Move);
	}

	{
		FEnemyMoveDef Move;
		Move.MoveId = TEXT("Bolt");
		Move.Skill = MakeDefaultMissileSkill();
		Move.Skill.DisplayName = FText::FromString(TEXT("Bolt"));
		Move.Skill.Damage = 14.f;
		Move.MinRange = 200.f;
		Move.MaxRange = 1400.f;
		Move.Weight = 1.1f;
		Move.TelegraphTime = 0.35f;
		Move.Cooldown = 2.f;
		Move.bGapCloser = true;
		OutMoves.Add(Move);
	}

	{
		FEnemyMoveDef Move;
		Move.MoveId = TEXT("GroundSlam");
		Move.Skill.DisplayName = FText::FromString(TEXT("Ground Slam"));
		Move.Skill.Exec = EEnemySkillExec::AoE;
		Move.Skill.Windup = 0.25f;
		Move.Skill.HitStart = 0.35f;
		Move.Skill.HitEnd = 0.45f;
		Move.Skill.Recovery = 0.55f;
		Move.Skill.Damage = 28.f;
		Move.Skill.Knockback = 500.f;
		Move.Skill.LaunchZ = 220.f;
		Move.Skill.Hit.Shape = ESlimeHitShape::Sphere;
		Move.Skill.Hit.Radius = 160.f;
		Move.Skill.Hit.Range = 0.f;
		Move.Skill.Hit.OriginForwardOffset = 0.f;
		Move.MinRange = 0.f;
		Move.MaxRange = 200.f;
		Move.Weight = 0.7f;
		Move.TelegraphTime = 0.55f;
		Move.Cooldown = 4.f;
		OutMoves.Add(Move);
	}
}

void EnemyCombat::FillWatchdogBiteMoves(TArray<FEnemyMoveDef>& OutMoves)
{
	OutMoves.Reset();

	{
		FEnemyMoveDef Move;
		Move.MoveId = TEXT("BiteSnap");
		Move.Skill.DisplayName = FText::FromString(TEXT("轻咬"));
		Move.Skill.Exec = EEnemySkillExec::Melee;
		Move.Skill.Windup = 0.1f;
		Move.Skill.HitStart = 0.16f;
		Move.Skill.HitEnd = 0.28f;
		Move.Skill.Recovery = 0.28f;
		Move.Skill.Damage = 10.f;
		Move.Skill.Knockback = 220.f;
		Move.Skill.Hit.Shape = ESlimeHitShape::Sphere;
		Move.Skill.Hit.Radius = 85.f;
		Move.Skill.Hit.Range = 140.f;
		Move.Skill.Hit.OriginForwardOffset = 45.f;
		Move.Skill.Hit.OriginZOffset = -55.f;
		Move.MinRange = 0.f;
		Move.MaxRange = 200.f;
		Move.Weight = 1.5f;
		Move.TelegraphTime = 0.12f;
		Move.Cooldown = 0.7f;
		OutMoves.Add(Move);
	}

	{
		FEnemyMoveDef Move;
		Move.MoveId = TEXT("BiteTear");
		Move.Skill.DisplayName = FText::FromString(TEXT("连撕"));
		Move.Skill.Exec = EEnemySkillExec::Melee;
		Move.Skill.Windup = 0.14f;
		Move.Skill.HitStart = 0.2f;
		Move.Skill.HitEnd = 0.42f;
		Move.Skill.Recovery = 0.4f;
		Move.Skill.Damage = 12.f;
		Move.Skill.Knockback = 280.f;
		Move.Skill.Hit.Shape = ESlimeHitShape::Sphere;
		Move.Skill.Hit.Radius = 85.f;
		Move.Skill.Hit.Range = 150.f;
		Move.Skill.Hit.OriginForwardOffset = 50.f;
		Move.Skill.Hit.OriginZOffset = -55.f;
		Move.MinRange = 0.f;
		Move.MaxRange = 200.f;
		Move.Weight = 1.1f;
		Move.TelegraphTime = 0.16f;
		Move.Cooldown = 1.1f;
		OutMoves.Add(Move);
	}

	{
		FEnemyMoveDef Move;
		Move.MoveId = TEXT("BiteLunge");
		Move.Skill.DisplayName = FText::FromString(TEXT("扑咬"));
		Move.Skill.Exec = EEnemySkillExec::Dash;
		Move.Skill.DashDistance = 180.f;
		Move.Skill.Windup = 0.16f;
		Move.Skill.HitStart = 0.22f;
		Move.Skill.HitEnd = 0.4f;
		Move.Skill.Recovery = 0.45f;
		Move.Skill.Damage = 14.f;
		Move.Skill.Knockback = 340.f;
		Move.Skill.Hit.Shape = ESlimeHitShape::Capsule;
		Move.Skill.Hit.Radius = 75.f;
		Move.Skill.Hit.Range = 200.f;
		Move.Skill.Hit.OriginForwardOffset = 60.f;
		Move.Skill.Hit.OriginZOffset = -40.f;
		Move.MinRange = 80.f;
		Move.MaxRange = 320.f;
		Move.Weight = 0.9f;
		Move.TelegraphTime = 0.2f;
		Move.Cooldown = 1.6f;
		Move.bGapCloser = true;
		OutMoves.Add(Move);
	}
}
