// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimExecutionContext.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNodeReference.h"
#include "Animation/TrajectoryTypes.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "PhoebeAnimInstance.generated.h"

class UAnimSequence;
class UBlendSpace;
class UChooserTable;
class UPhoebeClimbComponent;
class UPoseSearchDatabase;
class AEnemyCharacter;

UENUM(BlueprintType)
enum class EPhoebeGait : uint8
{
	Idle,
	Walk,
	Run,
	Sprint
};

/** Data-only AnimInstance for Phoebe locomotion. Pose comes from ABP_Phoebe (Blend Spaces). */
UCLASS(Blueprintable, BlueprintType, meta = (BlueprintThreadSafe))
class SLIMEFABLE_API UPhoebeAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPhoebeAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** Legacy MM hook — unused once WirePhoebeLocomotionGraph is applied. */
	UFUNCTION(BlueprintCallable, Category = "Phoebe|MotionMatching", meta = (BlueprintThreadSafe))
	void OnPhoebeMotionMatchingUpdated(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);

	UPROPERTY(BlueprintReadOnly, Category = "Phoebe|Locomotion", meta = (BlueprintThreadSafe))
	float Speed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Phoebe|Locomotion", meta = (BlueprintThreadSafe))
	float Direction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Phoebe|Locomotion", meta = (BlueprintThreadSafe))
	bool bIsInAir = false;

	UPROPERTY(BlueprintReadOnly, Category = "Phoebe|Locomotion", meta = (BlueprintThreadSafe))
	bool bIsFalling = false;

	UPROPERTY(BlueprintReadOnly, Category = "Phoebe|Locomotion", meta = (BlueprintThreadSafe))
	bool bIsCrouch = false;

	UPROPERTY(BlueprintReadOnly, Category = "Phoebe|Locomotion", meta = (BlueprintThreadSafe))
	bool bIsSprinting = false;

	UPROPERTY(BlueprintReadOnly, Category = "Phoebe|Locomotion", meta = (BlueprintThreadSafe))
	bool bHasAcceleration = false;

	UPROPERTY(BlueprintReadOnly, Category = "Phoebe|Locomotion", meta = (BlueprintThreadSafe))
	EPhoebeGait Gait = EPhoebeGait::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Phoebe|Climb", meta = (BlueprintThreadSafe))
	bool bIsClimbing = false;

	UPROPERTY(BlueprintReadOnly, Category = "Phoebe|Climb",
		meta = (BlueprintThreadSafe, ToolTip = "攀爬中按住加速（右键/Shift）且有方向输入。"))
	bool bIsClimbDashing = false;

	UPROPERTY(BlueprintReadOnly, Category = "Phoebe|Air",
		meta = (BlueprintThreadSafe, ToolTip = "正在播放下落攻击。"))
	bool bIsAirAttacking = false;

	UPROPERTY(BlueprintReadOnly, Category = "Phoebe|Climb", meta = (BlueprintThreadSafe))
	float ClimbYaw = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Phoebe|Climb", meta = (BlueprintThreadSafe))
	float ClimbPitch = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Phoebe|Climb",
		meta = (BlueprintThreadSafe, ToolTip = "攀爬 BlendSpace PlayRate，由切向移动强度驱动。静止约 0.6，满速约 1.4。"))
	float ClimbPlayRate = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Phoebe|Morph", meta = (BlueprintThreadSafe))
	bool bIsMorphed = false;

	/** World-space predicted trajectory for Motion Matching. */
	UPROPERTY(BlueprintReadOnly, Category = "Phoebe|MotionMatching")
	FTransformTrajectory Trajectory;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phoebe|MotionMatching")
	FPoseSearchTrajectoryData TrajectoryData;

	UPROPERTY(BlueprintReadOnly, Category = "Phoebe|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> ActiveDatabase;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phoebe|MotionMatching",
		meta = (ToolTip = "GASP Chooser：按攀爬/空中/Gait 选 PSD。空则走下方 Fallback 库。"))
	TObjectPtr<UChooserTable> DatabaseChooser;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phoebe|MotionMatching|Fallback")
	TObjectPtr<UPoseSearchDatabase> IdleDatabase;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phoebe|MotionMatching|Fallback")
	TObjectPtr<UPoseSearchDatabase> WalkDatabase;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phoebe|MotionMatching|Fallback")
	TObjectPtr<UPoseSearchDatabase> RunDatabase;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phoebe|MotionMatching|Fallback")
	TObjectPtr<UPoseSearchDatabase> SprintDatabase;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phoebe|MotionMatching|Fallback")
	TObjectPtr<UPoseSearchDatabase> AirDatabase;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phoebe|MotionMatching|Fallback")
	TObjectPtr<UPoseSearchDatabase> ClimbDatabase;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phoebe|Locomotion",
		meta = (ToolTip = "低于此速度且无加速度视为 Idle。默认 40。"))
	float WalkSpeedThreshold = 40.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phoebe|Locomotion",
		meta = (ToolTip = "高于此速度视为 Run。键盘满 WASD 落在这里。默认 180。"))
	float RunSpeedThreshold = 180.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phoebe|Locomotion",
		meta = (ClampMin = "0.0", ToolTip = "按下跳跃后强制空中姿态的宽限时长（秒）。默认 0.2。"))
	float JumpAirGraceSeconds = 0.2f;

	/** Third-person style Blend Spaces (wired by WirePhoebeLocomotionGraph). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phoebe|Locomotion|BlendSpaces",
		meta = (ToolTip = "地面 1D：Idle/Walk/Run/Sprint，X=Speed。"))
	TObjectPtr<UBlendSpace> GroundBlendSpace;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phoebe|Locomotion|BlendSpaces",
		meta = (ToolTip = "攀爬 2D：X=ClimbYaw，Y=ClimbPitch。"))
	TObjectPtr<UBlendSpace> ClimbBlendSpace;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phoebe|Locomotion|BlendSpaces",
		meta = (ToolTip = "攀爬加速 2D：八向 Climb_Dash_*。"))
	TObjectPtr<UBlendSpace> ClimbDashBlendSpace;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phoebe|Locomotion|BlendSpaces",
		meta = (ToolTip = "起跳循环序列。默认 Jump_Loop。"))
	TObjectPtr<UAnimSequence> JumpSequence;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phoebe|Locomotion|BlendSpaces",
		meta = (ToolTip = "下落循环序列。默认 Fall_Loop。"))
	TObjectPtr<UAnimSequence> FallSequence;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phoebe|Locomotion|BlendSpaces",
		meta = (ToolTip = "下落攻击循环序列。默认 AirAttack_Loop。"))
	TObjectPtr<UAnimSequence> AirAttackSequence;

protected:
	void UpdateTrajectory(float DeltaSeconds);
	void UpdateGait();
	void SelectActiveDatabase();
	UPoseSearchDatabase* SelectFallbackDatabase() const;

	float DesiredControllerYawLastUpdate = 0.f;
	float JumpAirGraceRemaining = 0.f;
	TObjectPtr<UPoseSearchDatabase> LastSearchedDatabase;
};
