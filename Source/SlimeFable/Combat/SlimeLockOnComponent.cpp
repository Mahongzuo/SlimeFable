// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeLockOnComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/OverlapResult.h"
#include "EnemyCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SlimeCharacter.h"
#include "SlimeFableCharacter.h"
#include "SlimeHealthComponent.h"
#include "SlimeLockTarget.h"
#include "Settings/SlimeInputSettings.h"
#include "Settings/SlimeInputTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "SlimeVehicleComponent.h"
#include "SlimeFablePlayerController.h"

USlimeLockOnComponent::USlimeLockOnComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USlimeLockOnComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool USlimeLockOnComponent::IsLockedByLocalPlayer(const UObject* WorldContextObject, const AActor* Target)
{
	if (!Target)
	{
		return false;
	}
	APawn* Player = UGameplayStatics::GetPlayerPawn(WorldContextObject, 0);
	if (!Player)
	{
		return false;
	}
	const USlimeLockOnComponent* Lock = Player->FindComponentByClass<USlimeLockOnComponent>();
	return Lock && Lock->GetLockedTarget() == Target;
}

void USlimeLockOnComponent::BindInput(UEnhancedInputComponent* EnhancedInput)
{
	if (EnhancedInput && LockOnAction)
	{
		EnhancedInput->BindAction(LockOnAction, ETriggerEvent::Started, this, &USlimeLockOnComponent::ToggleLockOn);
		bLockOnActionBound = true;
		bPollLockOnKey = false;
	}
}

APlayerController* USlimeLockOnComponent::GetPlayerController() const
{
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		return Cast<APlayerController>(Pawn->GetController());
	}
	return nullptr;
}

bool USlimeLockOnComponent::CanAcquireLock() const
{
	const UWorld* World = GetWorld();
	return !World || World->GetTimeSeconds() >= RelockBlockUntil;
}

void USlimeLockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsPlayerControlled())
	{
		return;
	}

	if (bPollLockOnKey && !bLockOnActionBound)
	{
		if (APlayerController* PC = GetPlayerController())
		{
			bool bDown = false;
			const USlimeInputSettings* InputSettings = nullptr;
			if (const UWorld* World = GetWorld())
			{
				if (const UGameInstance* GI = World->GetGameInstance())
				{
					InputSettings = GI->GetSubsystem<USlimeInputSettings>();
				}
			}
			if (InputSettings)
			{
				bDown = InputSettings->IsKeyDown(PC, ESlimeInputAction::LockOn);
			}
			else
			{
				bDown = PC->IsInputKeyDown(EKeys::MiddleMouseButton);
			}

			if (bDown && !bPollLockDown)
			{
				ToggleLockOn();
			}
			bPollLockDown = bDown;
		}
	}

	if (!LockedTarget.IsValid())
	{
		return;
	}

	AActor* Target = LockedTarget.Get();
	bool bKeep = true;
	if (const ISlimeLockTarget* LockTarget = Cast<ISlimeLockTarget>(Target))
	{
		bKeep = LockTarget->CanBeLockedOn();
	}
	if (const USlimeHealthComponent* CachedHealth = LockedHealth.Get())
	{
		bKeep = bKeep && CachedHealth->IsAlive();
	}
	else if (const USlimeHealthComponent* TargetHealth = Target->FindComponentByClass<USlimeHealthComponent>())
	{
		bKeep = bKeep && TargetHealth->IsAlive();
	}
	const float Dist = FVector::Dist(OwnerPawn->GetActorLocation(), Target->GetActorLocation());
	if (!bKeep || Dist > BreakRange)
	{
		ClearLockOn();
		return;
	}

	ApplyLockCamera(DeltaTime);
}

void USlimeLockOnComponent::BeginLockCameraFraming()
{
	if (ASlimeCharacter* Slime = Cast<ASlimeCharacter>(GetOwner()))
	{
		if (!bHaveSavedArmLength)
		{
			SavedDesiredArmLength = Slime->GetDesiredCameraArmLength();
			bHaveSavedArmLength = true;
		}
		Slime->SetLockOnFramingActive(true);
		Slime->SetLockOnFramingArm(Slime->CameraArmLengthMin, LockOnArmLengthMax);
	}
}

