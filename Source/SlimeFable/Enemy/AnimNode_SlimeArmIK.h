// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_SlimeArmIK.generated.h"

/**
 * Two-bone IK with explicit upper / lower / end bones.
 * Used for 心月狐 so twist bones between elbow and hand are not treated as the IK chain.
 */
USTRUCT(BlueprintInternalUseOnly)
struct SLIMEFABLE_API FAnimNode_SlimeArmIK : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = IK)
	FBoneReference UpperBone;

	UPROPERTY(EditAnywhere, Category = IK)
	FBoneReference LowerBone;

	UPROPERTY(EditAnywhere, Category = IK)
	FBoneReference EndBone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effector, meta = (PinShownByDefault))
	FVector EffectorLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = JointTarget, meta = (PinShownByDefault))
	FVector JointTargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = Effector)
	TEnumAsByte<EBoneControlSpace> EffectorLocationSpace = BCS_WorldSpace;

	UPROPERTY(EditAnywhere, Category = JointTarget)
	TEnumAsByte<EBoneControlSpace> JointTargetLocationSpace = BCS_WorldSpace;

	UPROPERTY(EditAnywhere, Category = IK)
	bool bAllowStretching = false;

	FAnimNode_SlimeArmIK();

	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

private:
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

	static FTransform ConvertTargetToCS(
		const FTransform& ComponentTransform,
		FCSPose<FCompactPose>& Pose,
		EBoneControlSpace Space,
		const FVector& Location);
};
