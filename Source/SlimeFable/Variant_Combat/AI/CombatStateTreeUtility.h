// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeConditionBase.h"

#include "CombatStateTreeUtility.generated.h"

class ACharacter;
class AAIController;
class ACombatEnemy;

/**
 *  Instance data struct for the FStateTreeCharacterGroundedCondition condition
 */
USTRUCT()
struct FStateTreeCharacterGroundedConditionInstanceData
{
	GENERATED_BODY()
	
	/** Character to check grounded status on */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<ACharacter> Character;

	/** If true, the condition passes if the character is not grounded instead */
	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bMustBeOnAir = false;
};

/**
 *  StateTree condition to check if the character is grounded
 */
USTRUCT(DisplayName = "Character is Grounded")
struct FStateTreeCharacterGroundedCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	/** Set the instance data type */
	using FInstanceDataType = FStateTreeCharacterGroundedConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/** Default constructor */
	FStateTreeCharacterGroundedCondition() = default;
	
	/** Tests the StateTree condition */
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR

	/** Provides the description string */
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

};

////////////////////////////////////////////////////////////////////

/**
 *  Instance data struct for the FStateTreeIsInDangerCondition condition
 */
USTRUCT()
struct FStateTreeIsInDangerConditionInstanceData
{
	GENERATED_BODY()
	
	/** Character to check danger status on */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<ACombatEnemy> Character;

	/** Minimum time to wait before reacting to the danger event */
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (Units = "s"))
	float MinReactionTime = 0.35f;

	/** Maximum time to wait before ignoring the danger event */
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (Units = "s"))
	float MaxReactionTime = 0.75f;

	/** Line of sight half angle for detecting incoming danger, in degrees*/
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (Units = "degrees"))
	float DangerSightConeAngle = 120.0f;
};

/**
 *  StateTree condition to check if the character is about to be hit by an attack
 */
USTRUCT(DisplayName = "Character is in Danger")
struct FStateTreeIsInDangerCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	/** Set the instance data type */
	using FInstanceDataType = FStateTreeIsInDangerConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/** Default constructor */
	FStateTreeIsInDangerCondition() = default;
	
	/** Tests the StateTree condition */
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR

	/** Provides the description string */
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

};

////////////////////////////////////////////////////////////////////

/**
 *  Instance data struct for the Combat StateTree tasks
 */
USTRUCT()
struct FStateTreeAttackInstanceData
{
	GENERATED_BODY()

	/** Character that will perform the attack */
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<ACombatEnemy> Character;
};

/**
 *  StateTree task to perform a combo attack
 */
USTRUCT(meta=(DisplayName="Combo Attack", Category="Combat"))
struct FStateTreeComboAttackTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Constructor */
	FStateTreeComboAttackTask()
	{
		// disable tick
		bShouldCallTick = false;

		// skip state change events if this is sustained
		bShouldStateChangeOnReselect = false;
	}

	/* Ensure we're using the correct instance data struct */
	using FInstanceDataType = FStateTreeAttackInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/** Runs when the owning state is entered */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** Runs when the owning state is ended */
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR
};

/**
 *  StateTree task to perform a charged attack
 */
USTRUCT(meta=(DisplayName="Charged Attack", Category="Combat"))
struct FStateTreeChargedAttackTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Constructor */
	FStateTreeChargedAttackTask()
	{
		// disable tick
		bShouldCallTick = false;

		// skip state change events if this is sustained
		bShouldStateChangeOnReselect = false;
	}

	/* Ensure we're using the correct instance data struct */
	using FInstanceDataType = FStateTreeAttackInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/** Runs when the owning state is entered */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** Runs when the owning state is ended */
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR
};

/**
 *  StateTree task to wait for the character to land
 */
USTRUCT(meta=(DisplayName="Wait for Landing", Category="Combat"))
struct FStateTreeWaitForLandingTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Constructor */
	FStateTreeWaitForLandingTask()
	{
		// disable tick
		bShouldCallTick = false;

		// skip state change events if this is sustained
		bShouldStateChangeOnReselect = false;
	}

	/* Ensure we're using the correct instance data struct */
	using FInstanceDataType = FStateTreeAttackInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/** Runs when the owning state is entered */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** Runs when the owning state is ended */
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR
};

////////////////////////////////////////////////////////////////////

/**
 *  Instance data struct for the Face Towards Actor StateTree task
 */
