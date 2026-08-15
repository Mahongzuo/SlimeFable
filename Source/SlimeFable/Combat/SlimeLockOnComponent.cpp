// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeLockOnComponent.h"

#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SlimeFableCharacter.h"
#include "SlimeHealthComponent.h"
#include "SlimeLockTarget.h"
#include "Settings/SlimeInputSettings.h"
#include "Settings/SlimeInputTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "SlimeVehicleComponent.h"

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

void USlimeLockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsPlayerControlled())
	{
		return;
	}

	if (bPollLockOnKey)
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
				bDown = InputSettings->WasKeyPressed(PC, ESlimeInputAction::LockOn);
			}
			else
			{
				bDown = PC->WasInputKeyJustPressed(EKeys::MiddleMouseButton);
			}
			if (bDown)
			{
				ToggleLockOn();
			}
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
	if (const USlimeHealthComponent* Health = Target->FindComponentByClass<USlimeHealthComponent>())
	{
		bKeep = bKeep && Health->IsAlive();
	}
	const float Dist = FVector::Dist(OwnerPawn->GetActorLocation(), Target->GetActorLocation());
	if (!bKeep || Dist > BreakRange)
	{
		ClearLockOn();
		return;
	}

	ApplyLockCamera(DeltaTime);
}

void USlimeLockOnComponent::ToggleLockOn()
{
	if (IsLockedOn())
	{
		ClearLockOn();
		return;
	}

	if (const AActor* Owner = GetOwner())
	{
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
	LockedTarget.Reset();
	RestoreMovement();
	RestoreCameraBoom();
}

void USlimeLockOnComponent::RestoreCameraBoom()
{
	if (!bHaveSavedBoom)
	{
		return;
	}
	if (ASlimeFableCharacter* Character = Cast<ASlimeFableCharacter>(GetOwner()))
	{
		if (USpringArmComponent* Boom = Character->GetCameraBoom())
		{
			Boom->SocketOffset = SavedBoomSocketOffset;
		}
	}
	bHaveSavedBoom = false;
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

	TArray<AActor*> Candidates;
	UGameplayStatics::GetAllActorsWithInterface(World, USlimeLockTarget::StaticClass(), Candidates);

	AActor* Best = nullptr;
	float BestScore = -1.f;
	const FVector OwnerLoc = OwnerPawn->GetActorLocation();
	for (AActor* Candidate : Candidates)
	{
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

void USlimeLockOnComponent::ApplyLockCamera(float DeltaTime)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	APlayerController* PC = GetPlayerController();
	AActor* Target = LockedTarget.Get();
	if (!Character || !PC || !Target)
	{
		return;
	}

	if (ASlimeFableCharacter* Slime = Cast<ASlimeFableCharacter>(Character))
	{
		if (USpringArmComponent* Boom = Slime->GetCameraBoom())
		{
			if (!bHaveSavedBoom)
			{
				SavedBoomSocketOffset = Boom->SocketOffset;
				bHaveSavedBoom = true;
			}
			Boom->SocketOffset = FVector(SavedBoomSocketOffset.X, SavedBoomSocketOffset.Y, 110.f);
		}
	}

	UCameraComponent* Cam = Character->FindComponentByClass<UCameraComponent>();
	const FVector From = Cam
		? Cam->GetComponentLocation()
		: Character->GetActorLocation() + FVector(0.f, 0.f, 110.f);
	const FVector Focus = (Character->GetActorLocation() + Target->GetActorLocation()) * 0.5f + FVector(0.f, 0.f, 90.f);
	FRotator Desired = (Focus - From).Rotation();
	Desired.Pitch = FMath::Clamp(Desired.Pitch, -18.f, -8.f);
	Desired.Roll = 0.f;

	FRotator Current = PC->GetControlRotation();
	Current = FMath::RInterpTo(Current, Desired, DeltaTime, 8.f);
	Current.Pitch = FMath::Clamp(Current.Pitch, -18.f, -8.f);
	Current.Roll = 0.f;
	PC->SetControlRotation(Current);

	FRotator ActorRot = Character->GetActorRotation();
	ActorRot.Yaw = FMath::RInterpTo(ActorRot, FRotator(0.f, Desired.Yaw, 0.f), DeltaTime, 10.f).Yaw;
	Character->SetActorRotation(ActorRot);
}
