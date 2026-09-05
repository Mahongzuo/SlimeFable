// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyCombatTypes.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationAsset.h"
#include "Camera/CameraComponent.h"
#include "Combat/SlimeDevourTarget.h"
#include "Components/ActorComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Settings/SlimeAudioPlay.h"
#include "SlimeFable.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

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

void EnemyCombat::FillDefaultGaspMoves(TArray<FEnemyMoveDef>& OutMoves)
{
	OutMoves.Reset();

	auto SoftMontage = [](const TCHAR* Path) -> TSoftObjectPtr<UAnimMontage>
	{
		return TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(Path));
	};

	// Standalone mannequin melee (Variant_Combat). Official GASP has no punch/kick montages.
	const TCHAR* Combo = TEXT("/Game/Variant_Combat/Anims/AM_ComboAttack.AM_ComboAttack");

	auto AddMelee = [&](const TCHAR* MoveId, const FText& Name, const TCHAR* MontagePath, float Weight,
		float Damage, float Knockback, float HitStart, float HitEnd, float Recovery, float Radius, float Range)
	{
		FEnemyMoveDef Move;
		Move.MoveId = MoveId;
		Move.Skill.DisplayName = Name;
		Move.Skill.Exec = EEnemySkillExec::Melee;
		Move.Skill.Windup = 0.15f;
		Move.Skill.HitStart = HitStart;
		Move.Skill.HitEnd = HitEnd;
		Move.Skill.Recovery = Recovery;
		Move.Skill.Damage = Damage;
		Move.Skill.Knockback = Knockback;
		Move.Skill.Hit.Shape = ESlimeHitShape::Sphere;
		Move.Skill.Hit.Radius = Radius;
		Move.Skill.Hit.Range = Range;
		Move.Skill.Hit.OriginForwardOffset = 45.f;
		Move.Skill.Hit.OriginZOffset = -40.f;
		Move.Skill.AttackMontage = SoftMontage(MontagePath);
		Move.MinRange = 0.f;
		Move.MaxRange = Range + 90.f;
		Move.Weight = Weight;
		Move.TelegraphTime = 0.22f;
		Move.Cooldown = 1.0f;
		OutMoves.Add(Move);
	};

	// LMB cycles Combo 1/2/3/4. Charged (AM_ChargedAttack) loops in place and still
	// lets the pawn walk; temporarily reuse the light combo montage for the 4th hit.
	AddMelee(TEXT("GaspCombo1"), FText::FromString(TEXT("Combo 1")), Combo, 1.5f, 14.f, 280.f, 0.28f, 0.48f, 0.45f, 70.f, 120.f);
	AddMelee(TEXT("GaspCombo2"), FText::FromString(TEXT("Combo 2")), Combo, 1.2f, 16.f, 300.f, 0.28f, 0.48f, 0.45f, 70.f, 120.f);
	AddMelee(TEXT("GaspCombo3"), FText::FromString(TEXT("Combo 3")), Combo, 1.0f, 18.f, 320.f, 0.28f, 0.48f, 0.5f, 75.f, 130.f);
	AddMelee(TEXT("GaspCharged"), FText::FromString(TEXT("Combo 4")), Combo, 0.8f, 18.f, 320.f, 0.28f, 0.48f, 0.5f, 75.f, 130.f);
}

void EnemyCombat::SanitizeGaspCombatMontage(UAnimMontage* Montage)
{
	if (!Montage)
	{
		return;
	}
	const FString Path = Montage->GetPathName();
	if (!Path.Contains(TEXT("/_Slime/Enemies/GASP/Montages/")))
	{
		return;
	}

	auto Matches = [](const FString& Text) -> bool
	{
		const FString Lower = Text.ToLower();
		return Lower.Contains(TEXT("triggerragdoll"))
			|| Lower.Contains(TEXT("overridemovementmode"))
			|| Lower.Contains(TEXT("motionwarping"))
			|| Lower.Contains(TEXT("montageblendout"));
	};

	int32 Removed = 0;
	for (int32 Index = Montage->Notifies.Num() - 1; Index >= 0; --Index)
	{
		const FAnimNotifyEvent& Ev = Montage->Notifies[Index];
		FString Blob = Ev.NotifyName.ToString();
		if (Ev.Notify)
		{
			Blob += Ev.Notify->GetClass()->GetName();
		}
		if (Ev.NotifyStateClass)
		{
			Blob += Ev.NotifyStateClass->GetName();
			Blob += Ev.NotifyStateClass->GetClass()->GetName();
		}
		if (Matches(Blob))
		{
			Montage->Notifies.RemoveAt(Index);
			++Removed;
		}
	}
	if (Removed > 0)
	{
		Montage->SortNotifies();
	}
}

