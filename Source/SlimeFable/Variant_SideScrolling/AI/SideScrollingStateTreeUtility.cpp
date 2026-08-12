// Copyright Epic Games, Inc. All Rights Reserved.


#include "SideScrollingStateTreeUtility.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeExecutionTypes.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"

EStateTreeRunStatus FStateTreeGetPlayerTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// reset the selected target
	APawn* SelectedTarget = nullptr;
	float ClosestDistance = 1000000000000000000.0f;

	// iterate through each local player
	const int32 NumPlayers = UGameplayStatics::GetNumLocalPlayerControllers(InstanceData.Controller.Get());

	for (int32 i = 0; i < NumPlayers; ++i)
	{
		if (APawn* Current = UGameplayStatics::GetPlayerPawn(InstanceData.Controller.Get(), i))
		{
			// compute the distance to the target
			const float TargetDist = (Current->GetActorLocation() - InstanceData.NPC->GetActorLocation()).Size();

			// if we haven't selected a target, or this one is closer to the current one, choose this pawn as target
			if (!SelectedTarget || TargetDist < ClosestDistance)
			{
				SelectedTarget = Current;
				ClosestDistance = TargetDist;
			}
		}
	}

	// set the selected target, assume out of range by default
	InstanceData.TargetPlayer = SelectedTarget;
	InstanceData.bValidTarget = false;

	if (SelectedTarget)
	{
		// consider this a valid target if it's within range 
		const float TargetDist = (SelectedTarget->GetActorLocation() - InstanceData.NPC->GetActorLocation()).Size();

		InstanceData.bValidTarget = TargetDist < InstanceData.RangeMax;
	}

	// succeed or fail depending on target validity
	return InstanceData.bValidTarget ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

#if WITH_EDITOR
FText FStateTreeGetPlayerTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Get Player</b>");
}
#endif // WITH_EDITOR