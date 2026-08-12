// Copyright Epic Games, Inc. All Rights Reserved.


#include "CombatCheckpointVolume.h"
#include "CombatCharacter.h"
#include "CombatPlayerController.h"

ACombatCheckpointVolume::ACombatCheckpointVolume()
{
	// create the box volume
	RootComponent = Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	check(Box);

	// set the box's extent
	Box->SetBoxExtent(FVector(500.0f, 500.0f, 500.0f));

	// set the default collision profile to overlap all dynamic
	Box->SetCollisionProfileName(FName("OverlapAllDynamic"));

	// bind the begin overlap 
	Box->OnComponentBeginOverlap.AddDynamic(this, &ACombatCheckpointVolume::OnOverlap);
}

void ACombatCheckpointVolume::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{		
	// has the player entered this volume?
	ACombatCharacter* PlayerCharacter = Cast<ACombatCharacter>(OtherActor);

	if (PlayerCharacter)
	{
		if (ACombatPlayerController* PC = Cast<ACombatPlayerController>(PlayerCharacter->GetController()))
		{
			// update the player's respawn checkpoint
			PC->SetRespawnTransform(PlayerCharacter->GetActorTransform());
		}

	}
}
