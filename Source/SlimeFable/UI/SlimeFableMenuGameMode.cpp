// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeFableMenuGameMode.h"
#include "UI/SlimeFableMenuPlayerController.h"
#include "GameFramework/SpectatorPawn.h"

ASlimeFableMenuGameMode::ASlimeFableMenuGameMode()
{
	PlayerControllerClass = ASlimeFableMenuPlayerController::StaticClass();
	DefaultPawnClass = ASpectatorPawn::StaticClass();
	bStartPlayersAsSpectators = true;
	HUDClass = nullptr;
}
