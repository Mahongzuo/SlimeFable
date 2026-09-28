// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "XinPhysicsAnimInstance.generated.h"

/**
 * Post-process anim instance for 心月狐.
 * NativeUpdate writes SlimeArmIK / ModifyBone targets before evaluate.
 */
UCLASS()
class SLIMEFABLE_API UXinPhysicsAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
