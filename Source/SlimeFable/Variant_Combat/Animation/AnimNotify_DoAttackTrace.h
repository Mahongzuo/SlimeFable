// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_DoAttackTrace.generated.h"

/**
 *  AnimNotify to tell the actor to perform an attack trace check to look for targets to damage.
 */
UCLASS()
class UAnimNotify_DoAttackTrace : public UAnimNotify
{
	GENERATED_BODY()
	
protected:

	/** Source bone for the attack trace */
	UPROPERTY(EditAnywhere, Category="Attack")
	FName AttackBoneName;

public:

	/** Perform the Anim Notify */
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	/** Get the notify name */
	virtual FString GetNotifyName_Implementation() const override;
};
