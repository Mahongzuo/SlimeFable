// Copyright Epic Games, Inc. All Rights Reserved.

#include "XinPhysicsAnimInstance.h"

#include "AnimNode_SlimeArmIK.h"
#include "BoneControllers/AnimNode_ModifyBone.h"
#include "LyraXinShooterEnemy.h"
#include "UObject/UnrealType.h"

void UXinPhysicsAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (ALyraXinShooterEnemy* Xin = Cast<ALyraXinShooterEnemy>(TryGetPawnOwner()))
	{
		Xin->ApplyHandIKToAnimInstance(this);
		return;
	}

	for (TFieldIterator<FStructProperty> It(GetClass()); It; ++It)
	{
		if (It->Struct == FAnimNode_SlimeArmIK::StaticStruct())
		{
			if (FAnimNode_SlimeArmIK* Node = It->ContainerPtrToValuePtr<FAnimNode_SlimeArmIK>(this))
			{
				Node->Alpha = 0.f;
			}
		}
		else if (It->Struct == FAnimNode_ModifyBone::StaticStruct())
		{
			if (FAnimNode_ModifyBone* Node = It->ContainerPtrToValuePtr<FAnimNode_ModifyBone>(this))
			{
				Node->Alpha = 0.f;
			}
		}
	}
}
