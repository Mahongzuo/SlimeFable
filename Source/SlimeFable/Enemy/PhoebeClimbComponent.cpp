// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhoebeClimbComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"
#include "Settings/SlimeInputSettings.h"
#include "Settings/SlimeInputTypes.h"
#include "SlimeLockOnComponent.h"

namespace
{
	constexpr float ClimbAnimAxisInterpSpeed = 10.f;
	constexpr float ClimbDashHysteresisSeconds = 0.1f;
	constexpr float ClimbStartBlendOut = 0.25f;
	constexpr float ClimbExitMontageBlend = 0.15f;
	constexpr float ClimbDropDownSpeed = 280.f;
}

UPhoebeClimbComponent::UPhoebeClimbComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	MantleMontage = TSoftObjectPtr<UAnimMontage>(
		FSoftObjectPath(TEXT("/Game/Models/Phoebe/Animations/Montages/AM_Phoebe_Climb_OnTop.AM_Phoebe_Climb_OnTop")));
	ClimbStartMontage = TSoftObjectPtr<UAnimMontage>(
		FSoftObjectPath(TEXT("/Game/Models/Phoebe/Animations/Montages/AM_Phoebe_Climb_Start_Up.AM_Phoebe_Climb_Start_Up")));
	LandMontage = TSoftObjectPtr<UAnimMontage>(
		FSoftObjectPath(TEXT("/Game/Models/Phoebe/Animations/Montages/AM_Phoebe_Land_Light.AM_Phoebe_Land_Light")));
	VaultMontage = TSoftObjectPtr<UAnimMontage>(
		FSoftObjectPath(TEXT("/Game/Models/Phoebe/Animations/Montages/AM_Phoebe_Climb_Vault.AM_Phoebe_Climb_Vault")));
}

void UPhoebeClimbComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<ACharacter>(GetOwner());
}

void UPhoebeClimbComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CooldownRemaining > 0.f)
	{
		CooldownRemaining = FMath::Max(0.f, CooldownRemaining - DeltaTime);
	}
	if (WrapCooldownRemaining > 0.f)
	{
		WrapCooldownRemaining = FMath::Max(0.f, WrapCooldownRemaining - DeltaTime);
	}

	if (!OwnerCharacter)
	{
		return;
	}

	EnforceNoOrphanFlying();

	if (bMantling)
	{
		FallingSeconds = 0.f;
		TickMantle(DeltaTime);
		return;
	}

	if (bClimbing)
	{
		FallingSeconds = 0.f;
		UpdateClimbDashHeld(DeltaTime);
		UpdateClimbMotion(DeltaTime);
		return;
	}

	bClimbDashing = false;
	ClimbDashHoldSeconds = 0.f;
	ClimbDashReleaseSeconds = 0.f;

	UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement();
	if (Move && Move->IsFalling())
	{
		FallingSeconds += DeltaTime;
	}
	else
	{
		FallingSeconds = 0.f;
	}

	if (CanAutoStartClimb())
	{
		TryStartClimb();
	}
}

void UPhoebeClimbComponent::HandleMorphMove(const FVector2D& MoveAxis)
{
	InputRight = FMath::Clamp(MoveAxis.X, -1.f, 1.f);
	InputForward = FMath::Clamp(MoveAxis.Y, -1.f, 1.f);

	if (bClimbing || bMantling)
	{
		return;
	}

	if (CanAutoStartClimb())
	{
		TryStartClimb();
	}
}

bool UPhoebeClimbComponent::HandleMorphJump()
{
	if (!OwnerCharacter)
	{
		return false;
	}

	UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement();
	if (!Move)
	{
		return false;
	}

	// Always force exit if we were in climb/mantle OR orphaned Flying from climb.
	const bool bWasClimbing = bClimbing || bMantling;
	const bool bOrphanFlying = !bWasClimbing && Move->MovementMode == MOVE_Flying;
	if (!bWasClimbing && !bOrphanFlying)
	{
		return false;
	}

	if (bMantling)
	{
		bMantling = false;
		SoftenMorphCamera(false);
	}

	const FVector JumpDir = (WallNormal + FVector::UpVector).GetSafeNormal();
	ExitClimb(false);
	Move->Velocity = JumpDir * WallJumpSpeed;
	Move->SetMovementMode(MOVE_Falling);
	CooldownRemaining = ReattachCooldown;

	if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
	{
		if (UAnimInstance* Anim = Mesh->GetAnimInstance())
		{
			if (UAnimMontage* Vault = VaultMontage.LoadSynchronous())
			{
				Anim->Montage_Play(Vault);
			}
		}
	}
	return true;
}

