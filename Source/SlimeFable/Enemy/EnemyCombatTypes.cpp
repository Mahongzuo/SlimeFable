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
	Skill.LaunchZ = 40.f;
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
		Move.MinRange = 0.f;
		Move.MaxRange = 220.f;
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
		Move.Skill.Hit.Radius = 55.f;
		Move.Skill.Hit.Range = 280.f;
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
		Move.MinRange = 300.f;
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
		Move.Skill.Hit.Radius = 280.f;
		Move.Skill.Hit.Range = 0.f;
		Move.Skill.Hit.OriginForwardOffset = 0.f;
		Move.MinRange = 0.f;
		Move.MaxRange = 320.f;
		Move.Weight = 0.7f;
		Move.TelegraphTime = 0.55f;
		Move.Cooldown = 4.f;
		OutMoves.Add(Move);
	}
}
