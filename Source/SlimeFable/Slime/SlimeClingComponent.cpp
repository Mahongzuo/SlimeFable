// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeClingComponent.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "SlimeBodyComponent.h"
#include "SlimeCharacterMovementComponent.h"
#include "SlimeVehicleComponent.h"

USlimeClingComponent::USlimeClingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
	bClinging = false;
	bHasMoveInput = false;
	bSavedMovement = false;
	bSavedOrientToMovement = true;
	bLockMountAfterLedgeDrop = false;
	bWallTopIsEave = false;
	bMantling = false;
	bSurfaceTransitionPending = false;
}

void USlimeClingComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (OwnerCharacter)
	{
		OwnerCapsule = OwnerCharacter->GetCapsuleComponent();
	}
	Body = GetOwner() ? GetOwner()->FindComponentByClass<USlimeBodyComponent>() : nullptr;
}

void USlimeClingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bClinging)
	{
		ExitCling(EClingExit::Falling);
	}
	Super::EndPlay(EndPlayReason);
}

void USlimeClingComponent::SetClingMoveInput(float Right, float Forward)
{
	MoveRight = Right;
	MoveForward = Forward;
	bHasMoveInput = !FMath::IsNearlyZero(Right) || !FMath::IsNearlyZero(Forward);
}

bool USlimeClingComponent::TryDetach()
{
	if (!bClinging)
	{
		return false;
	}
	ExitCling(EClingExit::Falling);
	DetachCooldownRemaining = DetachCooldown;
	return true;
}

bool USlimeClingComponent::TryWallJump()
{
	if (!bClinging || !OwnerCharacter)
	{
		return false;
	}

	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	const FVector LaunchDir = SlimeMovementPolicies::ClingJumpDirection(WallNormal);
	ExitCling(EClingExit::Falling);
	if (Movement)
	{
		Movement->Velocity = LaunchDir * WallJumpSpeed;
	}
	// Consume the grounded jump so the next Space is the air jump. Do not StopJumping.
	OwnerCharacter->JumpCurrentCount = 1;
	DetachCooldownRemaining = DetachCooldown;
	return true;
}

void USlimeClingComponent::UpdateCling(float DeltaTime)
{
	DetachCooldownRemaining = FMath::Max(DetachCooldownRemaining - DeltaTime, 0.f);

	if (!OwnerCharacter || !OwnerCapsule)
	{
		return;
	}

	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	if (const USlimeVehicleComponent* Vehicle = OwnerCharacter->FindComponentByClass<USlimeVehicleComponent>())
	{
		if (Vehicle->IsUsingVehicle())
		{
			if (bClinging)
			{
				ExitCling(EClingExit::Falling);
			}
			SetWalkStepBoost(0.f);
			bHasMoveInput = false;
			MoveRight = 0.f;
			MoveForward = 0.f;
			return;
		}
	}

	if (Body && Body->IsSpreading())
	{
		if (bClinging)
		{
			ExitCling(EClingExit::Falling);
		}
		SetWalkStepBoost(0.f);
		bHasMoveInput = false;
		return;
	}

	if (!bClinging && TryLedgeWalkOff(DeltaTime))
	{
		SetWalkStepBoost(0.f);
		bHasMoveInput = false;
		MoveRight = 0.f;
		MoveForward = 0.f;
		return;
	}

	FHitResult WallHit;
	if (bClinging)
	{
		SetWalkStepBoost(0.f);
		if (bMantling)
		{
			USlimeCharacterMovementComponent* SlimeMovement = GetSlimeMovement();
			if (SlimeMovement && SlimeMovement->IsSlimeMantling())
			{
				bHasMoveInput = false;
				MoveRight = 0.f;
				MoveForward = 0.f;
				return;
			}
			bMantling = false;
			if (!SlimeMovement || !SlimeMovement->IsSlimeClimbing())
			{
				ExitCling(EClingExit::Walking);
				bHasMoveInput = false;
				MoveRight = 0.f;
				MoveForward = 0.f;
				return;
			}
			LostContactTime = 0.f;
		}
		if (bSurfaceTransitionPending)
		{
			USlimeCharacterMovementComponent* SlimeMovement = GetSlimeMovement();
			if (SlimeMovement && SlimeMovement->IsSurfaceTransitioning())
			{
				LostContactTime = 0.f;
				bHasMoveInput = false;
				MoveRight = 0.f;
				MoveForward = 0.f;
				return;
			}
			const bool bFailed = !SlimeMovement || SlimeMovement->ConsumeSurfaceTransitionFailure();
			bSurfaceTransitionPending = false;
			if (!bFailed)
			{
				CommitClingHit(PendingSurfaceTransitionHit);
			}
			LostContactTime = 0.f;
		}
		if (USlimeCharacterMovementComponent* SlimeMovement = GetSlimeMovement();
			SlimeMovement && SlimeMovement->IsSurfaceTransitioning())
		{
			LostContactTime = 0.f;
			ApplyClingVelocity();
			PushVisualToBody();
			bHasMoveInput = false;
			MoveRight = 0.f;
			MoveForward = 0.f;
			return;
		}
		if (!MaintainWall(WallHit) || !IsAcceptableClingHit(WallHit))
		{
			if (bHasMoveInput && TryTransferAlongWall(WallHit, true))
			{
				LostContactTime = 0.f;
				CommitClingHit(WallHit);
			}
			else
			{
				LostContactTime += DeltaTime;
				if (USlimeCharacterMovementComponent* SlimeMovement = GetSlimeMovement())
				{
					SlimeMovement->SetClimbInput(0.f, 0.f, ClimbSpeed, 0.f);
					SlimeMovement->Velocity = FVector::ZeroVector;
				}
				else if (UCharacterMovementComponent* ClingMove = OwnerCharacter->GetCharacterMovement())
				{
					ClingMove->Velocity = FVector::ZeroVector;
				}
				if (LostContactTime >= LostContactGrace)
				{
					ExitCling(EClingExit::Falling);
				}
				else
				{
					PushVisualToBody();
				}
				bHasMoveInput = false;
				MoveRight = 0.f;
				MoveForward = 0.f;
				return;
			}
		}
		else if (bHasMoveInput && TryTransferAlongWall(WallHit, false))
		{
			LostContactTime = 0.f;
			CommitClingHit(WallHit);
		}

		LostContactTime = 0.f;
		CommitClingHit(WallHit);
		UpdateLedgeDropMountLock();

		TryLandWhileClinging();
		if (!bClinging)
		{
			bHasMoveInput = false;
			return;
		}

		TryMountLedge();
		if (!bClinging)
		{
			bHasMoveInput = false;
			return;
		}

		ApplyClingVelocity();
		PushVisualToBody();
	}
	else if (DetachCooldownRemaining <= 0.f && FindWall(WallHit, Movement->IsFalling()))
	{
		if (WantsEnterCling(WallHit))
		{
			SetWalkStepBoost(0.f);
			EnterCling(WallHit);
			ApplyClingVelocity();
			PushVisualToBody();
		}
		else
		{
			if (!bHasMoveInput)
			{
				SetWalkStepBoost(0.f);
			}
			else
			{
				FVector HorizVel = Movement->Velocity;
				HorizVel.Z = 0.0;
				if (HorizVel.SizeSquared() < 400.0)
				{
					SetWalkStepBoost(0.f);
				}
				TryWalkUpShortObstacle(WallHit);
			}
		}
	}
	else
	{
		SetWalkStepBoost(0.f);
	}

	bHasMoveInput = false;
	MoveRight = 0.f;
	MoveForward = 0.f;
}

