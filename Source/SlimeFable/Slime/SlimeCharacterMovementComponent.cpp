// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeCharacterMovementComponent.h"

#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/ScopedMovementUpdate.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarSlimeClingDebug(
	TEXT("slime.Cling.Debug"),
	0,
	TEXT("0=off, 1=state logs, 2=state logs and movement debug drawing."),
	ECVF_Default);

namespace SlimeMovementPolicies
{
	float CapsuleSupportDistance(float Radius, float HalfHeight, const FVector& SurfaceNormal)
	{
		const FVector Normal = SurfaceNormal.GetSafeNormal();
		return Radius + FMath::Max(HalfHeight - Radius, 0.f) * FMath::Abs(float(Normal.Z));
	}

	FVector TransportTangent(
		const FVector& Tangent,
		const FVector& OldSurfaceNormal,
		const FVector& NewSurfaceNormal)
	{
		const FVector OldNormal = OldSurfaceNormal.GetSafeNormal();
		const FVector NewNormal = NewSurfaceNormal.GetSafeNormal();
		if (OldNormal.IsNearlyZero() || NewNormal.IsNearlyZero())
		{
			return FVector::VectorPlaneProject(Tangent, NewNormal).GetSafeNormal();
		}

		const FQuat Rotation = FQuat::FindBetweenNormals(OldNormal, NewNormal);
		FVector Result = FVector::VectorPlaneProject(Rotation.RotateVector(Tangent), NewNormal);
		if (Result.IsNearlyZero())
		{
			Result = FVector::VectorPlaneProject(Tangent, NewNormal);
		}
		return Result.GetSafeNormal();
	}

	bool CanGrabCeiling(float VerticalSpeed, const FVector& SurfaceNormal, float MinUpSpeed)
	{
		return VerticalSpeed >= MinUpSpeed && SurfaceNormal.GetSafeNormal().Z <= -0.25f;
	}

	FVector ClingJumpDirection(const FVector& SurfaceNormal)
	{
		const FVector Normal = SurfaceNormal.GetSafeNormal();
		const FVector Combined = Normal + FVector::UpVector;
		if (!Combined.IsNearlyZero())
		{
			return Combined.GetSafeNormal();
		}
		return Normal.IsNearlyZero() ? FVector::UpVector : Normal;
	}

	float DisplacementBudget(float Speed, float DeltaTime, float CapsuleRadius)
	{
		const float ContactCorrection = FMath::Min(8.f, FMath::Max(CapsuleRadius, 0.f) * 0.25f);
		return FMath::Max(Speed, 0.f) * FMath::Max(DeltaTime, 0.f) + ContactCorrection;
	}

	bool IsWithinDisplacementBudget(float Distance, float Budget)
	{
		return Distance <= Budget + KINDA_SMALL_NUMBER;
	}

	void BuildSurfaceTransitionPath(
		const FVector& StartCenter,
		const FVector& TargetCenter,
		const FVector& OldSurfaceNormal,
		const FVector& NewSurfaceNormal,
		float MaxSegmentLength,
		float MaxNormalAngleDegrees,
		TArray<FVector>& OutPoints)
	{
		OutPoints.Reset();
		const float SegmentLength = FMath::Max(MaxSegmentLength, 0.5f);
		const FVector StartNormal = OldSurfaceNormal.GetSafeNormal();
		const FVector EndNormal = NewSurfaceNormal.GetSafeNormal();
		const float NormalAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
			float(StartNormal | EndNormal), -1.f, 1.f)));
		const FVector Midpoint = (StartCenter + TargetCenter) * 0.5;
		const FVector ClearanceDirection = (StartNormal + EndNormal).GetSafeNormal();
		const float DirectDistance = float(FVector::Distance(StartCenter, TargetCenter));
		const FVector Control = Midpoint + ClearanceDirection * FMath::Min(DirectDistance * 0.25f, 12.f);
		const float MaxControlLeg = FMath::Max(
			float(FVector::Distance(StartCenter, Control)),
			float(FVector::Distance(Control, TargetCenter)));
		const int32 LengthSegments = FMath::CeilToInt((2.f * MaxControlLeg) / SegmentLength);
		const int32 AngleSegments = FMath::CeilToInt(
			NormalAngle / FMath::Max(MaxNormalAngleDegrees, 1.f));
		const int32 SegmentCount = FMath::Max(1, FMath::Max(LengthSegments, AngleSegments));

		OutPoints.Reserve(SegmentCount);
		for (int32 Index = 1; Index <= SegmentCount; ++Index)
		{
			const double T = double(Index) / double(SegmentCount);
			const double InvT = 1.0 - T;
			OutPoints.Add(StartCenter * (InvT * InvT) + Control * (2.0 * InvT * T) + TargetCenter * (T * T));
		}
	}
}

