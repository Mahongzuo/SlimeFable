// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Platforming/PlatformingGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/World.h"

APlatformingGameMode::APlatformingGameMode()
{
	// stub
}

void APlatformingGameMode::BeginPlay()
{
	Super::BeginPlay();

	// create each additional local player.
	// Player 0 will be created automatically as part of regular game init
	for (int32 i = 2; i <= NumberOfLocalPlayers; ++i)
	{
		UGameplayStatics::CreatePlayer(GetWorld(), -1, true);
	}
}

AActor* APlatformingGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// build the current player tag
	FName PlayerTag = FName(*FString::Printf(TEXT("Player%d"), CurrentPlayerStartAssignment));

	// find all player starts with the matching player tag
	TArray<AActor*> PlayerStarts;

	UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), APlayerStart::StaticClass(), PlayerTag, PlayerStarts);

	// increment the player start assignment index
	++CurrentPlayerStartAssignment;

	// if no PlayerStarts were found, default to all PlayerStarts instead
	if (PlayerStarts.IsEmpty())
	{
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), PlayerStarts);
	}

	// have we found at least one PlayerStart?
	if (!PlayerStarts.IsEmpty())
	{
		return PlayerStarts[ FMath::RandRange(0, PlayerStarts.Num() - 1) ];
	}

	// no PlayerStarts in the level
	return nullptr;
}