bool UPhoebeClimbComponent::BeginAirAttackDrop()
{
	if (!OwnerCharacter || (!bClimbing && !bMantling))
	{
		return false;
	}

	if (bMantling)
	{
		SoftenMorphCamera(false);
	}
	ExitClimb(false);
	CooldownRemaining = FMath::Max(CooldownRemaining, ReattachCooldown);
	if (UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Falling);
	}
	return true;
}

FVector UPhoebeClimbComponent::GetClimbProbeDirection() const
{
	if (!OwnerCharacter)
	{
		return FVector::ForwardVector;
	}

	if (const AController* Ctrl = OwnerCharacter->GetController())
	{
		const FRotator Yaw(0.f, Ctrl->GetControlRotation().Yaw, 0.f);
		return Yaw.Vector();
	}

	return OwnerCharacter->GetActorForwardVector();
}

bool UPhoebeClimbComponent::TryFindWall(FHitResult& OutHit) const
{
	if (!OwnerCharacter)
	{
		return false;
	}

	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	if (!Capsule)
	{
		return false;
	}

	const FVector Start = Capsule->GetComponentLocation();
	const FVector Forward = GetClimbProbeDirection();
	const FVector End = Start + Forward * AttachProbeDistance;
	if (!TraceClimbWall(Start, End, OutHit))
	{
		return false;
	}
	return IsClimbableWallNormal(OutHit.ImpactNormal);
}

bool UPhoebeClimbComponent::TryStartClimb()
{
	if (bClimbing || bMantling || !OwnerCharacter)
	{
		return false;
	}

	UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement();
	if (!Move || !Move->IsFalling())
	{
		return false;
	}

	FHitResult Hit;
	if (!TryFindWall(Hit))
	{
		return false;
	}

	WallNormal = Hit.ImpactNormal.GetSafeNormal();
	WallPoint = Hit.ImpactPoint;
	if (USlimeLockOnComponent* Lock = OwnerCharacter->FindComponentByClass<USlimeLockOnComponent>())
	{
		Lock->ClearLockOn();
	}
	SavedGravityScale = Move->GravityScale;
	bSavedOrientToMovement = Move->bOrientRotationToMovement;
	bClimbing = true;
	LostContactSeconds = 0.f;
	FallingSeconds = 0.f;
	WrapCooldownRemaining = 0.f;

	Move->StopMovementImmediately();
	Move->GravityScale = 0.f;
	Move->SetMovementMode(MOVE_Flying);
	Move->bOrientRotationToMovement = false;
	if (!ApplyClingCorrection())
	{
		return false;
	}

	if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
	{
		if (UAnimInstance* Anim = Mesh->GetAnimInstance())
		{
			if (UAnimMontage* Start = ClimbStartMontage.LoadSynchronous())
			{
				Anim->Montage_Play(Start);
			}
		}
	}

	return true;
}

void UPhoebeClimbComponent::RestoreMovementAfterClimb(bool bToWalking)
{
	if (!OwnerCharacter)
	{
		return;
	}

	if (UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement())
	{
		Move->GravityScale = SavedGravityScale > KINDA_SMALL_NUMBER ? SavedGravityScale : 1.f;
		Move->bOrientRotationToMovement = bSavedOrientToMovement;
		if (bToWalking)
		{
			Move->Velocity = FVector::ZeroVector;
			Move->SetMovementMode(MOVE_Walking);
		}
		else
		{
			// Keep a slight downward bias so we leave Flying immediately.
			if (Move->Velocity.Z > 0.f)
			{
				Move->Velocity.Z = FMath::Min(Move->Velocity.Z, 0.f);
			}
			Move->SetMovementMode(MOVE_Falling);
		}
	}
}

void UPhoebeClimbComponent::ExitClimb(bool bToWalking)
{
	const bool bWasActive = bClimbing || bMantling;
	bClimbing = false;
	bMantling = false;
	bClimbDashing = false;
	ClimbYaw = 0.f;
	ClimbPitch = 0.f;
	InputRight = 0.f;
	InputForward = 0.f;
	ClimbDashHoldSeconds = 0.f;
	ClimbDashReleaseSeconds = 0.f;

	if (!OwnerCharacter)
	{
		return;
	}

	StopClimbMontages(ClimbExitMontageBlend);

	// Always restore movement even if flags were already cleared (orphan Flying).
	if (bWasActive || (OwnerCharacter->GetCharacterMovement()
		&& OwnerCharacter->GetCharacterMovement()->MovementMode == MOVE_Flying))
	{
		RestoreMovementAfterClimb(bToWalking);
		if (!bToWalking)
		{
			if (UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement())
			{
				Move->Velocity.Z = FMath::Min(Move->Velocity.Z, -ClimbDropDownSpeed);
			}
		}
	}
}

