// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hub/SlimeHubInteractActor.h"

bool ASlimeHubInteractActor::TryInteract(APawn* Interactor)
{
	return Interactor != nullptr;
}

FText ASlimeHubInteractActor::GetInteractPromptVerb() const
{
	return FText::FromString(TEXT("互动"));
}

FVector ASlimeHubInteractActor::GetPromptWorldLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, 120.f);
}