UAnimMontage* EnemyCombat::LoadGaspHitReactMontage(const TSoftObjectPtr<UAnimMontage>& SoftMontage, const TCHAR* SequencePath)
{
	if (UAnimMontage* Montage = SoftMontage.LoadSynchronous())
	{
		SanitizeGaspCombatMontage(Montage);
		return Montage;
	}
	if (!SequencePath)
	{
		return nullptr;
	}
	UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, SequencePath);
	if (!Seq)
	{
		return nullptr;
	}
	return UAnimMontage::CreateSlotAnimationAsDynamicMontage(Seq, TEXT("DefaultSlot"), 0.1f, 0.2f);
}

int32 EnemyCombat::ResolveGaspHitCardinal(const AActor* Actor, const FVector& HitLocation)
{
	if (!Actor)
	{
		return 0;
	}
	FVector ToHit = HitLocation - Actor->GetActorLocation();
	ToHit.Z = 0.f;
	if (ToHit.IsNearlyZero())
	{
		return 0;
	}
	ToHit.Normalize();
	const float ForwardDot = FVector::DotProduct(Actor->GetActorForwardVector().GetSafeNormal2D(), ToHit);
	const float RightDot = FVector::DotProduct(Actor->GetActorRightVector().GetSafeNormal2D(), ToHit);
	if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
	{
		return ForwardDot >= 0.f ? 0 : 1;
	}
	return RightDot >= 0.f ? 3 : 2;
}

UAnimMontage* EnemyCombat::LoadGaspDeathKnockdownMontage(int32 Cardinal)
{
	static const TCHAR* Takedowns[] = {
		TEXT("/Game/Characters/UEFN_Mannequin/Animations/Interactions/Takedowns/AM_M_relaxed_takedown_stand_F_V.AM_M_relaxed_takedown_stand_F_V"),
		TEXT("/Game/Characters/UEFN_Mannequin/Animations/Interactions/Takedowns/AM_M_relaxed_takedown_stand_B_V.AM_M_relaxed_takedown_stand_B_V"),
		TEXT("/Game/Characters/UEFN_Mannequin/Animations/Interactions/Takedowns/AM_M_relaxed_takedown_stand_L_V.AM_M_relaxed_takedown_stand_L_V"),
		TEXT("/Game/Characters/UEFN_Mannequin/Animations/Interactions/Takedowns/AM_M_relaxed_takedown_stand_R_V.AM_M_relaxed_takedown_stand_R_V"),
	};
	static const TCHAR* Shoves[] = {
		TEXT("/Game/Characters/UEFN_Mannequin/Animations/Interactions/Shoves/AM_M_relaxed_ragdoll_shove_stand_F_V.AM_M_relaxed_ragdoll_shove_stand_F_V"),
		TEXT("/Game/Characters/UEFN_Mannequin/Animations/Interactions/Shoves/AM_M_relaxed_ragdoll_shove_stand_B_V.AM_M_relaxed_ragdoll_shove_stand_B_V"),
		TEXT("/Game/Characters/UEFN_Mannequin/Animations/Interactions/Shoves/AM_M_relaxed_ragdoll_shove_stand_L_V.AM_M_relaxed_ragdoll_shove_stand_L_V"),
		TEXT("/Game/Characters/UEFN_Mannequin/Animations/Interactions/Shoves/AM_M_relaxed_ragdoll_shove_stand_R_V.AM_M_relaxed_ragdoll_shove_stand_R_V"),
	};
	Cardinal = FMath::Clamp(Cardinal, 0, 3);
	if (UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, Takedowns[Cardinal]))
	{
		return Montage;
	}
	return LoadObject<UAnimMontage>(nullptr, Shoves[Cardinal]);
}

UAnimationAsset* EnemyCombat::LoadGaspDeathKnockdownAnim(int32 Cardinal)
{
	UAnimMontage* Montage = LoadGaspDeathKnockdownMontage(Cardinal);
	if (!Montage)
	{
		return nullptr;
	}
	if (Montage->SlotAnimTracks.Num() > 0)
	{
		const TArray<FAnimSegment>& Segs = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments;
		if (Segs.Num() > 0)
		{
			if (UAnimSequenceBase* Seq = Segs[0].GetAnimReference())
			{
				return Seq;
			}
		}
	}
	return Montage;
}