void USlimeLockOnComponent::ToggleLockOn()
{
	if (IsLockedOn())
	{
		ClearLockOn();
		return;
	}

	if (!CanAcquireLock())
	{
		return;
	}

	if (const AActor* Owner = GetOwner())
	{
		if (const APawn* OwnerPawn = Cast<APawn>(Owner))
		{
			if (const ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(OwnerPawn->GetController()))
			{
				if (SlimePC->HasModalUI())
				{
					return;
				}
			}
		}
		if (const USlimeVehicleComponent* Vehicle = Owner->FindComponentByClass<USlimeVehicleComponent>())
		{
			if (Vehicle->IsUsingVehicle())
			{
				return;
			}
		}
	}

	if (AActor* Target = FindBestTarget())
	{
		LockedTarget = Target;
		LockedHealth = Target->FindComponentByClass<USlimeHealthComponent>();
		BeginLockCameraFraming();
		if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				if (!bHaveSavedMovement)
				{
					bSavedOrientToMovement = Movement->bOrientRotationToMovement;
					bHaveSavedMovement = true;
				}
				Movement->bOrientRotationToMovement = false;
			}
		}
	}
}

void USlimeLockOnComponent::ClearLockOn()
{
	const bool bHadLock = LockedTarget.IsValid();
	LockedTarget.Reset();
	LockedHealth.Reset();
	RestoreMovement();
	RestoreCameraBoom();
	if (APlayerController* PC = GetPlayerController())
	{
		FRotator Rot = PC->GetControlRotation();
		Rot.Pitch = 0.f;
		Rot.Roll = 0.f;
		PC->SetControlRotation(Rot);
	}
	if (bHadLock)
	{
		if (const UWorld* World = GetWorld())
		{
			RelockBlockUntil = World->GetTimeSeconds() + FMath::Max(RelockCooldown, 0.f);
		}
	}
}

void USlimeLockOnComponent::RestoreCameraBoom()
{
	AppliedFramingFloorArm = 0.f;
	FramingFloorArm = 120.f;
	if (ASlimeCharacter* Slime = Cast<ASlimeCharacter>(GetOwner()))
	{
		Slime->SetLockOnFramingActive(false);
		if (bHaveSavedArmLength)
		{
			Slime->SetDesiredCameraArmLengthClamped(SavedDesiredArmLength);
			bHaveSavedArmLength = false;
		}
	}

	if (bHaveSavedBoom)
	{
		if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			USpringArmComponent* Boom = nullptr;
			if (ASlimeFableCharacter* SlimeBase = Cast<ASlimeFableCharacter>(Character))
			{
				Boom = SlimeBase->GetCameraBoom();
			}
			if (!Boom)
			{
				Boom = Character->FindComponentByClass<USpringArmComponent>();
			}
			if (Boom)
			{
				Boom->SocketOffset = SavedBoomSocketOffset;
			}
		}
		bHaveSavedBoom = false;
	}
}

void USlimeLockOnComponent::RestoreMovement()
{
	if (!bHaveSavedMovement)
	{
		return;
	}
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->bOrientRotationToMovement = bSavedOrientToMovement;
		}
	}
	bHaveSavedMovement = false;
}