USlimeCharacterMovementComponent::USlimeCharacterMovementComponent()
{
	bHasPendingCapsuleResize = false;
	bClimbMoveBlocked = false;
	bSurfaceTransitionFailed = false;
	BlockedSlideRemaining = 0.f;
}

bool USlimeCharacterMovementComponent::RequestClimbStart(
	const FSlimeSurfaceContact& Contact,
	const FVector& PreferredForward)
{
	if (!Contact.IsValid())
	{
		return false;
	}

	SurfaceContact = Contact;
	SurfaceContact.Normal.Normalize();
	RebuildSurfaceFrame(PreferredForward, false);
	Velocity = FVector::ZeroVector;
	bClimbMoveBlocked = false;
	bSurfaceTransitionFailed = false;
	BlockedSlideRemaining = 0.f;
	TransitionPoints.Reset();
	TransitionPointIndex = 0;
	SetMovementMode(MOVE_Custom, static_cast<uint8>(ESlimeCustomMovementMode::Climbing));

	if (CVarSlimeClingDebug.GetValueOnGameThread() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("SlimeCling start point=%s normal=%s"),
			*SurfaceContact.Point.ToCompactString(), *SurfaceContact.Normal.ToCompactString());
	}
	return true;
}

void USlimeCharacterMovementComponent::RequestClimbStop(bool bWalking)
{
	if (!IsSlimeClimbing() && !IsSlimeMantling())
	{
		return;
	}

	bClimbMoveBlocked = false;
	bSurfaceTransitionFailed = false;
	TransitionPoints.Reset();
	TransitionPointIndex = 0;
	ClimbInputRight = 0.f;
	ClimbInputForward = 0.f;
	SetMovementMode(bWalking ? MOVE_Walking : MOVE_Falling);
}

void USlimeCharacterMovementComponent::UpdateClimbContact(
	const FSlimeSurfaceContact& Contact,
	bool bTransportFrame)
{
	if (!Contact.IsValid())
	{
		return;
	}

	const FVector OldForward = SurfaceForward;
	const FVector OldNormal = SurfaceContact.Normal;
	SurfaceContact = Contact;
	SurfaceContact.Normal.Normalize();
	if (bTransportFrame && !OldNormal.IsNearlyZero())
	{
		SurfaceForward = SlimeMovementPolicies::TransportTangent(
			OldForward, OldNormal, SurfaceContact.Normal);
		SurfaceRight = FVector::CrossProduct(SurfaceContact.Normal, SurfaceForward).GetSafeNormal();
	}
	else
	{
		RebuildSurfaceFrame(OldForward, false);
	}
}

void USlimeCharacterMovementComponent::SetClimbInput(
	float Right,
	float Forward,
	float InClimbSpeed,
	float InSlideSpeed)
{
	ClimbInputRight = FMath::Clamp(Right, -1.f, 1.f);
	ClimbInputForward = FMath::Clamp(Forward, -1.f, 1.f);
	ClimbSpeed = FMath::Max(InClimbSpeed, 0.f);
	SlideSpeed = FMath::Max(InSlideSpeed, 0.f);
}

