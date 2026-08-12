// Copyright Epic Games, Inc. All Rights Reserved.


#include "SideScrollingCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "Engine/World.h"
#include "SideScrollingInteractable.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"

ASideScrollingCharacter::ASideScrollingCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// create the camera component
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);

	Camera->SetRelativeLocationAndRotation(FVector(0.0f, 300.0f, 0.0f), FRotator(0.0f, -90.0f, 0.0f));

	// configure the collision capsule
	GetCapsuleComponent()->SetCapsuleSize(35.0f, 90.0f);

	// configure the Pawn properties
	bUseControllerRotationYaw = false;

	// configure the character movement component
	GetCharacterMovement()->GravityScale = 1.75f;
	GetCharacterMovement()->MaxAcceleration = 1500.0f;
	GetCharacterMovement()->BrakingFrictionFactor = 1.0f;
	GetCharacterMovement()->bUseSeparateBrakingFriction = true;
	GetCharacterMovement()->Mass = 500.0f;

	GetCharacterMovement()->SetWalkableFloorAngle(75.0f);
	GetCharacterMovement()->MaxWalkSpeed = 500.0f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f;
	GetCharacterMovement()->bIgnoreBaseRotation = true;

	GetCharacterMovement()->PerchRadiusThreshold = 15.0f;
	GetCharacterMovement()->LedgeCheckThreshold = 6.0f;

	GetCharacterMovement()->JumpZVelocity = 750.0f;
	GetCharacterMovement()->AirControl = 1.0f;

	GetCharacterMovement()->RotationRate = FRotator(0.0f, 750.0f, 0.0f);
	GetCharacterMovement()->bOrientRotationToMovement = true;

	GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.0f, 1.0f, 0.0f));
	GetCharacterMovement()->bConstrainToPlane = true;

	// enable double jump and coyote time
	JumpMaxCount = 3;
}

void ASideScrollingCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the wall jump timer
	GetWorld()->GetTimerManager().ClearTimer(WallJumpTimer);
}

void ASideScrollingCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ASideScrollingCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ASideScrollingCharacter::DoJumpEnd);

		// Interacting
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Triggered, this, &ASideScrollingCharacter::DoInteract);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASideScrollingCharacter::Move);

		// Dropping from platform
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Triggered, this, &ASideScrollingCharacter::Drop);
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Completed, this, &ASideScrollingCharacter::DropReleased);

	}
}

void ASideScrollingCharacter::NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	// only apply push impulse if we're falling
	if (!GetCharacterMovement()->IsFalling())
	{
		return;
	}

	// ensure the colliding component is valid
	if (OtherComp)
	{
		// ensure the component is movable and simulating physics
		if (OtherComp->Mobility == EComponentMobility::Movable && OtherComp->IsSimulatingPhysics())
		{
			const FVector PushDir = FVector(ActionValueY > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);

			// push the component away
			OtherComp->AddImpulse(PushDir * JumpPushImpulse, NAME_None, true);
		}
	}
}

void ASideScrollingCharacter::Landed(const FHitResult& Hit)
{
	// reset the double jump
	bHasDoubleJumped = false;
}

void ASideScrollingCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode /*= 0*/)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	// are we falling?
	if (GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Falling)
	{
		// save the game time when we started falling, so we can check it later for coyote time jumps
		LastFallTime = GetWorld()->GetTimeSeconds();
	}
}

void ASideScrollingCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MoveVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MoveVector.Y);
}

void ASideScrollingCharacter::Drop(const FInputActionValue& Value)
{
	// route the input
	DoDrop(Value.Get<float>());
}

void ASideScrollingCharacter::DropReleased(const FInputActionValue& Value)
{
	// reset the input
	DoDrop(0.0f);
}

void ASideScrollingCharacter::DoMove(float Forward)
{
	// is movement temporarily disabled after wall jumping?
	if (!bHasWallJumped)
	{
		// save the movement values
		ActionValueY = Forward;

		// figure out the movement direction
		const FVector MoveDir = FVector(1.0f, Forward > 0.0f ? 0.1f : -0.1f, 0.0f);

		// apply the movement input
		AddMovementInput(MoveDir, Forward);
	}
}

void ASideScrollingCharacter::DoDrop(float Value)
{
	// save the movement value
	DropValue = Value;
}

void ASideScrollingCharacter::DoJumpStart()
{
	// handle advanced jump behaviors
	MultiJump();
}