bool USlimeClingComponent::IsClingableWall(const FHitResult& Hit) const
{
	if (!Hit.bBlockingHit)
	{
		return false;
	}
	if (Hit.GetActor() && Hit.GetActor() == GetOwner())
	{
		return false;
	}
	if (Cast<APawn>(Hit.GetActor()))
	{
		return false;
	}
	const UCharacterMovementComponent* Movement = OwnerCharacter
		? OwnerCharacter->GetCharacterMovement()
		: nullptr;
	if (!Movement || Movement->IsWalkable(Hit))
	{
		return false;
	}
	return true;
}

bool USlimeClingComponent::WantsEnterCling(const FHitResult& Hit) const
{
	if (!HasTallWallFace(Hit))
	{
		return false;
	}

	UCharacterMovementComponent* Movement = OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return false;
	}

	if (Movement->IsFalling())
	{
		const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
		if (Normal.Z < -WallNormalZMax)
		{
			return SlimeMovementPolicies::CanGrabCeiling(
				float(Movement->Velocity.Z), Normal, CeilingGrabMinUpSpeed);
		}
		return FMath::Abs(float(Normal.Z)) <= WallNormalZMax;
	}

	if (IsLipWallWhileOnTop(Hit))
	{
		return false;
	}

	const FVector IntoWall = -Hit.ImpactNormal.GetSafeNormal();
	FVector HorizVel = Movement->Velocity;
	HorizVel.Z = 0.0;
	if ((HorizVel | IntoWall) > 30.0)
	{
		return true;
	}

	FVector Input = Movement->GetLastInputVector();
	Input.Z = 0.0;
	if ((Input.GetSafeNormal() | IntoWall) > 0.25)
	{
		return true;
	}
	return false;
}

bool USlimeClingComponent::FindWall(FHitResult& OutHit, bool bIncludeCardinals) const
{
	UWorld* World = GetWorld();
	UCharacterMovementComponent* Movement = OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
	if (!World || !OwnerCapsule || !Movement)
	{
		return false;
	}

	FCollisionQueryParams Query(TEXT("SlimeCling"), false, GetOwner());
	const FVector Loc = OwnerCapsule->GetComponentLocation();
	const float Radius = FMath::Max(OwnerCapsule->GetScaledCapsuleRadius() * 0.9f, 4.f);
	const float HalfH = FMath::Max(OwnerCapsule->GetScaledCapsuleHalfHeight() * 0.85f, Radius);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfH);

	auto TrySweep = [this, World, &Loc, &Shape, &Query, &OutHit](FVector Dir) -> bool
	{
		if (Dir.IsNearlyZero())
		{
			return false;
		}
		const FVector Unit = Dir.GetSafeNormal();
		const FVector Start = Loc - Unit * 6.0;
		const FVector End = Loc + Unit * double(WallProbeDistance);
		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn, Shape, Query)
			&& IsClingableWall(Hit))
		{
			OutHit = Hit;
			return true;
		}
		return false;
	};

	FVector HorizVel = Movement->Velocity;
	if (TrySweep(HorizVel))
	{
		return true;
	}

	FVector Accel = Movement->GetCurrentAcceleration();
	if (TrySweep(Accel))
	{
		return true;
	}

	FVector Input = Movement->GetLastInputVector();
	if (TrySweep(Input))
	{
		return true;
	}

	if (bIncludeCardinals)
	{
		const FVector Cardinals[] = {
			OwnerCharacter->GetActorForwardVector(),
			-OwnerCharacter->GetActorForwardVector(),
			OwnerCharacter->GetActorRightVector(),
			-OwnerCharacter->GetActorRightVector()
		};
		for (const FVector& Dir : Cardinals)
		{
			if (TrySweep(Dir))
			{
				return true;
			}
		}
	}
	return false;
}

bool USlimeClingComponent::HasTallWallFace(const FHitResult& Hit) const
{
	UWorld* World = GetWorld();
	if (!World || !OwnerCapsule || !IsClingableWall(Hit))
	{
		return false;
	}

	const FVector N = Hit.ImpactNormal.GetSafeNormal();
	if (N.IsNearlyZero())
	{
		return false;
	}
	if (FMath::Abs(float(N.Z)) > WallNormalZMax)
	{
		return true;
	}

	const FVector Loc = OwnerCapsule->GetComponentLocation();
	const float Offsets[] = { 40.f, 70.f, 100.f, 130.f };
	FCollisionQueryParams Query(TEXT("SlimeClingTallFace"), false, GetOwner());
	for (const float Dz : Offsets)
	{
		const FVector Sample(Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Loc.Z + double(Dz));
		const FVector Start = Sample + N * 8.0;
		const FVector End = Start - N * double(WallProbeDistance);
		FHitResult FaceHit;
		if (World->LineTraceSingleByChannel(FaceHit, Start, End, ECC_Pawn, Query)
			&& IsClingableWall(FaceHit)
			&& (FaceHit.ImpactNormal.GetSafeNormal() | N) >= 0.5f)
		{
			return true;
		}
	}
	return false;
}