float EnemyCombat::PlayGaspDeathSingleNode(USkeletalMeshComponent* Mesh, UAnimationAsset* Anim)
{
	if (!Mesh || !Anim)
	{
		return 0.f;
	}

	Mesh->bPauseAnims = false;
	Mesh->SetComponentTickEnabled(true);
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh->SetAnimInstanceClass(nullptr);
	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Mesh->PlayAnimation(Anim, false);

	if (const UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(Anim))
	{
		return FMath::Max(Seq->GetPlayLength(), 0.2f);
	}
	return 0.2f;
}

void EnemyCombat::PrepareGaspDeathPhysics(
	UCapsuleComponent* Capsule,
	USkeletalMeshComponent* SourceMesh,
	USkeletalMeshComponent* VisualMesh)
{
	auto StopBodies = [](USkeletalMeshComponent* Mesh)
	{
		if (!Mesh)
		{
			return;
		}
		Mesh->SetAllBodiesSimulatePhysics(false);
		Mesh->SetSimulatePhysics(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->bPauseAnims = false;
		Mesh->SetComponentTickEnabled(true);
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	};

	if (Capsule)
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	StopBodies(SourceMesh);
	if (VisualMesh && VisualMesh != SourceMesh)
	{
		StopBodies(VisualMesh);
	}
}

void EnemyCombat::ApplyGaspDeathDissolveOverlay(UMeshComponent* Mesh, UMaterialInterface* Material, UMaterialInstanceDynamic*& OutMID)
{
	if (!Mesh || !Material)
	{
		return;
	}
	OutMID = UMaterialInstanceDynamic::Create(Material, Mesh);
	Mesh->SetOverlayMaterial(OutMID ? static_cast<UMaterialInterface*>(OutMID) : Material);
}

void EnemyCombat::SetGaspDeathDissolveAmount(UMaterialInstanceDynamic* MID, float Amount)
{
	if (MID)
	{
		MID->SetScalarParameterValue(TEXT("DissolveAmount"), Amount);
	}
}

namespace
{
	bool IsGaspHitFlashCameraMesh(const UMeshComponent* Mesh)
	{
		for (const USceneComponent* Walk = Mesh; Walk; Walk = Walk->GetAttachParent())
		{
			if (Walk->IsA<UCameraComponent>())
			{
				return true;
			}
		}
		return false;
	}

	void ForEachGaspHitFlashMesh(AActor* Actor, TFunctionRef<void(UMeshComponent*)> Fn)
	{
		if (!Actor)
		{
			return;
		}

		TArray<AActor*> Roots;
		Roots.Add(Actor);
		TArray<UChildActorComponent*> ChildComps;
		Actor->GetComponents<UChildActorComponent>(ChildComps);
		for (UChildActorComponent* Child : ChildComps)
		{
			if (Child && Child->GetChildActor())
			{
				Roots.AddUnique(Child->GetChildActor());
			}
		}

		TSet<const UMeshComponent*> Seen;
		for (AActor* Root : Roots)
		{
			if (!Root)
			{
				continue;
			}
			TArray<UMeshComponent*> Meshes;
			Root->GetComponents<UMeshComponent>(Meshes);
			for (UMeshComponent* Mesh : Meshes)
			{
				if (!Mesh || Seen.Contains(Mesh) || Mesh->IsA<UWidgetComponent>()
					|| Mesh->IsVisualizationComponent() || IsGaspHitFlashCameraMesh(Mesh))
				{
					continue;
				}
				Seen.Add(Mesh);
				Fn(Mesh);
			}
		}
	}
}

UMaterialInterface* EnemyCombat::LoadDefaultHitFlashMaterial()
{
	return LoadObject<UMaterialInterface>(nullptr, DefaultHitFlashPath);
}

UMaterialInterface* EnemyCombat::LoadDefaultLightningHitOverlay()
{
	return LoadObject<UMaterialInterface>(nullptr, DefaultLightningOverlayPath);
}

UMaterialInterface* EnemyCombat::LoadDefaultWindHitOverlay()
{
	return LoadObject<UMaterialInterface>(nullptr, DefaultWindOverlayPath);
}

UMaterialInterface* EnemyCombat::ResolveGaspElementHitOverlay(
	ESlimeElement Element,
	UMaterialInterface* HitFlashFallback,
	UMaterialInterface* LightningOverlay,
	UMaterialInterface* WindOverlay)
{
	switch (Element)
	{
	case ESlimeElement::Lightning:
		if (LightningOverlay)
		{
			return LightningOverlay;
		}
		break;
	case ESlimeElement::Wind:
		if (WindOverlay)
		{
			return WindOverlay;
		}
		break;
	default:
		break;
	}
	return HitFlashFallback ? HitFlashFallback : LoadDefaultHitFlashMaterial();
}

bool EnemyCombat::GaspElementOverlayUsesHitFlashParams(
	ESlimeElement Element,
	UMaterialInterface* FlashMat,
	UMaterialInterface* HitFlashFallback)
{
	if (!FlashMat)
	{
		return true;
	}
	if (HitFlashFallback && FlashMat == HitFlashFallback)
	{
		return true;
	}
	return Element == ESlimeElement::Water
		|| Element == ESlimeElement::Fire
		|| Element == ESlimeElement::Dark
		|| Element == ESlimeElement::Physical;
}

void EnemyCombat::DriveGaspOverlayIntensity(
	UMaterialInstanceDynamic* MID,
	bool bUsesHitFlashParams,
	float Pulse,
	float OpacityMul,
	float HitTime)
{
	if (!MID)
	{
		return;
	}
	if (bUsesHitFlashParams)
	{
		MID->SetScalarParameterValue(TEXT("HitFlash"), Pulse);
	}
	else
	{
		MID->SetScalarParameterValue(TEXT("Opacity Multiplier"), OpacityMul * Pulse);
	}
	MID->SetScalarParameterValue(TEXT("HitTime"), HitTime);
}

void EnemyCombat::ApplyGaspVisualOverlay(AActor* Actor, UMaterialInstanceDynamic* MID)
{
	if (!Actor || !MID)
	{
		return;
	}
	if (ISlimeDevourTarget* Target = SlimeDevourUtil::As(Actor))
	{
		Target->ForEachVisualMesh([MID](UMeshComponent* Mesh)
		{
			if (!IsValid(Mesh) || !Mesh->IsRegistered())
			{
				return;
			}
			Mesh->SetOverlayMaterial(MID);
			Mesh->SetOverlayMaterialMaxDrawDistance(4000.f);
		});
		return;
	}
	ApplyGaspHitFlashOverlay(Actor, MID);
}

void EnemyCombat::ClearGaspVisualOverlay(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}
	if (ISlimeDevourTarget* Target = SlimeDevourUtil::As(Actor))
	{
		Target->ForEachVisualMesh([](UMeshComponent* Mesh)
		{
			if (IsValid(Mesh) && Mesh->IsRegistered())
			{
				Mesh->SetOverlayMaterial(nullptr);
			}
		});
		return;
	}
	ClearGaspHitFlashOverlay(Actor);
}

