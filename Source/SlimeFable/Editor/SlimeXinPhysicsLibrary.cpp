// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeXinPhysicsLibrary.h"

#include "Engine/SkeletalMesh.h"
#include <initializer_list>
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "ReferenceSkeleton.h"
#include "AnimationRuntime.h"
#include "SlimeFable.h"

#if WITH_EDITOR
#include "AnimGraphNode_KawaiiPhysics.h"
#include "AnimGraphNode_LinkedInputPose.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_Root.h"
#include "AnimNode_SlimeArmIK.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "Animation/AnimBlueprint.h"
#include "AnimationGraphSchema.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Enemy/XinPhysicsAnimInstance.h"
#include "KawaiiPhysicsTypes.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "PhysicsAssetUtils.h"
#endif

namespace
{
	bool IsDummyOrShadow(const FString& Name)
	{
		return Name.StartsWith(TEXT("_dummy"), ESearchCase::IgnoreCase)
			|| Name.StartsWith(TEXT("_shadow"), ESearchCase::IgnoreCase);
	}

	bool ShouldExcludeFromKawaii(const FString& Name)
	{
		return IsDummyOrShadow(Name)
			|| Name.Contains(TEXT("ZSpring"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("Hairpin"), ESearchCase::IgnoreCase);
	}

	enum class EXinKawaiiGroup : uint8
	{
		None,
		Tail,
		Ear,
		Hair,
		Dress,
		DressBody,
		DressPanel,
		Sleeve,
	};

	EXinKawaiiGroup ClassifyXinAccessory(const FString& Name)
	{
		if (ShouldExcludeFromKawaii(Name))
		{
			return EXinKawaiiGroup::None;
		}
		if (Name.Contains(TEXT("HairTail"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("FHair_"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("SHair_"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("BHair_"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("M_BHair"), ESearchCase::IgnoreCase)
			|| Name.StartsWith(TEXT("Hair_"), ESearchCase::IgnoreCase))
		{
			return EXinKawaiiGroup::Hair;
		}
		if (Name.Contains(TEXT("Tail_"), ESearchCase::IgnoreCase))
		{
			return EXinKawaiiGroup::Tail;
		}
		if (Name.Contains(TEXT("heart"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("wear"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("hearing"), ESearchCase::IgnoreCase))
		{
			return EXinKawaiiGroup::None;
		}
		if (Name.Contains(TEXT("Ear"), ESearchCase::IgnoreCase))
		{
			return EXinKawaiiGroup::Ear;
		}
		if (Name.Contains(TEXT("Sleeve"), ESearchCase::IgnoreCase))
		{
			return EXinKawaiiGroup::Sleeve;
		}
		if (Name.Contains(TEXT("DressE_"), ESearchCase::CaseSensitive))
		{
			return EXinKawaiiGroup::DressBody;
		}
		if (Name.Contains(TEXT("DressF_"), ESearchCase::CaseSensitive))
		{
			return EXinKawaiiGroup::DressPanel;
		}
		if (Name.Contains(TEXT("Liusu"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("Zhui"), ESearchCase::IgnoreCase))
		{
			return EXinKawaiiGroup::Dress;
		}
		if (Name.Contains(TEXT("Dress"), ESearchCase::IgnoreCase))
		{
			// Dress_A/G/H/J are side flaps. Do not put them in the tassel SyncBone group.
			return EXinKawaiiGroup::DressBody;
		}
		return EXinKawaiiGroup::None;
	}

	FBoneReference MakeBoneRef(const FName Name)
	{
		FBoneReference Ref;
		Ref.BoneName = Name;
		return Ref;
	}

	bool BoneHasSide(const FString& Name, const TCHAR* SideToken, const TCHAR* CjkSide)
	{
		return Name.Contains(SideToken, ESearchCase::IgnoreCase)
			|| Name.Contains(CjkSide);
	}

	FName FindXinLimbBone(const FReferenceSkeleton& RefSkel, std::initializer_list<const TCHAR*> Exact,
		const TCHAR* Needle, const TCHAR* SideToken, const TCHAR* CjkSide)
	{
		for (const TCHAR* Name : Exact)
		{
			if (RefSkel.FindBoneIndex(FName(Name)) != INDEX_NONE)
			{
				return FName(Name);
			}
		}

		FName Best = NAME_None;
		int32 BestLen = MAX_int32;
		for (int32 Bone = 0; Bone < RefSkel.GetNum(); ++Bone)
		{
			const FName BoneName = RefSkel.GetBoneName(Bone);
			const FString Name = BoneName.ToString();
			if (Name.Contains(TEXT("twist"), ESearchCase::IgnoreCase)
				|| Name.Contains(TEXT("dummy"), ESearchCase::IgnoreCase)
				|| Name.Contains(TEXT("ik_"), ESearchCase::IgnoreCase)
				|| Name.Contains(TEXT("weapon"), ESearchCase::IgnoreCase))
			{
				continue;
			}
			if (!Name.Contains(Needle, ESearchCase::IgnoreCase) || !BoneHasSide(Name, SideToken, CjkSide))
			{
				continue;
			}
			if (Name.Len() < BestLen)
			{
				BestLen = Name.Len();
				Best = BoneName;
			}
		}
		return Best;
	}

	void CollectXinCoreBones(const FReferenceSkeleton& RefSkel, TArray<FName>& OutBones)
	{
		auto FindExact = [&RefSkel](std::initializer_list<const TCHAR*> Names) -> FName
		{
			for (const TCHAR* Name : Names)
			{
				if (RefSkel.FindBoneIndex(FName(Name)) != INDEX_NONE)
				{
					return FName(Name);
				}
			}
			return NAME_None;
		};
		auto AddBone = [&OutBones](const FName Bone)
		{
			if (!Bone.IsNone())
			{
				OutBones.AddUnique(Bone);
			}
		};
		AddBone(FindExact({ TEXT("pelvis"), TEXT("Pelvis"), TEXT("Hips") }));
		AddBone(FindExact({ TEXT("spine_01"), TEXT("spine"), TEXT("Spine") }));
		AddBone(FindExact({ TEXT("spine_02"), TEXT("Spine2") }));
		AddBone(FindExact({ TEXT("spine_03"), TEXT("Spine3") }));
		AddBone(FindExact({ TEXT("neck_01"), TEXT("neck"), TEXT("Neck") }));
		AddBone(FindExact({ TEXT("head"), TEXT("Head") }));
		AddBone(FindXinLimbBone(RefSkel, { TEXT("upperarm_r"), TEXT("UpperArm_R") }, TEXT("upperarm"), TEXT("_r"), TEXT("右")));
		AddBone(FindXinLimbBone(RefSkel, { TEXT("lowerarm_r"), TEXT("LowerArm_R") }, TEXT("lowerarm"), TEXT("_r"), TEXT("右")));
		AddBone(FindXinLimbBone(RefSkel, { TEXT("hand_r"), TEXT("Hand_R") }, TEXT("hand"), TEXT("_r"), TEXT("右")));
		AddBone(FindXinLimbBone(RefSkel, { TEXT("upperarm_l"), TEXT("UpperArm_L") }, TEXT("upperarm"), TEXT("_l"), TEXT("左")));
		AddBone(FindXinLimbBone(RefSkel, { TEXT("lowerarm_l"), TEXT("LowerArm_L") }, TEXT("lowerarm"), TEXT("_l"), TEXT("左")));
		AddBone(FindXinLimbBone(RefSkel, { TEXT("hand_l"), TEXT("Hand_L") }, TEXT("hand"), TEXT("_l"), TEXT("左")));
		AddBone(FindXinLimbBone(RefSkel, { TEXT("thigh_r"), TEXT("Thigh_R") }, TEXT("thigh"), TEXT("_r"), TEXT("右")));
		AddBone(FindXinLimbBone(RefSkel, { TEXT("calf_r"), TEXT("Calf_R") }, TEXT("calf"), TEXT("_r"), TEXT("右")));
		AddBone(FindXinLimbBone(RefSkel, { TEXT("foot_r"), TEXT("Foot_R") }, TEXT("foot"), TEXT("_r"), TEXT("右")));
		AddBone(FindXinLimbBone(RefSkel, { TEXT("thigh_l"), TEXT("Thigh_L") }, TEXT("thigh"), TEXT("_l"), TEXT("左")));
		AddBone(FindXinLimbBone(RefSkel, { TEXT("calf_l"), TEXT("Calf_L") }, TEXT("calf"), TEXT("_l"), TEXT("左")));
		AddBone(FindXinLimbBone(RefSkel, { TEXT("foot_l"), TEXT("Foot_L") }, TEXT("foot"), TEXT("_l"), TEXT("左")));
	}

	const TCHAR* PhysicsTypeLabel(TEnumAsByte<EPhysicsType> Type)
	{
		switch (Type.GetValue())
		{
		case PhysType_Simulated: return TEXT("Simulated");
		case PhysType_Kinematic: return TEXT("Kinematic");
		default: return TEXT("Default");
		}
	}

	FString DescribeAggGeom(const FKAggregateGeom& Agg, float& OutMaxRadius)
	{
		FString Geom;
		for (const FKSphereElem& Sphere : Agg.SphereElems)
		{
			OutMaxRadius = FMath::Max(OutMaxRadius, Sphere.Radius);
			Geom += FString::Printf(TEXT(" sphere=%.1f@(%0.1f,%0.1f,%0.1f)"),
				Sphere.Radius, Sphere.Center.X, Sphere.Center.Y, Sphere.Center.Z);
		}
		for (const FKSphylElem& Capsule : Agg.SphylElems)
		{
			OutMaxRadius = FMath::Max(OutMaxRadius, Capsule.Radius);
			Geom += FString::Printf(TEXT(" sphyl=%.1fx%.1f@(%0.1f,%0.1f,%0.1f)"),
				Capsule.Radius, Capsule.Length, Capsule.Center.X, Capsule.Center.Y, Capsule.Center.Z);
		}
		for (const FKBoxElem& Box : Agg.BoxElems)
		{
			const float Extent = FMath::Max3(Box.X, Box.Y, Box.Z);
			OutMaxRadius = FMath::Max(OutMaxRadius, Extent);
			Geom += FString::Printf(TEXT(" box=%.1fx%.1fx%.1f"), Box.X, Box.Y, Box.Z);
		}
		for (const FKConvexElem& Convex : Agg.ConvexElems)
		{
			const FBox ConvexBox = Convex.ElemBox;
			const float Extent = ConvexBox.GetExtent().GetMax();
			OutMaxRadius = FMath::Max(OutMaxRadius, Extent);
			Geom += FString::Printf(TEXT(" convexVerts=%d extent=%.1f"), Convex.VertexData.Num(), Extent);
		}
		if (Geom.IsEmpty())
		{
			Geom = TEXT(" empty");
		}
		return Geom;
	}

	int32 ClearMeshSimpleCollision(USkeletalMesh* Mesh)
	{
		if (!Mesh)
		{
			return 0;
		}
		UBodySetup* MeshBody = Mesh->GetBodySetup();
		if (!MeshBody)
		{
			return 0;
		}
		const int32 Count = MeshBody->AggGeom.GetElementCount();
		if (Count <= 0)
		{
			return 0;
		}
		Mesh->Modify();
		MeshBody->Modify();
		MeshBody->AggGeom.EmptyElements();
		MeshBody->InvalidatePhysicsData();
		MeshBody->CreatePhysicsMeshes();
		Mesh->MarkPackageDirty();
		UE_LOG(LogSlimeFable, Log, TEXT("ClearMeshSimpleCollision %s removed=%d"), *Mesh->GetName(), Count);
		return Count;
	}

#if WITH_EDITOR
	/**
	 * Kawaii tuning per accessory group. Side skirt (DressBody) keeps the last visible cloth
	 * feel. Front panel is pinned to the authored pose — no SyncBone / planes (those flicker).
	 */
	struct FXinKawaiiTuning
	{
		float Damping;
		float Stiffness;
		float WorldDampingLocation;
		float WorldDampingRotation;
		float Radius;
		float LimitAngle;
		float GravityZ;
	};

	// Tail / ears / hair / misc dress tassels.
	constexpr FXinKawaiiTuning KawaiiDefaultTuning{ 0.28f, 0.2f, 0.85f, 0.85f, 3.f, 20.f, -120.f };
	// DressE + Dress_A/G/H/J: last settings that kept side skirts visible.
	constexpr FXinKawaiiTuning KawaiiDressBodyTuning{ 0.28f, 0.2f, 0.85f, 0.85f, 3.f, 12.f, -120.f };
	// DressF: stay on the bind pose so it covers the thighs without lag or pop.
	constexpr FXinKawaiiTuning KawaiiDressPanelTuning{ 0.4f, 0.7f, 0.9f, 0.9f, 3.f, 4.f, 0.f };
	// Sleeve cuffs.
	constexpr FXinKawaiiTuning KawaiiSleeveTuning{ 0.2f, 0.05f, 0.8f, 0.8f, 2.5f, 50.f, -200.f };

	// Body collision radii (cm) for the capsules that keep cloth off the limbs.
	constexpr float KawaiiThighRadius = 14.f;
	constexpr float KawaiiCalfRadius = 8.f;
	constexpr float KawaiiPelvisRadius = 13.f;
	constexpr float KawaiiUpperArmRadius = 5.5f;
	constexpr float KawaiiLowerArmRadius = 4.5f;

	// Dummy bones between dress joints so collision sees the thigh before it tunnels (plugin docs).
	constexpr int32 KawaiiDressSubdivision = 2;

	void ApplyTuning(FAnimNode_KawaiiPhysics& KP, const FXinKawaiiTuning& T)
	{
		KP.PhysicsSettings.Damping = T.Damping;
		KP.PhysicsSettings.Stiffness = T.Stiffness;
		KP.PhysicsSettings.WorldDampingLocation = T.WorldDampingLocation;
		KP.PhysicsSettings.WorldDampingRotation = T.WorldDampingRotation;
		KP.PhysicsSettings.Radius = T.Radius;
		KP.PhysicsSettings.LimitAngle = T.LimitAngle;
		KP.Gravity = FVector(0.f, 0.f, T.GravityZ);
	}

	/** Capsule from Bone toward ChildBone along the ref-pose bone axis (Kawaii capsules run along local Z). */
	bool AddBoneCapsule(FAnimNode_KawaiiPhysics& KP, const FReferenceSkeleton& RefSkel,
		const FName Bone, const FName ChildBone, float Radius)
	{
		const int32 BoneIndex = RefSkel.FindBoneIndex(Bone);
		const int32 ChildIndex = RefSkel.FindBoneIndex(ChildBone);
		if (BoneIndex == INDEX_NONE || ChildIndex == INDEX_NONE)
		{
			return false;
		}
		const FTransform BoneCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkel, BoneIndex);
		const FTransform ChildCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkel, ChildIndex);
		const FVector LocalChild = BoneCS.InverseTransformPosition(ChildCS.GetLocation());
		const float Length = LocalChild.Size();
		if (Length < KINDA_SMALL_NUMBER)
		{
			return false;
		}
		const FVector Dir = LocalChild / Length;

		FCapsuleLimit Limit;
		Limit.DrivingBone = MakeBoneRef(Bone);
		Limit.OffsetLocation = Dir * Length * 0.5f;
		Limit.OffsetRotation = FRotationMatrix::MakeFromZ(Dir).Rotator();
		Limit.Radius = Radius;
		Limit.Length = FMath::Max(Length - Radius * 0.5f, 1.f);
		Limit.bEnable = true;
		Limit.SourceType = ECollisionSourceType::AnimNode;
#if WITH_EDITORONLY_DATA
		Limit.Guid = FGuid::NewGuid();
		Limit.Type = ECollisionLimitType::Capsule;
#endif
		KP.CapsuleLimits.Add(Limit);
		return true;
	}
#endif

	const TCHAR* GroupLabel(EXinKawaiiGroup Group)
	{
		switch (Group)
		{
		case EXinKawaiiGroup::Tail: return TEXT("Tail");
		case EXinKawaiiGroup::Ear: return TEXT("Ears");
		case EXinKawaiiGroup::Hair: return TEXT("Hair");
		case EXinKawaiiGroup::Dress: return TEXT("Dress");
		case EXinKawaiiGroup::DressBody: return TEXT("DressBody");
		case EXinKawaiiGroup::DressPanel: return TEXT("DressPanel");
		case EXinKawaiiGroup::Sleeve: return TEXT("Sleeves");
		default: return TEXT("None");
		}
	}
}

int32 USlimeXinPhysicsLibrary::ClearXinSimulatedPhysics(USkeletalMesh* Mesh)
{
#if WITH_EDITOR
	if (!Mesh)
	{
		return -1;
	}
	UPhysicsAsset* PhysAsset = Mesh->GetPhysicsAsset();
	if (!PhysAsset)
	{
		return 0;
	}
	PhysAsset->Modify();
	int32 Destroyed = 0;
	for (int32 BodyIndex = PhysAsset->SkeletalBodySetups.Num() - 1; BodyIndex >= 0; --BodyIndex)
	{
		FPhysicsAssetUtils::DestroyBody(PhysAsset, BodyIndex);
		++Destroyed;
	}
	for (int32 ConstraintIndex = PhysAsset->ConstraintSetup.Num() - 1; ConstraintIndex >= 0; --ConstraintIndex)
	{
		FPhysicsAssetUtils::DestroyConstraint(PhysAsset, ConstraintIndex);
	}
	PhysAsset->UpdateBodySetupIndexMap();
	PhysAsset->UpdateBoundsBodiesArray();
	PhysAsset->InvalidateAllPhysicsMeshes();
	PhysAsset->MarkPackageDirty();
	ClearMeshSimpleCollision(Mesh);
	Mesh->MarkPackageDirty();
	UE_LOG(LogSlimeFable, Log, TEXT("ClearXinSimulatedPhysics %s destroyed=%d remaining=%d"),
		*Mesh->GetName(), Destroyed, PhysAsset->SkeletalBodySetups.Num());
	return PhysAsset->SkeletalBodySetups.Num();
#else
	(void)Mesh;
	return -1;
#endif
}

int32 USlimeXinPhysicsLibrary::DumpXinPhysicsAsset(USkeletalMesh* Mesh)
{
#if WITH_EDITOR
	if (!Mesh)
	{
		return -1;
	}
	const FBoxSphereBounds Imported = Mesh->GetImportedBounds();
	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	UE_LOG(LogSlimeFable, Log, TEXT("DumpXinPhysicsAsset %s importedOrigin=(%.1f,%.1f,%.1f) importedR=%.1f boundsR=%.1f"),
		*Mesh->GetName(), Imported.Origin.X, Imported.Origin.Y, Imported.Origin.Z,
		Imported.SphereRadius, Bounds.SphereRadius);

	if (const UBodySetup* MeshBody = Mesh->GetBodySetup())
	{
		float MeshMax = 0.f;
		const FString MeshGeom = DescribeAggGeom(MeshBody->AggGeom, MeshMax);
		UE_LOG(LogSlimeFable, Log, TEXT("  meshBodySetup elems=%d maxRadius=%.1f%s"),
			MeshBody->AggGeom.GetElementCount(), MeshMax, *MeshGeom);
	}
	else
	{
		UE_LOG(LogSlimeFable, Log, TEXT("  meshBodySetup <none>"));
	}

	UPhysicsAsset* PhysAsset = Mesh->GetPhysicsAsset();
	if (!PhysAsset)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("DumpXinPhysicsAsset %s has no PhysicsAsset"), *Mesh->GetName());
		return 0;
	}

	TArray<FName> CoreBones;
	CollectXinCoreBones(Mesh->GetRefSkeleton(), CoreBones);
	float MaxRadius = 0.f;
	for (int32 BodyIndex = 0; BodyIndex < PhysAsset->SkeletalBodySetups.Num(); ++BodyIndex)
	{
		USkeletalBodySetup* Body = PhysAsset->SkeletalBodySetups[BodyIndex];
		if (!Body)
		{
			UE_LOG(LogSlimeFable, Log, TEXT("  [%d] <null>"), BodyIndex);
			continue;
		}
		const FName BoneName = Body->BoneName;
		const bool bCore = CoreBones.Contains(BoneName);
		const FString Geom = DescribeAggGeom(Body->AggGeom, MaxRadius);
		UE_LOG(LogSlimeFable, Log, TEXT("  [%d] %s type=%s core=%d%s"),
			BodyIndex, *BoneName.ToString(), PhysicsTypeLabel(Body->PhysicsType), bCore ? 1 : 0, *Geom);
	}
	UE_LOG(LogSlimeFable, Log, TEXT("DumpXinPhysicsAsset %s bodies=%d constraints=%d maxRadius=%.1f"),
		*Mesh->GetName(), PhysAsset->SkeletalBodySetups.Num(), PhysAsset->ConstraintSetup.Num(), MaxRadius);
	return PhysAsset->SkeletalBodySetups.Num();
#else
	(void)Mesh;
	return -1;
#endif
}

int32 USlimeXinPhysicsLibrary::RestoreXinRagdollBodies(USkeletalMesh* Mesh)
{
#if WITH_EDITOR
	if (!Mesh)
	{
		return -1;
	}
	UPhysicsAsset* PhysAsset = Mesh->GetPhysicsAsset();
	if (!PhysAsset)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("RestoreXinRagdollBodies %s has no PhysicsAsset"), *Mesh->GetName());
		return 0;
	}

	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	TArray<FName> CoreBones;
	CollectXinCoreBones(RefSkel, CoreBones);

	FPhysAssetCreateParams Params;
	Params.GeomType = EFG_Sphyl;
	Params.bAutoOrientToBone = true;
	Params.bCreateConstraints = false;
	Params.bIncludeChildBones = false;
	Params.bBodyForAll = false;
	Params.MinBoneSize = 1.f;

	PhysAsset->Modify();
	for (int32 BodyIndex = PhysAsset->SkeletalBodySetups.Num() - 1; BodyIndex >= 0; --BodyIndex)
	{
		USkeletalBodySetup* Existing = PhysAsset->SkeletalBodySetups[BodyIndex];
		if (!Existing || !CoreBones.Contains(Existing->BoneName))
		{
			FPhysicsAssetUtils::DestroyBody(PhysAsset, BodyIndex);
		}
	}

	int32 Enabled = 0;
	for (const FName BoneName : CoreBones)
	{
		const int32 BoneIndex = RefSkel.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			continue;
		}

		int32 BodyIndex = PhysAsset->FindBodyIndex(BoneName);
		if (BodyIndex == INDEX_NONE)
		{
			BodyIndex = FPhysicsAssetUtils::CreateNewBody(PhysAsset, BoneName, Params);
		}
		if (!PhysAsset->SkeletalBodySetups.IsValidIndex(BodyIndex))
		{
			continue;
		}

		USkeletalBodySetup* Body = PhysAsset->SkeletalBodySetups[BodyIndex];
		if (!Body)
		{
			continue;
		}

		// CreateNewBody sizes from hair/dress weights — wipe and write human-scale capsules.
		Body->AggGeom.EmptyElements();
		const FString Name = BoneName.ToString();
		const bool bSphere = Name.Contains(TEXT("head"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("hand"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("foot"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("pelvis"), ESearchCase::IgnoreCase);
		float Radius = 7.f;
		if (Name.Contains(TEXT("pelvis"), ESearchCase::IgnoreCase))
		{
			Radius = 14.f;
		}
		else if (Name.Contains(TEXT("head"), ESearchCase::IgnoreCase))
		{
			Radius = 10.f;
		}
		else if (Name.Contains(TEXT("spine"), ESearchCase::IgnoreCase))
		{
			Radius = 10.f;
		}
		else if (Name.Contains(TEXT("thigh"), ESearchCase::IgnoreCase))
		{
			Radius = 8.f;
		}
		else if (Name.Contains(TEXT("calf"), ESearchCase::IgnoreCase))
		{
			Radius = 6.f;
		}
		else if (Name.Contains(TEXT("neck"), ESearchCase::IgnoreCase))
		{
			Radius = 5.f;
		}

		float Length = 10.f;
		const FTransform BoneCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkel, BoneIndex);
		for (int32 Child = 0; Child < RefSkel.GetNum(); ++Child)
		{
			if (RefSkel.GetParentIndex(Child) == BoneIndex)
			{
				const FName ChildName = RefSkel.GetBoneName(Child);
				if (!CoreBones.Contains(ChildName))
				{
					continue;
				}
				const FTransform ChildCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkel, Child);
				Length = FMath::Clamp((ChildCS.GetLocation() - BoneCS.GetLocation()).Size() * 0.7f, 6.f, 40.f);
				break;
			}
		}

		if (bSphere)
		{
			FKSphereElem Sphere;
			Sphere.Center = FVector::ZeroVector;
			Sphere.Radius = Radius;
			Body->AggGeom.SphereElems.Add(Sphere);
		}
		else
		{
			FKSphylElem Capsule;
			Capsule.Center = FVector(Length * 0.5f, 0.f, 0.f);
			Capsule.Rotation = FRotator(0.f, 0.f, -90.f);
			Capsule.Radius = Radius;
			Capsule.Length = FMath::Max(Length - Radius * 2.f, 2.f);
			Body->AggGeom.SphylElems.Add(Capsule);
		}

		// Default = follow the component flag. Simulated would ragdoll on spawn
		// (VisualMesh is attached to Manny and would fly away).
		Body->PhysicsType = PhysType_Default;
		Body->CollisionReponse = EBodyCollisionResponse::BodyCollision_Enabled;
		Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		Body->InvalidatePhysicsData();
		Body->CreatePhysicsMeshes();
		++Enabled;
	}

	for (const FName BoneName : CoreBones)
	{
		const int32 BoneIndex = RefSkel.FindBoneIndex(BoneName);
		const int32 BodyIndex = PhysAsset->FindBodyIndex(BoneName);
		if (BoneIndex == INDEX_NONE || BodyIndex == INDEX_NONE)
		{
			continue;
		}

		int32 Parent = RefSkel.GetParentIndex(BoneIndex);
		while (Parent != INDEX_NONE)
		{
			const FName ParentName = RefSkel.GetBoneName(Parent);
			const int32 ParentBody = PhysAsset->FindBodyIndex(ParentName);
			if (ParentBody != INDEX_NONE)
			{
				bool bHasConstraint = false;
				for (const UPhysicsConstraintTemplate* Constraint : PhysAsset->ConstraintSetup)
				{
					if (!Constraint)
					{
						continue;
					}
					const FName A = Constraint->DefaultInstance.ConstraintBone1;
					const FName B = Constraint->DefaultInstance.ConstraintBone2;
					if ((A == ParentName && B == BoneName) || (A == BoneName && B == ParentName))
					{
						bHasConstraint = true;
						break;
					}
				}
				if (!bHasConstraint)
				{
					const FName ConstraintName(*FString::Printf(TEXT("%s_%s"), *ParentName.ToString(), *BoneName.ToString()));
					const int32 NewIndex = FPhysicsAssetUtils::CreateNewConstraint(PhysAsset, ConstraintName);
					if (PhysAsset->ConstraintSetup.IsValidIndex(NewIndex) && PhysAsset->ConstraintSetup[NewIndex])
					{
						FConstraintInstance& Instance = PhysAsset->ConstraintSetup[NewIndex]->DefaultInstance;
						Instance.JointName = ConstraintName;
						Instance.ConstraintBone1 = ParentName;
						Instance.ConstraintBone2 = BoneName;
						Instance.SetAngularSwing1Limit(ACM_Limited, 44.f);
						Instance.SetAngularSwing2Limit(ACM_Limited, 44.f);
						Instance.SetAngularTwistLimit(ACM_Limited, 44.f);
					}
				}
				PhysAsset->DisableCollision(ParentBody, BodyIndex);
				break;
			}
			Parent = RefSkel.GetParentIndex(Parent);
		}
	}

	PhysAsset->UpdateBodySetupIndexMap();
	PhysAsset->UpdateBoundsBodiesArray();
	PhysAsset->InvalidateAllPhysicsMeshes();
	PhysAsset->SetPreviewMesh(Mesh);
	PhysAsset->MarkPackageDirty();
	ClearMeshSimpleCollision(Mesh);
	Mesh->MarkPackageDirty();
	UE_LOG(LogSlimeFable, Log, TEXT("RestoreXinRagdollBodies %s enabled=%d total=%d preview=%s"),
		*Mesh->GetName(), Enabled, PhysAsset->SkeletalBodySetups.Num(), *Mesh->GetName());
	return Enabled;
#else
	(void)Mesh;
	return -1;
#endif
}

bool USlimeXinPhysicsLibrary::WireXinKawaiiPhysics(UAnimBlueprint* AnimBP, USkeletalMesh* Mesh)
{
#if WITH_EDITOR
	if (!AnimBP || !Mesh)
	{
		return false;
	}
	USkeleton* Skeleton = Mesh->GetSkeleton();
	if (!Skeleton)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("WireXinKawaiiPhysics: %s has no skeleton"), *Mesh->GetName());
		return false;
	}