bool USlimeClingComponent::IsAcceptableClingHit(const FHitResult& Hit) const
{
	if (!IsClingableWall(Hit) || !OwnerCapsule)
	{
		return false;
	}

	const FVector NewNormal = Hit.ImpactNormal.GetSafeNormal();
	if ((NewNormal | WallNormal) < ClingAcceptNormalDot)
	{
		return false;
	}

	const FVector Loc = OwnerCapsule->GetComponentLocation();
	const float Radius = OwnerCapsule->GetScaledCapsuleRadius();
	const float HalfHeight = OwnerCapsule->GetScaledCapsuleHalfHeight();
	const float Support = SlimeMovementPolicies::CapsuleSupportDistance(Radius, HalfHeight, WallNormal);
	const float AlongInward = float((Hit.ImpactPoint - Loc) | (-WallNormal));
	if (AlongInward > Support + ClingSkin + 12.f)
	{
		return false;
	}
	return true;
}

bool USlimeClingComponent::MaintainWall(FHitResult& OutHit) const
{
	UWorld* World = GetWorld();
	if (!World || !OwnerCapsule || WallNormal.IsNearlyZero())
	{
		return false;
	}

	FCollisionQueryParams Query(TEXT("SlimeClingMaintain"), false, GetOwner());
	const FVector Loc = OwnerCapsule->GetComponentLocation();
	const float Radius = OwnerCapsule->GetScaledCapsuleRadius();
	const float HalfHeight = OwnerCapsule->GetScaledCapsuleHalfHeight();
	const float Support = SlimeMovementPolicies::CapsuleSupportDistance(Radius, HalfHeight, WallNormal);
	const float Probe = Support + ClingSkin + 12.f;
	const FCollisionShape Shape = FCollisionShape::MakeSphere(4.f);
	const FVector Right = GetWallRight();

	auto TryProbe = [&](const FVector& From, FHitResult& Hit) -> bool
	{
		return World->SweepSingleByChannel(Hit, From, From - WallNormal * double(Probe), FQuat::Identity, ECC_Pawn, Shape, Query)
			&& IsClingableWall(Hit);
	};

	FHitResult Hit;
	const bool bCenterHit = TryProbe(Loc, Hit);
	if (bCenterHit && IsAcceptableClingHit(Hit))
	{
		OutHit = Hit;
		return true;
	}

	FHitResult Fallback = Hit;
	bool bHaveFallback = bCenterHit;

	// Only probe sideways when the center maintain failed or was a bad seam normal.
	const float Lateral[] = { Radius * 0.5f, -Radius * 0.5f };
	for (const float Off : Lateral)
	{
		if (Right.IsNearlyZero())
		{
			break;
		}
		FHitResult SideHit;
		if (!TryProbe(Loc + Right * double(Off), SideHit))
		{
			continue;
		}
		if (IsAcceptableClingHit(SideHit))
		{
			OutHit = SideHit;
			return true;
		}
		Fallback = SideHit;
		bHaveFallback = true;
	}

	if (bHaveFallback && IsClingableWall(Fallback))
	{
		OutHit = Fallback;
		return true;
	}
	return false;
}

float USlimeClingComponent::ComputeWallTopZ(const FHitResult& Hit, bool* bOutEave) const
{
	if (bOutEave)
	{
		*bOutEave = false;
	}

	UWorld* World = GetWorld();
	const FVector Outward = Hit.ImpactNormal.GetSafeNormal();
	const FVector Base = Hit.ImpactPoint + Outward * 6.0;
	constexpr float MaxScan = 400.f;
	if (!World)
	{
		return float(Base.Z) + MaxScan;
	}

	FCollisionQueryParams Query(TEXT("SlimeClingTop"), false, GetOwner());
	const float Step = 8.f;
	for (float Offset = 0.f; Offset <= MaxScan; Offset += Step)
	{
		const FVector Sample(Base.X, Base.Y, Base.Z + double(Offset));
		FHitResult WallHit;
		if (!World->LineTraceSingleByChannel(WallHit, Sample, Sample - Outward * double(WallProbeDistance), ECC_Pawn, Query)
			|| !IsClingableWall(WallHit))
		{
			if (bOutEave && WallHit.bBlockingHit && float(WallHit.ImpactNormal.GetSafeNormal().Z) <= -0.45f)
			{
				*bOutEave = true;
			}
			return float(Base.Z) + Offset;
		}
	}
	return float(Base.Z) + MaxScan;
}

void USlimeClingComponent::EnterCling(const FHitResult& Hit)
{
	if (!OwnerCharacter)
	{
		return;
	}

	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	LostContactTime = 0.f;
	WallNormal = Hit.ImpactNormal.GetSafeNormal();
	bClinging = true;
	CommitClingHit(Hit);

	if (!bSavedMovement)
	{
		SavedGravityScale = Movement->GravityScale;
		SavedMaxAcceleration = Movement->MaxAcceleration;
		SavedMaxFlySpeed = Movement->MaxFlySpeed;
		SavedBrakingFlying = Movement->BrakingDecelerationFlying;
		bSavedOrientToMovement = Movement->bOrientRotationToMovement;
		bSavedMovement = true;
	}

	Movement->GravityScale = 0.f;
	Movement->bOrientRotationToMovement = false;
	if (USlimeCharacterMovementComponent* SlimeMovement = GetSlimeMovement())
	{
		FVector PreferredForward = GetWallUp();
		if (WallNormal.Z < -WallNormalZMax && OwnerCharacter->GetController())
		{
			const FRotator Yaw(0.0, OwnerCharacter->GetController()->GetControlRotation().Yaw, 0.0);
			PreferredForward = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X);
		}
		FSlimeSurfaceContact Contact;
		Contact.Point = Hit.ImpactPoint;
		Contact.Normal = Hit.ImpactNormal;
		Contact.Component = Hit.GetComponent();
		Contact.Skin = ClingSkin;
		SlimeMovement->RequestClimbStart(Contact, PreferredForward);
	}
}

