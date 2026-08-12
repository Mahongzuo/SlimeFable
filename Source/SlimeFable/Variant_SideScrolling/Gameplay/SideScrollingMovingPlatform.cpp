// Copyright Epic Games, Inc. All Rights Reserved.


#include "SideScrollingMovingPlatform.h"
#include "Components/SceneComponent.h"

ASideScrollingMovingPlatform::ASideScrollingMovingPlatform()
{
	PrimaryActorTick.bCanEverTick = false;

	// create the root comp
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ASideScrollingMovingPlatform::Interaction(AActor* Interactor)
{
	// ignore interactions if we're already moving
	if (bMoving)
	{
		return;
	}

	// raise the movement flag
	bMoving = true;

	// pass control to BP for the actual movement
	BP_MoveToTarget();
}

void ASideScrollingMovingPlatform::ResetInteraction()
{
	// ignore if this is a one-shot platform
	if (bOneShot)
	{
		return;
	}

	// reset the movement flag
	bMoving = false;
}