AActor* USlimeLockOnComponent::FindBestTarget() const
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APlayerController* PC = GetPlayerController();
	UWorld* World = GetWorld();
	if (!OwnerPawn || !World)
	{
		return nullptr;
	}

	FVector ViewLoc = OwnerPawn->GetActorLocation();
	FVector ViewDir = OwnerPawn->GetActorForwardVector();
	if (PC)
	{
		FRotator ViewRot;
		PC->GetPlayerViewPoint(ViewLoc, ViewRot);
		ViewDir = ViewRot.Vector();
	}

	const FVector OwnerLoc = OwnerPawn->GetActorLocation();
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeLockOn), false, OwnerPawn);
	World->OverlapMultiByObjectType(
		Overlaps,
		OwnerLoc,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(AcquireRange),
		Params);

	AActor* Best = nullptr;
	float BestScore = -1.f;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (!Candidate || Candidate == OwnerPawn)
		{
			continue;
		}
		const ISlimeLockTarget* LockTarget = Cast<ISlimeLockTarget>(Candidate);
		if (!LockTarget || !LockTarget->CanBeLockedOn())
		{
			continue;
		}
		if (const USlimeHealthComponent* Health = Candidate->FindComponentByClass<USlimeHealthComponent>())
		{
			if (!Health->IsAlive() || Health->Team == ESlimeTeam::Player)
			{
				continue;
			}
		}
		const FVector TargetLoc = LockTarget->GetLockOnLocation();
		const float Dist = FVector::Dist(OwnerLoc, TargetLoc);
		if (Dist > AcquireRange)
		{
			continue;
		}
		const FVector ToTarget = (TargetLoc - ViewLoc).GetSafeNormal();
		const float Facing = FMath::Max(FVector::DotProduct(ViewDir, ToTarget), 0.f);
		const float Score = Facing * 2.f + (1.f - Dist / AcquireRange);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}
	return Best;
}

void USlimeLockOnComponent::GetActorVerticalSpan(const AActor* Actor, float& OutMinZ, float& OutMaxZ) const
{
	OutMinZ = Actor ? Actor->GetActorLocation().Z : 0.f;
	OutMaxZ = OutMinZ + 100.f;
	if (!Actor)
	{
		return;
	}

	if (const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Actor))
	{
		FBox Box;
		if (Enemy->GetStableMeshBounds(Box))
		{
			OutMinZ = Box.Min.Z;
			OutMaxZ = Box.Max.Z;
			return;
		}
	}

	if (const ACharacter* Character = Cast<ACharacter>(Actor))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			const FVector Loc = Capsule->GetComponentLocation();
			const float Half = Capsule->GetScaledCapsuleHalfHeight();
			OutMinZ = Loc.Z - Half;
			OutMaxZ = Loc.Z + Half;
			return;
		}
	}

	FVector Origin;
	FVector Extent;
	Actor->GetActorBounds(true, Origin, Extent);
	OutMinZ = Origin.Z - Extent.Z;
	OutMaxZ = Origin.Z + Extent.Z;
}

float USlimeLockOnComponent::ComputeFramingArmLength(
	float FrameHeight,
	float HorizontalSep,
	const UCameraComponent* Cam) const
{
	float HalfVFovRad = FMath::DegreesToRadians(35.f);
	if (Cam)
	{
		float Aspect = 16.f / 9.f;
		if (const APlayerController* PC = GetPlayerController())
		{
			int32 SizeX = 0;
			int32 SizeY = 0;
			PC->GetViewportSize(SizeX, SizeY);
			if (SizeX > 0 && SizeY > 0)
			{
				Aspect = static_cast<float>(SizeX) / static_cast<float>(SizeY);
			}
		}
		const float HalfHFovRad = FMath::DegreesToRadians(Cam->FieldOfView * 0.5f);
		HalfVFovRad = FMath::Atan(FMath::Tan(HalfHFovRad) / FMath::Max(Aspect, 0.1f));
	}

	const float TanHalf = FMath::Max(FMath::Tan(HalfVFovRad), 0.05f);
	float Needed = (FrameHeight * 0.5f) / TanHalf * FramingSlack;
	Needed += HorizontalSep * 0.2f;
	return FMath::Min(Needed, LockOnArmLengthMax);
}