void USlimeClingComponent::CommitClingHit(const FHitResult& Hit)
{
	const FVector NewNormal = Hit.ImpactNormal.GetSafeNormal();
	if (!WallNormal.IsNearlyZero() && (NewNormal | WallNormal) >= ClingAcceptNormalDot)
	{
		WallNormal = (WallNormal * 0.75f + NewNormal * 0.25f).GetSafeNormal();
	}
	else
	{
		WallNormal = NewNormal;
	}

	WallPoint = Hit.ImpactPoint;
	WallComponent = Hit.GetComponent();
	if (FMath::Abs(WallNormal.Z) <= WallNormalZMax)
	{
		bool bEave = false;
		WallTopZ = ComputeWallTopZ(Hit, &bEave);
		bWallTopIsEave = bEave;
	}
	else
	{
		WallTopZ = -1.e9f;
		bWallTopIsEave = WallNormal.Z <= -0.7f;
	}

	if (USlimeCharacterMovementComponent* SlimeMovement = GetSlimeMovement())
	{
		FSlimeSurfaceContact Contact;
		Contact.Point = WallPoint;
		Contact.Normal = WallNormal;
		Contact.Component = WallComponent;
		Contact.Skin = ClingSkin;
		SlimeMovement->UpdateClimbContact(Contact, true);
	}
}

void USlimeClingComponent::ExitCling(EClingExit Exit)
{
	if (!bClinging)
	{
		return;
	}

	bClinging = false;
	bMantling = false;
	bSurfaceTransitionPending = false;
	bLockMountAfterLedgeDrop = false;
	bWallTopIsEave = false;
	LostContactTime = 0.f;
	WallComponent.Reset();
	RestoreMovementSettings();

	if (OwnerCharacter)
	{
		if (USlimeCharacterMovementComponent* Movement = GetSlimeMovement())
		{
			Movement->RequestClimbStop(Exit == EClingExit::Walking);
		}
		else if (UCharacterMovementComponent* BaseMovement = OwnerCharacter->GetCharacterMovement())
		{
			BaseMovement->SetMovementMode(Exit == EClingExit::Walking ? MOVE_Walking : MOVE_Falling);
		}
	}

	if (Body)
	{
		Body->SetClingVisual(false, FVector::ZeroVector, FVector::UpVector);
		if (Exit == EClingExit::Walking)
		{
			Body->SuppressHeightSqueeze(0.35f);
		}
	}
}

void USlimeClingComponent::RestoreMovementSettings()
{
	if (!bSavedMovement || !OwnerCharacter)
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
	{
		Movement->GravityScale = SavedGravityScale;
		Movement->MaxAcceleration = SavedMaxAcceleration;
		Movement->MaxFlySpeed = SavedMaxFlySpeed;
		Movement->BrakingDecelerationFlying = SavedBrakingFlying;
		Movement->bOrientRotationToMovement = bSavedOrientToMovement != 0;
	}
	bSavedMovement = false;
}

FVector USlimeClingComponent::GetWallUp() const
{
	if (const USlimeCharacterMovementComponent* Movement = GetSlimeMovement())
	{
		if (Movement->IsSlimeClimbing() || Movement->IsSlimeMantling())
		{
			return Movement->GetSurfaceForward();
		}
	}
	const FVector Projected = FVector::VectorPlaneProject(FVector::UpVector, WallNormal);
	if (Projected.IsNearlyZero())
	{
		return FVector::UpVector;
	}
	return Projected.GetSafeNormal();
}

FVector USlimeClingComponent::GetWallRight() const
{
	if (const USlimeCharacterMovementComponent* Movement = GetSlimeMovement())
	{
		if (Movement->IsSlimeClimbing() || Movement->IsSlimeMantling())
		{
			return Movement->GetSurfaceRight();
		}
	}
	const FVector Right = FVector::CrossProduct(WallNormal, FVector::UpVector);
	if (Right.IsNearlyZero())
	{
		return OwnerCharacter ? OwnerCharacter->GetActorRightVector() : FVector::RightVector;
	}
	return Right.GetSafeNormal();
}

void USlimeClingComponent::ApplyClingVelocity()
{
	if (!OwnerCharacter)
	{
		return;
	}

	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	if (USlimeCharacterMovementComponent* SlimeMovement = GetSlimeMovement())
	{
		SlimeMovement->SetClimbInput(
			bLockMountAfterLedgeDrop ? MoveRight : MoveRight,
			bLockMountAfterLedgeDrop ? -1.f : MoveForward,
			ClimbSpeed,
			SlideSpeed);
	}

	const FVector HorizontalNormal(WallNormal.X, WallNormal.Y, 0.0);
	if (!HorizontalNormal.IsNearlyZero())
	{
		FRotator FaceWall = (-HorizontalNormal).Rotation();
		FaceWall.Pitch = 0.f;
		FaceWall.Roll = 0.f;
		OwnerCharacter->SetActorRotation(FaceWall);
	}
}

void USlimeClingComponent::SetWalkStepBoost(float BoostedMaxStep)
{
	if (Body)
	{
		Body->SetStepHeightBoost(BoostedMaxStep);
	}
	else if (OwnerCharacter)
	{
		if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
		{
			if (BoostedMaxStep > KINDA_SMALL_NUMBER)
			{
				Movement->MaxStepHeight = BoostedMaxStep;
			}
		}
	}
}

