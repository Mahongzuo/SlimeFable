// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeBlueprintFixupLibrary.h"

#include "SlimeFable.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ClassDynamicCast.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Variable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

#if WITH_EDITOR
namespace SlimeBpFixup
{
	void CollectGraphs(UBlueprint* Blueprint, TArray<UEdGraph*>& OutGraphs)
	{
		Blueprint->GetAllGraphs(OutGraphs);
	}

	bool FixPinTypes(UEdGraphNode* Node, UClass* OldClass, UClass* NewClass)
	{
		bool bChanged = false;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin)
			{
				continue;
			}
			if (Pin->PinType.PinSubCategoryObject.Get() == OldClass)
			{
				Pin->PinType.PinSubCategoryObject = NewClass;
				bChanged = true;
			}
		}
		return bChanged;
	}

	/**
	 * Class literals sitting on pins, e.g. SpawnActor's Class pin. These are not pin *types*,
	 * but SpawnActor derives its Return Value type from this default, so missing them leaves
	 * "X is not compatible with Y (by ref)" errors behind.
	 */
	bool FixPinDefaults(UEdGraphNode* Node, UClass* OldClass, UClass* NewClass)
	{
		bool bChanged = false;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->DefaultObject == OldClass)
			{
				Pin->DefaultObject = NewClass;
				bChanged = true;
			}
		}
		return bChanged;
	}

	bool FixMemberReference(FMemberReference& Ref, UClass* OldClass, UClass* NewClass)
	{
		if (Ref.GetMemberParentClass() != OldClass)
		{
			return false;
		}
		Ref.SetExternalMember(Ref.GetMemberName(), NewClass);
		return true;
	}
}
#endif

int32 USlimeBlueprintFixupLibrary::RetargetBlueprintClassRefs(UBlueprint* Blueprint, UClass* OldClass, UClass* NewClass)
{
#if WITH_EDITOR
	if (!Blueprint || !OldClass || !NewClass || OldClass == NewClass)
	{
		return -1;
	}

	int32 Touched = 0;

	// Blueprint variables typed as the original class.
	for (FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarType.PinSubCategoryObject.Get() == OldClass)
		{
			Var.VarType.PinSubCategoryObject = NewClass;
			++Touched;
			UE_LOG(LogSlimeFable, Log, TEXT("[bp-fixup] var %s retyped to %s"),
				*Var.VarName.ToString(), *NewClass->GetName());
		}
	}

	TArray<UEdGraph*> Graphs;
	SlimeBpFixup::CollectGraphs(Blueprint, Graphs);

	TArray<UEdGraphNode*> ToReconstruct;
	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}
			bool bNodeChanged = false;

			if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
			{
				if (CastNode->TargetType == OldClass)
				{
					// The result pin is named after the target class, so a plain ReconstructNode
					// leaves the old wired pin behind as an orphan. Move the links by hand.
					TArray<UEdGraphPin*> SavedLinks;
					if (UEdGraphPin* OldResult = CastNode->GetCastResultPin())
					{
						SavedLinks = OldResult->LinkedTo;
					}
					CastNode->TargetType = NewClass;
					CastNode->ReconstructNode();
					if (UEdGraphPin* NewResult = CastNode->GetCastResultPin())
					{
						for (UEdGraphPin* Other : SavedLinks)
						{
							if (Other && !NewResult->LinkedTo.Contains(Other))
							{
								NewResult->MakeLinkTo(Other);
							}
						}
					}
					for (int32 PinIndex = CastNode->Pins.Num() - 1; PinIndex >= 0; --PinIndex)
					{
						UEdGraphPin* Pin = CastNode->Pins[PinIndex];
						if (Pin && Pin->bOrphanedPin)
						{
							Pin->BreakAllPinLinks();
							CastNode->RemovePin(Pin);
						}
					}
					++Touched;
					UE_LOG(LogSlimeFable, Log, TEXT("[bp-fixup] cast in %s -> %s (%d links moved)"),
						*Graph->GetName(), *NewClass->GetName(), SavedLinks.Num());
					continue;
				}
			}
			else if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
			{
				if (SlimeBpFixup::FixMemberReference(CallNode->FunctionReference, OldClass, NewClass))
				{
					bNodeChanged = true;
					UE_LOG(LogSlimeFable, Log, TEXT("[bp-fixup] call %s in %s -> %s"),
						*CallNode->FunctionReference.GetMemberName().ToString(),
						*Graph->GetName(), *NewClass->GetName());
				}
			}
			else if (UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(Node))
			{
				if (SlimeBpFixup::FixMemberReference(VarNode->VariableReference, OldClass, NewClass))
				{
					bNodeChanged = true;
					UE_LOG(LogSlimeFable, Log, TEXT("[bp-fixup] varnode %s in %s -> %s"),
						*VarNode->VariableReference.GetMemberName().ToString(),
						*Graph->GetName(), *NewClass->GetName());
				}
			}

			if (SlimeBpFixup::FixPinDefaults(Node, OldClass, NewClass))
			{
				bNodeChanged = true;
				UE_LOG(LogSlimeFable, Log, TEXT("[bp-fixup] class literal on %s in %s -> %s"),
					*Node->GetClass()->GetName(), *Graph->GetName(), *NewClass->GetName());
			}

			if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
			{
				for (FBPVariableDescription& Local : Entry->LocalVariables)
				{
					if (Local.VarType.PinSubCategoryObject.Get() == OldClass)
					{
						Local.VarType.PinSubCategoryObject = NewClass;
						bNodeChanged = true;
					}
				}
			}

			if (bNodeChanged)
			{
				++Touched;
				ToReconstruct.Add(Node);
			}
		}
	}

	// Reconstruct first so regenerated pins pick up the new class, then sweep leftovers
	// (user-defined pins, unconnected wildcards, function signature pins).
	for (UEdGraphNode* Node : ToReconstruct)
	{
		Node->ReconstructNode();
	}
	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}
			const bool bTypes = SlimeBpFixup::FixPinTypes(Node, OldClass, NewClass);
			const bool bDefaults = SlimeBpFixup::FixPinDefaults(Node, OldClass, NewClass);
			if (bTypes || bDefaults)
			{
				++Touched;
			}
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	UE_LOG(LogSlimeFable, Log, TEXT("[bp-fixup] %s: %s -> %s, touched %d"),
		*Blueprint->GetName(), *OldClass->GetName(), *NewClass->GetName(), Touched);
	return Touched;
