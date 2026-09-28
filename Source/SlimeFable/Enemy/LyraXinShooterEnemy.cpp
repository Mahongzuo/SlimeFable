// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraXinShooterEnemy.h"

#include "Animation/AnimInstance.h"
#include "AnimNode_SlimeArmIK.h"
#include "BoneControllers/AnimNode_ModifyBone.h"
#include "Combat/SlimeHealthComponent.h"
#include "Combat/SlimeWorldHealthBar.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Equipment/LyraEquipmentInstance.h"
#include "SlimeFable.h"
#include "UObject/UnrealType.h"
#include <initializer_list>

namespace
{
	UPhysicsAsset* LoadXinSidecarPhysicsAsset(const USkeletalMesh* Mesh)
	{
		if (!Mesh)
		{
			return nullptr;
		}
		if (UPhysicsAsset* Assigned = Mesh->GetPhysicsAsset())
		{
			return Assigned;
		}
		const FString Path = FString::Printf(TEXT("%s_PhysicsAsset.%s_PhysicsAsset"),
			*Mesh->GetOutermost()->GetName(), *Mesh->GetName());
		return LoadObject<UPhysicsAsset>(nullptr, *Path);
	}
}

ALyraXinShooterEnemy::ALyraXinShooterEnemy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = NSLOCTEXT("LyraXin", "DisplayName", "心月狐");

	Phase1Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
		TEXT("/Game/_Slime/Models/Xin/Xin_Form1_UE5.Xin_Form1_UE5")));
	Phase2Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
		TEXT("/Game/_Slime/Models/Xin2/Xin_Form2_UE5.Xin_Form2_UE5")));
	Phase1AnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(
		TEXT("/Game/_Slime/Enemies/Lyra/Visual/ABP_XinForm1_Retarget.ABP_XinForm1_Retarget_C")));
	Phase2AnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(
		TEXT("/Game/_Slime/Enemies/Lyra/Visual/ABP_XinForm2_Retarget.ABP_XinForm2_Retarget_C")));
	VisualOverlayMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/Game/Models/Phoebe/Materials/MI_PhoebeOutline_Overlay.MI_PhoebeOutline_Overlay")));

	VisualMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(GetMesh());
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetSimulatePhysics(false);
	VisualMesh->SetVisibility(true, false);
	VisualMesh->SetHiddenInGame(false, false);
	VisualMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	if (Health)
	{
		Health->MaxHP = Phase1MaxHP;
	}
	if (HealthBar)
	{
		HealthBar->SetDrawSize(FVector2D(72.f, 18.f));
	}
}

void ALyraXinShooterEnemy::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EnsureXinVisuals();
	VisualForm = HealthPhase;
	ApplyVisualForPhase(VisualForm);
#if WITH_EDITOR
	if (UWorld* World = GetWorld(); World && !World->IsGameWorld() && VisualMesh && VisualMesh->GetAnimClass())
	{
		VisualMesh->SetUpdateAnimationInEditor(true);
		if (!VisualMesh->GetAnimInstance())
		{
			VisualMesh->InitAnim(true);
		}
	}
#endif
}

void ALyraXinShooterEnemy::BeginPlay()
{
	HealthPhase = 1;
	VisualForm = 1;
	if (Health)
	{
		Health->MaxHP = Phase1MaxHP;
		Health->AbsorbLethalDamage.BindUObject(this, &ALyraXinShooterEnemy::AbsorbLethalDamage);
	}
	Super::BeginPlay();
	EnsureXinVisuals();
	ApplyVisualForPhase(1);
	SnapWeaponsToVisual();
	RefreshDualHealthBars();
}

void ALyraXinShooterEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SnapWeaponsToVisual();
}

void ALyraXinShooterEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Health)
	{
		Health->AbsorbLethalDamage.Unbind();
	}
	Super::EndPlay(EndPlayReason);
}

void ALyraXinShooterEnemy::EnsureXinVisuals()
{
	HideSourceMeshKeepChildren();
	if (!VisualMesh)
	{
		return;
	}
	VisualMesh->SetRelativeLocation(VisualRelativeLocation);
	VisualMesh->SetRelativeRotation(VisualRelativeRotation);
	if (USkeletalMeshComponent* Source = GetMesh())
	{
		Source->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Source->bEnableUpdateRateOptimizations = false;
		Source->bPauseAnims = false;
	}
	VisualMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	VisualMesh->SetLeaderPoseComponent(nullptr);
	StopLivingVisualPhysics();
	if (UMaterialInterface* Overlay = VisualOverlayMaterial.LoadSynchronous())
	{
		VisualMesh->SetOverlayMaterial(Overlay);
	}
}