bool USlimeCharacterMovementComponent::RequestMantle(
	const FVector& ClearancePoint,
	const FVector& TargetPoint,
	float InMantleSpeed)
{
	if (!IsSlimeClimbing() || !CanFollowMantlePath(ClearancePoint, TargetPoint))
	{
		return false;
	}

	MantlePoints[0] = ClearancePoint;
	MantlePoints[1] = TargetPoint;
	MantlePointIndex = 0;
	MantleSpeed = FMath::Max(InMantleSpeed, 1.f);
	Velocity = FVector::ZeroVector;
	bClimbMoveBlocked = false;
	SetMovementMode(MOVE_Custom, static_cast<uint8>(ESlimeCustomMovementMode::Mantling));
	return true;
}

bool USlimeCharacterMovementComponent::RequestSurfaceTransition(
	const FSlimeSurfaceContact& Contact,
	float InTransitionSpeed)
{
	UCapsuleComponent* Capsule = GetOwnerCapsule();
	if (!IsSlimeClimbing() || IsSurfaceTransitioning() || !Contact.IsValid()
		|| !Capsule || !UpdatedComponent)
	{
		return false;
	}

	FSlimeSurfaceContact TargetContact = Contact;
	TargetContact.Normal.Normalize();
	const float Support = SlimeMovementPolicies::CapsuleSupportDistance(
		Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight(), TargetContact.Normal);
	const FVector TargetCenter = TargetContact.Point + TargetContact.Normal * (Support + TargetContact.Skin);
	TArray<FVector> CandidatePoints;
	SlimeMovementPolicies::BuildSurfaceTransitionPath(
		UpdatedComponent->GetComponentLocation(), TargetCenter,
		SurfaceContact.Normal, TargetContact.Normal, 4.f, 10.f, CandidatePoints);
	if (CandidatePoints.IsEmpty() || !CanFollowSurfaceTransition(CandidatePoints))
	{
		if (CVarSlimeClingDebug.GetValueOnGameThread() > 0)
		{
			UE_LOG(LogTemp, Verbose, TEXT("SlimeCling reject surface transition: blocked capsule path"));
		}
		return false;
	}

	TransitionSourceContact = SurfaceContact;
	TransitionSourceForward = SurfaceForward;
	TransitionSourceRight = SurfaceRight;
	TransitionPoints = MoveTemp(CandidatePoints);
	TransitionPointIndex = 0;
	TransitionSpeed = FMath::Max(InTransitionSpeed, 1.f);
	UpdateClimbContact(TargetContact, true);
	bClimbMoveBlocked = false;
	bSurfaceTransitionFailed = false;
	return true;
}

bool USlimeCharacterMovementComponent::ConsumeSurfaceTransitionFailure()
{
	const bool bFailed = bSurfaceTransitionFailed != 0;
	bSurfaceTransitionFailed = false;
	return bFailed;
}

void USlimeCharacterMovementComponent::RequestCapsuleResize(float Radius, float HalfHeight)
{
	PendingCapsuleRadius = FMath::Max(Radius, 1.f);
	PendingCapsuleHalfHeight = FMath::Max(HalfHeight, PendingCapsuleRadius);
	bHasPendingCapsuleResize = true;
}

void USlimeCharacterMovementComponent::QueueExternalCorrection(const FVector& Correction)
{
	PendingExternalCorrection += Correction;
	const float MaxCorrection = GetOwnerCapsule()
		? FMath::Min(8.f, GetOwnerCapsule()->GetScaledCapsuleRadius() * 0.25f)
		: 8.f;
	PendingExternalCorrection = PendingExternalCorrection.GetClampedToMaxSize(MaxCorrection);
}

bool USlimeCharacterMovementComponent::IsSlimeClimbing() const
{
	return MovementMode == MOVE_Custom
		&& CustomMovementMode == static_cast<uint8>(ESlimeCustomMovementMode::Climbing);
}

bool USlimeCharacterMovementComponent::IsSlimeMantling() const
{
	return MovementMode == MOVE_Custom
		&& CustomMovementMode == static_cast<uint8>(ESlimeCustomMovementMode::Mantling);
}