#else
	(void)Blueprint;
	(void)OldClass;
	(void)NewClass;
	return -1;
#endif
}

TArray<FString> USlimeBlueprintFixupLibrary::FindBlueprintClassRefs(UBlueprint* Blueprint, UClass* Class)
{
	TArray<FString> Refs;
#if WITH_EDITOR
	if (!Blueprint || !Class)
	{
		return Refs;
	}

	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarType.PinSubCategoryObject.Get() == Class)
		{
			Refs.Add(FString::Printf(TEXT("var %s"), *Var.VarName.ToString()));
		}
	}

	TArray<UEdGraph*> Graphs;
	SlimeBpFixup::CollectGraphs(Blueprint, Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}
			const FString Where = FString::Printf(TEXT("%s/%s"), *Graph->GetName(), *Node->GetClass()->GetName());
			if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
			{
				if (CastNode->TargetType == Class)
				{
					Refs.Add(FString::Printf(TEXT("cast %s"), *Where));
				}
			}
			if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
			{
				if (CallNode->FunctionReference.GetMemberParentClass() == Class)
				{
					Refs.Add(FString::Printf(TEXT("call %s::%s"), *Where,
						*CallNode->FunctionReference.GetMemberName().ToString()));
				}
			}
			if (UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(Node))
			{
				if (VarNode->VariableReference.GetMemberParentClass() == Class)
				{
					Refs.Add(FString::Printf(TEXT("varnode %s::%s"), *Where,
						*VarNode->VariableReference.GetMemberName().ToString()));
				}
			}
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin)
				{
					continue;
				}
				if (Pin->PinType.PinSubCategoryObject.Get() == Class)
				{
					Refs.Add(FString::Printf(TEXT("pin type %s.%s"), *Where, *Pin->PinName.ToString()));
				}
				if (Pin->DefaultObject == Class)
				{
					Refs.Add(FString::Printf(TEXT("pin default %s.%s"), *Where, *Pin->PinName.ToString()));
				}
			}
		}
	}
#else
	(void)Blueprint;
	(void)Class;
#endif
	return Refs;
}

int32 USlimeBlueprintFixupLibrary::CompileBlueprintAndCountErrors(UBlueprint* Blueprint)
{
#if WITH_EDITOR
	if (!Blueprint)
	{
		return -1;
	}
	FCompilerResultsLog Results;
	Results.bSilentMode = false;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
	UE_LOG(LogSlimeFable, Log, TEXT("[bp-fixup] compiled %s: %d errors, %d warnings"),
		*Blueprint->GetName(), Results.NumErrors, Results.NumWarnings);
	return Results.NumErrors;
#else
	(void)Blueprint;
	return -1;
#endif
}

