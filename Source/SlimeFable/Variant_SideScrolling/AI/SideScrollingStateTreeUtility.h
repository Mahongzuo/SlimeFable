// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"

#include "SideScrollingStateTreeUtility.generated.h"

class AAIController;

/**
 *  Instance data for the FStateTreeGetPlayerTask task
 */
USTRUCT()
struct FStateTreeGetPlayerInstanceData
{
	GENERATED_BODY()

	/** NPC owning this task */
	UPROPERTY(VisibleAnywhere, Category="Context")
	TObjectPtr<APawn> NPC;

	/** Holds the found player pawn */
	UPROPERTY(VisibleAnywhere, Category="Context")
	TObjectPtr<AAIController> Controller;

	/** Holds the found player pawn */
	UPROPERTY(VisibleAnywhere, Category="Output")
	TObjectPtr<APawn> TargetPlayer;

	/** Is the pawn close enough to be considered a valid target? */
	UPROPERTY(VisibleAnywhere, Category="Output")
	bool bValidTarget = false;

	/** Max distance to be considered a valid target */
	UPROPERTY(EditAnywhere, Category="Parameter", meta = (ClampMin = 0, ClampMax = 10000, Units = "cm"))
	float RangeMax = 1000.0f;
};

/**
 *  StateTree task to get the player-controlled character
 */
USTRUCT(meta=(DisplayName="Get Player", Category="Side Scrolling"))
struct FStateTreeGetPlayerTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FStateTreeGetPlayerTask()
	{
		// disable tick
		bShouldCallTick = false;

		// skip state change events if this is sustained
		bShouldStateChangeOnReselect = false;
	}

	/* Ensure we're using the correct instance data struct */
	using FInstanceDataType = FStateTreeGetPlayerInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
		
	/** Runs when the owning state is entered */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR
};