void UPhoebeClimbComponent::StopClimbMontages(float BlendOut)
{
	if (!OwnerCharacter)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
	UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		return;
	}

	auto StopSoft = [Anim, BlendOut](const TSoftObjectPtr<UAnimMontage>& Soft)
	{
		UAnimMontage* Montage = Soft.Get();
		if (!Montage)
		{
			Montage = Soft.LoadSynchronous();
		}
		if (Montage && Anim->Montage_IsPlaying(Montage))
		{
			Anim->Montage_Stop(BlendOut, Montage);
		}
	};

	StopSoft(ClimbStartMontage);
	StopSoft(MantleMontage);
	StopSoft(VaultMontage);
}

void UPhoebeClimbComponent::EnforceNoOrphanFlying()
{
	if (bClimbing || bMantling || !OwnerCharacter)
	{
		return;
	}

	UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement();
	if (Move && Move->MovementMode == MOVE_Flying)
	{
		RestoreMovementAfterClimb(false);
		CooldownRemaining = FMath::Max(CooldownRemaining, ReattachCooldown);
	}
}

bool UPhoebeClimbComponent::ApplyClingCorrection()
{
	if (!OwnerCharacter)
	{
		return false;
	}

	UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement();
	if (!Capsule || !Move)
	{
		return false;
	}

	const float Radius = Capsule->GetScaledCapsuleRadius();
	const FVector Desired = WallPoint + WallNormal * (Radius + ClingSkin);
	const FVector Current = Capsule->GetComponentLocation();
	const FVector Delta = Desired - Current;
	if (!Delta.IsNearlyZero(0.5f))
	{
		FHitResult SweepHit;
		Move->SafeMoveUpdatedComponent(Delta, OwnerCharacter->GetActorQuat(), true, SweepHit);
		if (SweepHit.bBlockingHit)
		{
			const FVector Away = WallNormal.GetSafeNormal();
			const float IntoWall = FVector::DotProduct(Delta.GetSafeNormal(), -Away);
			if (SweepHit.bStartPenetrating || IntoWall > 0.45f)
			{
				ExitClimb(false);
				CooldownRemaining = FMath::Max(CooldownRemaining, ReattachCooldown);
				return false;
			}
		}
	}

	const FRotator FaceWall = (-WallNormal).Rotation();
	OwnerCharacter->SetActorRotation(FRotator(0.f, FaceWall.Yaw, 0.f));
	return true;
}

bool UPhoebeClimbComponent::CanAutoStartClimb() const
{
	if (!OwnerCharacter || CooldownRemaining > 0.f || FMath::Abs(InputForward) <= 0.2f)
	{
		return false;
	}

	const UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement();
	if (!Move || !Move->IsFalling() || Move->IsMovingOnGround())
	{
		return false;
	}

	// Walking into a wall briefly pops Falling. Require a real drop so we
	// never auto-cling from ground contact (that zero-G-locks and crashes BS).
	if (Move->CurrentFloor.bBlockingHit && Move->CurrentFloor.FloorDist < 12.f)
	{
		return false;
	}

	return Move->Velocity.Z < -160.f || FallingSeconds > 0.25f;
}

bool UPhoebeClimbComponent::CanStandAt(const FVector& CapsuleCenter) const
{
	if (!OwnerCharacter || !GetWorld())
	{
		return false;
	}

	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	if (!Capsule)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PhoebeClimbStand), false, OwnerCharacter);
	// Shrink so wooden eaves / grass trim don't false-fail. Never use ECC_Pawn.
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius() * 0.72f,
		Capsule->GetScaledCapsuleHalfHeight() * 0.82f);

	static const ECollisionChannel StandChannels[] = { ECC_WorldStatic, ECC_WorldDynamic };
	for (ECollisionChannel Channel : StandChannels)
	{
		if (GetWorld()->OverlapAnyTestByChannel(CapsuleCenter, FQuat::Identity, Channel, Shape, Params))
		{
			return false;
		}
	}
	return true;
}