TArray<FString> USlimeBlueprintFixupLibrary::GetBlueprintScsTree(UBlueprint* Blueprint)
{
	TArray<FString> Lines;
#if WITH_EDITOR
	if (!Blueprint)
	{
		return Lines;
	}
	for (UClass* Class = Blueprint->GeneratedClass; Class; Class = Class->GetSuperClass())
	{
		UBlueprintGeneratedClass* BpClass = Cast<UBlueprintGeneratedClass>(Class);
		if (!BpClass || !BpClass->SimpleConstructionScript)
		{
			continue;
		}
		for (USCS_Node* Node : BpClass->SimpleConstructionScript->GetAllNodes())
		{
			if (!Node)
			{
				continue;
			}
			Lines.Add(FString::Printf(TEXT("%s/%s -> parent=%s native=%d owner=%s"),
				*Class->GetName(),
				*Node->GetVariableName().ToString(),
				*Node->ParentComponentOrVariableName.ToString(),
				Node->bIsParentComponentNative ? 1 : 0,
				*Node->ParentComponentOwnerClassName.ToString()));
		}
	}
#else
	(void)Blueprint;
#endif
	return Lines;
}

bool USlimeBlueprintFixupLibrary::FixupScsParent(UBlueprint* Blueprint, FName ChildComponent, FName InheritedParent)
{
#if WITH_EDITOR
	if (!Blueprint || !Blueprint->SimpleConstructionScript || ChildComponent.IsNone() || InheritedParent.IsNone())
	{
		return false;
	}

	USCS_Node* Child = nullptr;
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (Node && Node->GetVariableName() == ChildComponent)
		{
			Child = Node;
			break;
		}
	}
	if (!Child)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("[bp-fixup] %s has no SCS node %s"),
			*Blueprint->GetName(), *ChildComponent.ToString());
		return false;
	}
	if (Child->ParentComponentOrVariableName == InheritedParent)
	{
		return true;
	}

	// Find which ancestor class actually declares the wanted parent component.
	FName OwnerClassName = NAME_None;
	for (UClass* Class = Blueprint->ParentClass; Class; Class = Class->GetSuperClass())
	{
		UBlueprintGeneratedClass* BpClass = Cast<UBlueprintGeneratedClass>(Class);
		if (!BpClass || !BpClass->SimpleConstructionScript)
		{
			continue;
		}
		for (USCS_Node* Node : BpClass->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->GetVariableName() == InheritedParent)
			{
				OwnerClassName = FName(*Class->GetName());
				break;
			}
		}
		if (!OwnerClassName.IsNone())
		{
			break;
		}
	}
	if (OwnerClassName.IsNone())
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("[bp-fixup] %s: no ancestor declares %s"),
			*Blueprint->GetName(), *InheritedParent.ToString());
		return false;
	}

	Child->ParentComponentOrVariableName = InheritedParent;
	Child->ParentComponentOwnerClassName = OwnerClassName;
	Child->bIsParentComponentNative = false;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	UE_LOG(LogSlimeFable, Log, TEXT("[bp-fixup] %s: %s reattached to %s (%s)"),
		*Blueprint->GetName(), *ChildComponent.ToString(),
		*InheritedParent.ToString(), *OwnerClassName.ToString());
	return true;
#else
	(void)Blueprint;
	(void)ChildComponent;
	(void)InheritedParent;
	return false;
#endif
}

TArray<FString> USlimeBlueprintFixupLibrary::GetBlueprintComponentNames(UBlueprint* Blueprint)
{
	TArray<FString> Names;
#if WITH_EDITOR
	if (!Blueprint)
	{
		return Names;
	}
	UClass* Class = Blueprint->GeneratedClass;
	while (Class)
	{
		if (UBlueprintGeneratedClass* BpClass = Cast<UBlueprintGeneratedClass>(Class))
		{
			if (BpClass->SimpleConstructionScript)
			{
				for (USCS_Node* Node : BpClass->SimpleConstructionScript->GetAllNodes())
				{
					if (Node && Node->ComponentTemplate)
					{
						Names.AddUnique(FString::Printf(TEXT("%s:%s"),
							*Node->GetVariableName().ToString(),
							*Node->ComponentTemplate->GetClass()->GetName()));
					}
				}
			}
		}
		Class = Class->GetSuperClass();
	}
	if (UClass* Generated = Blueprint->GeneratedClass)
	{
		if (AActor* Cdo = Cast<AActor>(Generated->GetDefaultObject()))
		{
			for (UActorComponent* Comp : Cdo->GetComponents())
			{
				if (Comp)
				{
					Names.AddUnique(FString::Printf(TEXT("%s:%s"),
						*Comp->GetName(), *Comp->GetClass()->GetName()));
				}
			}
		}
	}
#else
	(void)Blueprint;
#endif
	return Names;
}