void USlimeCharacterMovementComponent::PerformMovement(float DeltaTime)
{
	ApplyPendingCapsuleResize();
	ApplyPendingExternalCorrection();
	Super::PerformMovement(DeltaTime);
}

void USlimeCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	switch (static_cast<ESlimeCustomMovementMode>(CustomMovementMode))
	{
	case ESlimeCustomMovementMode::Climbing:
		PhysClimbing(DeltaTime, Iterations);
		return;
	case ESlimeCustomMovementMode::Mantling:
		PhysMantling(DeltaTime, Iterations);
		return;
	default:
		Super::PhysCustom(DeltaTime, Iterations);
		return;
	}
}

void USlimeCharacterMovementComponent::HandleImpact(
	const FHitResult& Hit,
	float TimeSlice,
	const FVector& MoveDelta)
{
	if (IsSlimeClimbing() && IsClingableImpact(Hit))
	{
		FSlimeSurfaceContact Contact;
		Contact.Point = Hit.ImpactPoint;
		Contact.Normal = Hit.ImpactNormal;
		Contact.Component = Hit.GetComponent();
		Contact.Skin = SurfaceContact.Skin;
		UpdateClimbContact(Contact, true);
	}
	Super::HandleImpact(Hit, TimeSlice, MoveDelta);
}

