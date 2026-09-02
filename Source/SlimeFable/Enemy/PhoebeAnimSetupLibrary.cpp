// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhoebeAnimSetupLibrary.h"

#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Chooser.h"
#include "UObject/Package.h"
#include "ChooserPropertyAccess.h"
#include "BoolColumn.h"
#include "EnumColumn.h"
#include "ObjectChooser_Asset.h"
#include "PhoebeAnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchFeatureChannel_Pose.h"
#include "PoseSearch/PoseSearchFeatureChannel_Trajectory.h"
#include "PoseSearch/PoseSearchSchema.h"

#if WITH_EDITOR
#include "AnimGraph/AnimGraphNode_OrientationWarping.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_MotionMatching.h"
#include "AnimGraphNode_PoseSearchHistoryCollector.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimGraphNode_TwoWayBlend.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "AnimStateEntryNode.h"
#include "AnimStateNode.h"
#include "AnimStateNodeBase.h"
#include "AnimStateTransitionNode.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/InputScaleBias.h"
#include "Animation/AnimEnums.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "AnimationStateGraph.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationTransitionGraph.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/MemberReference.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#endif

namespace PhoebeAnimSetupPrivate
{
	static void AddAssetRow(UChooserTable* Chooser, UPoseSearchDatabase* Database)
	{
		if (!Chooser || !Database)
		{
			return;
		}
		FInstancedStruct Row;
		Row.InitializeAs<FAssetChooser>();
		Row.GetMutable<FAssetChooser>().Asset = Database;
		Chooser->ResultsStructs.Add(MoveTemp(Row));
	}

	static FBoolColumn& AddBoolColumn(UChooserTable* Chooser, FName PropertyName)
	{
		FInstancedStruct ColumnStruct;
		ColumnStruct.InitializeAs<FBoolColumn>();
		FBoolColumn& Column = ColumnStruct.GetMutable<FBoolColumn>();
		Column.InputValue.InitializeAs<FBoolContextProperty>();
		FBoolContextProperty& Binding = Column.InputValue.GetMutable<FBoolContextProperty>();
		Binding.Binding.PropertyBindingChain = { PropertyName };
		Binding.Binding.ContextIndex = 0;
		Chooser->ColumnsStructs.Add(MoveTemp(ColumnStruct));
		return Chooser->ColumnsStructs.Last().GetMutable<FBoolColumn>();
	}

	static FEnumColumn& AddGaitColumn(UChooserTable* Chooser)
	{
		FInstancedStruct ColumnStruct;
		ColumnStruct.InitializeAs<FEnumColumn>();
		FEnumColumn& Column = ColumnStruct.GetMutable<FEnumColumn>();
		Column.InputValue.InitializeAs<FEnumContextProperty>();
		FEnumContextProperty& Binding = Column.InputValue.GetMutable<FEnumContextProperty>();
		Binding.Binding.PropertyBindingChain = { GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, Gait) };
		Binding.Binding.ContextIndex = 0;
#if WITH_EDITORONLY_DATA
		Binding.Binding.Enum = StaticEnum<EPhoebeGait>();
#endif
		Chooser->ColumnsStructs.Add(MoveTemp(ColumnStruct));
		return Chooser->ColumnsStructs.Last().GetMutable<FEnumColumn>();
	}

	static FChooserEnumRowData MakeGaitCell(EPhoebeGait Gait, bool bMatchAny)
	{
		FChooserEnumRowData Cell;
		Cell.Comparison = bMatchAny
			? EEnumColumnCellValueComparison::MatchAny
			: EEnumColumnCellValueComparison::MatchEqual;
		Cell.Value = static_cast<uint8>(Gait);
		return Cell;
	}
}