void ALyraXinShooterEnemy::StopLivingVisualPhysics()
{
	if (!VisualMesh || bDeathSequence)
	{
		return;
	}
	if (UPhysicsAsset* PhysAsset = VisualMesh->GetPhysicsAsset())
	{
		for (USkeletalBodySetup* Body : PhysAsset->SkeletalBodySetups)
		{
			if (Body && Body->PhysicsType == PhysType_Simulated)
			{
				Body->PhysicsType = PhysType_Default;
			}
		}
	}
	VisualMesh->RecreatePhysicsState();
	VisualMesh->SetSimulatePhysics(false);
	VisualMesh->SetAllBodiesSimulatePhysics(false);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ALyraXinShooterEnemy::HideSourceMeshKeepChildren()
{
	if (USkeletalMeshComponent* Source = GetMesh())
	{
		// Hide Manny only. Attached Lyra guns stay visible.
		Source->SetVisibility(false, false);
		Source->SetHiddenInGame(true, false);
	}
}

void ALyraXinShooterEnemy::ApplyVisualForPhase(int32 Phase)
{
	if (!VisualMesh)
	{
		return;
	}
	VisualForm = Phase >= 2 ? 2 : 1;
	const bool bPhase2 = VisualForm >= 2;
	TSoftObjectPtr<USkeletalMesh>& MeshRef = bPhase2 ? Phase2Mesh : Phase1Mesh;
	TSoftClassPtr<UAnimInstance>& AnimRef = bPhase2 ? Phase2AnimClass : Phase1AnimClass;
	const FName Tag = bPhase2 ? Phase2RetargetTag : Phase1RetargetTag;

	// ABP_GenericRetarget reads ComponentTags[0] when the anim instance starts.
	// Tags must be in place before SetAnimInstanceClass or Form2 stays in bind pose.
	VisualMesh->ComponentTags.Reset();
	if (!Tag.IsNone())
	{
		VisualMesh->ComponentTags.Add(Tag);
	}

	USkeletalMesh* Body = MeshRef.LoadSynchronous();
	if (!Body)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("LyraXin %s: phase %d mesh failed to load."), *GetName(), Phase);
	}
	else if (VisualMesh->GetSkeletalMeshAsset() != Body)
	{
		VisualMesh->SetAnimInstanceClass(nullptr);
		VisualMesh->SetSkeletalMeshAsset(Body);
		VisualMesh->EmptyOverrideMaterials();
	}

	UClass* AnimClass = AnimRef.LoadSynchronous();
	if (!AnimClass && bPhase2)
	{
		AnimClass = Phase1AnimClass.LoadSynchronous();
		UE_LOG(LogSlimeFable, Warning, TEXT("LyraXin %s: Phase2 ABP missing, falling back to Phase1 ABP with tag %s."),
			*GetName(), *Tag.ToString());
	}
	if (AnimClass)
	{
		VisualMesh->SetAnimInstanceClass(AnimClass);
	}
	else
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("LyraXin %s: phase %d anim class failed to load."), *GetName(), Phase);
	}

	VisualMesh->SetVisibility(true, false);
	VisualMesh->SetHiddenInGame(false, false);
	VisualMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	VisualMesh->SetLeaderPoseComponent(nullptr);
	if (VisualMesh->GetAnimClass())
	{
		VisualMesh->InitAnim(true);
	}
	StopLivingVisualPhysics();
	HideSourceMeshKeepChildren();
	if (UMaterialInterface* Overlay = VisualOverlayMaterial.LoadSynchronous())
	{
		VisualMesh->SetOverlayMaterial(Overlay);
	}
	SnapWeaponsToVisual();
}

void ALyraXinShooterEnemy::HandleSlimeHealthChanged(float CurrentHP, float MaxHP)
{
	Super::HandleSlimeHealthChanged(CurrentHP, MaxHP);
	RefreshDualHealthBars();
}

void ALyraXinShooterEnemy::RefreshDualHealthBars()
{
	float Phase1 = 1.f;
	float Phase2 = 1.f;
	GetDualHealthPercents(Phase1, Phase2);
	if (HealthBar)
	{
		if (USlimeWorldHealthBar* Bar = Cast<USlimeWorldHealthBar>(HealthBar->GetWidget()))
		{
			Bar->SetDualPhaseEnabled(true);
			Bar->SetPhasePercents(Phase1, Phase2);
		}
		else
		{
			HealthBar->InitWidget();
			if (USlimeWorldHealthBar* Retry = Cast<USlimeWorldHealthBar>(HealthBar->GetWidget()))
			{
				Retry->SetHealth(Health);
				Retry->SetDualPhaseEnabled(true);
				Retry->SetPhasePercents(Phase1, Phase2);
			}
		}
	}
}