void USlimeCharacterMovementComponent::PhysClimbing(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME || !UpdatedComponent || !SurfaceContact.IsValid())
	{
		return;
	}

	if (IsSurfaceTransitioning())
	{
		UCapsuleComponent* Capsule = GetOwnerCapsule();
		const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 32.f;
		const FVector TickStart = UpdatedComponent->GetComponentLocation();
		float RemainingDistance = TransitionSpeed * DeltaTime;
		FScopedMovementUpdate ScopedMove(UpdatedComponent, EScopedUpdate::DeferredUpdates);
		while (RemainingDistance > KINDA_SMALL_NUMBER && IsSurfaceTransitioning())
		{
			const FVector ToPoint = TransitionPoints[TransitionPointIndex]
				- UpdatedComponent->GetComponentLocation();
			const float Distance = float(ToPoint.Size());
			if (Distance <= 0.1f)
			{
				++TransitionPointIndex;
				continue;
			}
			const FVector Delta = ToPoint.GetClampedToMaxSize(RemainingDistance);
			FHitResult Hit;
			SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
			if (Hit.IsValidBlockingHit() || IsFinalCapsuleOverlapping())
			{
				ScopedMove.RevertMove();
				SurfaceContact = TransitionSourceContact;
				SurfaceForward = TransitionSourceForward;
				SurfaceRight = TransitionSourceRight;
				TransitionPoints.Reset();
				TransitionPointIndex = 0;
				bClimbMoveBlocked = true;
				bSurfaceTransitionFailed = true;
				Velocity = FVector::ZeroVector;
				if (CVarSlimeClingDebug.GetValueOnGameThread() > 0)
				{
					UE_LOG(LogTemp, Verbose, TEXT("SlimeCling surface transition stopped: dynamic obstruction"));
				}
				return;
			}
			RemainingDistance -= FMath::Min(RemainingDistance, Distance);
			if (Distance <= Delta.Size() + 0.1f)
			{
				++TransitionPointIndex;
			}
		}

		const float Travel = float(FVector::Distance(TickStart, UpdatedComponent->GetComponentLocation()));
		const float Budget = SlimeMovementPolicies::DisplacementBudget(TransitionSpeed, DeltaTime, Radius);
		if (!SlimeMovementPolicies::IsWithinDisplacementBudget(Travel, Budget))
		{
			ScopedMove.RevertMove();
			SurfaceContact = TransitionSourceContact;
			SurfaceForward = TransitionSourceForward;
			SurfaceRight = TransitionSourceRight;
			TransitionPoints.Reset();
			TransitionPointIndex = 0;
			bClimbMoveBlocked = true;
			bSurfaceTransitionFailed = true;
			Velocity = FVector::ZeroVector;
			return;
		}
		Velocity = (UpdatedComponent->GetComponentLocation() - TickStart)
			/ FMath::Max(DeltaTime, MIN_TICK_TIME);
		bClimbMoveBlocked = false;
		if (!IsSurfaceTransitioning())
		{
			TransitionPoints.Reset();
			TransitionPointIndex = 0;
		}
		return;
	}

	BlockedSlideRemaining = FMath::Max(BlockedSlideRemaining - DeltaTime, 0.f);
	const bool bHasInput = !FMath::IsNearlyZero(ClimbInputRight)
		|| !FMath::IsNearlyZero(ClimbInputForward);
	FVector DesiredVelocity = FVector::ZeroVector;
	if (BlockedSlideRemaining > 0.f)
	{
		DesiredVelocity = -SurfaceForward * SlideSpeed;
	}
	else
	{
		DesiredVelocity = SurfaceRight * (ClimbInputRight * ClimbSpeed);
		DesiredVelocity += SurfaceForward * (bHasInput ? ClimbInputForward * ClimbSpeed : -SlideSpeed);
	}
	DesiredVelocity = FVector::VectorPlaneProject(DesiredVelocity, SurfaceContact.Normal);
	Velocity = DesiredVelocity;

	UCapsuleComponent* Capsule = GetOwnerCapsule();
	const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 32.f;
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : Radius;
	const float DesiredDepth = SlimeMovementPolicies::CapsuleSupportDistance(
		Radius, HalfHeight, SurfaceContact.Normal) + SurfaceContact.Skin;
	const float CurrentDepth = float((UpdatedComponent->GetComponentLocation() - SurfaceContact.Point)
		| SurfaceContact.Normal);
	const float MaxCorrection = FMath::Min(8.f, Radius * 0.25f);
	const FVector ContactCorrection = SurfaceContact.Normal
		* FMath::Clamp(DesiredDepth - CurrentDepth, -MaxCorrection, MaxCorrection);
	const FVector Delta = DesiredVelocity * DeltaTime + ContactCorrection;
	const float Budget = SlimeMovementPolicies::DisplacementBudget(
		FMath::Max(ClimbSpeed, SlideSpeed), DeltaTime, Radius);

	FScopedMovementUpdate ScopedMove(UpdatedComponent, EScopedUpdate::DeferredUpdates);
	const FVector Start = UpdatedComponent->GetComponentLocation();
	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, Delta);
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
	}

	const float Travel = float(FVector::Dist(Start, UpdatedComponent->GetComponentLocation()));
	bClimbMoveBlocked = !SlimeMovementPolicies::IsWithinDisplacementBudget(Travel, Budget)
		|| IsFinalCapsuleOverlapping();
	if (bClimbMoveBlocked)
	{
		ScopedMove.RevertMove();
		Velocity = FVector::ZeroVector;
		BlockedSlideRemaining = FMath::Max(BlockedSlideRemaining, 0.1f);
	}
	if (CVarSlimeClingDebug.GetValueOnGameThread() > 0)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("SlimeCling move source=climb start=%s end=%s mode=%d normal=%s penetrating=%d blocked=%d"),
			*Start.ToCompactString(), *UpdatedComponent->GetComponentLocation().ToCompactString(),
			int32(CustomMovementMode), *SurfaceContact.Normal.ToCompactString(),
			Hit.bStartPenetrating ? 1 : 0, bClimbMoveBlocked ? 1 : 0);
	}

	if (CVarSlimeClingDebug.GetValueOnGameThread() >= 2 && GetWorld())
	{
		DrawDebugLine(GetWorld(), Start, Start + Delta,
			bClimbMoveBlocked ? FColor::Red : FColor::Green, false, 0.f, 0, 1.5f);
		DrawDebugDirectionalArrow(GetWorld(), SurfaceContact.Point,
			SurfaceContact.Point + SurfaceContact.Normal * 30.f, 8.f, FColor::Cyan, false, 0.f, 0, 1.5f);
	}
}

