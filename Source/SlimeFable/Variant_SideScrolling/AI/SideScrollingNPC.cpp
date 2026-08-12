// Copyright Epic Games, Inc. All Rights Reserved.


#include "SideScrollingNPC.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

ASideScrollingNPC::ASideScrollingNPC()
{
 	PrimaryActorTick.bCanEverTick = true;

	GetCharacterMovement()->MaxWalkSpeed = 150.0f;
}

void ASideScrollingNPC::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the deactivation timer
	GetWorld()->GetTimerManager().ClearTimer(DeactivationTimer);
}

void ASideScrollingNPC::Interaction(AActor* Interactor)
{
	// ignore if this NPC has already been deactivated
	if (bDeactivated)
	{
		return;
	}

	// reset the deactivation flag
	bDeactivated = true;

	// stop character movement immediately
	GetCharacterMovement()->StopMovementImmediately();

	// launch the NPC away from the interactor
	FVector LaunchVector = Interactor->GetActorForwardVector() * LaunchImpulse;
	LaunchVector.Y = 0.0f;
	LaunchVector.Z = LaunchVerticalImpulse;

	LaunchCharacter(LaunchVector, true, true);

	// set up a timer to schedule reactivation
	GetWorld()->GetTimerManager().SetTimer(DeactivationTimer, this, &ASideScrollingNPC::ResetDeactivation, DeactivationTime, false);
}

void ASideScrollingNPC::ResetDeactivation()
{
	// reset the deactivation flag
	bDeactivated = false;
}