bool UPhoebeAnimSetupLibrary::SetupPhoebeSchema(UPoseSearchSchema* Schema, USkeleton* Skeleton)
{
#if WITH_EDITOR
	if (!Schema || !Skeleton)
	{
		return false;
	}

	Schema->Modify();
	TArray<FPoseSearchRoledSkeleton>& Skeletons =
		const_cast<TArray<FPoseSearchRoledSkeleton>&>(Schema->GetRoledSkeletons());
	Skeletons.Reset();
	Schema->AddSkeleton(Skeleton);

	if (FArrayProperty* ChannelsProp = FindFProperty<FArrayProperty>(UPoseSearchSchema::StaticClass(), TEXT("Channels")))
	{
		FScriptArrayHelper Helper(ChannelsProp, ChannelsProp->ContainerPtrToValuePtr<void>(Schema));
		UE_LOG(LogTemp, Log, TEXT("[phoebe_mm] schema channels before clear=%d"), Helper.Num());
		Helper.EmptyAndAddValues(0);
		Helper.EmptyValues();
		UE_LOG(LogTemp, Log, TEXT("[phoebe_mm] schema channels after clear=%d skeletons=%d"), Helper.Num(), Schema->GetRoledSkeletons().Num());
	}

	Schema->AddChannel(NewObject<UPoseSearchFeatureChannel_Trajectory>(Schema, NAME_None, RF_Transactional));

	FName LeftFoot = NAME_None;
	FName RightFoot = NAME_None;
	const FReferenceSkeleton& RefSkel = Skeleton->GetReferenceSkeleton();
	for (int32 BoneIndex = 0; BoneIndex < RefSkel.GetNum(); ++BoneIndex)
	{
		const FName BoneName = RefSkel.GetBoneName(BoneIndex);
		const FString Lower = BoneName.ToString().ToLower();
		const bool bFootLike = Lower.Contains(TEXT("foot")) || Lower.Contains(TEXT("ankle"));
		if (!bFootLike || Lower.Contains(TEXT("ik_hand")) || Lower.Contains(TEXT("weapon")))
		{
			continue;
		}
		const bool bLeft = Lower.Contains(TEXT("left")) || Lower.Contains(TEXT("lfoot")) || Lower.Contains(TEXT("l_foot"))
			|| Lower.Contains(TEXT("_l")) || Lower.Contains(TEXT("-l")) || Lower.Contains(TEXT(" l "))
			|| Lower.StartsWith(TEXT("l_")) || Lower.StartsWith(TEXT("l-"));
		const bool bRight = Lower.Contains(TEXT("right")) || Lower.Contains(TEXT("rfoot")) || Lower.Contains(TEXT("r_foot"))
			|| Lower.Contains(TEXT("_r")) || Lower.Contains(TEXT("-r")) || Lower.Contains(TEXT(" r "))
			|| Lower.StartsWith(TEXT("r_")) || Lower.StartsWith(TEXT("r-"));
		if (bLeft && !bRight && LeftFoot == NAME_None)
		{
			LeftFoot = BoneName;
		}
		else if (bRight && !bLeft && RightFoot == NAME_None)
		{
			RightFoot = BoneName;
		}
		UE_LOG(LogTemp, Log, TEXT("[phoebe_mm] foot-like bone %s"), *BoneName.ToString());
	}

	if (LeftFoot != NAME_None && RightFoot != NAME_None)
	{
		UPoseSearchFeatureChannel_Pose* Pose = NewObject<UPoseSearchFeatureChannel_Pose>(Schema, NAME_None, RF_Transactional);
		Pose->SampledBones.Reset();
		FPoseSearchBone LeftBone;
		LeftBone.Reference.BoneName = LeftFoot;
		LeftBone.Flags = int32(EPoseSearchBoneFlags::Position | EPoseSearchBoneFlags::Velocity);
		FPoseSearchBone RightBone;
		RightBone.Reference.BoneName = RightFoot;
		RightBone.Flags = int32(EPoseSearchBoneFlags::Position | EPoseSearchBoneFlags::Velocity);
		Pose->SampledBones.Add(LeftBone);
		Pose->SampledBones.Add(RightBone);
		Schema->AddChannel(Pose);
		UE_LOG(LogTemp, Log, TEXT("[phoebe_mm] pose channel feet %s / %s"), *LeftFoot.ToString(), *RightFoot.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[phoebe_mm] no left/right foot bones; trajectory-only schema"));
	}

	Schema->PostEditChange();
	Schema->MarkPackageDirty();
	return Schema->GetRoledSkeletons().Num() == 1;
#else
	return false;
#endif
}

bool UPhoebeAnimSetupLibrary::AddAnimationToDatabase(UPoseSearchDatabase* Database, UObject* AnimAsset)
{
#if WITH_EDITOR
	if (!Database || !AnimAsset)
	{
		return false;
	}
	for (int32 Index = 0;; ++Index)
	{
		const FPoseSearchDatabaseAnimationAsset* Existing = Database->GetDatabaseAnimationAsset(Index);
		if (!Existing)
		{
			break;
		}
		if (Existing->GetAnimationAsset() == AnimAsset)
		{
			return true;
		}
	}
	FPoseSearchDatabaseAnimationAsset Entry;
	Entry.AnimAsset = AnimAsset;
	Database->Modify();
	Database->AddAnimationAsset(Entry);
	Database->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

int32 UPhoebeAnimSetupLibrary::GetDatabaseAnimationCount(const UPoseSearchDatabase* Database)
{
	if (!Database)
	{
		return 0;
	}
	int32 Count = 0;
	for (int32 Index = 0;; ++Index)
	{
		if (!Database->GetDatabaseAnimationAsset(Index))
		{
			break;
		}
		++Count;
	}
	return Count;
}

bool UPhoebeAnimSetupLibrary::SetupPhoebeChooser(
	UChooserTable* Chooser,
	UPoseSearchDatabase* ClimbDb,
	UPoseSearchDatabase* AirDb,
	UPoseSearchDatabase* SprintDb,
	UPoseSearchDatabase* RunDb,
	UPoseSearchDatabase* WalkDb,
	UPoseSearchDatabase* IdleDb)
{
#if WITH_EDITOR
	if (!Chooser)
	{
		return false;
	}

	Chooser->Modify();
	Chooser->ResultType = EObjectChooserResultType::ObjectResult;
	Chooser->OutputObjectType = UPoseSearchDatabase::StaticClass();

	Chooser->ContextData.Reset();
	{
		FInstancedStruct Ctx;
		Ctx.InitializeAs<FContextObjectTypeClass>();
		FContextObjectTypeClass& ClassCtx = Ctx.GetMutable<FContextObjectTypeClass>();
		ClassCtx.Class = UPhoebeAnimInstance::StaticClass();
		ClassCtx.Direction = EContextObjectDirection::Read;
		Chooser->ContextData.Add(MoveTemp(Ctx));
	}

	Chooser->ResultsStructs.Reset();
	PhoebeAnimSetupPrivate::AddAssetRow(Chooser, ClimbDb);
	PhoebeAnimSetupPrivate::AddAssetRow(Chooser, AirDb);
	PhoebeAnimSetupPrivate::AddAssetRow(Chooser, SprintDb);
	PhoebeAnimSetupPrivate::AddAssetRow(Chooser, RunDb);
	PhoebeAnimSetupPrivate::AddAssetRow(Chooser, WalkDb);
	PhoebeAnimSetupPrivate::AddAssetRow(Chooser, IdleDb);

	Chooser->ColumnsStructs.Reset();
	FBoolColumn& ClimbCol = PhoebeAnimSetupPrivate::AddBoolColumn(Chooser, GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsClimbing));
	ClimbCol.RowValuesWithAny = {
		EBoolColumnCellValue::MatchTrue,
		EBoolColumnCellValue::MatchFalse,
		EBoolColumnCellValue::MatchFalse,
		EBoolColumnCellValue::MatchFalse,
		EBoolColumnCellValue::MatchFalse,
		EBoolColumnCellValue::MatchFalse
	};

	FBoolColumn& AirCol = PhoebeAnimSetupPrivate::AddBoolColumn(Chooser, GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsInAir));
	AirCol.RowValuesWithAny = {
		EBoolColumnCellValue::MatchAny,
		EBoolColumnCellValue::MatchTrue,
		EBoolColumnCellValue::MatchFalse,
		EBoolColumnCellValue::MatchFalse,
		EBoolColumnCellValue::MatchFalse,
		EBoolColumnCellValue::MatchFalse
	};

	FEnumColumn& GaitCol = PhoebeAnimSetupPrivate::AddGaitColumn(Chooser);
	GaitCol.RowValues = {
		PhoebeAnimSetupPrivate::MakeGaitCell(EPhoebeGait::Idle, true),
		PhoebeAnimSetupPrivate::MakeGaitCell(EPhoebeGait::Idle, true),
		PhoebeAnimSetupPrivate::MakeGaitCell(EPhoebeGait::Sprint, false),
		PhoebeAnimSetupPrivate::MakeGaitCell(EPhoebeGait::Run, false),
		PhoebeAnimSetupPrivate::MakeGaitCell(EPhoebeGait::Walk, false),
		PhoebeAnimSetupPrivate::MakeGaitCell(EPhoebeGait::Idle, false)
	};

	Chooser->Compile(true);
	Chooser->MarkPackageDirty();
	return Chooser->ResultsStructs.Num() == 6 && Chooser->ColumnsStructs.Num() == 3;
#else
	return false;
#endif
}