void UPhoebeClimbComponent::DrawMantleDebug(const FVector& Start, const FVector& End, bool bHit) const
{
	if (!bDebugMantleDraw || !GetWorld())
	{
		return;
	}
	DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Green : FColor::Red, false, 0.08f, 0, 1.5f);
}

bool UPhoebeClimbComponent::FindMantleStandLocation(FVector& OutStandLoc) const
{
	if (!OwnerCharacter || !GetWorld())
	{
		return false;
	}

	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	if (!Capsule)
	{
		return false;
	}

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector CapsuleTop = Capsule->GetComponentLocation() + FVector(0.f, 0.f, HalfHeight);
	const FVector Out = (-WallNormal).GetSafeNormal();

	const float OutDistances[3] = {
		FMath::Max(20.f, MantleOutNear),
		FMath::Max(MantleOutNear + 10.f, MantleOutMid),
		FMath::Max(MantleOutMid + 10.f, MantleOutFar)
	};

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PhoebeClimbMantleFind), false, OwnerCharacter);
	const ECollisionChannel Channels[3] = { ECC_WorldStatic, ECC_WorldDynamic, ECC_Visibility };
	const float DownScan = MantleCheckHeight + HalfHeight * 2.f + 80.f;

	for (float OutDist : OutDistances)
	{
		// Over the ledge from capsule top, then sweep down for a walkable surface.
		const FVector OverEdge = CapsuleTop + Out * OutDist + FVector(0.f, 0.f, 12.f);
		const FVector DownEnd = OverEdge - FVector(0.f, 0.f, DownScan);

		for (ECollisionChannel Channel : Channels)
		{
			FHitResult FloorHit;
			const bool bHit = GetWorld()->LineTraceSingleByChannel(FloorHit, OverEdge, DownEnd, Channel, Params);
			DrawMantleDebug(OverEdge, bHit ? FloorHit.ImpactPoint : DownEnd, bHit && FloorHit.bBlockingHit);

			if (!bHit || !FloorHit.bBlockingHit || FloorHit.ImpactNormal.Z < 0.55f)
			{
				continue;
			}

			const float HeightPads[3] = { 2.f, 8.f, 16.f };
			const float OutPads[3] = { 0.f, 20.f, 40.f };
			for (float OutPad : OutPads)
			{
				for (float HeightPad : HeightPads)
				{
					const FVector Candidate =
						FloorHit.ImpactPoint + Out * OutPad + FVector(0.f, 0.f, HalfHeight + HeightPad);
					if (CanStandAt(Candidate))
					{
						OutStandLoc = Candidate;
						return true;
					}
				}
			}
		}
	}

	return false;
}

void UPhoebeClimbComponent::UpdateClimbAnimAxes(
	const FVector& WallRight,
	const FVector& WallUp,
	const FVector& TangentialVelocity,
	float DeltaTime)
{
	float TargetYaw = 0.f;
	float TargetPitch = 0.f;
	const float InputMag = FMath::Abs(InputRight) + FMath::Abs(InputForward);
	if (InputMag > 0.05f)
	{
		TargetYaw = FMath::Clamp(InputRight, -1.f, 1.f);
		TargetPitch = FMath::Clamp(InputForward, -1.f, 1.f);
	}
	else
	{
		const float SpeedAlong = TangentialVelocity.Size();
		if (SpeedAlong > 15.f)
		{
			const FVector Dir = TangentialVelocity.GetSafeNormal();
			TargetYaw = FMath::Clamp(FVector::DotProduct(Dir, WallRight), -1.f, 1.f);
			TargetPitch = FMath::Clamp(FVector::DotProduct(Dir, WallUp), -1.f, 1.f);
		}
	}

	ClimbYaw = FMath::FInterpTo(ClimbYaw, TargetYaw, DeltaTime, ClimbAnimAxisInterpSpeed);
	ClimbPitch = FMath::FInterpTo(ClimbPitch, TargetPitch, DeltaTime, ClimbAnimAxisInterpSpeed);
}

