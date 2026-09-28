// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SlimeXinPhysicsLibrary.generated.h"

class UAnimBlueprint;
class USkeletalMesh;

/**
 * Editor-only helpers for 心月狐: strip leftover Chaos accessory bodies and wire
 * KawaiiPhysics spring chains onto a post-process anim blueprint.
 */
UCLASS()
class SLIMEFABLE_API USlimeXinPhysicsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Destroy every PhysicsAsset body/constraint. Restore recreates only core ragdoll capsules. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Editor")
	static int32 ClearXinSimulatedPhysics(USkeletalMesh* Mesh);

	/**
	 * Recreate Default collision bodies on the core humanoid bones so death ragdoll can hit the floor.
	 * Always overwrites auto-sized CreateNewBody geom with 6–14cm capsules. Hair/dress stay out of the PA.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|Editor")
	static int32 RestoreXinRagdollBodies(USkeletalMesh* Mesh);

	/** Log each PA body bone/type/radius. Returns body count, or -1. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Editor")
	static int32 DumpXinPhysicsAsset(USkeletalMesh* Mesh);

	/**
	 * Wire Input Pose -> Hand SlimeArmIK -> KawaiiPhysics (tail / ears / hair / dress / sleeves)
	 * -> Output Pose, compile, and assign the generated class as the mesh's post-process ABP.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|Editor")
	static bool WireXinKawaiiPhysics(UAnimBlueprint* AnimBP, USkeletalMesh* Mesh);
};