bool UPhoebeAnimSetupLibrary::WirePhoebeMotionMatchingGraph(UAnimBlueprint* AnimBP, UPoseSearchDatabase* DefaultDatabase)
{
#if WITH_EDITOR
	if (!AnimBP)
	{
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
		return false;
	}

	AnimGraph->Modify();
	AnimBP->Modify();

	UAnimGraphNode_Root* Root = nullptr;
	UAnimGraphNode_Slot* Slot = nullptr;
	UAnimGraphNode_MotionMatching* Motion = nullptr;
	UAnimGraphNode_PoseSearchHistoryCollector* History = nullptr;
	UAnimGraphNode_OrientationWarping* Warp = nullptr;
	UK2Node_VariableGet* ActiveGet = nullptr;
	UK2Node_VariableGet* DirectionGet = nullptr;
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		if (UAnimGraphNode_Root* AsRoot = Cast<UAnimGraphNode_Root>(Node))
		{
			Root = AsRoot;
		}
		else if (UAnimGraphNode_Slot* AsSlot = Cast<UAnimGraphNode_Slot>(Node))
		{
			Slot = AsSlot;
		}
		else if (UAnimGraphNode_MotionMatching* AsMM = Cast<UAnimGraphNode_MotionMatching>(Node))
		{
			Motion = AsMM;
		}
		else if (UAnimGraphNode_PoseSearchHistoryCollector* AsHistory = Cast<UAnimGraphNode_PoseSearchHistoryCollector>(Node))
		{
			History = AsHistory;
		}
		else if (UAnimGraphNode_OrientationWarping* AsWarp = Cast<UAnimGraphNode_OrientationWarping>(Node))
		{
			Warp = AsWarp;
		}
		else if (UK2Node_VariableGet* AsGet = Cast<UK2Node_VariableGet>(Node))
		{
			if (AsGet->GetVarName() == GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, ActiveDatabase))
			{
				ActiveGet = AsGet;
			}
			else if (AsGet->GetVarName() == GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, Direction))
			{
				DirectionGet = AsGet;
			}
		}
	}

	auto InitNode = [AnimGraph](UEdGraphNode* Node, int32 X, int32 Y)
	{
		Node->CreateNewGuid();
		Node->PostPlacedNewNode();
		Node->AllocateDefaultPins();
		Node->NodePosX = X;
		Node->NodePosY = Y;
		AnimGraph->AddNode(Node, true, false);
	};

	if (!Slot)
	{
		Slot = NewObject<UAnimGraphNode_Slot>(AnimGraph, NAME_None, RF_Transactional);
		Slot->Node.SlotName = TEXT("DefaultSlot");
		InitNode(Slot, 720, 0);
	}

	if (!Motion)
	{
		Motion = NewObject<UAnimGraphNode_MotionMatching>(AnimGraph, NAME_None, RF_Transactional);
		InitNode(Motion, 0, 0);
	}

	if (!History)
	{
		History = NewObject<UAnimGraphNode_PoseSearchHistoryCollector>(AnimGraph, NAME_None, RF_Transactional);
		InitNode(History, 280, 0);
	}

	if (FStructProperty* HistoryNodeProp = FindFProperty<FStructProperty>(
			UAnimGraphNode_PoseSearchHistoryCollector::StaticClass(), TEXT("Node")))
	{
		if (FAnimNode_PoseSearchHistoryCollector* HistoryNode =
				HistoryNodeProp->ContainerPtrToValuePtr<FAnimNode_PoseSearchHistoryCollector>(History))
		{
			HistoryNode->PoseCount = 8;
			HistoryNode->SamplingInterval = 0.04f;
			HistoryNode->bGenerateTrajectory = true;
			HistoryNode->TrajectoryHistoryCount = 5;
			HistoryNode->TrajectoryPredictionCount = 4;
			HistoryNode->PredictionSamplingInterval = 0.1f;
			if (USkeleton* Skel = AnimBP->TargetSkeleton.Get())
			{
				const FReferenceSkeleton& RefSkel = Skel->GetReferenceSkeleton();
				HistoryNode->CollectedBones.Reset();
				for (const TCHAR* FootName : { TEXT("Bip001LFoot"), TEXT("Bip001RFoot") })
				{
					if (RefSkel.FindBoneIndex(FName(FootName)) != INDEX_NONE)
					{
						FBoneReference BoneRef;
						BoneRef.BoneName = FootName;
						HistoryNode->CollectedBones.Add(BoneRef);
					}
				}
			}
		}
	}

	if (FStructProperty* NodeProp = FindFProperty<FStructProperty>(UAnimGraphNode_MotionMatching::StaticClass(), TEXT("Node")))
	{
		if (FAnimNode_MotionMatching* MMNode = NodeProp->ContainerPtrToValuePtr<FAnimNode_MotionMatching>(Motion))
		{
			MMNode->SetMaxActiveBlends(4);
			if (FFloatProperty* BlendTimeProp = FindFProperty<FFloatProperty>(FAnimNode_MotionMatching::StaticStruct(), TEXT("BlendTime")))
			{
				BlendTimeProp->SetPropertyValue_InContainer(MMNode, 0.1f);
			}
			if (FStructProperty* PlayRateProp = FindFProperty<FStructProperty>(FAnimNode_MotionMatching::StaticStruct(), TEXT("PlayRate")))
			{
				if (FFloatInterval* Interval = PlayRateProp->ContainerPtrToValuePtr<FFloatInterval>(MMNode))
				{
					*Interval = FFloatInterval(0.85f, 1.2f);
				}
			}
		}
	}

	if (UFunction* UpdateFn = UPhoebeAnimInstance::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(UPhoebeAnimInstance, OnPhoebeMotionMatchingUpdated)))
	{
		if (FStructProperty* FnProp = FindFProperty<FStructProperty>(
				UAnimGraphNode_MotionMatching::StaticClass(), TEXT("OnMotionMatchingStateUpdatedFunction")))
		{
			if (FMemberReference* Ref = FnProp->ContainerPtrToValuePtr<FMemberReference>(Motion))
			{
				Ref->SetFromField<UFunction>(UpdateFn, true);
			}
		}
	}

	if (!ActiveGet)
	{
		if (FProperty* Prop = UPhoebeAnimInstance::StaticClass()->FindPropertyByName(
				GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, ActiveDatabase)))
		{
			ActiveGet = NewObject<UK2Node_VariableGet>(AnimGraph, NAME_None, RF_Transactional);
			ActiveGet->SetFromProperty(Prop, true, UPhoebeAnimInstance::StaticClass());
			InitNode(ActiveGet, -420, 40);
		}
	}

	USkeleton* Skeleton = AnimBP->TargetSkeleton.Get();
	FName SpineBone = NAME_None;
	if (Skeleton)
	{
		const FReferenceSkeleton& RefSkel = Skeleton->GetReferenceSkeleton();
		static const TCHAR* SpineCandidates[] = {
			TEXT("spine_01"), TEXT("Spine1"), TEXT("Bip001Spine"), TEXT("Bip001Pelvis"),
			TEXT("Bip001-Spine"), TEXT("Bip001 Spine"), TEXT("spine"), TEXT("Spine"),
			TEXT("pelvis"), TEXT("Pelvis"), TEXT("hip"), TEXT("Hip")
		};
		for (const TCHAR* Candidate : SpineCandidates)
		{
			if (RefSkel.FindBoneIndex(FName(Candidate)) != INDEX_NONE)
			{
				SpineBone = FName(Candidate);
				break;
			}
		}
	}

	// Skip Orientation Warping: Phoebe has no IK Foot Root; the compiler warning
	// plus a broken warp pose would hide locomotion. History Collector is required instead.
	Warp = nullptr;

	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
	auto FindPosePin = [](UEdGraphNode* Node, EEdGraphPinDirection Dir) -> UEdGraphPin*
	{
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
			if (Pin && Pin->Direction == Dir && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
			{
				return Pin;
			}
		}
		return nullptr;
	};

	if (UEdGraphPin* DatabasePin = Motion->FindPin(TEXT("Database"), EGPD_Input))
	{
		if (DefaultDatabase)
		{
			DatabasePin->DefaultObject = DefaultDatabase;
		}
		if (ActiveGet)
		{
			if (UEdGraphPin* ValuePin = ActiveGet->GetValuePin())
			{
				Schema->TryCreateConnection(ValuePin, DatabasePin);
			}
		}
	}

	if (Warp && DirectionGet)
	{
		if (UEdGraphPin* AnglePin = Warp->FindPin(TEXT("LocomotionAngle"), EGPD_Input))
		{
			if (UEdGraphPin* ValuePin = DirectionGet->GetValuePin())
			{
				Schema->TryCreateConnection(ValuePin, AnglePin);
			}
		}
	}

	if (Root && Slot && Motion && History)
	{
		UEdGraphPin* MMOut = FindPosePin(Motion, EGPD_Output);
		UEdGraphPin* HistoryIn = FindPosePin(History, EGPD_Input);
		UEdGraphPin* HistoryOut = FindPosePin(History, EGPD_Output);
		UEdGraphPin* SlotIn = FindPosePin(Slot, EGPD_Input);
		UEdGraphPin* SlotOut = FindPosePin(Slot, EGPD_Output);
		UEdGraphPin* RootIn = FindPosePin(Root, EGPD_Input);

		auto BreakLinks = [Schema](UEdGraphPin* Pin)
		{
			if (Pin)
			{
				Schema->BreakPinLinks(*Pin, true);
			}
		};
		BreakLinks(MMOut);
		BreakLinks(HistoryIn);
		BreakLinks(HistoryOut);
		BreakLinks(SlotIn);
		BreakLinks(SlotOut);
		BreakLinks(RootIn);
		if (Warp)
		{
			BreakLinks(FindPosePin(Warp, EGPD_Input));
			BreakLinks(FindPosePin(Warp, EGPD_Output));
		}

		// History Collector must wrap MM so Update publishes IPoseHistory before MM searches.
		if (MMOut && HistoryIn)
		{
			Schema->TryCreateConnection(MMOut, HistoryIn);
		}

		UEdGraphPin* AfterHistory = HistoryOut;
		if (Warp)
		{
			UEdGraphPin* WarpIn = FindPosePin(Warp, EGPD_Input);
			UEdGraphPin* WarpOut = FindPosePin(Warp, EGPD_Output);
			if (HistoryOut && WarpIn)
			{
				Schema->TryCreateConnection(HistoryOut, WarpIn);
			}
			AfterHistory = WarpOut;
		}
		if (AfterHistory && SlotIn)
		{
			Schema->TryCreateConnection(AfterHistory, SlotIn);
		}
		if (SlotOut && RootIn)
		{
			Schema->TryCreateConnection(SlotOut, RootIn);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(AnimBP);
	return Root && Slot && Motion && History;
#else
	return false;
#endif
}

bool UPhoebeAnimSetupLibrary::WirePhoebeLocomotionGraph(
	UAnimBlueprint* AnimBP,
	UBlendSpace* GroundBlendSpace,
	UBlendSpace* ClimbBlendSpace,
	UAnimSequence* JumpSequence,
	UAnimSequence* FallSequence,
	UBlendSpace* ClimbDashBlendSpace,
	UAnimSequence* AirAttackSequence)
{
#if WITH_EDITOR
	// Unarmed-style: Locomotion SM → SaveCachedPose → UseCachedPose → DefaultSlot → Root
	if (!AnimBP || !GroundBlendSpace)
	{
		UE_LOG(LogTemp, Error, TEXT("[phoebe_loco] WirePhoebeLocomotionGraph missing AnimBP or GroundBS"));
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
		UE_LOG(LogTemp, Error, TEXT("[phoebe_loco] AnimGraph not found"));
		return false;
	}

	AnimGraph->Modify();
	AnimBP->Modify();
	if (AnimBP->ParentClass != UPhoebeAnimInstance::StaticClass())
	{
		AnimBP->ParentClass = UPhoebeAnimInstance::StaticClass();
		UE_LOG(LogTemp, Log, TEXT("[phoebe_loco] set ABP parent to PhoebeAnimInstance"));
	}

	const UAnimationGraphSchema* AnimSchema = GetDefault<UAnimationGraphSchema>();
	const UEdGraphSchema* GenericSchema = AnimGraph->GetSchema();

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
			UE_LOG(LogTemp, Error, TEXT("[phoebe_loco] connect missing pin: %s"), Label);
			return false;
		}
		AnimSchema->BreakPinLinks(*To, true);
		if (!AnimSchema->TryCreateConnection(From, To))
		{
			UE_LOG(LogTemp, Error, TEXT("[phoebe_loco] TryCreateConnection failed: %s (%s -> %s)"),
				Label, *From->PinName.ToString(), *To->PinName.ToString());
			return false;
		}
		if (To->LinkedTo.Num() == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("[phoebe_loco] link empty after connect: %s"), Label);
			return false;
		}
		return true;
	};

	auto ConnectAny = [](const UEdGraphSchema* Schema, UEdGraphPin* From, UEdGraphPin* To, const TCHAR* Label) -> bool
	{
		if (!Schema || !From || !To)
		{
			UE_LOG(LogTemp, Error, TEXT("[phoebe_loco] SM connect missing: %s"), Label);
			return false;
		}
		Schema->BreakPinLinks(*To, true);
		if (!Schema->TryCreateConnection(From, To))
		{
			UE_LOG(LogTemp, Error, TEXT("[phoebe_loco] SM TryCreateConnection failed: %s"), Label);
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
		FProperty* Prop = UPhoebeAnimInstance::StaticClass()->FindPropertyByName(PropName);
		if (!Prop || !Graph)
		{
			UE_LOG(LogTemp, Error, TEXT("[phoebe_loco] missing property %s"), *PropName.ToString());
			return nullptr;
		}
		UK2Node_VariableGet* Get = NewObject<UK2Node_VariableGet>(Graph, NAME_None, RF_Transactional);
		Get->SetFromProperty(Prop, true, UPhoebeAnimInstance::StaticClass());
		Get->CreateNewGuid();
		Get->PostPlacedNewNode();
		Get->AllocateDefaultPins();
		Get->NodePosX = X;
		Get->NodePosY = Y;
		Graph->AddNode(Get, true, false);
		return Get;
	};

	// Clear previous locomotion / MM / old BlendList graphs.
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
		if (Cast<UAnimGraphNode_MotionMatching>(Node)
			|| Cast<UAnimGraphNode_PoseSearchHistoryCollector>(Node)
			|| Cast<UAnimGraphNode_OrientationWarping>(Node)
			|| Cast<UAnimGraphNode_BlendSpacePlayer>(Node)
			|| Cast<UAnimGraphNode_SequencePlayer>(Node)
			|| Cast<UAnimGraphNode_TwoWayBlend>(Node)
			|| Cast<UAnimGraphNode_BlendListByBool>(Node)
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
			const bool bBlendStack = Name.Contains(TEXT("BlendStack"));
			if (bSM || bLocoName || bBlendStack)
			{
				Orphans.AddUnique(Graph);
			}
		}
		for (UEdGraph* Graph : Orphans)
		{
			FBlueprintEditorUtils::RemoveGraph(AnimBP, Graph);
		}
		UE_LOG(LogTemp, Log, TEXT("[phoebe_loco] stripped %d orphan locomotion/blendstack graphs"), Orphans.Num());
	}

	if (!Root)
	{
		UE_LOG(LogTemp, Error, TEXT("[phoebe_loco] Root missing"));
		return false;
	}
	Root->NodePosX = 900;
	Root->NodePosY = 0;

	if (!Slot)
	{
		Slot = NewObject<UAnimGraphNode_Slot>(AnimGraph, NAME_None, RF_Transactional);
		Slot->Node.SlotName = TEXT("DefaultSlot");
		InitAnimGraphNode(Slot, 700, 0);
	}
	else
	{
		Slot->NodePosX = 700;
		Slot->NodePosY = 0;
		Slot->Node.SlotName = TEXT("DefaultSlot");
	}

	// --- State Machine (ThirdPerson style) ---
	UAnimGraphNode_StateMachine* LocoSM =
		NewObject<UAnimGraphNode_StateMachine>(AnimGraph, NAME_None, RF_Transactional);
	InitAnimGraphNode(LocoSM, 200, 0);
	UAnimationStateMachineGraph* SMGraph = LocoSM->EditorStateMachineGraph;
	if (!SMGraph || !SMGraph->EntryNode)
	{
		UE_LOG(LogTemp, Error, TEXT("[phoebe_loco] StateMachine graph/entry missing"));
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

	UAnimStateNode* MoveState = MakeState(TEXT("Move"), 200, 0);
	UAnimStateNode* JumpState = MakeState(TEXT("Jump"), 200, -220);
	UAnimStateNode* FallState = MakeState(TEXT("Fall"), 480, -220);
	UAnimStateNode* AirAttackState = AirAttackSequence ? MakeState(TEXT("AirAttack"), 760, -220) : nullptr;
	UAnimStateNode* ClimbState = ClimbBlendSpace ? MakeState(TEXT("Climb"), 200, 240) : nullptr;
	UAnimStateNode* ClimbDashState = ClimbDashBlendSpace ? MakeState(TEXT("ClimbDash"), 480, 240) : nullptr;

	bool bOk = true;

	// Entry → Move
	bOk &= ConnectAny(
		SMSchema,
		SMGraph->EntryNode->GetOutputPin(),
		MoveState->GetInputPin(),
		TEXT("Entry->Move"));

	auto WireStateBlendSpace = [&](UAnimStateNode* State, UBlendSpace* BS, FName XProp, FName YProp, FName RateProp) -> bool
	{
		UAnimationStateGraph* StateGraph = Cast<UAnimationStateGraph>(State->BoundGraph);
		if (!StateGraph || !StateGraph->MyResultNode || !BS)
		{
			UE_LOG(LogTemp, Error, TEXT("[phoebe_loco] state graph invalid for BS"));
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
			TEXT("X->BS"));
		if (!YProp.IsNone())
		{
			UK2Node_VariableGet* YGet = MakeVarGetInGraph(StateGraph, YProp, -420, 40);
			LocalOk &= ConnectChecked(
				YGet ? YGet->GetValuePin() : nullptr,
				FindNamedPin(Player, TEXT("Y"), EGPD_Input),
				TEXT("Y->BS"));
		}
		if (!RateProp.IsNone())
		{
			if (UEdGraphPin* RatePin = FindNamedPin(Player, TEXT("PlayRate"), EGPD_Input))
			{
				UK2Node_VariableGet* RateGet = MakeVarGetInGraph(StateGraph, RateProp, -420, 120);
				ConnectChecked(RateGet ? RateGet->GetValuePin() : nullptr, RatePin, TEXT("PlayRate->BS"));
			}
		}
		LocalOk &= ConnectChecked(
			FindPosePin(Player, EGPD_Output),
			FindPosePin(StateGraph->MyResultNode, EGPD_Input),
			TEXT("BS->StateResult"));
		return LocalOk;
	};

	auto WireStateSequence = [&](UAnimStateNode* State, UAnimSequence* Seq) -> bool
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
		Player->Node.SetLoopAnimation(true);
		Player->CreateNewGuid();
		Player->PostPlacedNewNode();
		Player->AllocateDefaultPins();
		Player->NodePosX = -120;
		Player->NodePosY = 0;
		StateGraph->AddNode(Player, true, false);
		return ConnectChecked(
			FindPosePin(Player, EGPD_Output),
			FindPosePin(StateGraph->MyResultNode, EGPD_Input),
			TEXT("Seq->StateResult"));
	};

	bOk &= WireStateBlendSpace(
		MoveState,
		GroundBlendSpace,
		GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, Direction),
		GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, Speed),
		NAME_None);
	bOk &= WireStateSequence(JumpState, JumpSequence);
	bOk &= WireStateSequence(FallState, FallSequence);
	if (AirAttackState)
	{
		bOk &= WireStateSequence(AirAttackState, AirAttackSequence);
	}
	if (ClimbState)
	{
		bOk &= WireStateBlendSpace(
			ClimbState,
			ClimbBlendSpace,
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, ClimbYaw),
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, ClimbPitch),
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, ClimbPlayRate));
	}
	if (ClimbDashState)
	{
		bOk &= WireStateBlendSpace(
			ClimbDashState,
			ClimbDashBlendSpace,
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, ClimbYaw),
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, ClimbPitch),
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, ClimbPlayRate));
	}

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
			UE_LOG(LogTemp, Error, TEXT("[phoebe_loco] transition rule graph missing"));
			return false;
		}

		UK2Node_VariableGet* BoolGet = MakeVarGetInGraph(RuleGraph, BoolProp, -320, 0);
		UEdGraphPin* BoolPin = BoolGet ? BoolGet->GetValuePin() : nullptr;
		UEdGraphPin* ResultPin = RuleGraph->MyResultNode->FindPin(TEXT("bCanEnterTransition"));
		if (!BoolPin || !ResultPin)
		{
			UE_LOG(LogTemp, Error, TEXT("[phoebe_loco] transition pins missing for %s"), *BoolProp.ToString());
			return false;
		}

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
				UE_LOG(LogTemp, Error, TEXT("[phoebe_loco] Not_PreBool pins missing"));
				return false;
			}
			const UEdGraphSchema* RuleSchema = RuleGraph->GetSchema();
			RuleSchema->TryCreateConnection(BoolPin, NotIn);
			RuleSchema->BreakPinLinks(*ResultPin, true);
			if (!RuleSchema->TryCreateConnection(NotOut, ResultPin) || ResultPin->LinkedTo.Num() == 0)
			{
				UE_LOG(LogTemp, Error, TEXT("[phoebe_loco] failed Not->TransitionResult for %s"), *BoolProp.ToString());
				return false;
			}
			return true;
		}

		const UEdGraphSchema* RuleSchema = RuleGraph->GetSchema();
		RuleSchema->BreakPinLinks(*ResultPin, true);
		if (!RuleSchema->TryCreateConnection(BoolPin, ResultPin) || ResultPin->LinkedTo.Num() == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("[phoebe_loco] failed Bool->TransitionResult for %s"), *BoolProp.ToString());
			return false;
		}
		return true;
	};

	auto MakeBoolPin = [&](UEdGraph* Graph, FName Prop, bool bNegate, int32 X, int32 Y) -> UEdGraphPin*
	{
		UK2Node_VariableGet* BoolGet = MakeVarGetInGraph(Graph, Prop, X, Y);
		UEdGraphPin* BoolPin = BoolGet ? BoolGet->GetValuePin() : nullptr;
		if (!BoolPin || !bNegate)
		{
			return BoolPin;
		}
		UK2Node_CallFunction* NotNode = NewObject<UK2Node_CallFunction>(Graph, NAME_None, RF_Transactional);
		NotNode->FunctionReference.SetExternalMember(
			GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool),
			UKismetMathLibrary::StaticClass());
		NotNode->CreateNewGuid();
		NotNode->PostPlacedNewNode();
		NotNode->AllocateDefaultPins();
		NotNode->NodePosX = X + 180;
		NotNode->NodePosY = Y;
		Graph->AddNode(NotNode, true, false);
		UEdGraphPin* NotIn = NotNode->FindPin(TEXT("A"));
		UEdGraphPin* NotOut = NotNode->GetReturnValuePin();
		if (!NotIn || !NotOut)
		{
			return nullptr;
		}
		Graph->GetSchema()->TryCreateConnection(BoolPin, NotIn);
		return NotOut;
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
		UEdGraphPin* PinA = MakeBoolPin(RuleGraph, PropA, bNegA, -420, -40);
		UEdGraphPin* PinB = MakeBoolPin(RuleGraph, PropB, bNegB, -420, 40);
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

	bOk &= MakeTransition(MoveState, JumpState, GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsInAir), false, 0.1f);
	bOk &= MakeTransition(JumpState, FallState, GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsFalling), false, 0.1f);
	bOk &= MakeTransition(JumpState, MoveState, GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsInAir), true, 0.15f);
	bOk &= MakeTransition(FallState, MoveState, GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsInAir), true, 0.15f);
	bOk &= MakeTransition(FallState, JumpState, GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsFalling), true, 0.1f);

	if (AirAttackState)
	{
		bOk &= MakeTransition(JumpState, AirAttackState, GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsAirAttacking), false, 0.08f);
		bOk &= MakeTransition(FallState, AirAttackState, GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsAirAttacking), false, 0.08f);
		bOk &= MakeAndTransition(
			AirAttackState, MoveState,
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsAirAttacking), true,
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsInAir), true, 0.12f);
		bOk &= MakeAndTransition(
			AirAttackState, FallState,
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsAirAttacking), true,
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsInAir), false, 0.1f);
	}

	if (ClimbState)
	{
		bOk &= MakeTransition(MoveState, ClimbState, GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsClimbing), false, 0.15f);
		bOk &= MakeTransition(JumpState, ClimbState, GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsClimbing), false, 0.1f);
		bOk &= MakeTransition(FallState, ClimbState, GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsClimbing), false, 0.1f);
		bOk &= MakeAndTransition(
			ClimbState, MoveState,
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsClimbing), true,
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsInAir), true, 0.2f);
		bOk &= MakeAndTransition(
			ClimbState, FallState,
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsClimbing), true,
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsInAir), false, 0.2f);
	}

	if (ClimbState && ClimbDashState)
	{
		bOk &= MakeTransition(ClimbState, ClimbDashState, GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsClimbDashing), false, 0.28f);
		bOk &= MakeAndTransition(
			ClimbDashState, ClimbState,
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsClimbDashing), true,
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsClimbing), false, 0.28f);
		bOk &= MakeAndTransition(
			ClimbDashState, FallState,
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsClimbing), true,
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsInAir), false, 0.2f);
		bOk &= MakeAndTransition(
			ClimbDashState, MoveState,
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsClimbing), true,
			GET_MEMBER_NAME_CHECKED(UPhoebeAnimInstance, bIsInAir), true, 0.2f);
	}

	// Unarmed-style: SM → SaveCachedPose("Locomotion") ; UseCachedPose → Slot → Root
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
		if (UPhoebeAnimInstance* CDO = Cast<UPhoebeAnimInstance>(Gen->GetDefaultObject()))
		{
			CDO->GroundBlendSpace = GroundBlendSpace;
			CDO->ClimbBlendSpace = ClimbBlendSpace;
			CDO->ClimbDashBlendSpace = ClimbDashBlendSpace;
			CDO->JumpSequence = JumpSequence;
			CDO->FallSequence = FallSequence;
			CDO->AirAttackSequence = AirAttackSequence;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(AnimBP);
	const FString Audit = AuditPhoebeLocomotionGraph(AnimBP);
	UE_LOG(LogTemp, Log, TEXT("[phoebe_loco] WirePhoebeLocomotionGraph (StateMachine) ok=%d audit=%s"),
		bOk ? 1 : 0, *Audit);
	return bOk;
#else
	return false;
#endif
}

