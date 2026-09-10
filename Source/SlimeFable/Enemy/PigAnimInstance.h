// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PigAnimInstance.generated.h"

class UAnimSequence;
class UBlendSpace;

UENUM(BlueprintType)
enum class EPigGait : uint8
{
	Idle,
	Walk,
	Run
};

/** Data-only AnimInstance for the pig Chaser. Pose comes from ABP_Pig. */
UCLASS(Blueprintable, BlueprintType, meta = (BlueprintThreadSafe))
class SLIMEFABLE_API UPigAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPigAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadOnly, Category = "Pig|Locomotion", meta = (BlueprintThreadSafe))
	float Speed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Pig|Locomotion", meta = (BlueprintThreadSafe))
	float Direction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Pig|Locomotion", meta = (BlueprintThreadSafe))
	EPigGait Gait = EPigGait::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Pig|Locomotion", meta = (BlueprintThreadSafe))
	bool bHasAcceleration = false;

	UPROPERTY(BlueprintReadOnly, Category = "Pig|Locomotion", meta = (BlueprintThreadSafe))
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "Pig|Locomotion", meta = (BlueprintThreadSafe))
	bool bIsRunning = false;

	UPROPERTY(BlueprintReadOnly, Category = "Pig|Combat", meta = (BlueprintThreadSafe))
	bool bIsInCombat = false;

	UPROPERTY(BlueprintReadOnly, Category = "Pig|Combat", meta = (BlueprintThreadSafe))
	bool bIsAttacking = false;

	UPROPERTY(BlueprintReadOnly, Category = "Pig|Combat", meta = (BlueprintThreadSafe))
	bool bIsHit = false;

	UPROPERTY(BlueprintReadOnly, Category = "Pig|Combat", meta = (BlueprintThreadSafe))
	bool bIsDead = false;

	UPROPERTY(BlueprintReadOnly, Category = "Pig|Idle", meta = (BlueprintThreadSafe))
	bool bWantsRest = false;

	UPROPERTY(BlueprintReadOnly, Category = "Pig|Idle", meta = (BlueprintThreadSafe))
	TObjectPtr<UAnimSequence> IdleSequence;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pig|Locomotion",
		meta = (ToolTip = "低于此速度且无加速度视为 Idle。默认 40。"))
	float WalkSpeedThreshold = 40.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pig|Locomotion",
		meta = (ToolTip = "高于此速度视为 Run。默认 350。"))
	float RunSpeedThreshold = 350.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pig|Idle",
		meta = (ClampMin = "1.0", Units = "s",
			ToolTip = "非战斗静止这么久后进入趴下。默认 6。"))
	float RestIdleSeconds = 6.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pig|Locomotion|Assets")
	TObjectPtr<UBlendSpace> GroundBlendSpace;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pig|Locomotion|Assets")
	TArray<TObjectPtr<UAnimSequence>> IdleVariants;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pig|Locomotion|Assets")
	TObjectPtr<UAnimSequence> RestSequence;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pig|Locomotion|Assets")
	TObjectPtr<UAnimSequence> HitSequence;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pig|Locomotion|Assets")
	TObjectPtr<UAnimSequence> DeathSequence;

protected:
	void UpdateGait();
	void UpdateIdleVariant(float DeltaSeconds);
	void UpdateRest(float DeltaSeconds);

	float IdleStillSeconds = 0.f;
	float IdleVariantRemaining = 0.f;
};
