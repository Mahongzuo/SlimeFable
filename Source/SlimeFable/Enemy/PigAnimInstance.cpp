// Copyright Epic Games, Inc. All Rights Reserved.

#include "PigAnimInstance.h"

#include "Animation/AnimSequence.h"
#include "EnemyCharacter.h"
#include "EnemyCombatComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UPigAnimInstance::UPigAnimInstance()
{
}

void UPigAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	IdleStillSeconds = 0.f;
	IdleVariantRemaining = 0.f;
	if (!IdleSequence && IdleVariants.Num() > 0)
	{
		IdleSequence = IdleVariants[0];
	}
}

void UPigAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	APawn* Pawn = TryGetPawnOwner();
	ACharacter* Character = Cast<ACharacter>(Pawn);
	if (!Character)
	{
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
	Speed = FMath::Max(Velocity.Size2D(), InputMag * MaxSpd);
	if (Speed < 1.f)
	{
		Speed = FMath::Max(Speed, Velocity.Size());
	}

	bHasAcceleration = (Move && !Move->GetCurrentAcceleration().IsNearlyZero(1.f))
		|| InputMag > 0.05f;

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

	bIsInCombat = false;
	bIsAttacking = false;
	bIsHit = false;
	bIsDead = false;
	if (const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Character))
	{
		bIsInCombat = Enemy->IsInCombat();
		bIsHit = Enemy->IsStaggered();
		bIsDead = Enemy->IsInDeathSequence();
		if (const UEnemyCombatComponent* Combat = Enemy->GetEnemyCombat())
		{
			bIsAttacking = Combat->IsAttacking();
		}
	}

	UpdateGait();
	bIsMoving = Gait != EPigGait::Idle;
	bIsRunning = Gait == EPigGait::Run;

	UpdateRest(DeltaSeconds);
	UpdateIdleVariant(DeltaSeconds);
}

void UPigAnimInstance::UpdateGait()
{
	if (!bHasAcceleration && Speed < WalkSpeedThreshold)
	{
		Gait = EPigGait::Idle;
		return;
	}

	const float ExitRunSpeed = FMath::Max(WalkSpeedThreshold + 1.f, RunSpeedThreshold - 40.f);
	if (Gait == EPigGait::Run)
	{
		Gait = (Speed >= ExitRunSpeed) ? EPigGait::Run : EPigGait::Walk;
		return;
	}

	Gait = (Speed >= RunSpeedThreshold) ? EPigGait::Run : EPigGait::Walk;
}

void UPigAnimInstance::UpdateRest(float DeltaSeconds)
{
	if (bIsDead || bIsHit || bIsInCombat || bIsMoving)
	{
		IdleStillSeconds = 0.f;
		bWantsRest = false;
		return;
	}

	IdleStillSeconds += DeltaSeconds;
	if (IdleStillSeconds >= RestIdleSeconds)
	{
		bWantsRest = true;
	}
}

void UPigAnimInstance::UpdateIdleVariant(float DeltaSeconds)
{
	if (IdleVariants.Num() == 0)
	{
		return;
	}
	if (bIsMoving || bWantsRest || bIsHit || bIsDead)
	{
		return;
	}

	IdleVariantRemaining -= DeltaSeconds;
	if (IdleVariantRemaining > 0.f && IdleSequence)
	{
		return;
	}

	const int32 Index = FMath::RandRange(0, IdleVariants.Num() - 1);
	IdleSequence = IdleVariants[Index];
	IdleVariantRemaining = IdleSequence ? FMath::Max(IdleSequence->GetPlayLength(), 1.5f) : 3.f;
}
