// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeVehicleComponent.h"

#include "SlimeCharacter.h"
#include "SlimeClingComponent.h"
#include "SlimeLockOnComponent.h"
#include "Inventory/SlimeVehiclePickup.h"
#include "Settings/SlimeInputSettings.h"
#include "Settings/SlimeInputTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

USlimeVehicleComponent::USlimeVehicleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USlimeVehicleComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!VehicleMesh)
	{
		if (ASlimeCharacter* Slime = Cast<ASlimeCharacter>(GetOwner()))
		{
			VehicleMesh = Slime->GetVehicleMesh();
		}
	}
	SetVehicleMeshVisible(false);
	ApplyVehicleMeshCollision(false);
}

void USlimeVehicleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bUsingVehicle)
	{
		PollVerticalInput();
	}
}

void USlimeVehicleComponent::SetVehicleMeshVisible(bool bVisible)
{
	if (!VehicleMesh)
	{
		return;
	}
	VehicleMesh->SetHiddenInGame(!bVisible);
	VehicleMesh->SetVisibility(bVisible);
}

void USlimeVehicleComponent::ApplyVehicleMeshCollision(bool bEnable)
{
	if (!VehicleMesh)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;

	if (!bEnable)
	{
		if (Capsule)
		{
			VehicleMesh->IgnoreComponentWhenMoving(Capsule, false);
			Capsule->IgnoreComponentWhenMoving(VehicleMesh, false);
		}
		VehicleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		VehicleMesh->SetGenerateOverlapEvents(false);
		return;
	}

	VehicleMesh->SetGenerateOverlapEvents(false);
	VehicleMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	VehicleMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	// Block world / other actors; ignore the slime capsule explicitly.
	VehicleMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	if (Capsule)
	{
		VehicleMesh->IgnoreComponentWhenMoving(Capsule, true);
		Capsule->IgnoreComponentWhenMoving(VehicleMesh, true);
	}
}

void USlimeVehicleComponent::ApplyFlyingMovement(bool bEnable)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}

	if (bEnable)
	{
		SavedMaxFlySpeed = Movement->MaxFlySpeed;
		SavedBrakingFlying = Movement->BrakingDecelerationFlying;
		Movement->MaxFlySpeed = FlySpeed;
		Movement->BrakingDecelerationFlying = 2000.f;
		Movement->SetMovementMode(MOVE_Flying);
	}
	else
	{
		Movement->MaxFlySpeed = SavedMaxFlySpeed;
		Movement->BrakingDecelerationFlying = SavedBrakingFlying;
		Movement->SetMovementMode(MOVE_Falling);
	}
}

bool USlimeVehicleComponent::EnterVehicle(ASlimeVehiclePickup* Pickup)
{
	if (bUsingVehicle || !Pickup)
	{
		return false;
	}

	ASlimeCharacter* Slime = Cast<ASlimeCharacter>(GetOwner());
	if (!Slime)
	{
		return false;
	}

	CachedPickupClass = Pickup->GetClass();
	CachedPickupMesh = Pickup->Mesh ? Pickup->Mesh->GetStaticMesh() : nullptr;
	// Include actor-scale so editor-enlarged pickups stay the same size when mounted/dropped.
	CachedPickupWorldScale = Pickup->Mesh ? Pickup->Mesh->GetComponentScale() : FVector(0.55f);

	if (VehicleMesh && CachedPickupMesh)
	{
		VehicleMesh->SetStaticMesh(CachedPickupMesh);
		VehicleMesh->SetWorldScale3D(CachedPickupWorldScale);
	}

	if (USlimeClingComponent* Cling = Slime->GetSlimeCling())
	{
		Cling->TryDetach();
	}
	if (USlimeLockOnComponent* LockOn = Slime->GetSlimeLockOn())
	{
		LockOn->ClearLockOn();
	}

	ApplyFlyingMovement(true);
	SetVehicleMeshVisible(true);
	ApplyVehicleMeshCollision(true);

	if (MountLiftZ > 0.f)
	{
		const FVector Lifted = Slime->GetActorLocation() + FVector(0.f, 0.f, MountLiftZ);
		Slime->TeleportTo(Lifted, Slime->GetActorRotation(), false, true);
	}

	bUsingVehicle = true;
	return true;
}

ASlimeVehiclePickup* USlimeVehicleComponent::SpawnDroppedPickup() const
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return nullptr;
	}

	UClass* SpawnClass = CachedPickupClass
		? CachedPickupClass.Get()
		: ASlimeVehiclePickup::StaticClass();

	const FVector Loc = Owner->GetActorLocation() + FVector(0.f, 0.f, DropSpawnZOffset);
	const FRotator Rot = Owner->GetActorRotation();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ASlimeVehiclePickup* Dropped = World->SpawnActor<ASlimeVehiclePickup>(SpawnClass, Loc, Rot, Params);
	if (!Dropped)
	{
		return nullptr;
	}

	if (Dropped->Mesh)
	{
		if (CachedPickupMesh)
		{
			Dropped->Mesh->SetStaticMesh(CachedPickupMesh);
		}
		// C++ defaults bake a relative scale into Mesh; reset to 1 and put size on the actor
		// so world scale matches the mounted pickup (actor scale × mesh relative).
		Dropped->Mesh->SetRelativeScale3D(FVector::OneVector);
		Dropped->SetActorScale3D(CachedPickupWorldScale);
	}
	else
	{
		Dropped->SetActorScale3D(CachedPickupWorldScale);
	}

	Dropped->BeginDropped(Cast<APawn>(Owner), 0.4f);
	return Dropped;
}

void USlimeVehicleComponent::ExitVehicle(bool bEjectSlime)
{
	if (!bUsingVehicle)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	bUsingVehicle = false;
	ApplyVehicleMeshCollision(false);
	SetVehicleMeshVisible(false);
	ApplyFlyingMovement(false);

	SpawnDroppedPickup();

	if (bEjectSlime && Character)
	{
		Character->LaunchCharacter(FVector(0.f, 0.f, EjectZVelocity), false, true);
	}
}

void USlimeVehicleComponent::PollVerticalInput()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const USlimeInputSettings* InputSettings = GI ? GI->GetSubsystem<USlimeInputSettings>() : nullptr;

	auto IsDown = [PC, InputSettings](ESlimeInputAction Action, const FKey& Fallback) -> bool
	{
		if (InputSettings)
		{
			return InputSettings->IsKeyDown(PC, Action);
		}
		return PC->IsInputKeyDown(Fallback);
	};

	float Vertical = 0.f;
	// Skill2 default E = up, Skill1 default Q = down.
	if (IsDown(ESlimeInputAction::Skill2, EKeys::E))
	{
		Vertical += VerticalInputScale;
	}
	if (IsDown(ESlimeInputAction::Skill1, EKeys::Q))
	{
		Vertical -= VerticalInputScale;
	}

	if (!FMath::IsNearlyZero(Vertical))
	{
		Pawn->AddMovementInput(FVector::UpVector, Vertical);
	}
}