FString UPhoebeAnimSetupLibrary::AuditPhoebeLocomotionGraph(UAnimBlueprint* AnimBP)
{
#if WITH_EDITOR
	if (!AnimBP)
	{
		return TEXT("missing AnimBP");
	}

	int32 SMCount = 0;
	int32 LocoGraphCount = 0;
	TArray<FString> Parts;
	TArray<UEdGraph*> Graphs;
	AnimBP->GetAllGraphs(Graphs);
	UEdGraph* AnimGraph = nullptr;
	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}
		const FString Name = Graph->GetName();
		if (Graph->GetFName() == TEXT("AnimGraph"))
		{
			AnimGraph = Graph;
		}
		if (Name.Equals(TEXT("Locomotion")) || Name.StartsWith(TEXT("Locomotion")))
		{
			++LocoGraphCount;
		}
		if (Graph->IsA<UAnimationStateMachineGraph>())
		{
			++SMCount;
		}
	}
	Parts.Add(FString::Printf(TEXT("graphs=%d sm=%d loco=%d"), Graphs.Num(), SMCount, LocoGraphCount));

	int32 AnimGraphSM = 0;
	int32 SavePoseLinks = 0;
	FString SaveCacheName;
	int32 RootLinks = 0;
	int32 SlotLinks = 0;
	if (AnimGraph)
	{
		for (UEdGraphNode* Node : AnimGraph->Nodes)
		{
			if (Cast<UAnimGraphNode_StateMachine>(Node))
			{
				++AnimGraphSM;
			}
			if (const UAnimGraphNode_SaveCachedPose* SavePose = Cast<UAnimGraphNode_SaveCachedPose>(Node))
			{
				SaveCacheName = SavePose->CacheName;
				for (UEdGraphPin* Pin : SavePose->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Input && Pin->PinName == TEXT("Pose"))
					{
						SavePoseLinks = Pin->LinkedTo.Num();
						break;
					}
				}
			}
			if (const UAnimGraphNode_Root* Root = Cast<UAnimGraphNode_Root>(Node))
			{
				for (UEdGraphPin* Pin : Root->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Input)
					{
						RootLinks = Pin->LinkedTo.Num();
						break;
					}
				}
			}
			if (const UAnimGraphNode_Slot* Slot = Cast<UAnimGraphNode_Slot>(Node))
			{
				for (UEdGraphPin* Pin : Slot->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Input)
					{
						SlotLinks = Pin->LinkedTo.Num();
						break;
					}
				}
			}
		}
	}
	Parts.Add(FString::Printf(TEXT("rootIn=%d slotIn=%d animSM=%d savePoseIn=%d cache=%s"),
		RootLinks, SlotLinks, AnimGraphSM, SavePoseLinks, *SaveCacheName));

	int32 ClimbBS = 0;
	int32 ClimbSeq = 0;
	int32 DashBS = 0;
	int32 DashSeq = 0;
	for (UEdGraph* Graph : Graphs)
	{
		const UAnimationStateGraph* StateGraph = Cast<UAnimationStateGraph>(Graph);
		if (!StateGraph)
		{
			continue;
		}
		const FString GraphName = Graph->GetName();
		const bool bClimbDashGraph = GraphName.Contains(TEXT("ClimbDash"));
		const bool bClimbGraph = !bClimbDashGraph && GraphName.Contains(TEXT("Climb"));
		if (!bClimbGraph && !bClimbDashGraph)
		{
			continue;
		}
		for (UEdGraphNode* Node : StateGraph->Nodes)
		{
			if (Cast<UAnimGraphNode_BlendSpacePlayer>(Node))
			{
				if (bClimbDashGraph)
				{
					++DashBS;
				}
				else
				{
					++ClimbBS;
				}
			}
			if (Cast<UAnimGraphNode_SequencePlayer>(Node))
			{
				if (bClimbDashGraph)
				{
					++DashSeq;
				}
				else
				{
					++ClimbSeq;
				}
			}
		}
	}
	Parts.Add(FString::Printf(TEXT("climbBS=%d climbSeq=%d dashBS=%d dashSeq=%d"),
		ClimbBS, ClimbSeq, DashBS, DashSeq));

	for (UEdGraph* Graph : Graphs)
	{
		const UAnimationStateGraph* StateGraph = Cast<UAnimationStateGraph>(Graph);
		if (!StateGraph)
		{
			continue;
		}
		if (!Graph->GetName().Contains(TEXT("Move")))
		{
			continue;
		}
		for (UEdGraphNode* Node : StateGraph->Nodes)
		{
			const UAnimGraphNode_BlendSpacePlayer* Player = Cast<UAnimGraphNode_BlendSpacePlayer>(Node);
			if (!Player)
			{
				continue;
			}
			FString PinDump;
			for (UEdGraphPin* Pin : Player->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Input)
				{
					continue;
				}
				PinDump += FString::Printf(TEXT("%s:%d,"), *Pin->PinName.ToString(), Pin->LinkedTo.Num());
			}
			Parts.Add(FString::Printf(TEXT("MoveBS pins=%s"), *PinDump));
		}
	}

	const FString Result = FString::Join(Parts, TEXT(" | "));
	UE_LOG(LogTemp, Log, TEXT("[phoebe_loco] audit %s"), *Result);
	return Result;
