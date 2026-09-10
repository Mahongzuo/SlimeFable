// Copyright Epic Games, Inc. All Rights Reserved.

#include "PigAnimSetupLibrary.h"

#include "PigAnimInstance.h"

#if WITH_EDITOR
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "AnimStateEntryNode.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "AnimationGraphSchema.h"
#include "AnimationStateGraph.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationTransitionGraph.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"
#endif

bool UPigAnimSetupLibrary::WirePigLocomotionGraph(
	UAnimBlueprint* AnimBP,
	UBlendSpace* GroundBlendSpace,
	UAnimSequence* IdleBreathe,
	UAnimSequence* IdleLook,
	UAnimSequence* IdleSniff,
	UAnimSequence* IdleChew,
	UAnimSequence* RestSequence,
	UAnimSequence* HitSequence,
	UAnimSequence* DeathSequence)
{
#if WITH_EDITOR
	if (!AnimBP || !GroundBlendSpace)
	{
		UE_LOG(LogTemp, Error, TEXT("[pig_loco] WirePigLocomotionGraph missing AnimBP or GroundBS"));
		return false;
	}

	UEdGraph* AnimGraph = nullptr;
	for (UEdGraph* Graph : AnimBP->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == TEXT("AnimGraph"))
		{
			AnimGraph = Graph;
			break;
		}
	}
	if (!AnimGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("[pig_loco] AnimGraph not found"));
		return false;
	}

	AnimGraph->Modify();
	AnimBP->Modify();
	if (AnimBP->ParentClass != UPigAnimInstance::StaticClass())
	{
		AnimBP->ParentClass = UPigAnimInstance::StaticClass();
		UE_LOG(LogTemp, Log, TEXT("[pig_loco] set ABP parent to PigAnimInstance"));
	}

	const UAnimationGraphSchema* AnimSchema = GetDefault<UAnimationGraphSchema>();

	auto FindPosePin = [](UEdGraphNode* Node, EEdGraphPinDirection Dir) -> UEdGraphPin*
	{
		if (!Node)
		{
			return nullptr;
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Dir && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
			{
				const FName PinName = Pin->PinName;
				if (PinName == TEXT("Pose") || PinName == TEXT("Result") || PinName == TEXT("Source")
					|| PinName == TEXT("Default") || PinName == TEXT("ComponentPose"))
				{
					return Pin;
				}
			}
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Dir && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct
				&& !Pin->PinName.ToString().Contains(TEXT("BlendPose")))
			{
				return Pin;
			}
		}
		return nullptr;
	};

	auto FindNamedPin = [](UEdGraphNode* Node, FName Name, EEdGraphPinDirection Dir) -> UEdGraphPin*
	{
		if (!Node)
		{
			return nullptr;
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Dir && Pin->PinName == Name)
			{
				return Pin;
			}
		}
		return nullptr;
	};

	auto ConnectChecked = [AnimSchema](UEdGraphPin* From, UEdGraphPin* To, const TCHAR* Label) -> bool
	{
		if (!From || !To)
		{
			UE_LOG(LogTemp, Error, TEXT("[pig_loco] connect missing pin: %s"), Label);
			return false;
		}
		AnimSchema->BreakPinLinks(*To, true);
		if (!AnimSchema->TryCreateConnection(From, To))
		{
			UE_LOG(LogTemp, Error, TEXT("[pig_loco] TryCreateConnection failed: %s"), Label);
			return false;
		}
		return To->LinkedTo.Num() > 0;
	};

	auto ConnectAny = [](const UEdGraphSchema* Schema, UEdGraphPin* From, UEdGraphPin* To, const TCHAR* Label) -> bool
	{
		if (!Schema || !From || !To)
		{
			UE_LOG(LogTemp, Error, TEXT("[pig_loco] SM connect missing: %s"), Label);
			return false;
		}
		Schema->BreakPinLinks(*To, true);
		if (!Schema->TryCreateConnection(From, To))
		{
			UE_LOG(LogTemp, Error, TEXT("[pig_loco] SM TryCreateConnection failed: %s"), Label);
			return false;
		}
		return To->LinkedTo.Num() > 0;
	};

	auto InitAnimGraphNode = [AnimGraph](UEdGraphNode* Node, int32 X, int32 Y)
	{
		Node->CreateNewGuid();
		Node->PostPlacedNewNode();
		Node->AllocateDefaultPins();
		Node->NodePosX = X;
		Node->NodePosY = Y;
		AnimGraph->AddNode(Node, true, false);
	};

	auto MakeVarGetInGraph = [](UEdGraph* Graph, FName PropName, int32 X, int32 Y) -> UK2Node_VariableGet*
	{
		FProperty* Prop = UPigAnimInstance::StaticClass()->FindPropertyByName(PropName);
		if (!Prop || !Graph)
		{
			UE_LOG(LogTemp, Error, TEXT("[pig_loco] missing property %s"), *PropName.ToString());
			return nullptr;
		}
		UK2Node_VariableGet* Get = NewObject<UK2Node_VariableGet>(Graph, NAME_None, RF_Transactional);
		Get->SetFromProperty(Prop, true, UPigAnimInstance::StaticClass());
		Get->CreateNewGuid();
		Get->PostPlacedNewNode();
		Get->AllocateDefaultPins();
		Get->NodePosX = X;
		Get->NodePosY = Y;
		Graph->AddNode(Get, true, false);
		return Get;
	};

	TArray<UEdGraphNode*> ToRemove;
	UAnimGraphNode_Root* Root = nullptr;
	UAnimGraphNode_Slot* Slot = nullptr;
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		if (!Node)
		{
			continue;
		}
		if (UAnimGraphNode_Root* AsRoot = Cast<UAnimGraphNode_Root>(Node))
		{
			Root = AsRoot;
			continue;
		}
		if (UAnimGraphNode_Slot* AsSlot = Cast<UAnimGraphNode_Slot>(Node))
		{
			Slot = AsSlot;
			continue;
		}
		if (Cast<UAnimGraphNode_BlendSpacePlayer>(Node)
			|| Cast<UAnimGraphNode_SequencePlayer>(Node)
			|| Cast<UAnimGraphNode_StateMachine>(Node)
			|| Cast<UAnimGraphNode_SaveCachedPose>(Node)
			|| Cast<UAnimGraphNode_UseCachedPose>(Node)
			|| Cast<UK2Node_VariableGet>(Node))
		{
			ToRemove.Add(Node);
		}
	}
	for (UEdGraphNode* Node : ToRemove)
	{
		if (UAnimGraphNode_StateMachine* SM = Cast<UAnimGraphNode_StateMachine>(Node))
		{
			if (UEdGraph* Sub = SM->EditorStateMachineGraph)
			{
				FBlueprintEditorUtils::RemoveGraph(AnimBP, Sub);
			}
			FBlueprintEditorUtils::RemoveNode(AnimBP, SM, true);
			continue;
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin)
			{
				AnimSchema->BreakPinLinks(*Pin, true);
			}
		}
		AnimGraph->RemoveNode(Node);
	}

	{
		TArray<UEdGraph*> Graphs;
		AnimBP->GetAllGraphs(Graphs);
		TArray<UEdGraph*> Orphans;
		for (UEdGraph* Graph : Graphs)
		{
			if (!Graph || Graph == AnimGraph)
			{
				continue;
			}
			const FString Name = Graph->GetName();
			const bool bSM = Graph->IsA<UAnimationStateMachineGraph>();
			const bool bLocoName = Name.Equals(TEXT("Locomotion")) || Name.StartsWith(TEXT("Locomotion"));
			if (bSM || bLocoName)
			{
				Orphans.AddUnique(Graph);
			}
		}
		for (UEdGraph* Graph : Orphans)
		{
			FBlueprintEditorUtils::RemoveGraph(AnimBP, Graph);
		}
	}

	if (!Root)
	{
		UE_LOG(LogTemp, Error, TEXT("[pig_loco] Root missing"));
		return false;
	}

	if (!Slot)
	{
		Slot = NewObject<UAnimGraphNode_Slot>(AnimGraph, NAME_None, RF_Transactional);
		Slot->Node.SlotName = TEXT("DefaultSlot");
		InitAnimGraphNode(Slot, 700, 0);
	}
	else
	{
		Slot->Node.SlotName = TEXT("DefaultSlot");
	}

	UAnimGraphNode_StateMachine* LocoSM =
		NewObject<UAnimGraphNode_StateMachine>(AnimGraph, NAME_None, RF_Transactional);
	InitAnimGraphNode(LocoSM, 200, 0);
	UAnimationStateMachineGraph* SMGraph = LocoSM->EditorStateMachineGraph;
	if (!SMGraph || !SMGraph->EntryNode)
	{
		UE_LOG(LogTemp, Error, TEXT("[pig_loco] StateMachine graph/entry missing"));
		return false;
	}
	FBlueprintEditorUtils::RenameGraph(SMGraph, TEXT("Locomotion"));
	const UEdGraphSchema* SMSchema = SMGraph->GetSchema();

	auto MakeState = [SMGraph](const FString& Name, int32 X, int32 Y) -> UAnimStateNode*
	{
		UAnimStateNode* State = NewObject<UAnimStateNode>(SMGraph, NAME_None, RF_Transactional);
		State->CreateNewGuid();
		State->PostPlacedNewNode();
		State->AllocateDefaultPins();
		State->NodePosX = X;
		State->NodePosY = Y;
		SMGraph->AddNode(State, true, false);
		if (State->BoundGraph)
		{
			TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(State);
			FBlueprintEditorUtils::RenameGraphWithSuggestion(State->BoundGraph, NameValidator, Name);
		}
		return State;
	};

	UAnimStateNode* IdleState = MakeState(TEXT("Idle"), 200, 0);
	UAnimStateNode* MoveState = MakeState(TEXT("Move"), 480, 0);
	UAnimStateNode* RestState = MakeState(TEXT("Rest"), 200, 240);
	UAnimStateNode* HitState = MakeState(TEXT("Hit"), 480, -220);
	UAnimStateNode* DeathState = MakeState(TEXT("Death"), 760, -220);

	bool bOk = true;
	bOk &= ConnectAny(SMSchema, SMGraph->EntryNode->GetOutputPin(), IdleState->GetInputPin(), TEXT("Entry->Idle"));

	auto WireStateBlendSpace = [&](UAnimStateNode* State, UBlendSpace* BS, FName XProp) -> bool
	{
		UAnimationStateGraph* StateGraph = Cast<UAnimationStateGraph>(State->BoundGraph);
		if (!StateGraph || !StateGraph->MyResultNode || !BS)
		{
			return false;
		}
		UAnimGraphNode_BlendSpacePlayer* Player =
			NewObject<UAnimGraphNode_BlendSpacePlayer>(StateGraph, NAME_None, RF_Transactional);
		Player->Node.SetBlendSpace(BS);
		Player->Node.SetLoop(true);
		Player->CreateNewGuid();
		Player->PostPlacedNewNode();
		Player->AllocateDefaultPins();
		Player->NodePosX = -120;
		Player->NodePosY = 0;
		StateGraph->AddNode(Player, true, false);
		if (UEdGraphPin* BSPin = FindNamedPin(Player, TEXT("BlendSpace"), EGPD_Input))
		{
			BSPin->DefaultObject = BS;
		}

		UK2Node_VariableGet* XGet = MakeVarGetInGraph(StateGraph, XProp, -420, -40);
		bool LocalOk = ConnectChecked(
			XGet ? XGet->GetValuePin() : nullptr,
			FindNamedPin(Player, TEXT("X"), EGPD_Input),
			TEXT("Speed->BS"));
		LocalOk &= ConnectChecked(
			FindPosePin(Player, EGPD_Output),
			FindPosePin(StateGraph->MyResultNode.Get(), EGPD_Input),
			TEXT("BS->StateResult"));
		return LocalOk;
	};

	auto WireStateSequence = [&](UAnimStateNode* State, UAnimSequence* Seq, bool bLoop, FName SeqProp) -> bool
	{
		UAnimationStateGraph* StateGraph = Cast<UAnimationStateGraph>(State->BoundGraph);
		if (!StateGraph || !StateGraph->MyResultNode)
		{
			return false;
		}
		UAnimGraphNode_SequencePlayer* Player =
			NewObject<UAnimGraphNode_SequencePlayer>(StateGraph, NAME_None, RF_Transactional);
		if (Seq)
		{
			Player->Node.SetSequence(Seq);
		}
		Player->Node.SetLoopAnimation(bLoop);
		Player->CreateNewGuid();
		Player->PostPlacedNewNode();
		Player->AllocateDefaultPins();
		Player->NodePosX = -120;
		Player->NodePosY = 0;
		StateGraph->AddNode(Player, true, false);
		if (!SeqProp.IsNone())
		{
			if (UK2Node_VariableGet* SeqGet = MakeVarGetInGraph(StateGraph, SeqProp, -420, 0))
			{
				if (UEdGraphPin* SeqPin = FindNamedPin(Player, TEXT("Sequence"), EGPD_Input))
				{
					ConnectChecked(SeqGet->GetValuePin(), SeqPin, TEXT("SeqVar->Player"));
				}
			}
		}
		return ConnectChecked(
			FindPosePin(Player, EGPD_Output),
			FindPosePin(StateGraph->MyResultNode.Get(), EGPD_Input),
			TEXT("Seq->StateResult"));
	};

	bOk &= WireStateSequence(
		IdleState,
		IdleBreathe,
		true,
		GET_MEMBER_NAME_CHECKED(UPigAnimInstance, IdleSequence));
	bOk &= WireStateBlendSpace(
		MoveState,
		GroundBlendSpace,
		GET_MEMBER_NAME_CHECKED(UPigAnimInstance, Speed));
	bOk &= WireStateSequence(
		RestState,
		RestSequence,
		true,
		GET_MEMBER_NAME_CHECKED(UPigAnimInstance, RestSequence));
	bOk &= WireStateSequence(
		HitState,
		HitSequence,
		false,
		GET_MEMBER_NAME_CHECKED(UPigAnimInstance, HitSequence));
	bOk &= WireStateSequence(
		DeathState,
		DeathSequence,
		false,
		GET_MEMBER_NAME_CHECKED(UPigAnimInstance, DeathSequence));

	auto MakeTransition = [&](UAnimStateNode* From, UAnimStateNode* To, FName BoolProp, bool bNegate, float BlendTime) -> bool
	{
		if (!From || !To)
		{
			return false;
		}
		UAnimStateTransitionNode* Trans = NewObject<UAnimStateTransitionNode>(SMGraph, NAME_None, RF_Transactional);
		Trans->CreateNewGuid();
		Trans->PostPlacedNewNode();
		Trans->AllocateDefaultPins();
		Trans->NodePosX = (From->NodePosX + To->NodePosX) / 2;
		Trans->NodePosY = (From->NodePosY + To->NodePosY) / 2;
		Trans->CrossfadeDuration = BlendTime;
		SMGraph->AddNode(Trans, true, false);
		Trans->CreateConnections(From, To);

		UAnimationTransitionGraph* RuleGraph = Cast<UAnimationTransitionGraph>(Trans->BoundGraph);
		if (!RuleGraph || !RuleGraph->MyResultNode)
		{
			return false;
		}

		UK2Node_VariableGet* BoolGet = MakeVarGetInGraph(RuleGraph, BoolProp, -320, 0);
		UEdGraphPin* BoolPin = BoolGet ? BoolGet->GetValuePin() : nullptr;
		UEdGraphPin* ResultPin = RuleGraph->MyResultNode->FindPin(TEXT("bCanEnterTransition"));
		if (!BoolPin || !ResultPin)
		{
			return false;
		}

		const UEdGraphSchema* RuleSchema = RuleGraph->GetSchema();
		if (bNegate)
		{
			UK2Node_CallFunction* NotNode = NewObject<UK2Node_CallFunction>(RuleGraph, NAME_None, RF_Transactional);
			NotNode->FunctionReference.SetExternalMember(
				GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool),
				UKismetMathLibrary::StaticClass());
			NotNode->CreateNewGuid();
			NotNode->PostPlacedNewNode();
			NotNode->AllocateDefaultPins();
			NotNode->NodePosX = -120;
			NotNode->NodePosY = 0;
			RuleGraph->AddNode(NotNode, true, false);
			UEdGraphPin* NotIn = NotNode->FindPin(TEXT("A"));
			UEdGraphPin* NotOut = NotNode->GetReturnValuePin();
			if (!NotIn || !NotOut)
			{
				return false;
			}
			RuleSchema->TryCreateConnection(BoolPin, NotIn);
			RuleSchema->BreakPinLinks(*ResultPin, true);
			return RuleSchema->TryCreateConnection(NotOut, ResultPin) && ResultPin->LinkedTo.Num() > 0;
		}

		RuleSchema->BreakPinLinks(*ResultPin, true);
		return RuleSchema->TryCreateConnection(BoolPin, ResultPin) && ResultPin->LinkedTo.Num() > 0;
	};

	auto MakeAndTransition = [&](UAnimStateNode* From, UAnimStateNode* To,
		FName PropA, bool bNegA, FName PropB, bool bNegB, float BlendTime) -> bool
	{
		if (!From || !To)
		{
			return false;
		}
		UAnimStateTransitionNode* Trans = NewObject<UAnimStateTransitionNode>(SMGraph, NAME_None, RF_Transactional);
		Trans->CreateNewGuid();
		Trans->PostPlacedNewNode();
		Trans->AllocateDefaultPins();
		Trans->NodePosX = (From->NodePosX + To->NodePosX) / 2;
		Trans->NodePosY = (From->NodePosY + To->NodePosY) / 2;
		Trans->CrossfadeDuration = BlendTime;
		SMGraph->AddNode(Trans, true, false);
		Trans->CreateConnections(From, To);

		UAnimationTransitionGraph* RuleGraph = Cast<UAnimationTransitionGraph>(Trans->BoundGraph);
		if (!RuleGraph || !RuleGraph->MyResultNode)
		{
			return false;
		}

		auto MakeBoolPin = [&](FName Prop, bool bNegate, int32 X, int32 Y) -> UEdGraphPin*
		{
			UK2Node_VariableGet* BoolGet = MakeVarGetInGraph(RuleGraph, Prop, X, Y);
			UEdGraphPin* BoolPin = BoolGet ? BoolGet->GetValuePin() : nullptr;
			if (!BoolPin || !bNegate)
			{
				return BoolPin;
			}
			UK2Node_CallFunction* NotNode = NewObject<UK2Node_CallFunction>(RuleGraph, NAME_None, RF_Transactional);
			NotNode->FunctionReference.SetExternalMember(
				GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool),
				UKismetMathLibrary::StaticClass());
			NotNode->CreateNewGuid();
			NotNode->PostPlacedNewNode();
			NotNode->AllocateDefaultPins();
			NotNode->NodePosX = X + 180;
			NotNode->NodePosY = Y;
			RuleGraph->AddNode(NotNode, true, false);
			UEdGraphPin* NotIn = NotNode->FindPin(TEXT("A"));
			UEdGraphPin* NotOut = NotNode->GetReturnValuePin();
			if (!NotIn || !NotOut)
			{
				return nullptr;
			}
			RuleGraph->GetSchema()->TryCreateConnection(BoolPin, NotIn);
			return NotOut;
		};

		UEdGraphPin* PinA = MakeBoolPin(PropA, bNegA, -420, -40);
		UEdGraphPin* PinB = MakeBoolPin(PropB, bNegB, -420, 40);
		UEdGraphPin* ResultPin = RuleGraph->MyResultNode->FindPin(TEXT("bCanEnterTransition"));
		if (!PinA || !PinB || !ResultPin)
		{
			return false;
		}
		UK2Node_CallFunction* AndNode = NewObject<UK2Node_CallFunction>(RuleGraph, NAME_None, RF_Transactional);
		AndNode->FunctionReference.SetExternalMember(
			GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanAND),
			UKismetMathLibrary::StaticClass());
		AndNode->CreateNewGuid();
		AndNode->PostPlacedNewNode();
		AndNode->AllocateDefaultPins();
		AndNode->NodePosX = -120;
		AndNode->NodePosY = 0;
		RuleGraph->AddNode(AndNode, true, false);
		UEdGraphPin* AndA = AndNode->FindPin(TEXT("A"));
		UEdGraphPin* AndB = AndNode->FindPin(TEXT("B"));
		UEdGraphPin* AndOut = AndNode->GetReturnValuePin();
		if (!AndA || !AndB || !AndOut)
		{
			return false;
		}
		const UEdGraphSchema* RuleSchema = RuleGraph->GetSchema();
		RuleSchema->TryCreateConnection(PinA, AndA);
		RuleSchema->TryCreateConnection(PinB, AndB);
		RuleSchema->BreakPinLinks(*ResultPin, true);
		return RuleSchema->TryCreateConnection(AndOut, ResultPin) && ResultPin->LinkedTo.Num() > 0;
	};

	// Idle <-> Move
	bOk &= MakeTransition(IdleState, MoveState, GET_MEMBER_NAME_CHECKED(UPigAnimInstance, bIsMoving), false, 0.18f);
	bOk &= MakeTransition(MoveState, IdleState, GET_MEMBER_NAME_CHECKED(UPigAnimInstance, bIsMoving), true, 0.2f);

	// Rest
	bOk &= MakeAndTransition(
		IdleState, RestState,
		GET_MEMBER_NAME_CHECKED(UPigAnimInstance, bWantsRest), false,
		GET_MEMBER_NAME_CHECKED(UPigAnimInstance, bIsInCombat), true, 0.25f);
	bOk &= MakeTransition(RestState, IdleState, GET_MEMBER_NAME_CHECKED(UPigAnimInstance, bWantsRest), true, 0.22f);
	bOk &= MakeTransition(RestState, IdleState, GET_MEMBER_NAME_CHECKED(UPigAnimInstance, bIsInCombat), false, 0.18f);
	bOk &= MakeTransition(RestState, MoveState, GET_MEMBER_NAME_CHECKED(UPigAnimInstance, bIsMoving), false, 0.18f);

	// Hit
	bOk &= MakeTransition(IdleState, HitState, GET_MEMBER_NAME_CHECKED(UPigAnimInstance, bIsHit), false, 0.08f);
	bOk &= MakeTransition(MoveState, HitState, GET_MEMBER_NAME_CHECKED(UPigAnimInstance, bIsHit), false, 0.08f);
	bOk &= MakeTransition(RestState, HitState, GET_MEMBER_NAME_CHECKED(UPigAnimInstance, bIsHit), false, 0.08f);
	bOk &= MakeAndTransition(
		HitState, IdleState,
		GET_MEMBER_NAME_CHECKED(UPigAnimInstance, bIsHit), true,
		GET_MEMBER_NAME_CHECKED(UPigAnimInstance, bIsMoving), true, 0.15f);
	bOk &= MakeAndTransition(
		HitState, MoveState,
		GET_MEMBER_NAME_CHECKED(UPigAnimInstance, bIsHit), true,
		GET_MEMBER_NAME_CHECKED(UPigAnimInstance, bIsMoving), false, 0.15f);

	// Death from every living state
	bOk &= MakeTransition(IdleState, DeathState, GET_MEMBER_NAME_CHECKED(UPigAnimInstance, bIsDead), false, 0.1f);
	bOk &= MakeTransition(MoveState, DeathState, GET_MEMBER_NAME_CHECKED(UPigAnimInstance, bIsDead), false, 0.1f);
	bOk &= MakeTransition(RestState, DeathState, GET_MEMBER_NAME_CHECKED(UPigAnimInstance, bIsDead), false, 0.1f);
	bOk &= MakeTransition(HitState, DeathState, GET_MEMBER_NAME_CHECKED(UPigAnimInstance, bIsDead), false, 0.08f);

	UAnimGraphNode_SaveCachedPose* SavePose =
		NewObject<UAnimGraphNode_SaveCachedPose>(AnimGraph, NAME_None, RF_Transactional);
	SavePose->CacheName = TEXT("Locomotion");
	SavePose->Node.CachePoseName = TEXT("Locomotion");
	InitAnimGraphNode(SavePose, 480, 0);

	UAnimGraphNode_UseCachedPose* UsePose =
		NewObject<UAnimGraphNode_UseCachedPose>(AnimGraph, NAME_None, RF_Transactional);
	UsePose->SaveCachedPoseNode = SavePose;
	InitAnimGraphNode(UsePose, 520, 180);

	Slot->NodePosX = 700;
	Slot->NodePosY = 180;
	Root->NodePosX = 900;
	Root->NodePosY = 180;

	bOk &= ConnectChecked(FindPosePin(LocoSM, EGPD_Output), FindPosePin(SavePose, EGPD_Input), TEXT("SM->SaveCachedPose"));
	bOk &= ConnectChecked(FindPosePin(UsePose, EGPD_Output), FindPosePin(Slot, EGPD_Input), TEXT("UseCachedPose->Slot"));
	bOk &= ConnectChecked(FindPosePin(Slot, EGPD_Output), FindPosePin(Root, EGPD_Input), TEXT("Slot->Root"));

	if (UClass* Gen = AnimBP->GeneratedClass.Get())
	{
		if (UPigAnimInstance* CDO = Cast<UPigAnimInstance>(Gen->GetDefaultObject()))
		{
			CDO->GroundBlendSpace = GroundBlendSpace;
			CDO->RestSequence = RestSequence;
			CDO->HitSequence = HitSequence;
			CDO->DeathSequence = DeathSequence;
			CDO->IdleVariants.Reset();
			if (IdleBreathe) { CDO->IdleVariants.Add(IdleBreathe); }
			if (IdleLook) { CDO->IdleVariants.Add(IdleLook); }
			if (IdleSniff) { CDO->IdleVariants.Add(IdleSniff); }
			if (IdleChew) { CDO->IdleVariants.Add(IdleChew); }
			CDO->IdleSequence = IdleBreathe;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(AnimBP);
	UE_LOG(LogTemp, Log, TEXT("[pig_loco] WirePigLocomotionGraph ok=%d"), bOk ? 1 : 0);
	return bOk;
#else
	return false;
#endif
}