void USlimeLockOnComponent::ApplyLockCamera(float DeltaTime)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	APlayerController* PC = GetPlayerController();
	AActor* Target = LockedTarget.Get();
	if (!Character || !PC || !Target)
	{
		return;
	}

	USpringArmComponent* Boom = Character->FindComponentByClass<USpringArmComponent>();
	if (ASlimeFableCharacter* SlimeBase = Cast<ASlimeFableCharacter>(Character))
	{
		if (USpringArmComponent* SlimeBoom = SlimeBase->GetCameraBoom())
		{
			Boom = SlimeBoom;
		}
	}
	if (Boom)
	{
		if (!bHaveSavedBoom)
		{
			SavedBoomSocketOffset = Boom->SocketOffset;
			bHaveSavedBoom = true;
		}
		FVector Offset = SavedBoomSocketOffset;
		Offset.Z = FMath::Max(Offset.Z, LockSocketLiftZ);
		Boom->SocketOffset = Offset;
	}

	float SelfMinZ = 0.f;
	float SelfMaxZ = 0.f;
	float TargetMinZ = 0.f;
	float TargetMaxZ = 0.f;
	GetActorVerticalSpan(Character, SelfMinZ, SelfMaxZ);
	GetActorVerticalSpan(Target, TargetMinZ, TargetMaxZ);

	const float FrameMinZ = FMath::Min(SelfMinZ, TargetMinZ) - FramingPadding;
	const float FrameMaxZ = FMath::Max(SelfMaxZ, TargetMaxZ) + FramingPadding;
	const float FrameHeight = FMath::Max(FrameMaxZ - FrameMinZ, 80.f);

	const FVector SelfLoc = Character->GetActorLocation();
	FVector TargetFocusXY = Target->GetActorLocation();
	if (const ISlimeLockTarget* LockTarget = Cast<ISlimeLockTarget>(Target))
	{
		TargetFocusXY = LockTarget->GetLockOnLocation();
	}
	const float W = FMath::Clamp(FocusEnemyWeight, 0.f, 1.f);
	FVector Focus;
	Focus.X = FMath::Lerp(SelfLoc.X, TargetFocusXY.X, W);
	Focus.Y = FMath::Lerp(SelfLoc.Y, TargetFocusXY.Y, W);
	Focus.Z = (FrameMinZ + FrameMaxZ) * 0.5f;

	const FVector From = Boom
		? Boom->GetComponentLocation()
		: (SelfLoc + FVector(0.f, 0.f, 40.f));

	UCameraComponent* Cam = Character->FindComponentByClass<UCameraComponent>();
	const float HorizontalSep = FVector::Dist2D(SelfLoc, Target->GetActorLocation());
	const float NewFloor = ComputeFramingArmLength(FrameHeight, HorizontalSep, Cam);
	if (AppliedFramingFloorArm <= KINDA_SMALL_NUMBER
		|| NewFloor > AppliedFramingFloorArm + KINDA_SMALL_NUMBER
		|| FMath::Abs(NewFloor - AppliedFramingFloorArm) >= FramingArmHysteresis)
	{
		AppliedFramingFloorArm = NewFloor;
	}
	FramingFloorArm = AppliedFramingFloorArm;

	if (ASlimeCharacter* Slime = Cast<ASlimeCharacter>(Character))
	{
		Slime->SetLockOnFramingActive(true);
		Slime->SetLockOnFramingArm(FramingFloorArm, LockOnArmLengthMax);
		if (Slime->GetDesiredCameraArmLength() < FramingFloorArm)
		{
			Slime->SetDesiredCameraArmLengthClamped(FramingFloorArm);
		}
	}

	FRotator Desired = (Focus - From).Rotation();
	Desired.Pitch = FMath::Clamp(Desired.Pitch, LockPitchMin, LockPitchMax);
	Desired.Roll = 0.f;

	FRotator Current = PC->GetControlRotation();
	Current = FMath::RInterpTo(Current, Desired, DeltaTime, 8.f);
	Current.Pitch = FMath::Clamp(Current.Pitch, LockPitchMin, LockPitchMax);
	Current.Roll = 0.f;
	PC->SetControlRotation(Current);

	FRotator ActorRot = Character->GetActorRotation();
	ActorRot.Yaw = FMath::RInterpTo(ActorRot, FRotator(0.f, Desired.Yaw, 0.f), DeltaTime, 10.f).Yaw;
	Character->SetActorRotation(ActorRot);
}
