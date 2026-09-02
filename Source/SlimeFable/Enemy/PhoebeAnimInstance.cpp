// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhoebeAnimInstance.h"

#include "ChooserFunctionLibrary.h"
#include "EnemyCharacter.h"
#include "EnemyCombatComponent.h"
#include "EnemyFighter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhoebeClimbComponent.h"
#include "PoseSearch/MotionMatchingAnimNodeLibrary.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchLibrary.h"

UPhoebeAnimInstance::UPhoebeAnimInstance()
{
}

void UPhoebeAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	DesiredControllerYawLastUpdate = 0.f;
	JumpAirGraceRemaining = 0.f;
}

void UPhoebeAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	APawn* Pawn = TryGetPawnOwner();
	ACharacter* Character = Cast<ACharacter>(Pawn);
	if (!Character)
	{
		// ABP preview / some morph paths have no PawnOwner yet — still drive Speed.
		Character = Cast<ACharacter>(GetOwningActor());
	}
	if (!Character)
	{
		return;
	}

	const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	const FVector Velocity = Character->GetVelocity();
	const FVector PendingInput = Character->GetPendingMovementInputVector();
	const FVector LastInput = Character->GetLastMovementInputVector();
	const float InputMag = FMath::Clamp(FMath::Max(PendingInput.Size(), LastInput.Size()), 0.f, 1.f);
	const float MaxSpd = Move ? FMath::Max(Move->MaxWalkSpeed, 1.f) : 500.f;
	// Ground BlendSpace Y is Speed. Never leave it at 0 while the player is actually moving,
	// or the Move state samples only Stand1 (idle slide).
	Speed = FMath::Max(Velocity.Size2D(), InputMag * MaxSpd);
	if (Speed < 1.f)
	{
		Speed = FMath::Max(Speed, Velocity.Size());
	}

	bHasAcceleration = (Move && !Move->GetCurrentAcceleration().IsNearlyZero(1.f))
		|| InputMag > 0.05f;

	bIsClimbing = false;
	bIsClimbDashing = false;
	bIsAirAttacking = false;
	ClimbYaw = 0.f;
	ClimbPitch = 0.f;
	ClimbPlayRate = 1.f;
	if (UPhoebeClimbComponent* Climb = Character->FindComponentByClass<UPhoebeClimbComponent>())
	{
		bIsClimbing = Climb->IsClimbing();
		bIsClimbDashing = Climb->IsClimbDashing();
		ClimbYaw = Climb->GetClimbYaw();
		ClimbPitch = Climb->GetClimbPitch();
		const float Tangential = FVector::VectorPlaneProject(Velocity, Climb->GetWallNormal()).Size();
		const float RefSpeed = FMath::Max(1.f,
			Climb->ClimbSpeed * (bIsClimbDashing ? Climb->ClimbDashMul : 1.f));
		if (bIsClimbing)
		{
			const float AxisMag = FMath::Clamp(FMath::Abs(ClimbYaw) + FMath::Abs(ClimbPitch), 0.f, 1.f);
			ClimbPlayRate = (AxisMag < 0.05f)
				? 0.8f
				: FMath::Clamp(Tangential / RefSpeed, 0.5f, 1.25f);
		}
	}
	if (const AEnemyCharacter* EnemyOwner = Cast<AEnemyCharacter>(Character))
	{
		if (const UEnemyCombatComponent* Combat = EnemyOwner->GetEnemyCombat())
		{
			bIsAirAttacking = Combat->IsAirAttacking();
		}
	}

	const bool bFalling = Move && Move->IsFalling();
	const bool bOrphanFlying = Move && Move->MovementMode == MOVE_Flying && !bIsClimbing;
	if (Character->bPressedJump || Character->bWasJumping)
	{
		JumpAirGraceRemaining = JumpAirGraceSeconds;
	}
	else if (JumpAirGraceRemaining > 0.f)
	{
		JumpAirGraceRemaining = FMath::Max(0.f, JumpAirGraceRemaining - DeltaSeconds);
	}
	if (bFalling || bOrphanFlying)
	{
		JumpAirGraceRemaining = JumpAirGraceSeconds;
	}
	if (Move && Move->IsMovingOnGround() && !bIsClimbing)
	{
		JumpAirGraceRemaining = 0.f;
		bIsInAir = false;
		bIsFalling = false;
	}
	else
	{
		bIsInAir = !bIsClimbing && (bFalling || bOrphanFlying || JumpAirGraceRemaining > 0.f);
		bIsFalling = bIsInAir && Velocity.Z < 0.f;
	}
	bIsCrouch = Character->bIsCrouched;

	if (Speed > 10.f)
	{
		const FRotator ActorRot = Character->GetActorRotation();
		const FVector Forward = ActorRot.Vector();
		const FVector Right = FRotationMatrix(ActorRot).GetScaledAxis(EAxis::Y);
		const FVector FlatVel = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
		const float ForwardDot = FVector::DotProduct(Forward, FlatVel);
		const float RightDot = FVector::DotProduct(Right, FlatVel);
		Direction = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));
	}
	else
	{
		Direction = 0.f;
	}

	bIsMorphed = false;
	bIsSprinting = false;
	if (const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Character))
	{
		bIsMorphed = Enemy->IsMorphTarget();
		if (const AEnemyFighter* Fighter = Cast<AEnemyFighter>(Enemy))
		{
			if (Move && Fighter->WalkSpeed > 1.f)
			{
				bIsSprinting = Move->MaxWalkSpeed > Fighter->WalkSpeed * 1.25f;
			}
		}
	}

	UpdateGait();
	UpdateTrajectory(DeltaSeconds);
	SelectActiveDatabase();
}

