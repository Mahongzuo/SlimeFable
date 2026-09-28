// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNode_SlimeArmIK.h"

#include "Animation/AnimInstanceProxy.h"
#include "AnimationRuntime.h"
#include "TwoBoneIK.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_SlimeArmIK)

FAnimNode_SlimeArmIK::FAnimNode_SlimeArmIK()
{
}

void FAnimNode_SlimeArmIK::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += TEXT("(");
	AddDebugNodeData(DebugLine);
	DebugLine += FString::Printf(TEXT(" End: %s)"), *EndBone.BoneName.ToString());
	DebugData.AddDebugItem(DebugLine);
	ComponentPose.GatherDebugData(DebugData);
}

FTransform FAnimNode_SlimeArmIK::ConvertTargetToCS(
	const FTransform& ComponentTransform,
	FCSPose<FCompactPose>& Pose,
	EBoneControlSpace Space,
	const FVector& Location)
{
	FTransform OutTransform;
	OutTransform.SetLocation(Location);
	FAnimationRuntime::ConvertBoneSpaceTransformToCS(
		ComponentTransform,
		Pose,
		OutTransform,
		FCompactPoseBoneIndex(INDEX_NONE),
		Space);
	return OutTransform;
}

namespace
{
	bool CollectBonesBetween(
		const FBoneContainer& BoneContainer,
		FCompactPoseBoneIndex Child,
		FCompactPoseBoneIndex Ancestor,
		TArray<FCompactPoseBoneIndex>& OutBetween)
	{
		OutBetween.Reset();
		FCompactPoseBoneIndex Current = BoneContainer.GetParentBoneIndex(Child);
		int32 Guard = 0;
		while (Current != INDEX_NONE && Current != Ancestor && Guard++ < 16)
		{
			OutBetween.Insert(Current, 0);
			Current = BoneContainer.GetParentBoneIndex(Current);
		}
		return Current == Ancestor;
	}

	FTransform FollowLocalsToCS(
		FCSPose<FCompactPose>& Pose,
		const TArray<FCompactPoseBoneIndex>& Between,
		const FTransform& RootCS,
		TArray<FBoneTransform>& OutBoneTransforms)
	{
		FTransform ParentCS = RootCS;
		for (const FCompactPoseBoneIndex Mid : Between)
		{
			const FTransform NewCS = Pose.GetLocalSpaceTransform(Mid) * ParentCS;
			OutBoneTransforms.Add(FBoneTransform(Mid, NewCS));
			ParentCS = NewCS;
		}
		return ParentCS;
	}
}

void FAnimNode_SlimeArmIK::EvaluateSkeletalControl_AnyThread(
	FComponentSpacePoseContext& Output,
	TArray<FBoneTransform>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FCompactPoseBoneIndex UpperIndex = UpperBone.GetCompactPoseIndex(BoneContainer);
	const FCompactPoseBoneIndex LowerIndex = LowerBone.GetCompactPoseIndex(BoneContainer);
	const FCompactPoseBoneIndex EndIndex = EndBone.GetCompactPoseIndex(BoneContainer);
	if (UpperIndex == INDEX_NONE || LowerIndex == INDEX_NONE || EndIndex == INDEX_NONE)
	{
		return;
	}

	FTransform UpperCS = Output.Pose.GetComponentSpaceTransform(UpperIndex);
	FTransform LowerCS = Output.Pose.GetComponentSpaceTransform(LowerIndex);
	FTransform EndCS = Output.Pose.GetComponentSpaceTransform(EndIndex);
	const FTransform EndLocal = Output.Pose.GetLocalSpaceTransform(EndIndex);

	const FTransform ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();
	const FVector DesiredRaw = ConvertTargetToCS(
		ComponentTransform, Output.Pose, EffectorLocationSpace, EffectorLocation).GetLocation();
	const FVector JointTargetPos = ConvertTargetToCS(
		ComponentTransform, Output.Pose, JointTargetLocationSpace, JointTargetLocation).GetLocation();

	const FVector RootPos = UpperCS.GetLocation();
	const float UpperLen = FVector::Dist(RootPos, LowerCS.GetLocation());
	const float LowerLen = FVector::Dist(LowerCS.GetLocation(), EndCS.GetLocation());
	const float MaxReach = FMath::Max(1.f, (UpperLen + LowerLen) * 0.99f);
	FVector DesiredPos = DesiredRaw;
	const FVector Delta = DesiredPos - RootPos;
	const float Dist = Delta.Size();
	if (Dist > MaxReach && Dist > KINDA_SMALL_NUMBER)
	{
		DesiredPos = RootPos + Delta * (MaxReach / Dist);
	}

	AnimationCore::SolveTwoBoneIK(
		UpperCS,
		LowerCS,
		EndCS,
		JointTargetPos,
		DesiredPos,
		bAllowStretching,
		1.0,
		1.0);

	TArray<FCompactPoseBoneIndex> Between;
	OutBoneTransforms.Add(FBoneTransform(UpperIndex, UpperCS));
	if (CollectBonesBetween(BoneContainer, LowerIndex, UpperIndex, Between))
	{
		FollowLocalsToCS(Output.Pose, Between, UpperCS, OutBoneTransforms);
	}
	OutBoneTransforms.Add(FBoneTransform(LowerIndex, LowerCS));

	FTransform HandParentCS = LowerCS;
	if (CollectBonesBetween(BoneContainer, EndIndex, LowerIndex, Between))
	{
		HandParentCS = FollowLocalsToCS(Output.Pose, Between, LowerCS, OutBoneTransforms);
	}
	EndCS.SetRotation((EndLocal * HandParentCS).GetRotation());
	OutBoneTransforms.Add(FBoneTransform(EndIndex, EndCS));
	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}

bool FAnimNode_SlimeArmIK::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return UpperBone.IsValidToEvaluate(RequiredBones)
		&& LowerBone.IsValidToEvaluate(RequiredBones)
		&& EndBone.IsValidToEvaluate(RequiredBones);
}

void FAnimNode_SlimeArmIK::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	UpperBone.Initialize(RequiredBones);
	LowerBone.Initialize(RequiredBones);
	EndBone.Initialize(RequiredBones);
}
