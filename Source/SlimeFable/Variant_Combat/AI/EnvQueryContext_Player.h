// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvQueryContext_Player.generated.h"

/**
 *  UEnvQueryContext_Player
 *  Basic EnvQuery Context that returns the first local player
 */
UCLASS()
class UEnvQueryContext_Player : public UEnvQueryContext
{
	GENERATED_BODY()
	
public:

	/** Provides the context locations or actors for this EnvQuery */
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