void UPhoebeClimbComponent::UpdateClimbDashHeld(float DeltaTime)
{
	if (!bClimbing || !OwnerCharacter)
	{
		bClimbDashing = false;
		ClimbDashHoldSeconds = 0.f;
		ClimbDashReleaseSeconds = 0.f;
		return;
	}

	const float InputMag = FMath::Abs(InputRight) + FMath::Abs(InputForward);
	bool bHeld = false;
	if (InputMag >= 0.1f)
	{
		if (APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController()))
		{
			if (const UWorld* World = GetWorld())
			{
				if (const UGameInstance* GI = World->GetGameInstance())
				{
					if (const USlimeInputSettings* InputSettings = GI->GetSubsystem<USlimeInputSettings>())
					{
						bHeld = InputSettings->IsKeyDown(PC, ESlimeInputAction::Sprint)
							|| InputSettings->IsKeyDown(PC, ESlimeInputAction::Dodge);
					}
				}
			}
			if (!bHeld)
			{
				bHeld = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightMouseButton);
			}
		}
	}

	if (bHeld)
	{
		ClimbDashReleaseSeconds = 0.f;
		ClimbDashHoldSeconds += DeltaTime;
		if (!bClimbDashing && ClimbDashHoldSeconds >= ClimbDashHysteresisSeconds)
		{
			bClimbDashing = true;
		}
	}
	else
	{
		ClimbDashHoldSeconds = 0.f;
		ClimbDashReleaseSeconds += DeltaTime;
		if (bClimbDashing && ClimbDashReleaseSeconds >= ClimbDashHysteresisSeconds)
		{
			bClimbDashing = false;
		}
	}
}

bool UPhoebeClimbComponent::TraceClimbWall(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	if (!GetWorld() || !OwnerCharacter)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PhoebeClimbTrace), false, OwnerCharacter);
	const ECollisionChannel Channels[3] = { ECC_WorldStatic, ECC_WorldDynamic, ECC_Visibility };
	for (ECollisionChannel Channel : Channels)
	{
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, Channel, Params)
			&& Hit.bBlockingHit
			&& IsClimbableWallNormal(Hit.ImpactNormal))
		{
			OutHit = Hit;
			return true;
		}
	}
	return false;
}