void USlimeCharacterMovementComponent::PhysMantling(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME || !UpdatedComponent)
	{
		return;
	}

	while (MantlePointIndex < 2)
	{
		const FVector ToTarget = MantlePoints[MantlePointIndex] - UpdatedComponent->GetComponentLocation();
		if (ToTarget.SizeSquared() <= FMath::Square(1.f))
		{
			++MantlePointIndex;
			continue;
		}

		const FVector Delta = ToTarget.GetClampedToMaxSize(MantleSpeed * DeltaTime);
		FScopedMovementUpdate ScopedMove(UpdatedComponent, EScopedUpdate::DeferredUpdates);
		FHitResult Hit;
		SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
		bClimbMoveBlocked = Hit.IsValidBlockingHit() || IsFinalCapsuleOverlapping();
		if (bClimbMoveBlocked)
		{
			ScopedMove.RevertMove();
			Velocity = FVector::ZeroVector;
			SetMovementMode(MOVE_Custom, static_cast<uint8>(ESlimeCustomMovementMode::Climbing));
			return;
		}

		Velocity = Delta / FMath::Max(DeltaTime, MIN_TICK_TIME);
		return;
	}

	Velocity = FVector::ZeroVector;
	bClimbMoveBlocked = false;
	SetMovementMode(MOVE_Walking);
}

void USlimeCharacterMovementComponent::RebuildSurfaceFrame(
	const FVector& PreferredForward,
	bool bTransportFrame)
{
	const FVector Normal = SurfaceContact.Normal.GetSafeNormal();
	FVector Forward = FVector::VectorPlaneProject(PreferredForward, Normal).GetSafeNormal();

	if (Forward.IsNearlyZero())
	{
		Forward = FVector::VectorPlaneProject(FVector::UpVector, Normal).GetSafeNormal();
	}
	if (Forward.IsNearlyZero() && CharacterOwner)
	{
		Forward = FVector::VectorPlaneProject(CharacterOwner->GetActorForwardVector(), Normal).GetSafeNormal();
	}
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::VectorPlaneProject(FVector::ForwardVector, Normal).GetSafeNormal();
	}

	SurfaceForward = Forward;
	SurfaceRight = FVector::CrossProduct(Normal, SurfaceForward).GetSafeNormal();
}

void USlimeCharacterMovementComponent::ApplyPendingCapsuleResize()
{
	if (!bHasPendingCapsuleResize || !UpdatedComponent || !CharacterOwner)
	{
		return;
	}
	bHasPendingCapsuleResize = false;

	UCapsuleComponent* Capsule = GetOwnerCapsule();
	if (!Capsule)
	{
		return;
	}

	const float OldRadius = Capsule->GetUnscaledCapsuleRadius();
	const float OldHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	FVector TargetLocation = OldLocation + FVector::UpVector * (PendingCapsuleHalfHeight - OldHalfHeight);
	if (IsSlimeClimbing() && SurfaceContact.IsValid())
	{
		const float Support = SlimeMovementPolicies::CapsuleSupportDistance(
			PendingCapsuleRadius, PendingCapsuleHalfHeight, SurfaceContact.Normal);
		TargetLocation = SurfaceContact.Point + SurfaceContact.Normal * (Support + SurfaceContact.Skin);
	}

	const FCollisionShape TargetShape = FCollisionShape::MakeCapsule(
		PendingCapsuleRadius, PendingCapsuleHalfHeight);
	if (OverlapTest(TargetLocation, UpdatedComponent->GetComponentQuat(), UpdatedComponent->GetCollisionObjectType(),
		TargetShape, CharacterOwner))
	{
		return;
	}

	FScopedMovementUpdate ScopedMove(UpdatedComponent, EScopedUpdate::DeferredUpdates);
	const bool bShrinking = PendingCapsuleRadius <= OldRadius
		&& PendingCapsuleHalfHeight <= OldHalfHeight;
	if (bShrinking)
	{
		Capsule->SetCapsuleSize(PendingCapsuleRadius, PendingCapsuleHalfHeight, true);
	}
	FHitResult Hit;
	SafeMoveUpdatedComponent(TargetLocation - OldLocation, UpdatedComponent->GetComponentQuat(), true, Hit);
	if (Hit.IsValidBlockingHit())
	{
		if (bShrinking)
		{
			Capsule->SetCapsuleSize(OldRadius, OldHalfHeight, true);
		}
		ScopedMove.RevertMove();
		return;
	}

	if (!bShrinking)
	{
		Capsule->SetCapsuleSize(PendingCapsuleRadius, PendingCapsuleHalfHeight, true);
	}
	if (IsFinalCapsuleOverlapping())
	{
		Capsule->SetCapsuleSize(OldRadius, OldHalfHeight, true);
		ScopedMove.RevertMove();
	}
}

