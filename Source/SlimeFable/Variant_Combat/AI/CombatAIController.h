// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "CombatAIController.generated.h"

class UStateTreeAIComponent;

/**
 *	A basic AI Controller capable of running StateTree
 */
UCLASS(abstract)
class ACombatAIController : public AAIController
{
	GENERATED_BODY()

	/** StateTree Component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStateTreeAIComponent* StateTreeAI;

public:

	/** Constructor */
	ACombatAIController();

protected:

	/** Pawn Initialization */
	virtual void OnPossess(APawn* InPawn) override;
};
