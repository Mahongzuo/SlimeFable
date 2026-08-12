// Copyright Epic Games, Inc. All Rights Reserved.


#include "CombatStateTreeUtility.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeExecutionTypes.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "CombatEnemy.h"
#include "Kismet/GameplayStatics.h"
#include "StateTreeAsyncExecutionContext.h"

bool FStateTreeCharacterGroundedCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// is the character currently grounded?
	bool bCondition = InstanceData.Character->GetMovementComponent()->IsMovingOnGround();

	return InstanceData.bMustBeOnAir ? !bCondition : bCondition;
}

#if WITH_EDITOR
FText FStateTreeCharacterGroundedCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Is Character Grounded</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

bool FStateTreeIsInDangerCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// ensure we have a valid enemy character
	if (InstanceData.Character)
	{
		// is the last detected danger event within the reaction threshold?
		const float ReactionDelta = InstanceData.Character->GetWorld()->GetTimeSeconds() - InstanceData.Character->GetLastDangerTime();

		if (ReactionDelta < InstanceData.MaxReactionTime && ReactionDelta > InstanceData.MinReactionTime)
		{
			// do a dot product check to determine if the danger location is within the character's detection cone
			const FVector DangerDir = (InstanceData.Character->GetLastDangerLocation() - InstanceData.Character->GetActorLocation()).GetSafeNormal2D();

			const float DangerDot = FVector::DotProduct(DangerDir, InstanceData.Character->GetActorForwardVector());
			const float ConeAngleCos = FMath::Cos(FMath::DegreesToRadians(InstanceData.DangerSightConeAngle));

			return DangerDot > ConeAngleCos;
		}
	}

	return false;
}

#if WITH_EDITOR
FText FStateTreeIsInDangerCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Is Character In Danger</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FStateTreeComboAttackTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// bind to the on attack completed delegate
	InstanceData.Character->OnAttackCompleted.BindLambda(
		[WeakContext = Context.MakeWeakExecutionContext()]()
		{
			WeakContext.FinishTask(EStateTreeFinishTaskType::Succeeded);
		}
	);


	// tell the character to do a combo attack
	InstanceData.Character->DoAIComboAttack();

	return EStateTreeRunStatus::Running;
}

void FStateTreeComboAttackTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// unbind the on attack completed delegate
	InstanceData.Character->OnAttackCompleted.Unbind();
}

#if WITH_EDITOR
FText FStateTreeComboAttackTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Do Combo Attack</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FStateTreeChargedAttackTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// bind to the on attack completed delegate
	InstanceData.Character->OnAttackCompleted.BindLambda(
		[WeakContext = Context.MakeWeakExecutionContext()]()
		{
			WeakContext.FinishTask(EStateTreeFinishTaskType::Succeeded);
		}
	);

	// tell the character to do a charged attack
	InstanceData.Character->DoAIChargedAttack();

	return EStateTreeRunStatus::Running;
}

void FStateTreeChargedAttackTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// unbind the on attack completed delegate
	InstanceData.Character->OnAttackCompleted.Unbind();
}

#if WITH_EDITOR
FText FStateTreeChargedAttackTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Do Charged Attack</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FStateTreeWaitForLandingTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// bind to the on enemy landed delegate
	InstanceData.Character->OnEnemyLanded.BindLambda(
		[WeakContext = Context.MakeWeakExecutionContext()]()
		{
			WeakContext.FinishTask(EStateTreeFinishTaskType::Succeeded);
		}
	);

	return EStateTreeRunStatus::Running;
}

void FStateTreeWaitForLandingTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// unbind the on enemy landed delegate
	InstanceData.Character->OnEnemyLanded.Unbind();
}

#if WITH_EDITOR
FText FStateTreeWaitForLandingTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Wait for Landing</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FStateTreeFaceActorTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// set the AI Controller's focus
	InstanceData.Controller->SetFocus(InstanceData.ActorToFaceTowards);

	return EStateTreeRunStatus::Running;
}

void FStateTreeFaceActorTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// clear the AI Controller's focus
	InstanceData.Controller->ClearFocus(EAIFocusPriority::Gameplay);
}

#if WITH_EDITOR
FText FStateTreeFaceActorTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Face Towards Actor</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FStateTreeFaceLocationTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// set the AI Controller's focus
	InstanceData.Controller->SetFocalPoint(InstanceData.FaceLocation);

	return EStateTreeRunStatus::Running;
}

void FStateTreeFaceLocationTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// clear the AI Controller's focus
	InstanceData.Controller->ClearFocus(EAIFocusPriority::Gameplay);
}

#if WITH_EDITOR
FText FStateTreeFaceLocationTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Face Towards Location</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FStateTreeSetCharacterSpeedTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// set the character's max ground speed
	InstanceData.Character->GetCharacterMovement()->MaxWalkSpeed = InstanceData.Speed;

	return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FStateTreeSetCharacterSpeedTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Set Character Speed</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FStateTreeGetPlayerInfoTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// get the instance data
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		
	// reset the selected target
	ACharacter* SelectedTarget = nullptr;

	// iterate through each local player
	const int32 NumPlayers = UGameplayStatics::GetNumLocalPlayerControllers(InstanceData.Character);

	for (int32 i = 0; i < NumPlayers; ++i)
	{
		if (ACharacter* Current = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(InstanceData.Character, i)))
		{
			// compute the distance to the target
			const float TargetDist = (Current->GetActorLocation() - InstanceData.Character->GetActorLocation()).Size();

			// is this target within range?
			if (TargetDist < InstanceData.MaxRange)
			{
				// have we selected a valid target already?
				if (SelectedTarget)
				{
					// randomly switch to the new target
					if (FMath::RandBool())
					{
						SelectedTarget = Current;
					}
				}
				else
				{
					// no valid target yet, so choose this one
					SelectedTarget = Current;
				}
			}
		}
	}

	// set the new target
	InstanceData.TargetPlayerCharacter = SelectedTarget;

	// if the target is not valid, fail the task
	if (!SelectedTarget)
	{
		return EStateTreeRunStatus::Failed;
	}

	// set the target location and distance
	InstanceData.TargetPlayerLocation = SelectedTarget->GetActorLocation();
	InstanceData.DistanceToTarget = (SelectedTarget->GetActorLocation() - InstanceData.Character->GetActorLocation()).Size();

	// succeed
	return EStateTreeRunStatus::Succeeded;
}

#if WITH_EDITOR
FText FStateTreeGetPlayerInfoTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Get Player Info</b>");
}
#endif // WITH_EDITOR