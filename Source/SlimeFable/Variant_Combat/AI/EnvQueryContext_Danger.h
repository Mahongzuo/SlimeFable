// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvQueryContext_Danger.generated.h"

/**
 *  UEnvQueryContext_Danger
 *  Returns the enemy character's last known danger location
 */
UCLASS()
class SLIMEFABLE_API UEnvQueryContext_Danger : public UEnvQueryContext
{
	GENERATED_BODY()
	
public:

	/** Provides the context locations or actors for this EnvQuery */
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;

};
