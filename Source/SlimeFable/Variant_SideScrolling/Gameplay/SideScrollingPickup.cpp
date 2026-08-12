// Copyright Epic Games, Inc. All Rights Reserved.


#include "SideScrollingPickup.h"
#include "GameFramework/Character.h"
#include "SideScrollingGameMode.h"
#include "Components/SphereComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

ASideScrollingPickup::ASideScrollingPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	// create the root comp
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the bounding sphere
	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Sphere->SetupAttachment(RootComponent);

	Sphere->SetSphereRadius(100.0f);

	Sphere->SetCollisionObjectType(ECC_WorldDynamic);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// add the overlap handler
	OnActorBeginOverlap.AddDynamic(this, &ASideScrollingPickup::BeginOverlap);
}

void ASideScrollingPickup::BeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	// have we collided against a character?
	if (ACharacter* OverlappedCharacter = Cast<ACharacter>(OtherActor))
	{
		// is this the player character?
		if (OverlappedCharacter->IsPlayerControlled())
		{
			// get the game mode
			if (ASideScrollingGameMode* GM = Cast<ASideScrollingGameMode>(GetWorld()->GetAuthGameMode()))
			{
				// tell the game mode to process a pickup
				GM->ProcessPickup();

				// disable collision so we don't get picked up again
				SetActorEnableCollision(false);

				// Call the BP handler. It will be responsible for destroying the pickup
				BP_OnPickedUp();
			}
		}
	}
}