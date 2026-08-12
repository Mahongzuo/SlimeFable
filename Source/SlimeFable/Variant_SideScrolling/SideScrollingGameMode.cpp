// Copyright Epic Games, Inc. All Rights Reserved.


#include "SideScrollingGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "SideScrollingUI.h"
#include "SideScrollingPickup.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/World.h"

void ASideScrollingGameMode::BeginPlay()
{
	Super::BeginPlay();

	// create the game UI
	APlayerController* OwningPlayer = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	
	UserInterface = CreateWidget<USideScrollingUI>(OwningPlayer, UserInterfaceClass);

	// create each additional local player.
	// Player 0 will be created automatically as part of regular game init
	for (int32 i = 2; i <= NumberOfLocalPlayers; ++i)
	{
		UGameplayStatics::CreatePlayer(GetWorld(), -1, true);
	}
}

AActor* ASideScrollingGameMode::ChoosePlayerStart_Implementation(AController* Player)
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

void ASideScrollingGameMode::ProcessPickup()
{
	// increment the pickups counter
	++PickupsCollected;

	if (UserInterface)
	{
		// if this is the first pickup we collect, show the UI
		if (PickupsCollected == 1)
		{
		
			UserInterface->AddToViewport(0);
		}

		// update the pickups counter on the UI
		UserInterface->UpdatePickups(PickupsCollected);
	}
	
}