void ALyraXinShooterEnemy::GetDualHealthPercents(float& OutPhase1, float& OutPhase2) const
{
	const float Current = (Health && Health->MaxHP > 0.f) ? Health->GetHealthPercent() : 0.f;
	if (HealthPhase <= 1)
	{
		OutPhase1 = Current;
		OutPhase2 = 1.f;
	}
	else
	{
		OutPhase1 = 0.f;
		OutPhase2 = Current;
	}
}

bool ALyraXinShooterEnemy::IsDevourableNow() const
{
	return Super::IsDevourableNow() && HealthPhase >= 2;
}

USkeletalMeshComponent* ALyraXinShooterEnemy::GetDevourPreviewMesh() const
{
	return VisualMesh ? VisualMesh.Get() : GetMesh();
}

USkeletalMeshComponent* ALyraXinShooterEnemy::GetMorphVisualMesh() const
{
	return VisualMesh ? VisualMesh.Get() : GetMesh();
}

void ALyraXinShooterEnemy::ForEachVisualMesh(TFunctionRef<void(UMeshComponent*)> Fn) const
{
	if (VisualMesh)
	{
		Fn(VisualMesh);
	}
}

FVector ALyraXinShooterEnemy::GetVisualBoundsCenter() const
{
	if (VisualMesh)
	{
		return VisualMesh->Bounds.Origin;
	}
	return Super::GetVisualBoundsCenter();
}

bool ALyraXinShooterEnemy::GetStableMeshBounds(FBox& OutBox) const
{
	if (VisualMesh)
	{
		OutBox = VisualMesh->Bounds.GetBox();
		return true;
	}
	return Super::GetStableMeshBounds(OutBox);
}

void ALyraXinShooterEnemy::StopMeshAnimation()
{
	Super::StopMeshAnimation();
	if (VisualMesh)
	{
		VisualMesh->bPauseAnims = true;
	}
}

USkeletalMeshComponent* ALyraXinShooterEnemy::GetDeathRagdollMesh() const
{
	return VisualMesh ? VisualMesh.Get() : GetMesh();
}

void ALyraXinShooterEnemy::EnableDeathRagdoll(USkeletalMeshComponent* MeshComp)
{
	if (!MeshComp)
	{
		return;
	}
	MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComp->SetDisablePostProcessBlueprint(true);
	MeshComp->bPauseAnims = true;
	MeshComp->SetSimulatePhysics(true);
	MeshComp->SetAllBodiesSimulatePhysics(false);

	static const TCHAR* CoreBones[] = {
		TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"),
		TEXT("neck_01"), TEXT("head"),
		TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
		TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
		TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"),
		TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"),
	};
	for (const TCHAR* Bone : CoreBones)
	{
		const FName BoneName(Bone);
		if (MeshComp->GetBodyInstance(BoneName))
		{
			MeshComp->SetBodySimulatePhysics(BoneName, true);
		}
	}
	MeshComp->WakeAllRigidBodies();
}

void ALyraXinShooterEnemy::HandleDeath()
{
	if (VisualMesh)
	{
		VisualMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		if (USkeletalMesh* Body = VisualMesh->GetSkeletalMeshAsset())
		{
			if (UPhysicsAsset* PhysAsset = LoadXinSidecarPhysicsAsset(Body))
			{
				VisualMesh->SetPhysicsAsset(PhysAsset, true);
			}
		}
	}
	Super::HandleDeath();
}

void ALyraXinShooterEnemy::CycleVisualSkin()
{
	ApplyVisualForPhase(VisualForm >= 2 ? 1 : 2);
	UE_LOG(LogSlimeFable, Log, TEXT("LyraXin %s cycled visual form to %d"), *GetName(), VisualForm);
}

bool ALyraXinShooterEnemy::AbsorbLethalDamage(AActor* DamageCauser)
{
	(void)DamageCauser;
	if (HealthPhase >= 2 || bDeathSequence || bDevouredDeath || bMorphTarget)
	{
		return false;
	}
	BeginPhase2();
	return true;
}

void ALyraXinShooterEnemy::BeginPhase2()
{
	HealthPhase = 2;
	VisualForm = 2;
	if (Health)
	{
		Health->SetMaxAndRefill(Phase2MaxHP);
		Health->SetInvulnerableFor(PhaseTransitionInvulnSeconds);
	}
	ApplyVisualForPhase(2);
	RefreshDualHealthBars();
	SyncLyraHealthFromSlime();
	UE_LOG(LogSlimeFable, Log, TEXT("LyraXin %s entered phase 2 (max %.0f)"), *GetName(), Phase2MaxHP);
}