USTRUCT()
struct FStateTreeFaceActorInstanceData
{
	GENERATED_BODY()

	/** AI Controller that will determine the focused actor */
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> Controller;

	/** Actor that will be faced towards */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<AActor> ActorToFaceTowards;
};

/**
 *  StateTree task to face an AI-Controlled Pawn towards an Actor
 */
USTRUCT(meta=(DisplayName="Face Towards Actor", Category="Combat"))
struct FStateTreeFaceActorTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Constructor */
	FStateTreeFaceActorTask()
	{
		// disable tick
		bShouldCallTick = false;

		// skip state change events if this is sustained
		bShouldStateChangeOnReselect = false;
	}

	/* Ensure we're using the correct instance data struct */
	using FInstanceDataType = FStateTreeFaceActorInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/** Runs when the owning state is entered */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** Runs when the owning state is ended */
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR
};

////////////////////////////////////////////////////////////////////

/**
 *  Instance data struct for the Face Towards Location StateTree task
 */
USTRUCT()
struct FStateTreeFaceLocationInstanceData
{
	GENERATED_BODY()

	/** AI Controller that will determine the focused location */
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> Controller;

	/** Location that will be faced towards */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FVector FaceLocation = FVector::ZeroVector;
};

/**
 *  StateTree task to face an AI-Controlled Pawn towards a world location
 */
USTRUCT(meta=(DisplayName="Face Towards Location", Category="Combat"))
struct FStateTreeFaceLocationTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Constructor */
	FStateTreeFaceLocationTask()
	{
		// disable tick
		bShouldCallTick = false;

		// skip state change events if this is sustained
		bShouldStateChangeOnReselect = false;
	}

	/* Ensure we're using the correct instance data struct */
	using FInstanceDataType = FStateTreeFaceLocationInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/** Runs when the owning state is entered */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** Runs when the owning state is ended */
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR
};

////////////////////////////////////////////////////////////////////

/**
 *  Instance data struct for the Set Character Speed StateTree task
 */
USTRUCT()
struct FStateTreeSetCharacterSpeedInstanceData
{
	GENERATED_BODY()

	/** Character that will be affected */
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<ACharacter> Character;

	/** Max ground speed to set for the character */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Speed = 600.0f;
};

/**
 *  StateTree task to change a Character's ground speed
 */
USTRUCT(meta=(DisplayName="Set Character Speed", Category="Combat"))
struct FStateTreeSetCharacterSpeedTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Constructor */
	FStateTreeSetCharacterSpeedTask()
	{
		// disable tick
		bShouldCallTick = false;

		// skip state change events if this is sustained
		bShouldStateChangeOnReselect = false;
	}

	/* Ensure we're using the correct instance data struct */
	using FInstanceDataType = FStateTreeSetCharacterSpeedInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/** Runs when the owning state is entered */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR
};

////////////////////////////////////////////////////////////////////

/**
 *  Instance data struct for the Get Player Info task
 */
USTRUCT()
struct FStateTreeGetPlayerInfoInstanceData
{
	GENERATED_BODY()

	/** Character that owns this task */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<ACharacter> Character;

	/** Character that owns this task */
	UPROPERTY(VisibleAnywhere, Category="Output")
	TObjectPtr<ACharacter> TargetPlayerCharacter;

	/** Last known location for the target */
	UPROPERTY(VisibleAnywhere, Category="Output")
	FVector TargetPlayerLocation = FVector::ZeroVector;

	/** Distance to the target */
	UPROPERTY(VisibleAnywhere, Category="Output")
	float DistanceToTarget = 0.0f;

	/** Maximum allowed targeting range */
	UPROPERTY(VisibleAnywhere, Category="Parameter", meta = (ClampMin = 0, ClampMax = 10000, Units = "cm"))
	float MaxRange = 2500.0f;
};

/**
 *  StateTree task to get information about the player character
 */
USTRUCT(meta=(DisplayName="GetPlayerInfo", Category="Combat"))
struct FStateTreeGetPlayerInfoTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	/** Constructor */
	FStateTreeGetPlayerInfoTask()
	{
		// disable tick
		bShouldCallTick = false;

		// skip state change events if this is sustained
		bShouldStateChangeOnReselect = false;
	}

	/* Ensure we're using the correct instance data struct */
	using FInstanceDataType = FStateTreeGetPlayerInfoInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/** Runs when the owning state is entered */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR
};