void UPhoebeClimbComponent::UpdateClimbMotion(float DeltaTime)
{
	if (!OwnerCharacter)
	{
		ExitClimb(false);
		return;
	}

	UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement();
	UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	if (!Move || !Capsule)
	{
		ExitClimb(false);
		return;
	}

	// Maintain wall contact along -WallNormal.
	FHitResult Hit;
	const FVector Start = Capsule->GetComponentLocation();
	const FVector End = Start - WallNormal * (AttachProbeDistance + 30.f);
	const FVector PrevWallNormal = WallNormal;
	const FVector WallUpHint = FVector::VectorPlaneProject(FVector::UpVector, PrevWallNormal).GetSafeNormal();
	const FVector WallRightHint = FVector::CrossProduct(PrevWallNormal, WallUpHint).GetSafeNormal();
	const bool bHaveWall = TraceClimbWall(Start, End, Hit);
	if (!bHaveWall
		|| FMath::Abs(Hit.ImpactNormal.Z) > MaxWallNormalZ)
	{
		bool bWrapped = false;
		if (FMath::Abs(InputRight) > 0.15f)
		{
			bWrapped = TryWrapToAdjacentWall(WallRightHint, FMath::Sign(InputRight));
		}
		if (bWrapped)
		{
			LostContactSeconds = 0.f;
		}
		else
		{
			LostContactSeconds += DeltaTime;
			if (LostContactSeconds >= 0.22f)
			{
				ExitClimb(false);
				CooldownRemaining = ReattachCooldown;
				return;
			}
		}
	}
	else
	{
		LostContactSeconds = 0.f;
		WallNormal = Hit.ImpactNormal.GetSafeNormal();
		WallPoint = Hit.ImpactPoint;
	}

	FVector WallUp = FVector::VectorPlaneProject(FVector::UpVector, WallNormal).GetSafeNormal();
	FVector WallRight = FVector::CrossProduct(WallNormal, WallUp).GetSafeNormal();

	// ClimbStart is only the attach transition. As soon as the player moves on
	// the wall, reveal the looping Climb state instead of letting DefaultSlot
	// mask it until the one-shot reaches its final pose.
	if (FMath::Abs(InputRight) + FMath::Abs(InputForward) > 0.05f)
	{
		if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
		{
			if (UAnimInstance* Anim = Mesh->GetAnimInstance())
			{
				if (UAnimMontage* StartMontage = ClimbStartMontage.Get())
				{
					if (Anim->Montage_IsPlaying(StartMontage))
					{
						Anim->Montage_Stop(ClimbStartBlendOut, StartMontage);
					}
				}
			}
		}
	}

	// Wrap only at real corners. A tangent side-trace misses on a flat wall and
	// must not trigger wrap. Probe into the wall from a laterally offset point.
	if (FMath::Abs(InputRight) > 0.15f)
	{
		const float LateralSign = FMath::Sign(InputRight);
		const float Radius = Capsule->GetScaledCapsuleRadius();
		FHitResult OffsetWallHit;
		const FVector OffsetLoc = Capsule->GetComponentLocation() + WallRight * LateralSign * Radius;
		const FVector OffsetIntoWall = OffsetLoc - WallNormal * (AttachProbeDistance + 20.f);
		const bool bOffsetStillOnFace = TraceClimbWall(OffsetLoc, OffsetIntoWall, OffsetWallHit);

		FHitResult SideProbe;
		const FVector InnerStart = Capsule->GetComponentLocation() + WallNormal * 10.f;
		const FVector InnerEnd = InnerStart + WallRight * LateralSign * (AttachProbeDistance + 35.f);
		const bool bSideHit = TraceClimbWall(InnerStart, InnerEnd, SideProbe);
		const bool bInnerCorner = bSideHit
			&& FVector::DotProduct(SideProbe.ImpactNormal.GetSafeNormal(), WallNormal) < 0.85f;
		const bool bOuterCorner = !bOffsetStillOnFace;
		if (bInnerCorner || bOuterCorner)
		{
			if (TryWrapToAdjacentWall(WallRight, LateralSign))
			{
				WallUp = FVector::VectorPlaneProject(FVector::UpVector, WallNormal).GetSafeNormal();
				WallRight = FVector::CrossProduct(WallNormal, WallUp).GetSafeNormal();
			}
		}
	}

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	FHitResult UpWallHit;
	const FVector UpProbeStart = Capsule->GetComponentLocation() + WallUp * (HalfHeight * 0.65f);
	const FVector UpProbeEnd = UpProbeStart - WallNormal * (AttachProbeDistance + 20.f);
	const bool bWallAbove = TraceClimbWall(UpProbeStart, UpProbeEnd, UpWallHit);

	FVector StandCandidate = FVector::ZeroVector;
	const bool bHasStand = FindMantleStandLocation(StandCandidate);

	// Mantle / drop only with upward intent. Pure A/D at a lip stays on the wall.
	if (InputForward > 0.15f)
	{
		if (bHasStand && TryMantle())
		{
			return;
		}
		if (!bWallAbove && !bHasStand)
		{
			ExitClimb(false);
			CooldownRemaining = ReattachCooldown;
			return;
		}
	}

	float SpeedMul = bClimbDashing ? ClimbDashMul : 1.f;
	if (bHasStand && InputForward > 0.1f)
	{
		SpeedMul *= 0.45f;
	}

	const FVector Desired =
		WallRight * (InputRight * ClimbSpeed * SpeedMul) + WallUp * (InputForward * ClimbSpeed * SpeedMul);

	const FVector TangentialForAnim = Desired.IsNearlyZero(1.f)
		? FVector::VectorPlaneProject(Move->Velocity, WallNormal)
		: Desired;
	UpdateClimbAnimAxes(WallRight, WallUp, TangentialForAnim, DeltaTime);

	Move->Velocity = Desired;

	if (!Desired.IsNearlyZero())
	{
		FHitResult SweepHit;
		Move->SafeMoveUpdatedComponent(Desired * DeltaTime, OwnerCharacter->GetActorQuat(), true, SweepHit);

		const bool bLateralBlocked = SweepHit.bBlockingHit && FMath::Abs(InputRight) > 0.15f;
		if (bLateralBlocked)
		{
			const float LateralSign = FMath::Sign(InputRight);
			if (TryWrapToAdjacentWall(WallRight, LateralSign))
			{
				WallUp = FVector::VectorPlaneProject(FVector::UpVector, WallNormal).GetSafeNormal();
				WallRight = FVector::CrossProduct(WallNormal, WallUp).GetSafeNormal();
				ApplyClingCorrection();
				if (!bClimbing)
				{
					return;
				}
			}
		}

		if (SweepHit.bBlockingHit && InputForward > 0.1f)
		{
			if (TryMantle())
			{
				return;
			}
			if (!bWallAbove)
			{
				ExitClimb(false);
				CooldownRemaining = ReattachCooldown;
				return;
			}
		}
	}
	else
	{
		Move->Velocity = FVector::ZeroVector;
	}

	if (!ApplyClingCorrection())
	{
		return;
	}
}

bool UPhoebeClimbComponent::IsClimbableWallNormal(const FVector& Normal) const
{
	return !Normal.IsNearlyZero() && FMath::Abs(Normal.Z) <= MaxWallNormalZ;
}

