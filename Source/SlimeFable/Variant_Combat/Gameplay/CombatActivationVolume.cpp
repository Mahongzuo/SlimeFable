// Copyright Epic Games, Inc. All Rights Reserved.


#include "CombatActivationVolume.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "CombatActivatable.h"

ACombatActivationVolume::ACombatActivationVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	// create the box volume
	RootComponent = Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	check(Box);

	// set the box's extent
	Box->SetBoxExtent(FVector(500.0f, 500.0f, 500.0f));

	// set the default collision profile to overlap all dynamic
	Box->SetCollisionProfileName(FName("OverlapAllDynamic"));

	// bind the begin overlap 
	Box->OnComponentBeginOverlap.AddDynamic(this, &ACombatActivationVolume::OnOverlap);
}

void ACombatActivationVolume::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// has a Character entered the volume?
	ACharacter* PlayerCharacter = Cast<ACharacter>(OtherActor);

	if (PlayerCharacter)
	{
		// is the Character controlled by a player
		if (PlayerCharacter->IsPlayerControlled())
		{
			// process the actors to activate list
			for (AActor* CurrentActor : ActorsToActivate)
			{
				// is the referenced actor activatable?
				if(ICombatActivatable* Activatable = Cast<ICombatActivatable>(CurrentActor))
				{
					Activatable->ActivateInteraction(PlayerCharacter);
				}
			}
		}
	}

}