// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeClingComponent.generated.h"

class ACharacter;
class UCapsuleComponent;
class USlimeBodyComponent;

/**
 *  Wall cling / slow climb for the slime pawn.
 *
 *  Movement stays on stock CharacterMovementComponent (MOVE_Flying while stuck).
 *  Visual hemisphere flattening is forwarded to USlimeBodyComponent, not pancake spread.
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
	FVector GetWallPoint() const { return WallPoint; }

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Cling", meta = (ClampMin = "20.0"))
	float ClimbSpeed = 110.f;

	/** Downward slide with no input, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Cling", meta = (ClampMin = "10.0"))
	float SlideSpeed = 65.f;

	/** Extra gap between capsule surface and the wall, cm. Keeps the body outside geometry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Cling", meta = (ClampMin = "1.0"))
	float ClingSkin = 4.f;

	/** |ImpactNormal.Z| below this counts as a vertical wall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Cling", meta = (ClampMin = "0.05", ClampMax = "0.7"))
	float WallNormalZMax = 0.35f;

	/** How far the capsule probes for a wall, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Cling", meta = (ClampMin = "8.0"))
	float WallProbeDistance = 28.f;

	/** Mount the ledge when the capsule top is this close to WallTopZ, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Cling", meta = (ClampMin = "0.0"))
	float LedgeGrabSlack = 10.f;

	/** Seconds after detach/jump before a wall can be grabbed again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Cling", meta = (ClampMin = "0.0"))
	float DetachCooldown = 0.35f;

	/** Launch speed along (WallNormal + Up), cm/s. Vertical walls leave at ~45°. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Cling", meta = (ClampMin = "100.0"))
	float WallJumpSpeed = 700.f;

	/** Seconds of lost wall contact before falling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Cling", meta = (ClampMin = "0.0"))
	float LostContactGrace = 0.1f;

	/** New hit Normal·WallNormal below this is treated as a crevice / inner window face. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Cling", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ClingAcceptNormalDot = 0.5f;

	/** COM must pass this fraction of capsule radius past the lip before dropping onto the wall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Cling", meta = (ClampMin = "0.15", ClampMax = "1.0"))
	float LedgeDropOverhangFraction = 0.5f;

	/** Walk-off cling only if the drop past the lip exceeds this height, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Cling", meta = (ClampMin = "0.0", Units = "cm"))
	float LedgeDropMinHeight = 200.f;

	/** Short rocks / kerbs: raise step height up to this lip rise, cm. Taller than this is cling, not a step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Cling", meta = (ClampMin = "20.0", Units = "cm"))
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
	void PlaceOnWallSkin();
	void ApplyClingVelocity();
	void TryMountLedge();
	bool TryTransferAlongWall(FHitResult& OutHit, bool bLostContact) const;
	bool IsWallOpenToSide() const;
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
};
