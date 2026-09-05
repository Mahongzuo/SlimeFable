// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SlimeBlueprintFixupLibrary.generated.h"

class UBlueprint;

/**
 * Editor-only Blueprint surgery that the Python API cannot do: UE 5.8 marks UBlueprint's
 * FunctionGraphs / UbergraphPages / NewVariables as protected, so scripts cannot touch nodes.
 *
 * Used when a copy of an official Blueprint is reparented onto a copy of its parent: every node
 * that hard-references the original class (casts, parent function calls, variable types) has to be
 * pointed at the copy, or the Blueprint will not compile.
 */
UCLASS()
class SLIMEFABLE_API USlimeBlueprintFixupLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Repoint every reference to OldClass inside Blueprint at NewClass.
	 * Covers dynamic casts, function / variable member references and pin types.
	 * @return number of nodes and variables touched, or -1 on bad input.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|Editor")
	static int32 RetargetBlueprintClassRefs(UBlueprint* Blueprint, UClass* OldClass, UClass* NewClass);

	/**
	 * Every remaining reference to Class inside Blueprint, one description per line.
	 * Run after RetargetBlueprintClassRefs: anything left here will fail to compile.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|Editor")
	static TArray<FString> FindBlueprintClassRefs(UBlueprint* Blueprint, UClass* Class);

	/** Compile Blueprint and return the number of compiler errors (-1 on bad input). */
	UFUNCTION(BlueprintCallable, Category = "Slime|Editor")
	static int32 CompileBlueprintAndCountErrors(UBlueprint* Blueprint);

	/** SCS component names, including inherited ones, for post-reparent assertions. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Editor")
	static TArray<FString> GetBlueprintComponentNames(UBlueprint* Blueprint);

	/** One line per SCS node: "Class/Node:Component -> parent" — reparenting can drop parents. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Editor")
	static TArray<FString> GetBlueprintScsTree(UBlueprint* Blueprint);

	/**
	 * Re-point an SCS node at a component inherited from a parent Blueprint.
	 * Reparenting clears this when the new parent class has not compiled yet, which silently
	 * moves components (e.g. PhysicsControl) off the skeletal mesh and onto the actor root.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|Editor")
	static bool FixupScsParent(UBlueprint* Blueprint, FName ChildComponent, FName InheritedParent);
};
