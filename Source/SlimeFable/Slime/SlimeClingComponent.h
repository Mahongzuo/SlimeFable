// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeClingComponent.generated.h"

class ACharacter;
class UCapsuleComponent;
class USlimeBodyComponent;
class USlimeCharacterMovementComponent;

/**
 *  Wall cling / slow climb for the slime pawn.
 *
 *  Gameplay capsule movement is owned by USlimeCharacterMovementComponent custom modes.
 *  This component owns surface probes and decisions; visual deformation stays in USlimeBodyComponent.
 */
UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API USlimeClingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeClingComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Called from ASlimeCharacter::Tick before CMC so move input is already latched. */
	void UpdateCling(float DeltaTime);

	UFUNCTION(BlueprintPure, Category = "Slime|Cling")
	bool IsClinging() const { return bClinging; }

	UFUNCTION(BlueprintPure, Category = "Slime|Cling")
	FVector GetWallNormal() const { return WallNormal; }

	UFUNCTION(BlueprintPure, Category = "Slime|Cling")
	FVector GetSurfaceNormal() const { return WallNormal; }

	UFUNCTION(BlueprintPure, Category = "Slime|Cling")
	FVector GetWallPoint() const { return WallPoint; }

	UFUNCTION(BlueprintPure, Category = "Slime|Cling")
	FVector GetSurfacePoint() const { return WallPoint; }

	UFUNCTION(BlueprintPure, Category = "Slime|Cling")
	float GetWallTopZ() const { return WallTopZ; }

	/** Camera-relative WASD while stuck: W/S climb, A/D strafe. */
	void SetClingMoveInput(float Right, float Forward);

	/** T while stuck: let go and fall. Returns true if it consumed the key. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Cling")
	bool TryDetach();

	/** Space while stuck: hop off the wall. Returns true if it consumed the jump. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Cling")
	bool TryWallJump();

	/** Climb along the wall, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb", meta = (ClampMin = "20.0", ToolTip = "贴附任意表面时的移动速度，默认 110 cm/s；墙面、斜悬面和屋檐底面共用。"))
	float ClimbSpeed = 110.f;

	/** Downward slide with no input, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb", meta = (ClampMin = "10.0", ToolTip = "没有移动输入时沿当前表面坐标向后滑动的速度，默认 65 cm/s。"))
	float SlideSpeed = 65.f;

	/** Extra gap between capsule surface and the wall, cm. Keeps the body outside geometry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb", meta = (ClampMin = "1.0", ToolTip = "胶囊与攀爬表面之间保留的碰撞皮肤距离，默认 4 cm。"))
	float ClingSkin = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb", meta = (ClampMin = "20.0", ToolTip = "通过完整胶囊路径校验后，自动登顶沿两段路径移动的速度，默认 220 cm/s。"))
	float MantleSpeed = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb", meta = (ClampMin = "0.0", ToolTip = "从空中向上撞到屋檐底面时允许吸附的最小上升速度，默认 30 cm/s。"))
	float CeilingGrabMinUpSpeed = 30.f;

	/** |ImpactNormal.Z| below this counts as a vertical wall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb", meta = (ClampMin = "0.05", ClampMax = "0.7", ToolTip = "用于区分近似竖墙与斜悬面的法线 Z 阈值。"))
	float WallNormalZMax = 0.35f;

	/** How far the capsule probes for a wall, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb", meta = (ClampMin = "8.0", ToolTip = "维持贴附和搜索相邻攀爬面的探测距离。"))
	float WallProbeDistance = 28.f;

	/** Mount the ledge when the capsule top is this close to WallTopZ, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb", meta = (ClampMin = "0.0", ToolTip = "胶囊顶部距离墙顶小于此值时允许开始屋檐过渡或登顶。"))
	float LedgeGrabSlack = 10.f;

	/** Seconds after detach/jump before a wall can be grabbed again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb", meta = (ClampMin = "0.0", ToolTip = "主动脱离后禁止立即重新吸附的时间。"))
	float DetachCooldown = 0.35f;

	/** Launch speed along (WallNormal + Up), cm/s. Vertical walls leave at ~45°. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb", meta = (ClampMin = "100.0", ToolTip = "墙跳沿表面外法线与世界向上的合成方向发射速度。"))
	float WallJumpSpeed = 700.f;

	/** Seconds of lost wall contact before falling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb", meta = (ClampMin = "0.0", ToolTip = "暂时失去接触后仍保持攀爬状态的宽限时间，默认 0.1 秒。"))
	float LostContactGrace = 0.1f;

	/** New hit Normal·WallNormal below this is treated as a crevice / inner window face. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "相邻表面法线连续性阈值，低于该值时按转角候选处理。"))
	float ClingAcceptNormalDot = 0.5f;

	/** COM must pass this fraction of capsule radius past the lip before dropping onto the wall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb", meta = (ClampMin = "0.15", ClampMax = "1.0", ToolTip = "从平台边缘下落吸附时，质心越过边缘所需的胶囊半径比例。"))
	float LedgeDropOverhangFraction = 0.5f;

	/** Walk-off cling only if the drop past the lip exceeds this height, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb", meta = (ClampMin = "0.0", Units = "cm", ToolTip = "平台边缘下方达到此落差时才自动转为墙面攀爬。"))
	float LedgeDropMinHeight = 200.f;

	/** Short rocks / kerbs: raise step height up to this lip rise, cm. Taller than this is cling, not a step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Climb", meta = (ClampMin = "20.0", Units = "cm", ToolTip = "低于此高度的障碍继续使用行走跨步，不进入攀爬。"))
	float MaxWalkUpHeight = 45.f;

private:
	struct FLedgeOverhang
	{
		bool bValid = false;
		FVector EdgePoint = FVector::ZeroVector;
		FVector Outward = FVector::ForwardVector;
		float Overhang = 0.f;
		float FloorZ = 0.f;
		FHitResult SideWall;
		TWeakObjectPtr<UPrimitiveComponent> FloorComponent;
	};
	enum class EClingExit : uint8
	{
		Falling,
		Walking
	};

	bool IsClingableWall(const FHitResult& Hit) const;
	bool IsAcceptableClingHit(const FHitResult& Hit) const;
	bool HasTallWallFace(const FHitResult& Hit) const;
	bool FindWall(FHitResult& OutHit, bool bIncludeCardinals) const;
	bool MaintainWall(FHitResult& OutHit) const;
	bool WantsEnterCling(const FHitResult& Hit) const;
	float ComputeWallTopZ(const FHitResult& Hit, bool* bOutEave = nullptr) const;
	void EnterCling(const FHitResult& Hit);
	void ExitCling(EClingExit Exit);
	void CommitClingHit(const FHitResult& Hit);
	void ApplyClingVelocity();
	void TryMountLedge();
	bool TryTransferAlongWall(FHitResult& OutHit, bool bLostContact) const;
	void TryLandWhileClinging();
	void PushVisualToBody();
	void RestoreMovementSettings();
	bool TryWalkUpShortObstacle(const FHitResult& Hit);
	void SetWalkStepBoost(float BoostedMaxStep);
	FVector GetWallUp() const;
	FVector GetWallRight() const;

	FVector GetCOM() const;
	float GetBodyRadius() const;
	float GetLedgeDropThreshold() const;
	FVector GetWalkOutward() const;
	bool TraceWalkableFloor(const FVector& Origin, FHitResult& OutHit) const;
	bool ProbeLedgeOverhang(FLedgeOverhang& OutLedge) const;
	bool ProbeSideWall(const FVector& EdgePoint, const FVector& Outward, FHitResult& OutHit) const;
	bool IsStandingOnLedgeTop() const;
	bool IsLipWallWhileOnTop(const FHitResult& Hit) const;
	void NudgeAlongLedge(const FLedgeOverhang& Ledge, float DeltaTime);
	void EnterClingFromLedgeDrop(const FHitResult& SideWall);
	void UpdateLedgeDropMountLock();
	bool TryLedgeWalkOff(float DeltaTime);
	bool IsLedgeTallEnough(const FLedgeOverhang& Ledge) const;
	bool TryTransferToOverhang();
	USlimeCharacterMovementComponent* GetSlimeMovement() const;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UCapsuleComponent> OwnerCapsule;

	UPROPERTY(Transient)
	TObjectPtr<USlimeBodyComponent> Body;

	UPROPERTY(Transient)
	TWeakObjectPtr<UPrimitiveComponent> WallComponent;

	FVector WallNormal = FVector::ForwardVector;
	FVector WallPoint = FVector::ZeroVector;
	float WallTopZ = -1.e9f;

	float MoveRight = 0.f;
	float MoveForward = 0.f;
	float DetachCooldownRemaining = 0.f;
	float LostContactTime = 0.f;

	float SavedGravityScale = 1.f;
	float SavedMaxAcceleration = 2048.f;
	float SavedMaxFlySpeed = 600.f;
	float SavedBrakingFlying = 0.f;
	uint8 bSavedOrientToMovement : 1;

	uint8 bClinging : 1;
	uint8 bHasMoveInput : 1;
	uint8 bSavedMovement : 1;
	uint8 bLockMountAfterLedgeDrop : 1;
	uint8 bWallTopIsEave : 1;
	uint8 bMantling : 1;
	uint8 bSurfaceTransitionPending : 1;
	FHitResult PendingSurfaceTransitionHit;
};
