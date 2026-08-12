// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SlimeFableGameMode.generated.h"

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class ASlimeFableGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	ASlimeFableGameMode();
};