void ALyraXinShooterEnemy::SnapWeaponsToVisual()
{
	USkeletalMeshComponent* Source = GetMesh();
	if (!Source || !EquippedWeapon)
	{
		return;
	}

	static const FName MannyWeapon(TEXT("weapon_r"));
	if (!Source->DoesSocketExist(MannyWeapon))
	{
		return;
	}

	for (AActor* Actor : EquippedWeapon->GetSpawnedActors())
	{
		if (!Actor)
		{
			continue;
		}
		USceneComponent* Root = Actor->GetRootComponent();
		if (!Root)
		{
			continue;
		}

		if (Root->GetAttachParent() != Source || Actor->GetAttachParentSocketName() != MannyWeapon)
		{
			Actor->AttachToComponent(Source, FAttachmentTransformRules::SnapToTargetNotIncludingScale, MannyWeapon);
			WeaponSpawnRelative.Remove(Actor);
		}

		if (!WeaponSpawnRelative.Contains(Actor))
		{
			WeaponSpawnRelative.Add(Actor, Root->GetRelativeTransform());
		}

		const FTransform Extra(WeaponHandRotation, WeaponHandOffset, FVector::OneVector);
		FTransform World = Extra * WeaponSpawnRelative[Actor] * Source->GetSocketTransform(MannyWeapon);
		World.AddToTranslation(WeaponWorldOffset);
		Actor->SetActorTransform(World, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

namespace
{
	bool IsRightSideBone(const FName Bone)
	{
		const FString Name = Bone.ToString();
		return Name.EndsWith(TEXT("_r"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("右"));
	}

	FName FirstExistingSocket(const USkeletalMeshComponent* Mesh, std::initializer_list<const TCHAR*> Names)
	{
		if (!Mesh)
		{
			return NAME_None;
		}
		for (const TCHAR* Name : Names)
		{
			const FName Socket(Name);
			if (Mesh->DoesSocketExist(Socket))
			{
				return Socket;
			}
		}
		return NAME_None;
	}

	FVector MakeArmPole(const USkeletalMeshComponent* Mesh, const FName Upper, const FName Mid, const FName End, const FVector& Fallback)
	{
		if (!Mesh || Upper.IsNone() || Mid.IsNone() || End.IsNone())
		{
			return Fallback;
		}
		const FVector Shoulder = Mesh->GetSocketTransform(Upper).GetLocation();
		const FTransform ElbowXf = Mesh->GetSocketTransform(Mid);
		const FVector Elbow = ElbowXf.GetLocation();
		const FVector Wrist = Mesh->GetSocketTransform(End).GetLocation();
		const FVector Closest = FMath::ClosestPointOnSegment(Elbow, Shoulder, Wrist);
		FVector Bend = Elbow - Closest;
		if (Bend.Normalize())
		{
			return Elbow + Bend * 40.f;
		}
		FVector FallbackDir = ElbowXf.GetRotation().GetRightVector();
		if (!FallbackDir.Normalize())
		{
			FallbackDir = -Mesh->GetUpVector();
		}
		return Elbow + FallbackDir * 40.f;
	}
}

void ALyraXinShooterEnemy::ApplyHandIKToAnimInstance(UAnimInstance* AnimInstance) const
{
	if (!AnimInstance)
	{
		return;
	}

	USkeletalMeshComponent* Source = GetMesh();
	float Alpha = 0.f;
	if (bPullHandsToSource && Source && HandIKAlpha > 0.f)
	{
		const FName Pelvis = FirstExistingSocket(Source, { TEXT("pelvis"), TEXT("Pelvis") });
		const FName RaisedHand = FirstExistingSocket(Source, { TEXT("hand_r"), TEXT("Hand_R") });
		if (!Pelvis.IsNone() && !RaisedHand.IsNone())
		{
			const float RaiseCm = Source->GetSocketTransform(RaisedHand).GetLocation().Z
				- Source->GetSocketTransform(Pelvis).GetLocation().Z;
			const float RaiseAlpha = FMath::GetMappedRangeValueClamped(
				FVector2D(HandIKMinRaiseAbovePelvis - 10.f, HandIKMinRaiseAbovePelvis + 10.f),
				FVector2D(0.f, 1.f),
				RaiseCm);
			Alpha = HandIKAlpha * RaiseAlpha;
		}
		else
		{
			Alpha = HandIKAlpha;
		}
	}
	if (Alpha <= 0.f)
	{
		for (TFieldIterator<FStructProperty> It(AnimInstance->GetClass()); It; ++It)
		{
			if (It->Struct == FAnimNode_SlimeArmIK::StaticStruct())
			{
				if (FAnimNode_SlimeArmIK* Node = It->ContainerPtrToValuePtr<FAnimNode_SlimeArmIK>(AnimInstance))
				{
					Node->Alpha = 0.f;
				}
			}
			else if (It->Struct == FAnimNode_ModifyBone::StaticStruct())
			{
				if (FAnimNode_ModifyBone* Node = It->ContainerPtrToValuePtr<FAnimNode_ModifyBone>(AnimInstance))
				{
					Node->Alpha = 0.f;
				}
			}
		}
		return;
	}

	if (!Source)
	{
		return;
	}

	const FName SrcRightHand = FirstExistingSocket(Source, { TEXT("hand_r"), TEXT("Hand_R") });
	const FName SrcLeftHand = FirstExistingSocket(Source, { TEXT("hand_l"), TEXT("Hand_L") });
	const FName SrcRightUpper = FirstExistingSocket(Source, { TEXT("upperarm_r"), TEXT("UpperArm_R") });
	const FName SrcLeftUpper = FirstExistingSocket(Source, { TEXT("upperarm_l"), TEXT("UpperArm_L") });
	const FName SrcRightElbow = FirstExistingSocket(Source, { TEXT("lowerarm_r"), TEXT("LowerArm_R") });
	const FName SrcLeftElbow = FirstExistingSocket(Source, { TEXT("lowerarm_l"), TEXT("LowerArm_L") });
	if (SrcRightHand.IsNone() && SrcLeftHand.IsNone())
	{
		return;
	}

	const FVector RightEffector = Source->GetSocketTransform(SrcRightHand).TransformPosition(RightHandAlignOffset);
	const FVector LeftEffector = Source->GetSocketTransform(SrcLeftHand).TransformPosition(LeftHandAlignOffset);
	const FVector RightPole = MakeArmPole(Source, SrcRightUpper, SrcRightElbow, SrcRightHand, RightEffector);
	const FVector LeftPole = MakeArmPole(Source, SrcLeftUpper, SrcLeftElbow, SrcLeftHand, LeftEffector);

	int32 WrittenIK = 0;
	int32 WrittenRot = 0;
	for (TFieldIterator<FStructProperty> It(AnimInstance->GetClass()); It; ++It)
	{
		if (It->Struct == FAnimNode_SlimeArmIK::StaticStruct())
		{
			FAnimNode_SlimeArmIK* Node = It->ContainerPtrToValuePtr<FAnimNode_SlimeArmIK>(AnimInstance);
			if (!Node)
			{
				continue;
			}
			const bool bRight = IsRightSideBone(Node->EndBone.BoneName);
			Node->EffectorLocationSpace = BCS_WorldSpace;
			Node->JointTargetLocationSpace = BCS_WorldSpace;
			Node->bAllowStretching = false;
			Node->EffectorLocation = bRight ? RightEffector : LeftEffector;
			Node->JointTargetLocation = bRight ? RightPole : LeftPole;
			Node->Alpha = Alpha;
			++WrittenIK;
		}
		else if (It->Struct == FAnimNode_ModifyBone::StaticStruct())
		{
			FAnimNode_ModifyBone* Node = It->ContainerPtrToValuePtr<FAnimNode_ModifyBone>(AnimInstance);
			if (!Node)
			{
				continue;
			}
			const bool bRight = IsRightSideBone(Node->BoneToModify.BoneName);
			const FRotator AlignRot = bRight ? RightHandAlignRotation : LeftHandAlignRotation;
			Node->TranslationMode = BMM_Ignore;
			Node->ScaleMode = BMM_Ignore;
			Node->RotationMode = BMM_Additive;
			Node->RotationSpace = BCS_ParentBoneSpace;
			Node->Rotation = AlignRot;
			Node->Alpha = AlignRot.IsNearlyZero() ? 0.f : Alpha;
			++WrittenRot;
		}
	}

#if !UE_BUILD_SHIPPING
	static bool bLoggedHandIK = false;
	if (!bLoggedHandIK)
	{
		bLoggedHandIK = true;
		UE_LOG(LogSlimeFable, Log, TEXT("LyraXin %s HandIK ArmIK=%d Modify=%d srcR=%s srcL=%s"),
			*GetName(), WrittenIK, WrittenRot, *SrcRightHand.ToString(), *SrcLeftHand.ToString());
	}
#endif
}