void EnemyCombat::ApplyGaspHitFlashOverlay(AActor* Actor, UMaterialInstanceDynamic* MID)
{
	if (!Actor || !MID)
	{
		return;
	}
	ForEachGaspHitFlashMesh(Actor, [MID](UMeshComponent* Mesh)
	{
		if (!IsValid(Mesh) || !Mesh->IsRegistered())
		{
			return;
		}
		Mesh->SetOverlayMaterial(MID);
		Mesh->SetOverlayMaterialMaxDrawDistance(4000.f);
	});
}

void EnemyCombat::ClearGaspHitFlashOverlay(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}
	ForEachGaspHitFlashMesh(Actor, [](UMeshComponent* Mesh)
	{
		if (IsValid(Mesh) && Mesh->IsRegistered())
		{
			Mesh->SetOverlayMaterial(nullptr);
		}
	});
}

void EnemyCombat::PlayGaspSfxAt(
	const UObject* WorldContext,
	const TSoftObjectPtr<USoundBase>& Soft,
	const TCHAR* FallbackPath,
	const FVector& Location)
{
	USoundBase* Sound = nullptr;
	if (!Soft.IsNull())
	{
		Sound = Soft.LoadSynchronous();
	}
	if (!Sound && FallbackPath)
	{
		Sound = LoadObject<USoundBase>(nullptr, FallbackPath);
	}
	SlimeAudioPlay::PlaySfxAt(WorldContext, Sound, Location);
}

namespace
{
	void FillGaspRagdollParms(UFunction* Fn, void* Buffer, const EnemyCombat::FGaspRagdollHitArgs& Args)
	{
		if (!Fn || !Buffer)
		{
			return;
		}
		for (TFieldIterator<FProperty> It(Fn); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop || !Prop->HasAnyPropertyFlags(CPF_Parm))
			{
				continue;
			}
			if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				continue;
			}
			if (Prop->HasAnyPropertyFlags(CPF_OutParm) && !Prop->HasAnyPropertyFlags(CPF_ReferenceParm))
			{
				continue;
			}

