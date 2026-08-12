// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimePlayGameMode.h"

#include "GameFramework/PlayerController.h"
#include "SlimeCharacter.h"
#include "SlimeFable.h"

ASlimePlayGameMode::ASlimePlayGameMode()
{
	DefaultPawnClass = ASlimeCharacter::StaticClass();
	// Concrete fallback: the abstract ASlimeFablePlayerController cannot be spawned.
	PlayerControllerClass = APlayerController::StaticClass();

	SlimePawnClassPath = TSoftClassPtr<APawn>(
		FSoftObjectPath(TEXT("/Game/Characters/Slime/BP_SlimeCharacter.BP_SlimeCharacter_C")));
	PlayerControllerClassPath = TSoftClassPtr<APlayerController>(
		FSoftObjectPath(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C")));

	// Resolve in the constructor so World Settings / CDO show the real Blueprint, not a stub.
	if (UClass* ControllerClass = PlayerControllerClassPath.LoadSynchronous())
	{
		PlayerControllerClass = ControllerClass;
	}
	if (UClass* PawnClass = SlimePawnClassPath.LoadSynchronous())
	{
		DefaultPawnClass = PawnClass;
	}
}

void ASlimePlayGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (!SlimePawnClassPath.IsNull())
	{
		if (UClass* PawnClass = SlimePawnClassPath.LoadSynchronous())
		{
			DefaultPawnClass = PawnClass;
		}
		else
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("SlimePlayGameMode: '%s' is missing; falling back to ASlimeCharacter without Blueprint input bindings."), *SlimePawnClassPath.ToString());
		}
	}

	if (!PlayerControllerClassPath.IsNull())
	{
		if (UClass* ControllerClass = PlayerControllerClassPath.LoadSynchronous())
		{
			PlayerControllerClass = ControllerClass;
		}
		else
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("SlimePlayGameMode: '%s' is missing; falling back to APlayerController (no DefaultMappingContexts)."), *PlayerControllerClassPath.ToString());
			PlayerControllerClass = APlayerController::StaticClass();
		}
	}
}
