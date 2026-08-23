#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeEvaluatorBase.h"
#include "Encounter/EnemyEncounterDefinition.h"
#include "EnemyStateTreeTasks.generated.h"

class AAIController;
class AEnemyCharacter;
class AActor;

USTRUCT()
struct SLIMEFABLE_API FEnemyStateTreeInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEnemyCharacter> Enemy;

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	TObjectPtr<AActor> Target;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FName AbilityId = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (ClampMin = "0.1", Units = "s"))
	float Duration = 1.f;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	EEnemyCombatPosition Position = EEnemyCombatPosition::Melee;
};

#define DECLARE_ENEMY_ST_TASK(Name, Label) \
USTRUCT(meta = (DisplayName = Label, Category = "Enemy Combat")) \
struct SLIMEFABLE_API Name : public FStateTreeTaskCommonBase \
{ \
	GENERATED_BODY() \
	using FInstanceDataType = FEnemyStateTreeInstanceData; \
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); } \
	Name() { bShouldCallTick = false; } \
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override; \
};

// Kept as individual declarations so UHT can index each native StateTree node.
USTRUCT(meta = (DisplayName = "Find Combat Target", Category = "Enemy Combat"))
struct SLIMEFABLE_API FSTTask_FindCombatTarget : public FStateTreeTaskCommonBase { GENERATED_BODY() using FInstanceDataType = FEnemyStateTreeInstanceData; virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); } FSTTask_FindCombatTarget() { bShouldCallTick = false; } virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override; };
USTRUCT(meta = (DisplayName = "Request Attack Slot", Category = "Enemy Combat"))
struct SLIMEFABLE_API FSTTask_RequestAttackSlot : public FStateTreeTaskCommonBase { GENERATED_BODY() using FInstanceDataType = FEnemyStateTreeInstanceData; virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); } FSTTask_RequestAttackSlot() { bShouldCallTick = false; } virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override; };
USTRUCT(meta = (DisplayName = "Move To Combat Position", Category = "Enemy Combat"))
struct SLIMEFABLE_API FSTTask_MoveToCombatPosition : public FStateTreeTaskCommonBase { GENERATED_BODY() using FInstanceDataType = FEnemyStateTreeInstanceData; virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); } FSTTask_MoveToCombatPosition() { bShouldCallTick = false; } virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override; };
USTRUCT(meta = (DisplayName = "Telegraph Enemy Ability", Category = "Enemy Combat"))
struct SLIMEFABLE_API FSTTask_TelegraphAbility : public FStateTreeTaskCommonBase { GENERATED_BODY() using FInstanceDataType = FEnemyStateTreeInstanceData; virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); } FSTTask_TelegraphAbility() { bShouldCallTick = false; } virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override; };
USTRUCT(meta = (DisplayName = "Activate Enemy Ability", Category = "Enemy Combat"))
struct SLIMEFABLE_API FSTTask_ActivateEnemyAbility : public FStateTreeTaskCommonBase { GENERATED_BODY() using FInstanceDataType = FEnemyStateTreeInstanceData; virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); } FSTTask_ActivateEnemyAbility() { bShouldCallTick = false; } virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override; };
USTRUCT(meta = (DisplayName = "Recover From Attack", Category = "Enemy Combat"))
struct SLIMEFABLE_API FSTTask_RecoverFromAttack : public FStateTreeTaskCommonBase { GENERATED_BODY() using FInstanceDataType = FEnemyStateTreeInstanceData; virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); } FSTTask_RecoverFromAttack() { bShouldCallTick = true; } virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override; virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext&, const float) const override; };
USTRUCT(meta = (DisplayName = "React To Combat Event", Category = "Enemy Combat"))
struct SLIMEFABLE_API FSTTask_ReactToCombatEvent : public FStateTreeTaskCommonBase { GENERATED_BODY() using FInstanceDataType = FEnemyStateTreeInstanceData; virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); } FSTTask_ReactToCombatEvent() { bShouldCallTick = false; } virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext&, const FStateTreeTransitionResult&) const override; };

#undef DECLARE_ENEMY_ST_TASK

USTRUCT()
struct SLIMEFABLE_API FEnemyThreatEvaluatorData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Context") TObjectPtr<AEnemyCharacter> Enemy;
	UPROPERTY(VisibleAnywhere, Category = "Output") TObjectPtr<AActor> Target;
	UPROPERTY(VisibleAnywhere, Category = "Output") float Threat = 0.f;
};

USTRUCT(meta = (DisplayName = "Enemy Threat", Category = "Enemy Combat"))
struct SLIMEFABLE_API FSTEvaluator_EnemyThreat : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()
	using FInstanceDataType = FEnemyThreatEvaluatorData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

USTRUCT()
struct SLIMEFABLE_API FEnemyBossPhaseEvaluatorData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Context") TObjectPtr<AEnemyCharacter> Enemy;
	UPROPERTY(VisibleAnywhere, Category = "Output") int32 Phase = 1;
};

USTRUCT(meta = (DisplayName = "Boss Phase", Category = "Enemy Combat"))
struct SLIMEFABLE_API FSTEvaluator_BossPhase : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()
	using FInstanceDataType = FEnemyBossPhaseEvaluatorData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

USTRUCT()
struct SLIMEFABLE_API FEnemyPressureEvaluatorData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Context") TObjectPtr<AEnemyCharacter> Enemy;
	UPROPERTY(VisibleAnywhere, Category = "Output") float Pressure = 0.f;
};

USTRUCT(meta = (DisplayName = "Encounter Pressure", Category = "Enemy Combat"))
struct SLIMEFABLE_API FSTEvaluator_EncounterPressure : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()
	using FInstanceDataType = FEnemyPressureEvaluatorData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
