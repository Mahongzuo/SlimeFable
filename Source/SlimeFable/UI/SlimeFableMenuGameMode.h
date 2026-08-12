// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SlimeFableMenuGameMode.generated.h"

/** Hub / main-menu GameMode: no gameplay pawn, UI-driven. */
UCLASS()
class SLIMEFABLE_API ASlimeFableMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASlimeFableMenuGameMode();
};
