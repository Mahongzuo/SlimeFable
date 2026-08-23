// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SlimeCharacterMovementComponent.generated.h"

class UCapsuleComponent;
class UPrimitiveComponent;

UENUM()
enum class ESlimeCustomMovementMode : uint8
{
	Climbing = 0,
	Mantling = 1
};

struct FSlimeSurfaceContact
{
	FVector Point = FVector::ZeroVector;
	FVector Normal = FVector::ForwardVector;
	TWeakObjectPtr<UPrimitiveComponent> Component;
	float Skin = 4.f;

	bool IsValid() const { return !Normal.IsNearlyZero(); }
};

namespace SlimeMovementPolicies
{
	SLIMEFABLE_API float CapsuleSupportDistance(float Radius, float HalfHeight, const FVector& SurfaceNormal);
	SLIMEFABLE_API FVector TransportTangent(
		const FVector& Tangent,
		const FVector& OldSurfaceNormal,
		const FVector& NewSurfaceNormal);
	SLIMEFABLE_API bool CanGrabCeiling(float VerticalSpeed, const FVector& SurfaceNormal, float MinUpSpeed);
	SLIMEFABLE_API FVector ClingJumpDirection(const FVector& SurfaceNormal);
	SLIMEFABLE_API float DisplacementBudget(float Speed, float DeltaTime, float CapsuleRadius);
	SLIMEFABLE_API bool IsWithinDisplacementBudget(float Distance, float Budget);
	SLIMEFABLE_API void BuildSurfaceTransitionPath(
		const FVector& StartCenter,
		const FVector& TargetCenter,
		const FVector& OldSurfaceNormal,
		const FVector& NewSurfaceNormal,
		float MaxSegmentLength,
		float MaxNormalAngleDegrees,
		TArray<FVector>& OutPoints);
}

UCLASS()
class SLIMEFABLE_API USlimeCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	USlimeCharacterMovementComponent();

	bool RequestClimbStart(const FSlimeSurfaceContact& Contact, const FVector& PreferredForward);
	void RequestClimbStop(bool bWalking);
	void UpdateClimbContact(const FSlimeSurfaceContact& Contact, bool bTransportFrame = true);
	void SetClimbInput(float Right, float Forward, float InClimbSpeed, float InSlideSpeed);
	bool RequestMantle(const FVector& ClearancePoint, const FVector& TargetPoint, float InMantleSpeed);
	bool RequestSurfaceTransition(const FSlimeSurfaceContact& Contact, float InTransitionSpeed);
	void RequestCapsuleResize(float Radius, float HalfHeight);
	void QueueExternalCorrection(const FVector& Correction);

	bool IsSlimeClimbing() const;
	bool IsSlimeMantling() const;
	const FSlimeSurfaceContact& GetSurfaceContact() const { return SurfaceContact; }
	FVector GetSurfaceForward() const { return SurfaceForward; }
	FVector GetSurfaceRight() const { return SurfaceRight; }
	bool WasClimbMoveBlocked() const { return bClimbMoveBlocked; }
	bool IsSurfaceTransitioning() const { return TransitionPointIndex < TransitionPoints.Num(); }
	bool ConsumeSurfaceTransitionFailure();

protected:
	virtual void PerformMovement(float DeltaTime) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta) override;

private:
	void PhysClimbing(float DeltaTime, int32 Iterations);
	void PhysMantling(float DeltaTime, int32 Iterations);
	void RebuildSurfaceFrame(const FVector& PreferredForward, bool bTransportFrame);
	void ApplyPendingCapsuleResize();
	void ApplyPendingExternalCorrection();
	bool IsClingableImpact(const FHitResult& Hit) const;
	bool IsFinalCapsuleOverlapping() const;
	bool CanFollowMantlePath(const FVector& ClearancePoint, const FVector& TargetPoint) const;
	bool CanFollowSurfaceTransition(const TArray<FVector>& Points) const;
	UCapsuleComponent* GetOwnerCapsule() const;

	FSlimeSurfaceContact SurfaceContact;
	FSlimeSurfaceContact TransitionSourceContact;
	FVector SurfaceForward = FVector::UpVector;
	FVector SurfaceRight = FVector::RightVector;
	FVector TransitionSourceForward = FVector::UpVector;
	FVector TransitionSourceRight = FVector::RightVector;
	float ClimbInputRight = 0.f;
	float ClimbInputForward = 0.f;
	float ClimbSpeed = 110.f;
	float SlideSpeed = 65.f;
	float MantleSpeed = 220.f;
	FVector MantlePoints[2] = { FVector::ZeroVector, FVector::ZeroVector };
	int32 MantlePointIndex = 0;
	TArray<FVector> TransitionPoints;
	int32 TransitionPointIndex = 0;
	float TransitionSpeed = 110.f;
	float BlockedSlideRemaining = 0.f;
	FVector PendingExternalCorrection = FVector::ZeroVector;
	float PendingCapsuleRadius = 0.f;
	float PendingCapsuleHalfHeight = 0.f;
	uint8 bHasPendingCapsuleResize : 1;
	uint8 bClimbMoveBlocked : 1;
	uint8 bSurfaceTransitionFailed : 1;
};
