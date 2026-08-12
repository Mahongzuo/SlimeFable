// Copyright Epic Games, Inc. All Rights Reserved.


#include "SideScrollingJumpPad.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SceneComponent.h"

ASideScrollingJumpPad::ASideScrollingJumpPad()
{
	PrimaryActorTick.bCanEverTick = false;

	// create the root comp
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the bounding box
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetupAttachment(RootComponent);

	// configure the bounding box
	Box->SetBoxExtent(FVector(115.0f, 90.0f, 20.0f), false);
	Box->SetRelativeLocation(FVector(0.0f, 0.0f, 16.0f));

	Box->SetCollisionObjectType(ECC_WorldDynamic);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// add the overlap handler
	OnActorBeginOverlap.AddDynamic(this, &ASideScrollingJumpPad::BeginOverlap);
}

void ASideScrollingJumpPad::BeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	// were we overlapped by a character?
	if (ACharacter* OverlappingCharacter = Cast<ACharacter>(OtherActor))
	{
		// force the character to jump
		OverlappingCharacter->Jump();

		// launch the character to override its vertical velocity
		FVector LaunchVelocity = FVector::UpVector * ZStrength;
		OverlappingCharacter->LaunchCharacter(LaunchVelocity, false, true);
	}
}
