// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SideScrollingAIController.generated.h"

class UStateTreeAIComponent;

/**
 *  A basic AI Controller capable of running StateTree
 */
UCLASS(abstract)
class ASideScrollingAIController : public AAIController
{
	GENERATED_BODY()
	
	/** StateTree Component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI", meta = (AllowPrivateAccess = "true"))
	UStateTreeAIComponent* StateTreeAI;

public:

	/** Constructor */
	ASideScrollingAIController();
};
