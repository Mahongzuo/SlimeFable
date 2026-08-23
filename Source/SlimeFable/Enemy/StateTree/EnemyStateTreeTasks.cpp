#include "EnemyStateTreeTasks.h"

#include "AIController.h"
#include "EnemyCharacter.h"
#include "EnemyCombatComponent.h"
#include "EnemyEncounterSubsystem.h"
#include "EnemyFighter.h"
#include "Kismet/GameplayStatics.h"
#include "StateTreeExecutionContext.h"
#include "SlimeHealthComponent.h"
#include "Navigation/PathFollowingComponent.h"

EStateTreeRunStatus FSTTask_FindCombatTarget::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.Target = Data.Enemy ? UGameplayStatics::GetPlayerPawn(Data.Enemy, 0) : nullptr;
	return Data.Target ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FSTTask_RequestAttackSlot::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	return Data.Enemy && Data.Enemy->RequestAttackSlot(Data.Duration) ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FSTTask_MoveToCombatPosition::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.Enemy || !Data.Controller)
	{
		return EStateTreeRunStatus::Failed;
	}
	const FVector Offset = Data.Position == EEnemyCombatPosition::Melee ? FVector(220.f, 0.f, 0.f)
		: (Data.Position == EEnemyCombatPosition::Ranged ? FVector(-280.f, 0.f, 0.f)
		: (Data.Position == EEnemyCombatPosition::Flank ? FVector(0.f, 320.f, 0.f) : FVector(-350.f, 0.f, 0.f)));
	const EPathFollowingRequestResult::Type Result = Data.Controller->MoveToLocation(Data.Enemy->GetActorLocation() + Offset, 50.f);
	return Result == EPathFollowingRequestResult::Failed ? EStateTreeRunStatus::Failed : EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FSTTask_TelegraphAbility::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	return Context.GetInstanceData(*this).Enemy ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FSTTask_ActivateEnemyAbility::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	AEnemyFighter* Fighter = Data.Enemy ? Cast<AEnemyFighter>(Data.Enemy) : nullptr;
	if (!Fighter || !Data.Enemy->GetEnemyCombat())
	{
		return EStateTreeRunStatus::Failed;
	}
	for (const FEnemyMoveDef& Move : Fighter->GetMoves())
	{
		if ((Data.AbilityId.IsNone() || Move.MoveId == Data.AbilityId)
			&& Data.Enemy->GetEnemyCombat()->TryExecute(Move.Skill))
		{
			return EStateTreeRunStatus::Succeeded;
		}
	}
	return EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FSTTask_RecoverFromAttack::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.Enemy || !Data.Enemy->GetEnemyCombat())
	{
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTTask_RecoverFromAttack::Tick(FStateTreeExecutionContext& Context, const float) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.Enemy || !Data.Enemy->GetEnemyCombat())
	{
		return EStateTreeRunStatus::Failed;
	}
	return Data.Enemy->GetEnemyCombat()->IsAttacking() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FSTTask_ReactToCombatEvent::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	return Data.Enemy && (Data.Enemy->IsStaggered() || !Data.Enemy->GetEnemyHealth()->IsAlive())
		? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

void FSTEvaluator_EnemyThreat::Tick(FStateTreeExecutionContext& Context, const float) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.Target = Data.Enemy ? UGameplayStatics::GetPlayerPawn(Data.Enemy, 0) : nullptr;
	Data.Threat = Data.Target && Data.Enemy ? 1.f / FMath::Max(FVector::Dist(Data.Target->GetActorLocation(), Data.Enemy->GetActorLocation()), 1.f) * 1000.f : 0.f;
}

void FSTEvaluator_BossPhase::Tick(FStateTreeExecutionContext& Context, const float) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.Phase = Data.Enemy ? Data.Enemy->GetEncounterPhase() : 1;
}

void FSTEvaluator_EncounterPressure::Tick(FStateTreeExecutionContext& Context, const float) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.Pressure = Data.Enemy && Data.Enemy->GetWorld()
		? Data.Enemy->GetWorld()->GetSubsystem<UEnemyEncounterSubsystem>()->GetPressure() : 0.f;
}
