// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PhoebeClimbComponent.generated.h"

class ACharacter;
class UCapsuleComponent;
class UCharacterMovementComponent;
class UAnimMontage;
class USpringArmComponent;

/**
 * Capsule wall-climb for Phoebe (skeletal fighter).
 * Enter only while Falling; never auto-cling from Walking.
 * Exit on lost contact / failed mantle / wall-jump — never leave MOVE_Flying orphaned.
 */
UCLASS(ClassGroup = (Enemy), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API UPhoebeClimbComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPhoebeClimbComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "Phoebe|Climb")
	bool IsClimbing() const { return bClimbing || bMantling; }

	UFUNCTION(BlueprintPure, Category = "Phoebe|Climb")
	bool IsMantling() const { return bMantling; }

	UFUNCTION(BlueprintPure, Category = "Phoebe|Climb")
	bool IsClimbDashing() const { return bClimbing && bClimbDashing; }

	UFUNCTION(BlueprintPure, Category = "Phoebe|Climb")
	float GetClimbYaw() const { return ClimbYaw; }

	UFUNCTION(BlueprintPure, Category = "Phoebe|Climb")
	float GetClimbPitch() const { return ClimbPitch; }

	UFUNCTION(BlueprintPure, Category = "Phoebe|Climb")
	FVector GetWallNormal() const { return WallNormal; }

	void HandleMorphMove(const FVector2D& MoveAxis);
	bool HandleMorphJump();
	/** Leave climb/mantle safely and enter Falling for an aerial attack. */
	bool BeginAirAttackDrop();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb",
		meta = (ClampMin = "50.0", ToolTip = "贴墙切向移动速度（cm/s）。默认 220。"))
	float ClimbSpeed = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb",
		meta = (ClampMin = "1.0", ToolTip = "攀爬加速倍率（右键/Shift）。默认 1.8。"))
	float ClimbDashMul = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb",
		meta = (ClampMin = "20.0", ToolTip = "向前/向墙探测距离（cm）。默认 90。"))
	float AttachProbeDistance = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb",
		meta = (ClampMin = "0.0", ClampMax = "0.5", ToolTip = "墙法线 |Z| 超过此值视为不可爬斜面。默认 0.25。"))
	float MaxWallNormalZ = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb",
		meta = (ClampMin = "1.0", ToolTip = "胶囊表面与墙之间的间隙（cm）。默认 6。"))
	float ClingSkin = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb",
		meta = (ClampMin = "10.0", ToolTip = "顶沿向下扫落脚点的额外高度（cm）。默认 60。"))
	float MantleCheckHeight = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb",
		meta = (ClampMin = "100.0", ToolTip = "墙跳离墙冲量大小。默认 520。"))
	float WallJumpSpeed = 520.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb",
		meta = (ClampMin = "0.0", ToolTip = "离墙后再吸墙冷却（秒）。默认 0.35。"))
	float ReattachCooldown = 0.35f;

	/** Seconds to lerp capsule onto the roof (avoids teleport camera whip). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb",
		meta = (ClampMin = "0.15", Units = "s", ToolTip = "翻上顶沿的插值时长（秒）。默认 0.55。"))
	float MantleLerpSeconds = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb",
		meta = (ClampMin = "20.0", ToolTip = "多距离探顶：墙外最近距离（cm）。默认 40。"))
	float MantleOutNear = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb",
		meta = (ClampMin = "40.0", ToolTip = "多距离探顶：墙外中距（cm），盖住木檐。默认 90。"))
	float MantleOutMid = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb",
		meta = (ClampMin = "60.0", ToolTip = "多距离探顶：墙外最远（cm），盖住草皮屋顶。默认 140。"))
	float MantleOutFar = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb",
		meta = (ToolTip = "勾选后画登顶探测线（PIE）。默认关。"))
	bool bDebugMantleDraw = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb")
	TSoftObjectPtr<UAnimMontage> MantleMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb")
	TSoftObjectPtr<UAnimMontage> ClimbStartMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb")
	TSoftObjectPtr<UAnimMontage> LandMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb",
		meta = (ToolTip = "墙跳离墙时播。默认 AM_Phoebe_Climb_Vault。空则墙跳无 Montage。"))
	TSoftObjectPtr<UAnimMontage> VaultMontage;

protected:
	bool TryFindWall(FHitResult& OutHit) const;
	FVector GetClimbProbeDirection() const;
	bool TryStartClimb();
	void ExitClimb(bool bToWalking);
	void UpdateClimbMotion(float DeltaTime);
	bool TryMantle();
	bool FindMantleStandLocation(FVector& OutStandLoc) const;
	bool CanStandAt(const FVector& CapsuleCenter) const;
	bool ApplyClingCorrection();
	bool CanAutoStartClimb() const;
	void TickMantle(float DeltaTime);
	void FinishMantle();
	void SoftenMorphCamera(bool bSoft);
	void RestoreMovementAfterClimb(bool bToWalking);
	void EnforceNoOrphanFlying();
	void UpdateClimbAnimAxes(const FVector& WallRight, const FVector& WallUp, const FVector& TangentialVelocity, float DeltaTime);
	void UpdateClimbDashHeld(float DeltaTime);
	void StopClimbMontages(float BlendOut);
	/** Lateral climb blocked: reattach to an adjacent climbable face (inner/outer corner). */
	bool TryWrapToAdjacentWall(const FVector& WallRight, float LateralSign);
	bool IsClimbableWallNormal(const FVector& Normal) const;
	bool TraceClimbWall(const FVector& Start, const FVector& End, FHitResult& OutHit) const;
	void DrawMantleDebug(const FVector& Start, const FVector& End, bool bHit) const;

	bool bClimbing = false;
	bool bMantling = false;
	bool bClimbDashing = false;
	FVector WallNormal = FVector::BackwardVector;
	FVector WallPoint = FVector::ZeroVector;
	FVector MantleStartLoc = FVector::ZeroVector;
	FVector PendingStandLoc = FVector::ZeroVector;
	FRotator MantleStartRot = FRotator::ZeroRotator;
	FRotator MantleEndRot = FRotator::ZeroRotator;
	float MantleAlpha = 0.f;
	float ClimbYaw = 0.f;
	float ClimbPitch = 0.f;
	float InputRight = 0.f;
	float InputForward = 0.f;
	float ClimbDashHoldSeconds = 0.f;
	float ClimbDashReleaseSeconds = 0.f;
	float MantleHoldUntilTime = 0.f;
	float CooldownRemaining = 0.f;
	float WrapCooldownRemaining = 0.f;
	float LostContactSeconds = 0.f;
	float FallingSeconds = 0.f;
	float SavedGravityScale = 1.f;
	bool bSavedOrientToMovement = true;

	bool bCachedCameraLag = false;
	float CachedCameraLagSpeed = 0.f;
	float CachedCameraRotLagSpeed = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter = nullptr;
};