void ASideScrollingCharacter::DoJumpEnd()
{
	StopJumping();
}

void ASideScrollingCharacter::DoInteract()
{
	// do a sphere trace to look for interactive objects
	FHitResult OutHit;

	const FVector Start = GetActorLocation();
	const FVector End = Start + FVector(100.0f, 0.0f, 0.0f);

	FCollisionShape ColSphere;
	ColSphere.SetSphere(InteractionRadius);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->SweepSingleByObjectType(OutHit, Start, End, FQuat::Identity, ObjectParams, ColSphere, QueryParams))
	{
		// have we hit an interactable?
		if (ISideScrollingInteractable* Interactable = Cast<ISideScrollingInteractable>(OutHit.GetActor()))
		{
			// interact
			Interactable->Interaction(this);
		}
	}
}

void ASideScrollingCharacter::MultiJump()
{
	// does the user want to drop to a lower platform?
	if (DropValue > 0.0f)
	{
		CheckForSoftCollision();
		return;
	}

	// reset the drop value
	DropValue = 0.0f;

	// if we're grounded, disregard advanced jump logic
	if (!GetCharacterMovement()->IsFalling())
	{
		Jump();
		return;
	}

	// if we have a horizontal input, try for wall jump first
	if (!bHasWallJumped && !FMath::IsNearlyZero(ActionValueY))
	{
		// trace ahead of the character for walls
		FHitResult OutHit;

		const FVector Start = GetActorLocation();
		const FVector End = Start + (FVector(ActionValueY > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f) * WallJumpTraceDistance);

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);

		GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, QueryParams);

		if (OutHit.bBlockingHit)
		{
			// rotate to the bounce direction
			const FRotator BounceRot = UKismetMathLibrary::MakeRotFromX(OutHit.ImpactNormal);
			SetActorRotation(FRotator(0.0f, BounceRot.Yaw, 0.0f));

			// calculate the impulse vector
			FVector WallJumpImpulse = OutHit.ImpactNormal * WallJumpHorizontalImpulse;
			WallJumpImpulse.Z = GetCharacterMovement()->JumpZVelocity * WallJumpVerticalMultiplier;

			// launch the character away from the wall
			LaunchCharacter(WallJumpImpulse, true, true);

			// enable wall jump lockout for a bit
			bHasWallJumped = true;

			// schedule wall jump lockout reset
			GetWorld()->GetTimerManager().SetTimer(WallJumpTimer, this, &ASideScrollingCharacter::ResetWallJump, DelayBetweenWallJumps, false);

			return;
		}
	}



	// test for double jump only if we haven't already tested for wall jump
	if (!bHasWallJumped)
	{
		// are we still within coyote time frames?
		if (GetWorld()->GetTimeSeconds() - LastFallTime < MaxCoyoteTime)
		{
			UE_LOG(LogTemp, Warning, TEXT("Coyote Jump"));

			// use the built-in CMC functionality to do the jump
			Jump();

		// no coyote time jump
		} else {
		
			// The movement component handles double jump but we still need to manage the flag for animation
			if (!bHasDoubleJumped)
			{
				// raise the double jump flag
				bHasDoubleJumped = true;

				// let the CMC handle jump
				Jump();
			}
		}
	}
}

void ASideScrollingCharacter::CheckForSoftCollision()
{
	// reset the drop value
	DropValue = 0.0f;

	// trace down 
	FHitResult OutHit;

	const FVector Start = GetActorLocation();
	const FVector End = Start + (FVector::DownVector * SoftCollisionTraceDistance);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(SoftCollisionObjectType);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByObjectType(OutHit, Start, End, ObjectParams, QueryParams);

	// did we hit a soft floor?
	if (OutHit.GetActor())
	{
		// drop through the floor
		SetSoftCollision(true);
	}
}

void ASideScrollingCharacter::ResetWallJump()
{
	// reset the wall jump flag
	bHasWallJumped = false;
}

void ASideScrollingCharacter::SetSoftCollision(bool bEnabled)
{
	// enable or disable collision response to the soft collision channel
	GetCapsuleComponent()->SetCollisionResponseToChannel(SoftCollisionObjectType, bEnabled ? ECR_Ignore : ECR_Block);
}

bool ASideScrollingCharacter::HasDoubleJumped() const
{
	return bHasDoubleJumped;
}

bool ASideScrollingCharacter::HasWallJumped() const
{
	return bHasWallJumped;
}
