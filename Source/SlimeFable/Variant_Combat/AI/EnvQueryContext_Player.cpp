// Copyright Epic Games, Inc. All Rights Reserved.


#include "EnvQueryContext_Player.h"
#include "Kismet/GameplayStatics.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "GameFramework/Pawn.h"

void UEnvQueryContext_Player::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	// get the player pawn for the first local player
	AActor* PlayerPawn = UGameplayStatics::GetPlayerPawn(QueryInstance.Owner.Get(), 0);
	check(PlayerPawn);

	// add the actor data to the context
	UEnvQueryItemType_Actor::SetContextHelper(ContextData, PlayerPawn);
}