bool UPhoebeClimbComponent::TryWrapToAdjacentWall(const FVector& WallRight, float LateralSign)
{
	if (!OwnerCharacter || !GetWorld() || FMath::Abs(LateralSign) < 0.1f)
	{
		return false;
	}
	if (WrapCooldownRemaining > 0.f)
	{
		return false;
	}

	UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement();
	if (!Capsule || !Move)
	{
		return false;
	}

	const FVector CapsuleLoc = Capsule->GetComponentLocation();
	const FVector SideDir = WallRight * LateralSign;
	const FVector PrevNormal = WallNormal.GetSafeNormal();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Probe = Radius + ClingSkin + AttachProbeDistance;

	auto CommitWall = [this, Move, Capsule, Radius, PrevNormal](const FVector& NewNormal, const FVector& NewPoint) -> bool
	{
		const FVector N = NewNormal.GetSafeNormal();
		if (!IsClimbableWallNormal(N))
		{
			return false;
		}
		if (FVector::DotProduct(N, PrevNormal) > 0.72f)
		{
			return false;
		}

		WallNormal = N;
		WallPoint = NewPoint;
		WrapCooldownRemaining = 0.12f;
		LostContactSeconds = 0.f;

		const FVector Desired = WallPoint + WallNormal * (Radius + ClingSkin);
		const FVector Current = Capsule->GetComponentLocation();
		const FVector Delta = Desired - Current;
		if (!Delta.IsNearlyZero(0.5f) && Delta.Size() < 180.f)
		{
			FHitResult SweepHit;
			Move->SafeMoveUpdatedComponent(Delta, OwnerCharacter->GetActorQuat(), true, SweepHit);
		}

		const FRotator FaceWall = (-WallNormal).Rotation();
		OwnerCharacter->SetActorRotation(FRotator(0.f, FaceWall.Yaw, 0.f));
		return true;
	};

	auto TryHit = [&](const FHitResult& Hit) -> bool
	{
		if (!Hit.bBlockingHit)
		{
			return false;
		}
		return CommitWall(Hit.ImpactNormal, Hit.ImpactPoint);
	};

	// Inner corner: probe sideways from slightly off the old face so we don't re-hit it.
	{
		FHitResult InnerHit;
		const FVector InnerStart = CapsuleLoc + PrevNormal * 10.f;
		const FVector InnerEnd = InnerStart + SideDir * Probe;
		if (TraceClimbWall(InnerStart, InnerEnd, InnerHit) && TryHit(InnerHit))
		{
			return true;
		}
	}

	// Outer corner: step around the edge, then look back at the new face (normal ≈ -SideDir).
	const float StepOut = Radius + ClingSkin + 36.f;
	const float Heights[3] = { 0.f, HalfHeight * 0.4f, -HalfHeight * 0.3f };
	for (float HeightOff : Heights)
	{
		const FVector Around = CapsuleLoc + SideDir * StepOut - PrevNormal * StepOut + FVector(0.f, 0.f, HeightOff);

		FHitResult OuterHit;
		if (TraceClimbWall(Around, Around - SideDir * Probe, OuterHit) && TryHit(OuterHit))
		{
			return true;
		}
		if (TraceClimbWall(Around, Around - PrevNormal * Probe, OuterHit) && TryHit(OuterHit))
		{
			return true;
		}
	}

	return false;
}

