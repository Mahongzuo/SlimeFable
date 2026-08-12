// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PlatformingGameMode.generated.h"

/**
 *  Simple GameMode for a third person platforming game
 */
UCLASS()
class APlatformingGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	APlatformingGameMode();

protected:

	/** Initialization */
	virtual void BeginPlay() override;

	/** Assigns a PlayerStart to a specific player */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

protected:

	/** Determines how many local players should be spawned on game start */
	UPROPERTY(EditDefaultsOnly, Category="Local Multiplayer", meta = (ClampMin = 1, ClampMax = 4))
	int32 NumberOfLocalPlayers = 1;

	/** Used to assign players to different PlayerStarts in the level */
	int32 CurrentPlayerStartAssignment = 0;
};
