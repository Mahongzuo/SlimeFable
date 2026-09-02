// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PhoebeAnimSetupLibrary.generated.h"

class UAnimBlueprint;
class UAnimMontage;
class UAnimSequence;
class UBlendSpace;
class UChooserTable;
class UPoseSearchDatabase;
class UPoseSearchSchema;
class USkeleton;
class USkeletalMesh;

/** Editor/Python helpers to populate Pose Search assets that reflection cannot write. */
UCLASS()
class SLIMEFABLE_API UPhoebeAnimSetupLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Phoebe|MotionMatching")
	static bool SetupPhoebeSchema(UPoseSearchSchema* Schema, USkeleton* Skeleton);

	UFUNCTION(BlueprintCallable, Category = "Phoebe|MotionMatching")
	static bool AddAnimationToDatabase(UPoseSearchDatabase* Database, UObject* AnimAsset);

	UFUNCTION(BlueprintCallable, Category = "Phoebe|MotionMatching")
	static int32 GetDatabaseAnimationCount(const UPoseSearchDatabase* Database);

	UFUNCTION(BlueprintCallable, Category = "Phoebe|MotionMatching")
	static bool SetupPhoebeChooser(
		UChooserTable* Chooser,
		UPoseSearchDatabase* ClimbDb,
		UPoseSearchDatabase* AirDb,
		UPoseSearchDatabase* SprintDb,
		UPoseSearchDatabase* RunDb,
		UPoseSearchDatabase* WalkDb,
		UPoseSearchDatabase* IdleDb);

	UFUNCTION(BlueprintCallable, Category = "Phoebe|MotionMatching")
	static bool WirePhoebeMotionMatchingGraph(UAnimBlueprint* AnimBP, UPoseSearchDatabase* DefaultDatabase);

	/**
	 * Third-person style AnimGraph (mirrors Mannequin ThirdPerson_AnimBP):
	 * Locomotion StateMachine (Move / Jump / Fall / AirAttack / Climb / ClimbDash) → DefaultSlot → Root.
	 * Destroys leftover StateMachine subgraphs so only one Locomotion graph remains.
	 */
	UFUNCTION(BlueprintCallable, Category = "Phoebe|Locomotion")
	static bool WirePhoebeLocomotionGraph(
		UAnimBlueprint* AnimBP,
		UBlendSpace* GroundBlendSpace,
		UBlendSpace* ClimbBlendSpace,
		UAnimSequence* JumpSequence,
		UAnimSequence* FallSequence,
		UBlendSpace* ClimbDashBlendSpace,
		UAnimSequence* AirAttackSequence);

	/** Force in-place looping clips (no root motion) so BlendSpace feet match code-driven motion. */
	UFUNCTION(BlueprintCallable, Category = "Phoebe|Locomotion")
	static bool PrepareInPlaceLoopSequence(UAnimSequence* Sequence);

	/** Runtime: disable root motion and lock the root bone so the mesh stays on the capsule. */
	UFUNCTION(BlueprintCallable, Category = "Phoebe|Locomotion")
	static bool ApplyInPlaceRootLockToSequence(UAnimSequence* Sequence);

	/** Runtime: lock every sequence referenced by a montage. */
	UFUNCTION(BlueprintCallable, Category = "Phoebe|Locomotion")
	static bool ApplyInPlaceRootLockToMontage(UAnimMontage* Montage);

	/** Editor: apply in-place lock and dirty the montage + slot sequences. */
	UFUNCTION(BlueprintCallable, Category = "Phoebe|Locomotion")
	static bool PrepareInPlaceCombatMontage(UAnimMontage* Montage);

	/**
	 * Bake First then Second into a new (or overwritten) AnimSequence.
	 * Used to turn climb *_1 + *_2 halves into a left/right looping clip.
	 */
	UFUNCTION(BlueprintCallable, Category = "Phoebe|Locomotion")
	static UAnimSequence* ConcatenateAnimSequences(
		UAnimSequence* First,
		UAnimSequence* Second,
		const FString& PackagePath,
		const FString& AssetName);

	/** Log StateMachine count, pin links, and leftover Locomotion graphs. */
	UFUNCTION(BlueprintCallable, Category = "Phoebe|Locomotion")
	static FString AuditPhoebeLocomotionGraph(UAnimBlueprint* AnimBP);

	/** Stretch Climb/ClimbDash/Fall crossfades without rebuilding the AnimGraph. */
	UFUNCTION(BlueprintCallable, Category = "Phoebe|Locomotion")
	static bool PatchPhoebeClimbCrossfades(UAnimBlueprint* AnimBP);

	UFUNCTION(BlueprintCallable, Category = "Phoebe|Locomotion")
	static bool ConfigureBlendSpaceAxis(
		UBlendSpace* BlendSpace,
		int32 AxisIndex,
		FName DisplayName,
		float MinValue,
		float MaxValue,
		int32 GridNum);

	/** Editor helper: clear samples then add (Anim, X, Y). Returns samples added. */
	UFUNCTION(BlueprintCallable, Category = "Phoebe|Locomotion")
	static int32 ReplaceBlendSpaceSamples(
		UBlendSpace* BlendSpace,
		const TArray<UAnimSequence*>& Sequences,
		const TArray<FVector>& Positions);

	/**
	 * Editor: set LOD section bCastShadow for slots whose name contains SlotContains
	 * (case-insensitive). Returns how many sections were written.
	 */
	UFUNCTION(BlueprintCallable, Category = "Phoebe|Mesh")
	static int32 SetSectionCastShadowBySlot(USkeletalMesh* Mesh, const FString& SlotContains, bool bCastShadow);
};
