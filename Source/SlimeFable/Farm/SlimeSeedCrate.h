// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Hub/SlimeHubInteractActor.h"
#include "SlimeSeedCrate.generated.h"

class UStaticMeshComponent;

/** Once per real-world day, gives two of each museum seed. */
UCLASS(Blueprintable, meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeSeedCrate : public ASlimeHubInteractActor
{
	GENERATED_BODY()

public:
	ASlimeSeedCrate();

	virtual void BeginPlay() override;
	virtual bool TryInteract(APawn* Interactor) override;
	virtual FText GetInteractPromptVerb() const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Farm",
		meta = (ClampMin = "1", ToolTip = "每次领取时，每种种子给几颗。默认 2。每个现实日只能领一次。"))
	int32 SeedsPerType = 2;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Z_Components", meta = (AdvancedDisplay))
	TObjectPtr<UStaticMeshComponent> CrateMesh;
};