#else
	return TEXT("non-editor");
#endif
}

bool UPhoebeAnimSetupLibrary::PatchPhoebeClimbCrossfades(UAnimBlueprint* AnimBP)
{
#if WITH_EDITOR
	if (!AnimBP)
	{
		return false;
	}

	TArray<UEdGraph*> Graphs;
	AnimBP->GetAllGraphs(Graphs);

	int32 Patched = 0;
	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph || !Graph->IsA<UAnimationStateMachineGraph>())
		{
			continue;
		}

		bool bHasClimbDash = false;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (const UAnimStateNode* State = Cast<UAnimStateNode>(Node))
			{
				if (State->GetStateName().Contains(TEXT("ClimbDash")))
				{
					bHasClimbDash = true;
					break;
				}
			}
		}
		if (!bHasClimbDash)
		{
			continue;
		}

		Graph->Modify();
		UE_LOG(LogTemp, Log, TEXT("[phoebe_loco] patch SM graph=%s nodes=%d"), *Graph->GetName(), Graph->Nodes.Num());
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimStateTransitionNode* Trans = Cast<UAnimStateTransitionNode>(Node);
			if (!Trans)
			{
				continue;
			}

			const UAnimStateNodeBase* From = Trans->GetPreviousState();
			const UAnimStateNodeBase* To = Trans->GetNextState();
			if (!From || !To)
			{
				UE_LOG(LogTemp, Warning, TEXT("[phoebe_loco] transition missing endpoints from=%s to=%s"),
					From ? *From->GetStateName() : TEXT("null"),
					To ? *To->GetStateName() : TEXT("null"));
				continue;
			}

			const FString FromName = From->GetStateName();
			const FString ToName = To->GetStateName();
			const bool bFromDash = FromName.Contains(TEXT("ClimbDash"), ESearchCase::IgnoreCase);
			const bool bToDash = ToName.Contains(TEXT("ClimbDash"), ESearchCase::IgnoreCase);
			const bool bFromClimb = !bFromDash && FromName.Contains(TEXT("Climb"), ESearchCase::IgnoreCase);
			const bool bToClimb = !bToDash && ToName.Contains(TEXT("Climb"), ESearchCase::IgnoreCase);
			const bool bToFall = ToName.Contains(TEXT("Fall"), ESearchCase::IgnoreCase);

			float NewBlend = -1.f;
			if ((bFromClimb && bToDash) || (bFromDash && bToClimb))
			{
				NewBlend = 0.28f;
			}
			else if ((bFromClimb || bFromDash) && bToFall)
			{
				NewBlend = 0.2f;
			}

			UE_LOG(LogTemp, Log, TEXT("[phoebe_loco] trans %s -> %s blend=%.3f target=%.3f"),
				*FromName, *ToName, Trans->CrossfadeDuration, NewBlend);

			if (NewBlend > 0.f)
			{
				Trans->Modify();
				Trans->CrossfadeDuration = NewBlend;
				++Patched;
			}
		}
	}

	if (Patched > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(AnimBP);
		AnimBP->MarkPackageDirty();
	}

	UE_LOG(LogTemp, Log, TEXT("[phoebe_loco] PatchPhoebeClimbCrossfades patched=%d"), Patched);
	return true;