void UPhoebeAnimInstance::OnPhoebeMotionMatchingUpdated(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	FMotionMatchingAnimNodeReference MMRef;
	bool bConverted = false;
	UMotionMatchingAnimNodeLibrary::ConvertToMotionMatchingNodePure(Node, MMRef, bConverted);
	if (!bConverted || !ActiveDatabase)
	{
		return;
	}

	EPoseSearchInterruptMode InterruptMode = EPoseSearchInterruptMode::DoNotInterrupt;
	if (ActiveDatabase != LastSearchedDatabase)
	{
		const bool bFromIdle = LastSearchedDatabase && LastSearchedDatabase == IdleDatabase;
		const bool bToIdle = ActiveDatabase == IdleDatabase;
		const bool bIdleEdge = bFromIdle != bToIdle;
		const bool bFromGround = LastSearchedDatabase
			&& (LastSearchedDatabase == IdleDatabase
				|| LastSearchedDatabase == WalkDatabase
				|| LastSearchedDatabase == RunDatabase
				|| LastSearchedDatabase == SprintDatabase);
		const bool bToGround = ActiveDatabase == IdleDatabase
			|| ActiveDatabase == WalkDatabase
			|| ActiveDatabase == RunDatabase
			|| ActiveDatabase == SprintDatabase;
		const bool bClimbEdge = (ActiveDatabase == ClimbDatabase)
			|| (LastSearchedDatabase && LastSearchedDatabase == ClimbDatabase);
		const bool bAirEdge = (ActiveDatabase == AirDatabase)
			|| (LastSearchedDatabase && LastSearchedDatabase == AirDatabase);
		if (!LastSearchedDatabase || bIdleEdge || bFromGround != bToGround || bClimbEdge || bAirEdge)
		{
			InterruptMode = EPoseSearchInterruptMode::InterruptOnDatabaseChange;
		}
	}
	UMotionMatchingAnimNodeLibrary::SetDatabaseToSearch(MMRef, ActiveDatabase, InterruptMode);
	LastSearchedDatabase = ActiveDatabase;
}

void UPhoebeAnimInstance::UpdateGait()
{
	if (bIsSprinting)
	{
		Gait = EPhoebeGait::Sprint;
		return;
	}

	if (!bHasAcceleration && Speed < WalkSpeedThreshold)
	{
		Gait = EPhoebeGait::Idle;
		return;
	}

	const float ExitRunSpeed = FMath::Max(WalkSpeedThreshold + 1.f, RunSpeedThreshold - 20.f);
	if (Gait == EPhoebeGait::Run || Gait == EPhoebeGait::Sprint)
	{
		Gait = (Speed >= ExitRunSpeed) ? EPhoebeGait::Run : EPhoebeGait::Walk;
		return;
	}

	Gait = (Speed >= RunSpeedThreshold) ? EPhoebeGait::Run : EPhoebeGait::Walk;
}

void UPhoebeAnimInstance::UpdateTrajectory(float DeltaSeconds)
{
	FTransformTrajectory OutTrajectory;
	UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectory(
		this,
		TrajectoryData,
		DeltaSeconds,
		Trajectory,
		DesiredControllerYawLastUpdate,
		OutTrajectory);
	Trajectory = MoveTemp(OutTrajectory);
}

void UPhoebeAnimInstance::SelectActiveDatabase()
{
	if (bIsClimbing && ClimbDatabase)
	{
		ActiveDatabase = ClimbDatabase;
		return;
	}

	UPoseSearchDatabase* Chosen = nullptr;
	if (DatabaseChooser)
	{
		Chosen = Cast<UPoseSearchDatabase>(
			UChooserFunctionLibrary::EvaluateChooser(this, DatabaseChooser, UPoseSearchDatabase::StaticClass()));
	}
	if (Chosen)
	{
		if (!bIsClimbing && Chosen == ClimbDatabase)
		{
			Chosen = nullptr;
		}
		else if (!bIsInAir && !bIsClimbing && Chosen == AirDatabase)
		{
			Chosen = nullptr;
		}
	}
	if (!Chosen)
	{
		Chosen = SelectFallbackDatabase();
	}
	ActiveDatabase = Chosen;
}

UPoseSearchDatabase* UPhoebeAnimInstance::SelectFallbackDatabase() const
{
	if (bIsClimbing && ClimbDatabase)
	{
		return ClimbDatabase;
	}
	if (bIsInAir && AirDatabase)
	{
		return AirDatabase;
	}
	switch (Gait)
	{
	case EPhoebeGait::Walk:
		return WalkDatabase ? WalkDatabase.Get() : IdleDatabase.Get();
	case EPhoebeGait::Run:
		return RunDatabase ? RunDatabase.Get() : (WalkDatabase ? WalkDatabase.Get() : IdleDatabase.Get());
	case EPhoebeGait::Sprint:
		return SprintDatabase ? SprintDatabase.Get() : (RunDatabase ? RunDatabase.Get() : WalkDatabase.Get());
	default:
		return IdleDatabase.Get();
	}
}