bool USlimeClingComponent::TryWalkUpShortObstacle(const FHitResult& Hit)
{
	if (bClinging || !OwnerCapsule || !OwnerCharacter || !IsClingableWall(Hit))
	{
		SetWalkStepBoost(0.f);
		return false;
	}

	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	UWorld* World = GetWorld();
	if (!World || !Movement || Movement->IsFalling())
	{
		SetWalkStepBoost(0.f);
		return false;
	}

	const FVector Outward = Hit.ImpactNormal.GetSafeNormal();
	const FVector Loc = OwnerCapsule->GetComponentLocation();
	const float HalfH = OwnerCapsule->GetScaledCapsuleHalfHeight();
	const float Radius = OwnerCapsule->GetScaledCapsuleRadius();
	const float FootZ = float(Loc.Z) - HalfH;
	const float MaxRise = FMath::Max(MaxWalkUpHeight, 20.f);

	FCollisionQueryParams Query(TEXT("SlimeClingWalkUp"), false, GetOwner());
	const FVector Base = Hit.ImpactPoint + Outward * 6.0;
	float LipZ = float(Base.Z);
	const float Step = 8.f;
	bool bFoundLip = false;
	for (float Offset = 0.f; Offset <= MaxRise + 16.f; Offset += Step)
	{
		const FVector Sample(Base.X, Base.Y, Base.Z + double(Offset));
		FHitResult WallHit;
		if (!World->LineTraceSingleByChannel(WallHit, Sample, Sample - Outward * double(WallProbeDistance), ECC_Pawn, Query)
			|| !IsClingableWall(WallHit))
		{
			LipZ = float(Base.Z) + Offset;
			bFoundLip = true;
			break;
		}
	}

	if (!bFoundLip)
	{
		SetWalkStepBoost(0.f);
		return false;
	}

	const float LipRise = LipZ - FootZ;
	const float NativeStep = Body ? Body->GetDefaultStepHeight() : Movement->MaxStepHeight;
	const float BoostCap = NativeStep + 12.f;
	// CMC already clears lips within NativeStep; only assist a single extra band, never multi-stair hops.
	if (LipRise <= NativeStep || LipRise > MaxRise || LipRise < 8.f)
	{
		SetWalkStepBoost(0.f);
		return false;
	}

	const FVector Over = FVector(Hit.ImpactPoint.X, Hit.ImpactPoint.Y, double(LipZ + 12.f)) + Outward * double(Radius + 8.f);
	const FVector Start = Over + FVector(0.0, 0.0, 20.0);
	const FVector End = Over - FVector(0.0, 0.0, double(MaxRise + 40.f));
	FHitResult FloorHit;
	const FCollisionShape Shape = FCollisionShape::MakeSphere(FMath::Max(Radius * 0.45f, 6.f));
	if (!World->SweepSingleByChannel(FloorHit, Start, End, FQuat::Identity, ECC_Pawn, Shape, Query)
		|| FloorHit.ImpactNormal.Z < 0.7)
	{
		SetWalkStepBoost(0.f);
		return false;
	}

	SetWalkStepBoost(FMath::Min(LipRise + 10.f, BoostCap));
	return true;
}

bool USlimeClingComponent::TryTransferAlongWall(FHitResult& OutHit, bool bLostContact) const
{
	UWorld* World = GetWorld();
	if (!World || !OwnerCapsule || WallNormal.IsNearlyZero())
	{
		return false;
}

	const FVector Travel = (GetWallRight() * MoveRight + GetWallUp() * MoveForward).GetSafeNormal();
	if (Travel.IsNearlyZero())
	{
		return false;
	}

	const FVector Loc = OwnerCapsule->GetComponentLocation();
	const float Radius = OwnerCapsule->GetScaledCapsuleRadius();
	const float HalfHeight = OwnerCapsule->GetScaledCapsuleHalfHeight();
	const float Support = SlimeMovementPolicies::CapsuleSupportDistance(Radius, HalfHeight, WallNormal);
	const float MaxDist = Support + 45.f;
	const float SideOffsets[] = { 8.f, 20.f };
	const FVector SweepDirs[] = {
		-WallNormal,
		-Travel,
		(-Travel * 0.7 - WallNormal * 0.3).GetSafeNormal()
	};
	const FCollisionShape Shape = FCollisionShape::MakeSphere(4.f);

	auto AcceptTransferHit = [&](const FHitResult& Hit) -> bool
	{
		if (!IsClingableWall(Hit))
		{
			return false;
		}

		if (FVector::Dist(Hit.ImpactPoint, Loc) > double(MaxDist))
		{
			return false;
		}

		const FVector NewNormal = Hit.ImpactNormal.GetSafeNormal();
		const float FaceDot = float(NewNormal | WallNormal);
		if (FaceDot < -0.2f)
		{
			return false;
		}

		const float AlongTravel = float((Hit.ImpactPoint - Loc) | Travel);
		if (AlongTravel <= 4.f || AlongTravel > MaxDist)
		{
			return false;
		}

		const bool bDifferentComp = Hit.GetComponent() != nullptr && Hit.GetComponent() != WallComponent.Get();
		const bool bSeam = FaceDot >= ClingAcceptNormalDot
			&& bLostContact
			&& (bDifferentComp || AlongTravel > Radius * 0.35f);
		const bool bCorner = FaceDot < ClingAcceptNormalDot
			&& FMath::Abs(float(NewNormal | Travel)) > 0.25f;
		return bSeam || bCorner;
	};

	auto SweepPass = [&](bool bIgnoreCurrent) -> bool
	{
		FCollisionQueryParams Query(TEXT("SlimeClingTransfer"), false, GetOwner());
		if (bIgnoreCurrent && WallComponent.IsValid())
		{
			Query.AddIgnoredComponent(WallComponent.Get());
		}

		FHitResult BestHit;
		float BestDistSq = TNumericLimits<float>::Max();
		bool bFound = false;

		for (const float Side : SideOffsets)
		{
			const FVector Start = Loc + WallNormal * double(Support + 12.f) + Travel * double(Radius + Side);
			for (const FVector& Dir : SweepDirs)
			{
				if (Dir.IsNearlyZero())
				{
					continue;
				}

				const float Dist = (Dir | -WallNormal) > 0.7f
					? FMath::Max(WallProbeDistance, Radius * 2.f + 24.f)
					: Radius * 2.f + 40.f;
				TArray<FHitResult> Hits;
				if (!World->SweepMultiByChannel(Hits, Start, Start + Dir * double(Dist),
					FQuat::Identity, ECC_Pawn, Shape, Query))
				{
					continue;
				}
				for (const FHitResult& Hit : Hits)
				{
					if (!AcceptTransferHit(Hit))
					{
						continue;
					}
					const float DistSq = float(FVector::DistSquared(Hit.ImpactPoint, Loc));
					const float ContinuityPenalty = 1.f - FMath::Clamp(
						float(Hit.ImpactNormal.GetSafeNormal() | WallNormal), -1.f, 1.f);
					const float Score = DistSq + ContinuityPenalty * FMath::Square(Radius * 0.35f);
					if (Score < BestDistSq)
					{
						BestDistSq = Score;
						BestHit = Hit;
						bFound = true;
					}
				}
			}
			if (bFound)
			{
				break;
			}
		}

		if (bFound)
		{
			OutHit = BestHit;
			return true;
		}
		return false;
	};

	if (SweepPass(true))
	{
		return true;
	}
	return SweepPass(false);
}