#else
	return false;
#endif
}

bool UPhoebeAnimSetupLibrary::ApplyInPlaceRootLockToSequence(UAnimSequence* Sequence)
{
	if (!Sequence)
	{
		return false;
	}
	Sequence->bEnableRootMotion = false;
	Sequence->bForceRootLock = true;
	return true;
}

bool UPhoebeAnimSetupLibrary::ApplyInPlaceRootLockToMontage(UAnimMontage* Montage)
{
	if (!Montage)
	{
		return false;
	}
	bool bAny = false;
	for (const FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
	{
		for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
		{
			if (UAnimSequence* Seq = Cast<UAnimSequence>(Segment.GetAnimReference()))
			{
				bAny |= ApplyInPlaceRootLockToSequence(Seq);
			}
		}
	}
	if (UAnimSequence* First = Cast<UAnimSequence>(Montage->GetFirstAnimReference()))
	{
		bAny |= ApplyInPlaceRootLockToSequence(First);
	}
	return bAny;
}

bool UPhoebeAnimSetupLibrary::PrepareInPlaceLoopSequence(UAnimSequence* Sequence)
{
	if (!ApplyInPlaceRootLockToSequence(Sequence))
	{
		return false;
	}
#if WITH_EDITOR
	Sequence->Modify();
	Sequence->AdditiveAnimType = AAT_None;
	Sequence->PostEditChange();
	Sequence->MarkPackageDirty();
#endif
	return true;
}

bool UPhoebeAnimSetupLibrary::PrepareInPlaceCombatMontage(UAnimMontage* Montage)
{
	if (!ApplyInPlaceRootLockToMontage(Montage))
	{
		return false;
	}
#if WITH_EDITOR
	Montage->Modify();
	for (const FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
	{
		for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
		{
			if (UAnimSequence* Seq = Cast<UAnimSequence>(Segment.GetAnimReference()))
			{
				Seq->Modify();
				Seq->AdditiveAnimType = AAT_None;
				Seq->PostEditChange();
				Seq->MarkPackageDirty();
			}
		}
	}
	Montage->PostEditChange();
	Montage->MarkPackageDirty();
#endif
	return true;
}

UAnimSequence* UPhoebeAnimSetupLibrary::ConcatenateAnimSequences(
	UAnimSequence* First,
	UAnimSequence* Second,
	const FString& PackagePath,
	const FString& AssetName)
{
#if WITH_EDITOR
	if (!First || !Second || PackagePath.IsEmpty() || AssetName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[phoebe_loco] concatenate missing args First=%s Second=%s Path=%s Name=%s"),
			*GetNameSafe(First), *GetNameSafe(Second), *PackagePath, *AssetName);
		return nullptr;
	}
	if (First->GetSkeleton() != Second->GetSkeleton() || First->GetSkeleton() == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[phoebe_loco] concatenate skeleton mismatch %s / %s"),
			*GetNameSafe(First), *GetNameSafe(Second));
		return nullptr;
	}

	const IAnimationDataModel* ModelA = First->GetDataModel();
	const IAnimationDataModel* ModelB = Second->GetDataModel();
	if (!ModelA || !ModelB)
	{
		UE_LOG(LogTemp, Warning, TEXT("[phoebe_loco] concatenate missing data model"));
		return nullptr;
	}

	const int32 KeysA = FMath::Max(1, ModelA->GetNumberOfKeys());
	const int32 KeysB = FMath::Max(1, ModelB->GetNumberOfKeys());
	const int32 SkipB = (KeysA > 0 && KeysB > 1) ? 1 : 0;
	const int32 TotalKeys = KeysA + KeysB - SkipB;
	if (TotalKeys < 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("[phoebe_loco] concatenate too few keys %s+%s"),
			*GetNameSafe(First), *GetNameSafe(Second));
		return nullptr;
	}

	const FString LongPackageName = PackagePath / AssetName;
	const FString ObjectPath = LongPackageName + TEXT(".") + AssetName;
	UAnimSequence* Dest = LoadObject<UAnimSequence>(nullptr, *ObjectPath);
	bool bCreated = false;
	if (!Dest)
	{
		UPackage* Package = CreatePackage(*LongPackageName);
		if (!Package)
		{
			UE_LOG(LogTemp, Warning, TEXT("[phoebe_loco] concatenate CreatePackage failed %s"), *LongPackageName);
			return nullptr;
		}
		Dest = NewObject<UAnimSequence>(Package, FName(*AssetName), RF_Public | RF_Standalone);
		if (!Dest)
		{
			return nullptr;
		}
		Dest->SetSkeleton(First->GetSkeleton());
		FAssetRegistryModule::AssetCreated(Dest);
		bCreated = true;
	}
	else
	{
		Dest->Modify();
		if (Dest->GetSkeleton() != First->GetSkeleton())
		{
			Dest->SetSkeleton(First->GetSkeleton());
		}
	}

	IAnimationDataController& Controller = Dest->GetController();
	Controller.InitializeModel();
	{
		IAnimationDataController::FScopedBracket Bracket(
			Controller, NSLOCTEXT("PhoebeAnim", "ConcatClimb", "Concatenate climb halves"), false);

		Controller.RemoveAllBoneTracks(false);
		Controller.SetFrameRate(ModelA->GetFrameRate(), false);
		Controller.SetNumberOfFrames(FFrameNumber(TotalKeys - 1), false);

		TArray<FName> NamesA;
		TArray<FName> NamesB;
		ModelA->GetBoneTrackNames(NamesA);
		ModelB->GetBoneTrackNames(NamesB);
		TSet<FName> AllNames;
		AllNames.Append(NamesA);
		AllNames.Append(NamesB);

		auto SampleKey = [](const TArray<FTransform>& Keys, int32 Index) -> FTransform
		{
			if (Keys.Num() <= 0)
			{
				return FTransform::Identity;
			}
			if (Keys.Num() == 1)
			{
				return Keys[0];
			}
			return Keys[FMath::Clamp(Index, 0, Keys.Num() - 1)];
		};

		TArray<FVector3f> PosKeys;
		TArray<FQuat4f> RotKeys;
		TArray<FVector3f> ScaleKeys;
		TArray<FTransform> KeysFromA;
		TArray<FTransform> KeysFromB;

		for (const FName TrackName : AllNames)
		{
			KeysFromA.Reset();
			KeysFromB.Reset();
			if (ModelA->IsValidBoneTrackName(TrackName))
			{
				ModelA->GetBoneTrackTransforms(TrackName, KeysFromA);
			}
			if (ModelB->IsValidBoneTrackName(TrackName))
			{
				ModelB->GetBoneTrackTransforms(TrackName, KeysFromB);
			}

			PosKeys.Reset();
			RotKeys.Reset();
			ScaleKeys.Reset();
			PosKeys.Reserve(TotalKeys);
			RotKeys.Reserve(TotalKeys);
			ScaleKeys.Reserve(TotalKeys);

			for (int32 Index = 0; Index < KeysA; ++Index)
			{
				const FTransform T = KeysFromA.Num() > 0
					? SampleKey(KeysFromA, Index)
					: (KeysFromB.Num() > 0 ? SampleKey(KeysFromB, 0) : FTransform::Identity);
				PosKeys.Add(FVector3f(T.GetLocation()));
				RotKeys.Add(FQuat4f(T.GetRotation()));
				ScaleKeys.Add(FVector3f(T.GetScale3D()));
			}
			for (int32 Index = SkipB; Index < KeysB; ++Index)
			{
				const FTransform T = KeysFromB.Num() > 0
					? SampleKey(KeysFromB, Index)
					: (KeysFromA.Num() > 0 ? SampleKey(KeysFromA, KeysA - 1) : FTransform::Identity);
				PosKeys.Add(FVector3f(T.GetLocation()));
				RotKeys.Add(FQuat4f(T.GetRotation()));
				ScaleKeys.Add(FVector3f(T.GetScale3D()));
			}

			if (!Dest->GetDataModel() || !Dest->GetDataModel()->IsValidBoneTrackName(TrackName))
			{
				Controller.AddBoneCurve(TrackName, false);
			}
			Controller.SetBoneTrackKeys(TrackName, PosKeys, RotKeys, ScaleKeys, false);
		}

		Controller.NotifyPopulated();
	}

	Dest->bEnableRootMotion = false;
	Dest->bForceRootLock = true;
	Dest->AdditiveAnimType = AAT_None;
	Dest->Interpolation = First->Interpolation;
	Dest->PostEditChange();
	Dest->MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("[phoebe_loco] concatenate %s + %s -> %s keys=%d created=%d"),
		*GetNameSafe(First), *GetNameSafe(Second), *ObjectPath, TotalKeys, bCreated ? 1 : 0);
	return Dest;
