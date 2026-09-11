// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GaspRagdollEnemy.h"
#include "GaspCostumeEnemy.generated.h"

/**
 * GASP ragdoll enemy wearing a Content/Enemy costume mesh (UEFN walk + visual retarget).
 * Blueprint children of BP_GaspRagdollEnemy can also set CostumeKind on the sandbox parent.
 */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API AGaspCostumeEnemy : public AGaspRagdollEnemy
{
	GENERATED_BODY()

public:
	AGaspCostumeEnemy();

	virtual void EnsureMoveKit() override;
};