	UEdGraph* AnimGraph = nullptr;
	for (UEdGraph* Graph : AnimBP->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == UEdGraphSchema_K2::GN_AnimGraph)
		{
			AnimGraph = Graph;
			break;
		}
	}
	if (!AnimGraph)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("WireXinKawaiiPhysics: %s has no AnimGraph"), *AnimBP->GetName());
		return false;
	}

	AnimBP->Modify();
	AnimGraph->Modify();
	AnimBP->TargetSkeleton = Skeleton;
	if (AnimBP->ParentClass != UXinPhysicsAnimInstance::StaticClass())
	{
		AnimBP->ParentClass = UXinPhysicsAnimInstance::StaticClass();
	}

	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
	UAnimGraphNode_Root* Root = nullptr;
	TArray<UEdGraphNode*> ToRemove;
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		if (!Node)
		{
			continue;
		}
		if (UAnimGraphNode_Root* AsRoot = Cast<UAnimGraphNode_Root>(Node))
		{
			Root = AsRoot;
		}
		else
		{
			ToRemove.Add(Node);
		}
	}
	for (UEdGraphNode* Node : ToRemove)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin)
			{
				Schema->BreakPinLinks(*Pin, true);
			}
		}
		AnimGraph->RemoveNode(Node);
	}
	if (!Root)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("WireXinKawaiiPhysics: %s AnimGraph has no Output Pose"), *AnimBP->GetName());
		return false;
	}

	auto InitNode = [AnimGraph](UEdGraphNode* Node, int32 X, int32 Y)
	{
		Node->CreateNewGuid();
		Node->PostPlacedNewNode();
		Node->AllocateDefaultPins();
		Node->NodePosX = X;
		Node->NodePosY = Y;
		AnimGraph->AddNode(Node, true, false);
	};

	auto FindPosePin = [](UEdGraphNode* Node, EEdGraphPinDirection Dir) -> UEdGraphPin*
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Dir && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
			{
				const FName PinName = Pin->PinName;
				if (PinName == TEXT("Pose") || PinName == TEXT("Result") || PinName == TEXT("ComponentPose"))
				{
					return Pin;
				}
			}
		}
		return nullptr;
	};

	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	TMap<EXinKawaiiGroup, TArray<FName>> RootsByGroup;
	TArray<FBoneReference> ExcludeBones;
	for (int32 Bone = 0; Bone < RefSkel.GetNum(); ++Bone)
	{
		const FName BoneName = RefSkel.GetBoneName(Bone);
		const FString Name = BoneName.ToString();
		if (ShouldExcludeFromKawaii(Name))
		{
			ExcludeBones.Add(MakeBoneRef(BoneName));
			continue;
		}
		const EXinKawaiiGroup Group = ClassifyXinAccessory(Name);
		if (Group == EXinKawaiiGroup::None)
		{
			continue;
		}
		const int32 Parent = RefSkel.GetParentIndex(Bone);
		const EXinKawaiiGroup ParentGroup = Parent == INDEX_NONE
			? EXinKawaiiGroup::None
			: ClassifyXinAccessory(RefSkel.GetBoneName(Parent).ToString());
		if (ParentGroup == Group)
		{
			continue;
		}
		RootsByGroup.FindOrAdd(Group).Add(BoneName);
	}

	UAnimGraphNode_LinkedInputPose* Input = NewObject<UAnimGraphNode_LinkedInputPose>(AnimGraph, NAME_None, RF_Transactional);
	Input->Node.Name = FAnimNode_LinkedInputPose::DefaultInputPoseName;
	InitNode(Input, -400, 0);

	const FName RightUpperBone = FindXinLimbBone(RefSkel, { TEXT("upperarm_r"), TEXT("UpperArm_R") }, TEXT("upperarm"), TEXT("_r"), TEXT("右"));
	const FName RightLowerBone = FindXinLimbBone(RefSkel, { TEXT("lowerarm_r"), TEXT("LowerArm_R") }, TEXT("lowerarm"), TEXT("_r"), TEXT("右"));
	const FName RightHandBone = FindXinLimbBone(RefSkel, { TEXT("hand_r"), TEXT("Hand_R") }, TEXT("hand"), TEXT("_r"), TEXT("右"));
	const FName LeftUpperBone = FindXinLimbBone(RefSkel, { TEXT("upperarm_l"), TEXT("UpperArm_L") }, TEXT("upperarm"), TEXT("_l"), TEXT("左"));
	const FName LeftLowerBone = FindXinLimbBone(RefSkel, { TEXT("lowerarm_l"), TEXT("LowerArm_L") }, TEXT("lowerarm"), TEXT("_l"), TEXT("左"));
	const FName LeftHandBone = FindXinLimbBone(RefSkel, { TEXT("hand_l"), TEXT("Hand_L") }, TEXT("hand"), TEXT("_l"), TEXT("左"));

	TArray<UEdGraphNode*> Chain;
	Chain.Add(Input);
	int32 NodeX = 0;

	auto AddHandIK = [&](const FName UpperBone, const FName LowerBone, const FName HandBone, int32& InOutX)
	{
		if (UpperBone.IsNone() || LowerBone.IsNone() || HandBone.IsNone())
		{
			return;
		}
		FModuleManager::Get().LoadModule(TEXT("SlimeFableEditor"));
		UClass* IKClass = LoadObject<UClass>(nullptr, TEXT("/Script/SlimeFableEditor.AnimGraphNode_SlimeArmIK"));
		if (!IKClass)
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("WireXinKawaiiPhysics missing AnimGraphNode_SlimeArmIK"));
			return;
		}
		UEdGraphNode* IKGraphNode = NewObject<UEdGraphNode>(AnimGraph, IKClass, NAME_None, RF_Transactional);
		FStructProperty* NodeProp = FindFProperty<FStructProperty>(IKClass, TEXT("Node"));
		FAnimNode_SlimeArmIK* IKNode = NodeProp
			? NodeProp->ContainerPtrToValuePtr<FAnimNode_SlimeArmIK>(IKGraphNode)
			: nullptr;
		if (!IKNode)
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("WireXinKawaiiPhysics AnimGraphNode_SlimeArmIK has no Node"));
			return;
		}
		IKNode->UpperBone = MakeBoneRef(UpperBone);
		IKNode->LowerBone = MakeBoneRef(LowerBone);
		IKNode->EndBone = MakeBoneRef(HandBone);
		IKNode->EffectorLocationSpace = BCS_WorldSpace;
		IKNode->JointTargetLocationSpace = BCS_WorldSpace;
		IKNode->bAllowStretching = false;
		IKNode->Alpha = 1.f;
		InitNode(IKGraphNode, InOutX, 0);
		InOutX += 280;
		Chain.Add(IKGraphNode);

		UAnimGraphNode_ModifyBone* RotNode = NewObject<UAnimGraphNode_ModifyBone>(AnimGraph, NAME_None, RF_Transactional);
		RotNode->Node.BoneToModify = MakeBoneRef(HandBone);
		RotNode->Node.TranslationMode = BMM_Ignore;
		RotNode->Node.RotationMode = BMM_Additive;
		RotNode->Node.ScaleMode = BMM_Ignore;
		RotNode->Node.RotationSpace = BCS_ParentBoneSpace;
		RotNode->Node.Alpha = 0.f;
		InitNode(RotNode, InOutX, 0);
		InOutX += 280;
		Chain.Add(RotNode);

		UE_LOG(LogSlimeFable, Log, TEXT("WireXinKawaiiPhysics %s HandIK upper=%s lower=%s hand=%s"),
			*Mesh->GetName(), *UpperBone.ToString(), *LowerBone.ToString(), *HandBone.ToString());
	};

	auto ApplyCommonSettings = [&ExcludeBones](FAnimNode_KawaiiPhysics& KP)
	{
		KP.ExcludeBones = ExcludeBones;
		KP.SimulationSpace = EKawaiiPhysicsSimulationSpace::ComponentSpace;
		ApplyTuning(KP, KawaiiDefaultTuning);
		KP.bUseWorldSpaceGravity = true;
		KP.bUseDefaultGravityZProjectSetting = false;
	};

	const FName ThighL = FindXinLimbBone(RefSkel, { TEXT("thigh_l"), TEXT("Thigh_L") }, TEXT("thigh"), TEXT("_l"), TEXT("左"));
	const FName ThighR = FindXinLimbBone(RefSkel, { TEXT("thigh_r"), TEXT("Thigh_R") }, TEXT("thigh"), TEXT("_r"), TEXT("右"));
	const FName CalfL = FindXinLimbBone(RefSkel, { TEXT("calf_l"), TEXT("Calf_L") }, TEXT("calf"), TEXT("_l"), TEXT("左"));
	const FName CalfR = FindXinLimbBone(RefSkel, { TEXT("calf_r"), TEXT("Calf_R") }, TEXT("calf"), TEXT("_r"), TEXT("右"));
	const FName FootL = FindXinLimbBone(RefSkel, { TEXT("foot_l"), TEXT("Foot_L") }, TEXT("foot"), TEXT("_l"), TEXT("左"));
	const FName FootR = FindXinLimbBone(RefSkel, { TEXT("foot_r"), TEXT("Foot_R") }, TEXT("foot"), TEXT("_r"), TEXT("右"));

	// Legs: the front panel (DressF) hangs between the thighs and needs real capsules, not a sphere.
	auto AddLegCapsules = [&](FAnimNode_KawaiiPhysics& KP)
	{
		int32 Added = 0;
		Added += AddBoneCapsule(KP, RefSkel, ThighL, CalfL, KawaiiThighRadius) ? 1 : 0;
		Added += AddBoneCapsule(KP, RefSkel, ThighR, CalfR, KawaiiThighRadius) ? 1 : 0;
		Added += AddBoneCapsule(KP, RefSkel, CalfL, FootL, KawaiiCalfRadius) ? 1 : 0;
		Added += AddBoneCapsule(KP, RefSkel, CalfR, FootR, KawaiiCalfRadius) ? 1 : 0;
		return Added;
	};

	// Tassels only (Liusu / Zhui). Side flaps stay in DressBody with no SyncBone.
	auto AddLegSyncBones = [&](FAnimNode_KawaiiPhysics& KP, const TArray<FName>& DressRoots)
	{
		const FName Sources[] = { ThighL, ThighR, CalfL, CalfR };
		int32 Added = 0;
		for (const FName Source : Sources)
		{
			if (Source.IsNone())
			{
				continue;
			}
			FKawaiiPhysicsSyncBone Sync;
			Sync.Bone = MakeBoneRef(Source);
			for (const FName& DressRoot : DressRoots)
			{
				FKawaiiPhysicsSyncTargetRoot Target;
				Target.Bone = MakeBoneRef(DressRoot);
				Target.bIncludeChildBones = true;
				Sync.TargetRoots.Add(Target);
			}
			KP.SyncBones.Add(Sync);
			++Added;
		}
		return Added;
	};

	// Arms: sleeve cuffs hang off the forearm; keep them off both arm segments.
	auto AddArmCapsules = [&](FAnimNode_KawaiiPhysics& KP)
	{
		int32 Added = 0;
		Added += AddBoneCapsule(KP, RefSkel, RightUpperBone, RightLowerBone, KawaiiUpperArmRadius) ? 1 : 0;
		Added += AddBoneCapsule(KP, RefSkel, LeftUpperBone, LeftLowerBone, KawaiiUpperArmRadius) ? 1 : 0;
		Added += AddBoneCapsule(KP, RefSkel, RightLowerBone, RightHandBone, KawaiiLowerArmRadius) ? 1 : 0;
		Added += AddBoneCapsule(KP, RefSkel, LeftLowerBone, LeftHandBone, KawaiiLowerArmRadius) ? 1 : 0;
		return Added;
	};

	auto AddSphere = [](FAnimNode_KawaiiPhysics& KP, const TCHAR* Bone, float Radius)
	{
		FSphericalLimit Limit;
		Limit.DrivingBone = MakeBoneRef(FName(Bone));
		Limit.Radius = Radius;
		Limit.LimitType = ESphericalLimitType::Outer;
		Limit.bEnable = true;
		Limit.SourceType = ECollisionSourceType::AnimNode;
#if WITH_EDITORONLY_DATA
		Limit.Guid = FGuid::NewGuid();
		Limit.Type = ECollisionLimitType::Spherical;
#endif
		KP.SphericalLimits.Add(Limit);
	};

	const EXinKawaiiGroup Order[] = {
		EXinKawaiiGroup::Tail,
		EXinKawaiiGroup::Ear,
		EXinKawaiiGroup::Hair,
		EXinKawaiiGroup::Dress,
		EXinKawaiiGroup::DressBody,
		EXinKawaiiGroup::DressPanel,
		EXinKawaiiGroup::Sleeve,
	};

	AddHandIK(RightUpperBone, RightLowerBone, RightHandBone, NodeX);
	AddHandIK(LeftUpperBone, LeftLowerBone, LeftHandBone, NodeX);
	int32 WiredGroups = 0;
	for (const EXinKawaiiGroup Group : Order)
	{
		const TArray<FName>* Roots = RootsByGroup.Find(Group);
		if (!Roots || Roots->Num() == 0)
		{
			continue;
		}

		UAnimGraphNode_KawaiiPhysics* GraphNode = NewObject<UAnimGraphNode_KawaiiPhysics>(AnimGraph, NAME_None, RF_Transactional);
		FAnimNode_KawaiiPhysics& KP = GraphNode->Node;
		ApplyCommonSettings(KP);
		if (Group == EXinKawaiiGroup::DressBody)
		{
			ApplyTuning(KP, KawaiiDressBodyTuning);
			KP.BoneSubdivisionCount = KawaiiDressSubdivision;
			KP.bBoneSubdivisionCollisionOnly = true;
			KP.bBoneSubdivisionDensifyByRadius = true;
		}
		else if (Group == EXinKawaiiGroup::DressPanel)
		{
			// Pin to authored pose. Capsules / planes / SyncBone fight the pin and flicker.
			ApplyTuning(KP, KawaiiDressPanelTuning);
		}
		else if (Group == EXinKawaiiGroup::Sleeve)
		{
			ApplyTuning(KP, KawaiiSleeveTuning);
		}
		KP.RootBone = MakeBoneRef((*Roots)[0]);
		for (int32 Index = 1; Index < Roots->Num(); ++Index)
		{
			FKawaiiPhysicsRootBoneSetting Extra;
			Extra.RootBone = MakeBoneRef((*Roots)[Index]);
			KP.AdditionalRootBones.Add(Extra);
		}

		int32 Capsules = 0;
		if (Group == EXinKawaiiGroup::DressBody)
		{
			AddSphere(KP, TEXT("pelvis"), KawaiiPelvisRadius);
			Capsules += AddLegCapsules(KP);
		}
		else if (Group == EXinKawaiiGroup::Sleeve)
		{
			AddSphere(KP, TEXT("spine_02"), 16.f);
			Capsules += AddArmCapsules(KP);
		}
		else if (Group != EXinKawaiiGroup::DressPanel)
		{
			AddSphere(KP, TEXT("pelvis"), 18.f);
			AddSphere(KP, TEXT("spine_02"), 16.f);
			AddSphere(KP, TEXT("head"), 10.f);
			AddSphere(KP, TEXT("thigh_l"), 12.f);
			AddSphere(KP, TEXT("thigh_r"), 12.f);
		}

		int32 Syncs = 0;
		if (Group == EXinKawaiiGroup::Dress)
		{
			Syncs = AddLegSyncBones(KP, *Roots);
		}

		InitNode(GraphNode, NodeX, 0);
		NodeX += 360;
		Chain.Add(GraphNode);
		++WiredGroups;

		FString RootList;
		for (const FName& RootName : *Roots)
		{
			if (!RootList.IsEmpty())
			{
				RootList += TEXT(",");
			}
			RootList += RootName.ToString();
		}
		UE_LOG(LogSlimeFable, Log, TEXT("WireXinKawaiiPhysics %s group %s roots=%s spheres=%d capsules=%d syncs=%d subdiv=%d stiff=%.2f"),
			*Mesh->GetName(), GroupLabel(Group), *RootList, KP.SphericalLimits.Num(), Capsules, Syncs,
			KP.BoneSubdivisionCount, KP.PhysicsSettings.Stiffness);
	}

	Root->NodePosX = NodeX + 80;
	Root->NodePosY = 0;
	Chain.Add(Root);

	bool bOk = true;
	auto Connect = [Schema, &bOk](UEdGraphPin* From, UEdGraphPin* To, const TCHAR* Label)
	{
		if (!From || !To)
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("WireXinKawaiiPhysics: missing pin %s"), Label);
			bOk = false;
			return;
		}
		Schema->BreakPinLinks(*To, true);
		if (!Schema->TryCreateConnection(From, To) || To->LinkedTo.Num() == 0)
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("WireXinKawaiiPhysics: connect failed %s"), Label);
			bOk = false;
		}
	};

	for (int32 Index = 0; Index + 1 < Chain.Num(); ++Index)
	{
		Connect(FindPosePin(Chain[Index], EGPD_Output), FindPosePin(Chain[Index + 1], EGPD_Input), TEXT("KawaiiChain"));
	}

	if (WiredGroups == 0)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("WireXinKawaiiPhysics: no accessory roots on %s"), *Mesh->GetName());
		bOk = false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
	FKismetEditorUtilities::CompileBlueprint(AnimBP, EBlueprintCompileOptions::SkipGarbageCollection);

	UClass* Generated = AnimBP->GeneratedClass;
	if (!Generated || AnimBP->Status == BS_Error)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("WireXinKawaiiPhysics: %s failed to compile"), *AnimBP->GetName());
		return false;
	}

	Mesh->Modify();
	Mesh->SetPostProcessAnimBlueprint(Generated);
	Mesh->MarkPackageDirty();
	AnimBP->MarkPackageDirty();

	UE_LOG(LogSlimeFable, Log, TEXT("WireXinKawaiiPhysics %s -> %s groups=%d ok=%d"),
		*AnimBP->GetName(), *Mesh->GetName(), WiredGroups, bOk ? 1 : 0);
	return bOk;
#else
	(void)AnimBP;
	(void)Mesh;
	return false;
#endif
}
