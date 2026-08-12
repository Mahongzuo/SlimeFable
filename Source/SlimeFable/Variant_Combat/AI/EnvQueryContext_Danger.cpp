// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Combat/AI/EnvQueryContext_Danger.h"
#include "Variant_Combat/AI/CombatEnemy.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"

void UEnvQueryContext_Danger::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	// get the querying enemy
	if (ACombatEnemy* QuerierActor = Cast<ACombatEnemy>(QueryInstance.Owner.Get()))
	{
		// add the last recorded danger location to the context
		UEnvQueryItemType_Point::SetContextHelper(ContextData, QuerierActor->GetLastDangerLocation());
	}
}