bool UPhoebeClimbComponent::TryMantle()
{
	if (!OwnerCharacter || bMantling)
	{
		return false;
	}

	FVector StandLoc = FVector::ZeroVector;
	if (!FindMantleStandLocation(StandLoc))
	{
		return false;
	}

	MantleStartLoc = OwnerCharacter->GetActorLocation();
	MantleStartRot = OwnerCharacter->GetActorRotation();
	PendingStandLoc = StandLoc;
	{
		const float FaceYaw = (-WallNormal).Rotation().Yaw;
		MantleEndRot = FRotator(0.f, FaceYaw, 0.f);
	}
	MantleAlpha = 0.f;
	MantleHoldUntilTime = 0.f;
	bMantling = true;
	bClimbing = false;
	bClimbDashing = false;
	ClimbYaw = 0.f;
	ClimbPitch = 0.f;
	InputRight = 0.f;
	InputForward = 0.f;
	ClimbDashHoldSeconds = 0.f;
	ClimbDashReleaseSeconds = 0.f;

	if (UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->Velocity = FVector::ZeroVector;
		Move->GravityScale = 0.f;
		Move->SetMovementMode(MOVE_Flying);
		Move->bOrientRotationToMovement = false;
	}

	SoftenMorphCamera(true);

	if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
	{
		if (UAnimInstance* Anim = Mesh->GetAnimInstance())
		{
			if (UAnimMontage* Mantle = MantleMontage.LoadSynchronous())
			{
				Anim->Montage_Play(Mantle);
				const float MontageLen = FMath::Max(Mantle->GetPlayLength(), 0.f);
				if (const UWorld* World = GetWorld())
				{
					MantleHoldUntilTime = World->GetTimeSeconds()
						+ FMath::Max(MantleLerpSeconds, MontageLen);
				}
			}
		}
	}

	if (MantleHoldUntilTime <= 0.f)
	{
		if (const UWorld* World = GetWorld())
		{
			MantleHoldUntilTime = World->GetTimeSeconds() + FMath::Max(MantleLerpSeconds, 0.15f);
		}
	}

	return true;
}

void UPhoebeClimbComponent::TickMantle(float DeltaTime)
{
	if (!OwnerCharacter)
	{
		bMantling = false;
		SoftenMorphCamera(false);
		ExitClimb(false);
		return;
	}

	const float Duration = FMath::Max(MantleLerpSeconds, 0.15f);
	MantleAlpha = FMath::Clamp(MantleAlpha + DeltaTime / Duration, 0.f, 1.f);
	const float T = MantleAlpha * MantleAlpha * (3.f - 2.f * MantleAlpha);
	const FVector Loc = FMath::Lerp(MantleStartLoc, PendingStandLoc, T);
	OwnerCharacter->SetActorLocation(Loc, false, nullptr, ETeleportType::None);
	{
		const FQuat Q = FQuat::Slerp(MantleStartRot.Quaternion(), MantleEndRot.Quaternion(), T);
		OwnerCharacter->SetActorRotation(Q.Rotator());
	}

	if (MantleAlpha < 1.f)
	{
		return;
	}

	OwnerCharacter->SetActorLocation(PendingStandLoc, false, nullptr, ETeleportType::None);
	OwnerCharacter->SetActorRotation(MantleEndRot);
	FinishMantle();
}

void UPhoebeClimbComponent::FinishMantle()
{
	if (!OwnerCharacter)
	{
		bMantling = false;
		SoftenMorphCamera(false);
		return;
	}

	OwnerCharacter->SetActorLocation(PendingStandLoc, false, nullptr, ETeleportType::None);

	bMantling = false;
	bClimbing = false;
	RestoreMovementAfterClimb(true);
	SoftenMorphCamera(false);

	if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
	{
		if (UAnimInstance* Anim = Mesh->GetAnimInstance())
		{
			if (UAnimMontage* Mantle = MantleMontage.Get())
			{
				if (Anim->Montage_IsPlaying(Mantle))
				{
					Anim->Montage_Stop(0.12f, Mantle);
				}
			}
			if (UAnimMontage* Land = LandMontage.LoadSynchronous())
			{
				Anim->Montage_Play(Land);
			}
		}
	}

	CooldownRemaining = ReattachCooldown;
}

void UPhoebeClimbComponent::SoftenMorphCamera(bool bSoft)
{
	if (!OwnerCharacter)
	{
		return;
	}

	USpringArmComponent* Boom = OwnerCharacter->FindComponentByClass<USpringArmComponent>();
	if (!Boom)
	{
		return;
	}

	if (bSoft)
	{
		bCachedCameraLag = Boom->bEnableCameraLag;
		CachedCameraLagSpeed = Boom->CameraLagSpeed;
		CachedCameraRotLagSpeed = Boom->CameraRotationLagSpeed;
		Boom->bEnableCameraLag = true;
		Boom->bEnableCameraRotationLag = true;
		Boom->CameraLagSpeed = FMath::Min(CachedCameraLagSpeed > 0.f ? CachedCameraLagSpeed : 8.f, 4.f);
		Boom->CameraRotationLagSpeed = FMath::Min(CachedCameraRotLagSpeed > 0.f ? CachedCameraRotLagSpeed : 8.f, 4.f);
	}
	else
	{
		Boom->bEnableCameraLag = bCachedCameraLag;
		Boom->CameraLagSpeed = CachedCameraLagSpeed;
		Boom->CameraRotationLagSpeed = CachedCameraRotLagSpeed;
	}
}