void USlimeClingComponent::TryMountLedge()
{
	UWorld* World = GetWorld();
	if (!World || !OwnerCapsule || !OwnerCharacter)
	{
		return;
	}

	if (bLockMountAfterLedgeDrop || MoveForward <= 0.f || IsStandingOnLedgeTop())
	{
		return;
	}

	if (bWallTopIsEave && TryTransferToOverhang())
	{
		return;
	}

	const FVector COM = GetCOM();
	if (!bWallTopIsEave && COM.Z >= WallTopZ)
	{
		return;
	}

	const float HalfH = OwnerCapsule->GetScaledCapsuleHalfHeight();
	const float Radius = OwnerCapsule->GetScaledCapsuleRadius();
	const FVector Loc = OwnerCapsule->GetComponentLocation();
	const float CapsuleTop = float(Loc.Z) + HalfH;
	const float ReachSlack = bWallTopIsEave ? LedgeGrabSlack * 2.f : LedgeGrabSlack;
	if (CapsuleTop < WallTopZ - ReachSlack)
	{
		return;
	}

	FCollisionQueryParams Query(TEXT("SlimeClingLedge"), false, GetOwner());
	const float OutwardOffsets[] = { 18.f, 40.f, 70.f, 110.f };
	const FCollisionShape Shape = FCollisionShape::MakeSphere(FMath::Max(Radius * 0.6f, 6.f));
	FHitResult FloorHit;
	bool bFoundRoof = false;
	for (const float OutDist : OutwardOffsets)
	{
		const FVector Over = Loc - WallNormal * double(Radius + OutDist) + FVector(0.0, 0.0, 12.0);
		const FVector Start = Over + FVector(0.0, 0.0, 40.0);
		const FVector End = Over - FVector(0.0, 0.0, 90.0);
		if (World->SweepSingleByChannel(FloorHit, Start, End, FQuat::Identity, ECC_Pawn, Shape, Query)
			&& FloorHit.ImpactNormal.Z >= 0.7)
		{
			bFoundRoof = true;
			break;
		}
	}
	if (!bFoundRoof)
	{
		return;
	}

	const FVector Stand = FloorHit.ImpactPoint + FVector(0.0, 0.0, double(HalfH) + 2.0);
	const double ClearanceZ = FMath::Max(Stand.Z, double(WallTopZ) + double(HalfH) + 2.0);
	const FVector Clearance = Loc + WallNormal * double(ClingSkin + 2.f)
		+ FVector(0.0, 0.0, ClearanceZ - Loc.Z);
	if (USlimeCharacterMovementComponent* Movement = GetSlimeMovement())
	{
		if (Movement->RequestMantle(Clearance, Stand, MantleSpeed))
		{
			bMantling = true;
			bWallTopIsEave = false;
			LostContactTime = 0.f;
			WallComponent.Reset();
			if (Body)
			{
				Body->SetClingVisual(false, FVector::ZeroVector, FVector::UpVector);
				Body->SuppressHeightSqueeze(0.35f);
			}
		}
	}
}

void USlimeClingComponent::TryLandWhileClinging()
{
	UWorld* World = GetWorld();
	if (!World || !OwnerCapsule || !OwnerCharacter)
	{
		return;
	}

	FCollisionQueryParams Query(TEXT("SlimeClingLand"), false, GetOwner());
	const float HalfH = OwnerCapsule->GetScaledCapsuleHalfHeight();
	const float Radius = OwnerCapsule->GetScaledCapsuleRadius();
	const FVector Loc = OwnerCapsule->GetComponentLocation();
	const FVector Start = Loc;
	const FVector End = Loc - FVector(0.0, 0.0, double(HalfH + 10.f));
	FHitResult FloorHit;
	const FCollisionShape Shape = FCollisionShape::MakeSphere(FMath::Max(Radius * 0.55f, 4.f));
	if (!World->SweepSingleByChannel(FloorHit, Start, End, FQuat::Identity, ECC_Pawn, Shape, Query))
	{
		return;
	}
	if (FloorHit.ImpactNormal.Z < 0.7)
	{
		return;
	}

	if (bLockMountAfterLedgeDrop)
	{
		return;
	}

	if (FMath::Abs(float(FloorHit.ImpactPoint.Z) - WallTopZ) < GetBodyRadius() * 0.5f)
	{
		return;
	}

	if (IsStandingOnLedgeTop())
	{
		return;
	}

	// Holding climb-up from the ground should keep going; otherwise drop back to walking.
	if (bHasMoveInput)
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
	{
		Movement->Velocity = FVector::ZeroVector;
	}
	ExitCling(EClingExit::Walking);
}

void USlimeClingComponent::PushVisualToBody()
{
	if (Body)
	{
		Body->SetClingVisual(true, WallPoint, WallNormal);
	}
}

FVector USlimeClingComponent::GetCOM() const
{
	if (Body)
	{
		return Body->GetBlobCenter();
	}
	return OwnerCapsule ? OwnerCapsule->GetComponentLocation() : FVector::ZeroVector;
}

float USlimeClingComponent::GetBodyRadius() const
{
	return OwnerCapsule ? float(OwnerCapsule->GetScaledCapsuleRadius()) : 32.f;
}