void USlimeCharacterMovementComponent::ApplyPendingExternalCorrection()
{
	if (PendingExternalCorrection.IsNearlyZero() || !UpdatedComponent)
	{
		return;
	}

	const FVector Correction = PendingExternalCorrection;
	PendingExternalCorrection = FVector::ZeroVector;
	FScopedMovementUpdate ScopedMove(UpdatedComponent, EScopedUpdate::DeferredUpdates);
	FHitResult Hit;
	SafeMoveUpdatedComponent(Correction, UpdatedComponent->GetComponentQuat(), true, Hit);
	if (Hit.bStartPenetrating || IsFinalCapsuleOverlapping())
	{
		ScopedMove.RevertMove();
	}
}

bool USlimeCharacterMovementComponent::IsClingableImpact(const FHitResult& Hit) const
{
	return Hit.bBlockingHit && Hit.GetActor() != CharacterOwner
		&& !Cast<APawn>(Hit.GetActor()) && !IsWalkable(Hit);
}

bool USlimeCharacterMovementComponent::IsFinalCapsuleOverlapping() const
{
	const UCapsuleComponent* Capsule = GetOwnerCapsule();
	return Capsule && UpdatedComponent && CharacterOwner
		&& OverlapTest(UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat(),
			UpdatedComponent->GetCollisionObjectType(), Capsule->GetCollisionShape(-0.5f), CharacterOwner);
}

bool USlimeCharacterMovementComponent::CanFollowMantlePath(
	const FVector& ClearancePoint,
	const FVector& TargetPoint) const
{
	const UCapsuleComponent* Capsule = GetOwnerCapsule();
	UWorld* World = GetWorld();
	if (!Capsule || !UpdatedComponent || !World || !CharacterOwner)
	{
		return false;
	}

	const float Radius = FMath::Max(Capsule->GetScaledCapsuleRadius() - 2.f, 1.f);
	const float HalfHeight = FMath::Max(Capsule->GetScaledCapsuleHalfHeight() - 2.f, Radius);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	FCollisionQueryParams Query(TEXT("SlimeMantlePath"), false, CharacterOwner);
	const FVector Points[] = { UpdatedComponent->GetComponentLocation(), ClearancePoint, TargetPoint };
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, Points[Index], Points[Index + 1],
			UpdatedComponent->GetComponentQuat(), UpdatedComponent->GetCollisionObjectType(), Shape, Query))
		{
			return false;
		}
	}
	return true;
}

bool USlimeCharacterMovementComponent::CanFollowSurfaceTransition(const TArray<FVector>& Points) const
{
	const UCapsuleComponent* Capsule = GetOwnerCapsule();
	UWorld* World = GetWorld();
	if (!Capsule || !UpdatedComponent || !World || !CharacterOwner || Points.IsEmpty())
	{
		return false;
	}

	const FCollisionShape Shape = Capsule->GetCollisionShape(-0.5f);
	FCollisionQueryParams Query(TEXT("SlimeSurfaceTransition"), false, CharacterOwner);
	FVector Start = UpdatedComponent->GetComponentLocation();
	for (const FVector& End : Points)
	{
		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, Start, End, UpdatedComponent->GetComponentQuat(),
			ECC_Pawn, Shape, Query))
		{
			return false;
		}
		Start = End;
	}
	return true;
}

UCapsuleComponent* USlimeCharacterMovementComponent::GetOwnerCapsule() const
{
	return CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
}
