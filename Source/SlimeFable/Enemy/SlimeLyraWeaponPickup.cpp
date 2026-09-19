// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeLyraWeaponPickup.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Enemy/LyraShooterEnemy.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Equipment/LyraPickupDefinition.h"
#include "Inventory/LyraInventoryItemDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "SlimeFable.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ASlimeLyraWeaponPickup::ASlimeLyraWeaponPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Root);
	Trigger->InitSphereRadius(110.f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);

	// Engine basic shapes are always present; a flat cylinder reads as a "pad".
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(Root);
	if (CylinderFinder.Succeeded())
	{
		PadMesh->SetStaticMesh(CylinderFinder.Object);
	}
	PadMesh->SetRelativeScale3D(FVector(1.6f, 1.6f, 0.06f));
	PadMesh->SetRelativeLocation(FVector(0.f, 0.f, 3.f));
	PadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PadMesh->SetGenerateOverlapEvents(false);

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(Root);
	WeaponMesh->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);
}

void ASlimeLyraWeaponPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyDefinitionVisuals();
}

void ASlimeLyraWeaponPickup::ApplyDefinitionVisuals()
{
	if (!WeaponMesh)
	{
		return;
	}
	if (PickupDefinition)
	{
		WeaponMesh->SetStaticMesh(PickupDefinition->DisplayMesh);
		WeaponMesh->SetRelativeScale3D(PickupDefinition->WeaponMeshScale);
		WeaponMesh->SetRelativeLocation(FVector(0.f, 0.f, 60.f) + PickupDefinition->WeaponMeshOffset);
	}
	else
	{
		WeaponMesh->SetStaticMesh(nullptr);
	}
}

void ASlimeLyraWeaponPickup::BeginPlay()
{
	Super::BeginPlay();
	ApplyDefinitionVisuals();
	if (Trigger)
	{
		Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &ASlimeLyraWeaponPickup::OnTriggerBegin);
	}
	RefreshVisibility();
}

void ASlimeLyraWeaponPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (WeaponMesh && bAvailable && MeshSpinDegPerSec > 0.f)
	{
		WeaponMesh->AddRelativeRotation(FRotator(0.f, MeshSpinDegPerSec * DeltaSeconds, 0.f));
	}
}

void ASlimeLyraWeaponPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASlimeLyraWeaponPickup, bAvailable);
}

void ASlimeLyraWeaponPickup::OnRep_Available()
{
	RefreshVisibility();
}

void ASlimeLyraWeaponPickup::RefreshVisibility()
{
	if (WeaponMesh)
	{
		WeaponMesh->SetVisibility(bAvailable, true);
	}
}

void ASlimeLyraWeaponPickup::OnTriggerBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;
	if (!HasAuthority() || !bAvailable)
	{
		return;
	}
	if (TryGiveTo(OtherActor))
	{
		StartCoolDown();
	}
}

bool ASlimeLyraWeaponPickup::TryGiveTo(AActor* Other)
{
	ALyraShooterEnemy* Body = Cast<ALyraShooterEnemy>(Other);
	if (!Body || !PickupDefinition || !PickupDefinition->InventoryItemDefinition)
	{
		return false;
	}
	if (!Body->IsPlayerControlled() && !bAllowAIPickup)
	{
		return false;
	}
	if (Body->IsInDeathSequence())
	{
		return false;
	}
	const bool bHadIt = Body->FindInventoryItemOfDef(PickupDefinition->InventoryItemDefinition) != nullptr;
	if (bHadIt)
	{
		ULyraInventoryItemInstance* Item = Body->FindInventoryItemOfDef(PickupDefinition->InventoryItemDefinition);
		const int32 Rounds = Body->RefillSpareAmmo(Item, AmmoMagazines);
		if (Body->GetEquippedWeaponItem() != Item)
		{
			Body->EquipInventoryItem(Item);
		}
		UE_LOG(LogSlimeFable, Log, TEXT("WeaponPickup %s: +%d spare ammo for %s"), *GetName(), Rounds, *Body->GetName());
	}
	else
	{
		if (!Body->GiveWeaponItem(PickupDefinition->InventoryItemDefinition, true))
		{
			return false;
		}
		UE_LOG(LogSlimeFable, Log, TEXT("WeaponPickup %s: gave %s to %s"), *GetName(), *GetNameSafe(PickupDefinition->InventoryItemDefinition), *Body->GetName());
	}
	if (PickupDefinition->PickedUpSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupDefinition->PickedUpSound, GetActorLocation());
	}
	return true;
}

void ASlimeLyraWeaponPickup::StartCoolDown()
{
	bAvailable = false;
	RefreshVisibility();
	float Seconds = CoolDownOverride;
	if (Seconds <= 0.f && PickupDefinition)
	{
		Seconds = static_cast<float>(PickupDefinition->SpawnCoolDownSeconds);
	}
	if (Seconds <= 0.f)
	{
		Seconds = 10.f;
	}
	GetWorldTimerManager().SetTimer(CoolDownHandle, this, &ASlimeLyraWeaponPickup::OnCoolDownFinished, Seconds, false);
}

void ASlimeLyraWeaponPickup::OnCoolDownFinished()
{
	bAvailable = true;
	RefreshVisibility();
	if (PickupDefinition && PickupDefinition->RespawnedSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupDefinition->RespawnedSound, GetActorLocation());
	}
	// Somebody may be standing on the pad already.
	if (Trigger)
	{
		TArray<AActor*> Overlapping;
		Trigger->GetOverlappingActors(Overlapping, ALyraShooterEnemy::StaticClass());
		for (AActor* Other : Overlapping)
		{
			if (TryGiveTo(Other))
			{
				StartCoolDown();
				break;
			}
		}
	}
}