float USlimeClingComponent::GetLedgeDropThreshold() const
{
	return GetBodyRadius() * LedgeDropOverhangFraction;
}

FVector USlimeClingComponent::GetWalkOutward() const
{
	if (bHasMoveInput && OwnerCharacter)
	{
		if (const AController* Controller = OwnerCharacter->GetController())
		{
			const FRotator Yaw(0.0, Controller->GetControlRotation().Yaw, 0.0);
			const FVector Forward = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X);
			const FVector Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
			const FVector Dir = Forward * double(MoveForward) + Right * double(MoveRight);
			FVector Flat(Dir.X, Dir.Y, 0.0);
			if (!Flat.IsNearlyZero())
			{
				return Flat.GetSafeNormal();
			}
		}
	}

	if (OwnerCharacter)
	{
		if (const UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
		{
			FVector Input = Movement->GetLastInputVector();
			Input.Z = 0.0;
			if (!Input.IsNearlyZero())
			{
				return Input.GetSafeNormal();
			}
			FVector Vel = Movement->Velocity;
			Vel.Z = 0.0;
			if (!Vel.IsNearlyZero())
			{
				return Vel.GetSafeNormal();
			}
		}
	}
	return FVector::ZeroVector;
}

bool USlimeClingComponent::TraceWalkableFloor(const FVector& Origin, FHitResult& OutHit) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams Query(TEXT("SlimeClingLedgeFloor"), false, GetOwner());
	const float Radius = FMath::Max(GetBodyRadius() * 0.2f, 4.f);
	const FVector Start = Origin + FVector(0.0, 0.0, 12.0);
	const FVector End = Origin - FVector(0.0, 0.0, 80.0);
	if (!World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(Radius), Query))
	{
		return false;
	}
	return OutHit.ImpactNormal.Z >= 0.7;
}

bool USlimeClingComponent::ProbeSideWall(const FVector& EdgePoint, const FVector& Outward, FHitResult& OutHit) const
{
	UWorld* World = GetWorld();
	if (!World || Outward.IsNearlyZero())
	{
		return false;
	}

	FCollisionQueryParams Query(TEXT("SlimeClingLedgeWall"), false, GetOwner());
	const FVector Start = EdgePoint + Outward * 10.0 - FVector(0.0, 0.0, 10.0);
	const FVector End = Start - Outward * double(WallProbeDistance + 24.f);
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Pawn, Query) && IsClingableWall(Hit))
	{
		OutHit = Hit;
		return true;
	}

	const float Radius = FMath::Max(GetBodyRadius() * 0.25f, 4.f);
	if (World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(Radius), Query) && IsClingableWall(Hit))
	{
		OutHit = Hit;
		return true;
	}
	return false;
}

bool USlimeClingComponent::ProbeLedgeOverhang(FLedgeOverhang& OutLedge) const
{
	OutLedge = FLedgeOverhang();
	if (!OwnerCapsule)
	{
		return false;
	}

	FVector Outward = GetWalkOutward();
	if (Outward.IsNearlyZero() && bClinging)
	{
		Outward = FVector(WallNormal.X, WallNormal.Y, 0.0).GetSafeNormal();
	}
	if (Outward.IsNearlyZero())
	{
		return false;
	}

	const FVector COM = GetCOM();
	const float Radius = GetBodyRadius();
	FHitResult BehindHit;
	FHitResult AheadHit;
	const FVector Behind = COM - Outward * double(Radius);
	const FVector Ahead = COM + Outward * double(Radius);
	if (!TraceWalkableFloor(Behind, BehindHit))
	{
		return false;
	}
	if (TraceWalkableFloor(Ahead, AheadHit)
		&& AheadHit.ImpactPoint.Z > BehindHit.ImpactPoint.Z - 20.0)
	{
		return false;
	}

	float Lo = -Radius;
	float Hi = Radius;
	for (int32 Step = 0; Step < 8; ++Step)
	{
		const float Mid = (Lo + Hi) * 0.5f;
		FHitResult MidHit;
		const FVector Sample = COM + Outward * double(Mid);
		if (TraceWalkableFloor(Sample, MidHit)
			&& MidHit.ImpactPoint.Z > BehindHit.ImpactPoint.Z - 20.0)
		{
			Lo = Mid;
		}
		else
		{
			Hi = Mid;
		}
	}

	OutLedge.bValid = true;
	OutLedge.Outward = Outward;
	OutLedge.EdgePoint = COM + Outward * double(Lo);
	OutLedge.EdgePoint.Z = BehindHit.ImpactPoint.Z;
	OutLedge.Overhang = float((COM - OutLedge.EdgePoint) | Outward);
	OutLedge.FloorZ = float(BehindHit.ImpactPoint.Z);
	OutLedge.FloorComponent = BehindHit.GetComponent();
	ProbeSideWall(OutLedge.EdgePoint, Outward, OutLedge.SideWall);
	return true;
}

