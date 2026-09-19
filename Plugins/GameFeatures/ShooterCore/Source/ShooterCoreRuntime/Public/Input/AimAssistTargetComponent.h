// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/CapsuleComponent.h"
#include "GameplayTagContainer.h"
#include "IAimAssistTargetInterface.h"

#include "AimAssistTargetComponent.generated.h"

#define UE_API SHOOTERCORERUNTIME_API

class UObject;

/**
 * This component can be added to any actor to have it register with the Aim Assist Target Manager.
 */
UCLASS(MinimalAPI, BlueprintType, meta=(BlueprintSpawnableComponent))
class UAimAssistTargetComponent : public UCapsuleComponent, public IAimAssistTaget
{
	GENERATED_BODY()

public:
	
	//~ Begin IAimAssistTaget interface
	UE_API virtual void GatherTargetOptions(OUT FAimAssistTargetOptions& TargetData) override;
	//~ End IAimAssistTaget interface
	
protected:
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FAimAssistTargetOptions TargetData {};
};

#undef UE_API