			const FString Name = Prop->GetName();
			if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
			{
				if (StructProp->Struct && (StructProp->Struct == TBaseStructure<FHitResult>::Get()
					|| StructProp->Struct->GetName() == TEXT("HitResult")))
				{
					FHitResult Hit;
					Hit.ImpactPoint = Args.HitLocation;
					Hit.Location = Args.HitLocation;
					Hit.ImpactNormal = Args.HitNormal;
					Hit.Normal = Args.HitNormal;
					const FVector Dir = Args.Impulse.GetSafeNormal();
					Hit.TraceStart = Args.HitLocation - (Dir.IsNearlyZero() ? FVector::ForwardVector : Dir) * 32.f;
					Hit.TraceEnd = Args.HitLocation;
					Hit.bBlockingHit = true;
					*StructProp->ContainerPtrToValuePtr<FHitResult>(Buffer) = Hit;
					continue;
				}
				if (StructProp->Struct == TBaseStructure<FVector>::Get())
				{
					FVector Value = Args.HitLocation;
					if (Name.Contains(TEXT("Normal"), ESearchCase::IgnoreCase)
						|| Name.Contains(TEXT("Dir"), ESearchCase::IgnoreCase))
					{
						Value = Args.HitNormal;
					}
					else if (Name.Contains(TEXT("Impulse"), ESearchCase::IgnoreCase)
						|| Name.Contains(TEXT("Force"), ESearchCase::IgnoreCase))
					{
						Value = Args.Impulse;
					}
					*StructProp->ContainerPtrToValuePtr<FVector>(Buffer) = Value;
				}
			}
		}
	}

	bool ProcessGaspRagdollFunction(UObject* Target, UFunction* Fn, const EnemyCombat::FGaspRagdollHitArgs* Args)
	{
		if (!Target || !Fn)
		{
			return false;
		}
		uint8* Buffer = Fn->ParmsSize > 0
			? static_cast<uint8*>(FMemory_Alloca(Fn->ParmsSize))
			: nullptr;
		if (Buffer)
		{
			FMemory::Memzero(Buffer, Fn->ParmsSize);
			if (Args)
			{
				FillGaspRagdollParms(Fn, Buffer, *Args);
			}
		}
		Target->ProcessEvent(Fn, Buffer);
		return true;
	}
}

bool EnemyCombat::CallGaspRagdollFunction(AActor* Actor, FName FunctionName, const FGaspRagdollHitArgs* Args)
{
	if (!Actor || FunctionName.IsNone())
	{
		return false;
	}
	if (UFunction* Fn = Actor->FindFunction(FunctionName))
	{
		return ProcessGaspRagdollFunction(Actor, Fn, Args);
	}
	TArray<UActorComponent*> Comps;
	Actor->GetComponents(Comps);
	for (UActorComponent* Comp : Comps)
	{
		if (!Comp)
		{
			continue;
		}
		if (UFunction* Fn = Comp->FindFunction(FunctionName))
		{
			return ProcessGaspRagdollFunction(Comp, Fn, Args);
		}
	}
	return false;
}

void EnemyCombat::CallGaspRagdollOnHit(AActor* Actor, const FVector& HitLocation, const FVector& Impulse)
{
	if (!Actor)
	{
		return;
	}
	FGaspRagdollHitArgs Args;
	Args.HitLocation = HitLocation.IsNearlyZero() ? Actor->GetActorLocation() : HitLocation;
	Args.Impulse = Impulse;
	if (Args.Impulse.IsNearlyZero())
	{
		FVector Away = Actor->GetActorLocation() - Args.HitLocation;
		Away.Z = FMath::Max(Away.Z, 20.f);
		Args.Impulse = Away.GetSafeNormal() * 400.f;
	}
	Args.HitNormal = (-Args.Impulse).GetSafeNormal();
	if (Args.HitNormal.IsNearlyZero())
	{
		Args.HitNormal = FVector::UpVector;
	}

	static const FName OnHitNames[] = {
		FName(TEXT("Ragdoll_OnHit")),
		FName(TEXT("RagdollOnHit")),
		FName(TEXT("OnRagdollHit")),
	};
	static const FName ImpactNames[] = {
		FName(TEXT("Ragdoll_UpdateImpactDirection")),
		FName(TEXT("RagdollUpdateImpactDirection")),
	};
	for (const FName& Name : OnHitNames)
	{
		if (CallGaspRagdollFunction(Actor, Name, &Args))
		{
			break;
		}
	}
	for (const FName& Name : ImpactNames)
	{
		CallGaspRagdollFunction(Actor, Name, &Args);
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