bool USlimeClingComponent::IsStandingOnLedgeTop() const
{
	FHitResult FloorHit;
	if (!TraceWalkableFloor(GetCOM(), FloorHit))
	{
		return false;
	}

	if (bClinging && WallTopZ > -1.e8f)
	{
		return FMath::Abs(float(FloorHit.ImpactPoint.Z) - WallTopZ) < GetBodyRadius() * 0.6f;
	}

	if (OwnerCharacter)
	{
		if (const UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
		{
			if (Movement->CurrentFloor.bBlockingHit)
			{
				return FMath::Abs(float(FloorHit.ImpactPoint.Z) - float(Movement->CurrentFloor.HitResult.ImpactPoint.Z)) < 12.f;
			}
		}
	}
	return true;
}

bool USlimeClingComponent::IsLipWallWhileOnTop(const FHitResult& Hit) const
{
	if (!IsClingableWall(Hit) || !OwnerCharacter)
	{
		return false;
	}

	const UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	if (!Movement || Movement->IsFalling() || !Movement->CurrentFloor.bBlockingHit)
	{
		return false;
	}

	FLedgeOverhang Ledge;
	if (ProbeLedgeOverhang(Ledge) && Ledge.Overhang < GetLedgeDropThreshold())
	{
		return true;
	}

	const UPrimitiveComponent* FloorComp = Movement->CurrentFloor.HitResult.GetComponent();
	return FloorComp && FloorComp == Hit.GetComponent();
}

void USlimeClingComponent::NudgeAlongLedge(const FLedgeOverhang& Ledge, float DeltaTime)
{
	if (!OwnerCharacter || !OwnerCapsule || Ledge.Outward.IsNearlyZero())
	{
		return;
	}

	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	const float Speed = Movement ? Movement->MaxWalkSpeed : 420.f;
	const float HalfH = OwnerCapsule->GetScaledCapsuleHalfHeight();
	FVector Delta = Ledge.Outward * double(Speed * DeltaTime);
	Delta.Z = double(Ledge.FloorZ) + double(HalfH) - OwnerCharacter->GetActorLocation().Z;
	if (USlimeCharacterMovementComponent* SlimeMovement = GetSlimeMovement())
	{
		SlimeMovement->QueueExternalCorrection(Delta);
	}
	else if (Movement && Movement->UpdatedComponent)
	{
		FHitResult Hit;
		Movement->SafeMoveUpdatedComponent(Delta, OwnerCharacter->GetActorQuat(), true, Hit);
	}
	if (Movement)
	{
		Movement->Velocity = Ledge.Outward * Speed;
	}
}

void USlimeClingComponent::EnterClingFromLedgeDrop(const FHitResult& SideWall)
{
	EnterCling(SideWall);
	bLockMountAfterLedgeDrop = true;

	if (!OwnerCharacter || !OwnerCapsule)
	{
		return;
	}

	// EnterCling submitted the contact to the movement component. It will apply a bounded
	// surface correction during the next custom movement sub-step.
}

void USlimeClingComponent::UpdateLedgeDropMountLock()
{
	if (!bLockMountAfterLedgeDrop)
	{
		return;
	}
	if (GetCOM().Z < WallTopZ - GetBodyRadius() * 0.35f)
	{
		bLockMountAfterLedgeDrop = false;
	}
}

bool USlimeClingComponent::TryLedgeWalkOff(float DeltaTime)
{
	if (DetachCooldownRemaining > 0.f)
	{
		return false;
	}

	FLedgeOverhang Ledge;
	if (!ProbeLedgeOverhang(Ledge) || !IsLedgeTallEnough(Ledge))
	{
		return false;
	}

	const float Threshold = GetLedgeDropThreshold();
	if (Ledge.Overhang < Threshold)
	{
		if (bHasMoveInput)
		{
			NudgeAlongLedge(Ledge, DeltaTime);
			return true;
		}
		return false;
	}

	if (Ledge.SideWall.bBlockingHit && IsClingableWall(Ledge.SideWall) && HasTallWallFace(Ledge.SideWall))
	{
		EnterClingFromLedgeDrop(Ledge.SideWall);
		ApplyClingVelocity();
		PushVisualToBody();
		return true;
	}

	if (bHasMoveInput)
	{
		NudgeAlongLedge(Ledge, DeltaTime);
		return true;
	}
	return false;
}

bool USlimeClingComponent::IsLedgeTallEnough(const FLedgeOverhang& Ledge) const
{
	UWorld* World = GetWorld();
	if (!World || Ledge.Outward.IsNearlyZero())
	{
		return false;
	}

	const float MinDrop = FMath::Max(LedgeDropMinHeight, 0.f);
	FCollisionQueryParams Query(TEXT("SlimeClingLedgeDrop"), false, GetOwner());
	const FVector Origin = Ledge.EdgePoint + Ledge.Outward * 10.0;
	const FVector Start = Origin + FVector(0.0, 0.0, 8.0);
	const FVector End = Origin - FVector(0.0, 0.0, double(MinDrop + 40.f));
	const float Radius = FMath::Max(GetBodyRadius() * 0.2f, 4.f);
	FHitResult Hit;
	if (!World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(Radius), Query))
	{
		return true;
	}
	if (Hit.ImpactNormal.Z < 0.7)
	{
		return true;
	}

	const float Drop = Ledge.FloorZ - float(Hit.ImpactPoint.Z);
	return Drop > MinDrop;
}

USlimeCharacterMovementComponent* USlimeClingComponent::GetSlimeMovement() const
{
	return OwnerCharacter
		? Cast<USlimeCharacterMovementComponent>(OwnerCharacter->GetCharacterMovement())
		: nullptr;
}

bool USlimeClingComponent::TryTransferToOverhang()
{
	USlimeCharacterMovementComponent* Movement = GetSlimeMovement();
	UWorld* World = GetWorld();
	if (!OwnerCharacter || !OwnerCapsule || !Movement || !World || !bWallTopIsEave)
	{
		return false;
	}

	const float Radius = OwnerCapsule->GetScaledCapsuleRadius();
	const FVector Travel = GetWallUp().GetSafeNormal();
	const FVector Start = OwnerCapsule->GetComponentLocation() + Travel * 2.f;
	const FVector End = Start + Travel * FMath::Max(Radius + LedgeGrabSlack * 2.f, 24.f);
	FCollisionQueryParams Query(TEXT("SlimeClingOverhang"), false, GetOwner());
	TArray<FHitResult> Hits;
	if (!World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(3.f), Query))
	{
		return false;
	}

	for (const FHitResult& Hit : Hits)
	{
		if (!IsClingableWall(Hit) || Hit.ImpactNormal.GetSafeNormal().Z >= -WallNormalZMax)
		{
			continue;
		}
		FSlimeSurfaceContact Contact;
		Contact.Point = Hit.ImpactPoint;
		Contact.Normal = Hit.ImpactNormal;
		Contact.Component = Hit.GetComponent();
		Contact.Skin = ClingSkin;
		if (Movement->RequestSurfaceTransition(Contact, ClimbSpeed))
		{
			PendingSurfaceTransitionHit = Hit;
			bSurfaceTransitionPending = true;
			return true;
		}
	}
	return false;
}
