// Copyright 2019-2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "Misc/EngineVersionComparison.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"

#if !UE_VERSION_OLDER_THAN(5, 5, 0)
#include "StructUtils/InstancedStruct.h"
#else
#include "InstancedStruct.h"
#endif

#include "AnimNotifyState_KawaiiPhysicsAddExternalForce.generated.h"

/**
 * AnimNotifyState の区間中、KawaiiPhysics に外力を追加する（開始で追加・終了で除去、タグでフィルタ可能）。
 * AnimNotifyState that adds external forces to KawaiiPhysics nodes for its duration (added on begin, removed on end; filterable by tag).
 */
UCLASS(Blueprintable, meta = (DisplayName = "KawaiiPhysics: Add ExternalForce"))
class KAWAIIPHYSICS_API UAnimNotifyState_KawaiiPhysicsAddExternalForce : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_KawaiiPhysicsAddExternalForce(const FObjectInitializer& ObjectInitializer);

	virtual FString GetNotifyName_Implementation() const override;

	/** 区間開始時に外力を追加 / Adds the external forces when the state begins. */
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
	                         const FAnimNotifyEventReference& EventReference) override;

	/** 区間終了時に外力を除去 / Removes the external forces when the state ends. */
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	                       const FAnimNotifyEventReference& EventReference) override;

	/**
	 * Additional external forces to be applied to the skeletal mesh component.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalForce",
		meta = (BaseStruct = "/Script/KawaiiPhysics.KawaiiPhysics_ExternalForce", ExcludeBaseStruct))
	TArray<FInstancedStruct> AdditionalExternalForces;

	/**
	 * Tags used to filter which external forces are applied. If empty, all nodes are applied.
	 * 適用する外力をフィルタリングするためのTag。 空の場合は全てのノードに適用されます。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalForce")
	FGameplayTagContainer FilterTags;

	/**
	 * Whether to filter tags to exact matches (if False, parent tags will also be included).
	 * Tagのフィルタリングにて完全一致にするか否か（Falseの場合は親Tagも含めます）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExternalForce")
	bool bFilterExactMatch;

#if WITH_EDITOR
	/**
	 * Validates the associated assets in the editor.
	 */
	virtual void ValidateAssociatedAssets() override;
#endif
};
