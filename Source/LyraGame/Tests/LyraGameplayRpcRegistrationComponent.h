// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "LyraGameplayRpcRegistrationComponent.generated.h"

#define UE_API LYRAGAME_API

UCLASS(MinimalAPI)
class ULyraGameplayRpcRegistrationComponent : public UActorComponent
{
	GENERATED_BODY()
protected:
	static UE_API ULyraGameplayRpcRegistrationComponent* ObjectInstance;

public:
	static UE_API ULyraGameplayRpcRegistrationComponent* GetInstance();
};

#undef UE_API