#else
	return nullptr;
#endif
}

bool UPhoebeAnimSetupLibrary::ConfigureBlendSpaceAxis(
	UBlendSpace* BlendSpace,
	int32 AxisIndex,
	FName DisplayName,
	float MinValue,
	float MaxValue,
	int32 GridNum)
{
#if WITH_EDITOR
	if (!BlendSpace || AxisIndex < 0 || AxisIndex > 2)
	{
		return false;
	}
	BlendSpace->Modify();
	if (FProperty* Prop = FindFProperty<FProperty>(UBlendSpace::StaticClass(), TEXT("BlendParameters")))
	{
		FBlendParameter* Params = Prop->ContainerPtrToValuePtr<FBlendParameter>(BlendSpace);
		Params[AxisIndex].DisplayName = DisplayName.ToString();
		Params[AxisIndex].Min = MinValue;
		Params[AxisIndex].Max = MaxValue;
		Params[AxisIndex].GridNum = FMath::Max(1, GridNum);
	}
	else
	{
		return false;
	}
	// Runtime triangulation with 3x3 samples hits SampleData[9] (size 9) near walls.
	BlendSpace->bInterpolateUsingGrid = true;
	BlendSpace->ResampleData();
	BlendSpace->ValidateSampleData();
	BlendSpace->PostEditChange();
	BlendSpace->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

int32 UPhoebeAnimSetupLibrary::ReplaceBlendSpaceSamples(
	UBlendSpace* BlendSpace,
	const TArray<UAnimSequence*>& Sequences,
	const TArray<FVector>& Positions)
{
#if WITH_EDITOR
	if (!BlendSpace || Sequences.Num() != Positions.Num())
	{
		return 0;
	}

	BlendSpace->Modify();
	while (BlendSpace->GetNumberOfBlendSamples() > 0)
	{
		if (!BlendSpace->DeleteSample(0))
		{
			break;
		}
	}

	int32 Added = 0;
	for (int32 Index = 0; Index < Sequences.Num(); ++Index)
	{
		UAnimSequence* Seq = Sequences[Index];
		if (!Seq)
		{
			continue;
		}
		const int32 SampleIndex = BlendSpace->AddSample(Seq, Positions[Index]);
		if (SampleIndex != INDEX_NONE)
		{
			++Added;
		}
	}
	BlendSpace->bInterpolateUsingGrid = true;
	BlendSpace->ResampleData();
	BlendSpace->ValidateSampleData();
	BlendSpace->PostEditChange();
	BlendSpace->MarkPackageDirty();
	return Added;
#else
	return 0;
#endif
}

int32 UPhoebeAnimSetupLibrary::SetSectionCastShadowBySlot(USkeletalMesh* Mesh, const FString& SlotContains, bool bCastShadow)
{
#if WITH_EDITOR
	if (!Mesh || SlotContains.IsEmpty())
	{
		return 0;
	}

	FSkeletalMeshModel* ImportedModel = Mesh->GetImportedModel();
	if (!ImportedModel)
	{
		return 0;
	}

	TSet<int32> MatchIndices;
	const TArray<FSkeletalMaterial>& Materials = Mesh->GetMaterials();
	for (int32 Index = 0; Index < Materials.Num(); ++Index)
	{
		if (Materials[Index].MaterialSlotName.ToString().Contains(SlotContains, ESearchCase::IgnoreCase))
		{
			MatchIndices.Add(Index);
		}
	}
	if (MatchIndices.Num() == 0)
	{
		return 0;
	}

	Mesh->Modify();
	int32 Changed = 0;
	for (int32 LodIndex = 0; LodIndex < ImportedModel->LODModels.Num(); ++LodIndex)
	{
		FSkeletalMeshLODModel& LodModel = ImportedModel->LODModels[LodIndex];
		for (FSkelMeshSection& Section : LodModel.Sections)
		{
			if (!MatchIndices.Contains(Section.MaterialIndex))
			{
				continue;
			}
			if (Section.bCastShadow != bCastShadow)
			{
				Section.bCastShadow = bCastShadow;
				++Changed;
			}
			FSkelMeshSourceSectionUserData& UserData =
				FSkelMeshSourceSectionUserData::GetSourceSectionUserData(LodModel.UserSectionsData, Section);
			if (UserData.bCastShadow != bCastShadow)
			{
				UserData.bCastShadow = bCastShadow;
				++Changed;
			}
		}
		LodModel.SyncronizeUserSectionsDataArray();
	}

	if (Changed > 0)
	{
		ImportedModel->SyncronizeLODUserSectionsData();
		Mesh->PostEditChange();
		Mesh->MarkPackageDirty();
	}
	return Changed;
#else
	return 0;
#endif
}
