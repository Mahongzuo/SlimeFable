// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SideScrollingInteractable.h"
#include "SideScrollingNPC.generated.h"

/**
 *  Simple platforming NPC
 *  Its behaviors will be dictated by a possessing AI Controller
 *  It can be temporarily deactivated through Actor interactions
 */
UCLASS(abstract)
class ASideScrollingNPC : public ACharacter, public ISideScrollingInteractable
{
	GENERATED_BODY()

protected:

	/** Horizontal impulse to apply to the NPC when it's interacted with */
	UPROPERTY(EditAnywhere, Category="NPC", meta = (ClampMin = 0, ClampMax = 10000, Units="cm/s"))
	float LaunchImpulse = 500.0f;

	/** Vertical impulse to apply to the NPC when it's interacted with */
	UPROPERTY(EditAnywhere, Category="NPC", meta = (ClampMin = 0, ClampMax = 10000, Units="cm/s"))
	float LaunchVerticalImpulse = 500.0f;

	/** Time that the NPC remains deactivated after being interacted with */
	UPROPERTY(EditAnywhere, Category="NPC", meta = (ClampMin = 0, ClampMax = 10, Units="s"))
	float DeactivationTime = 3.0f;

public:

	/** If true, this NPC is deactivated and will not be interacted with */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPC")
	bool bDeactivated = false;

	/** Timer to reactivate the NPC */
	FTimerHandle DeactivationTimer;

public:

	/** Constructor */
	ASideScrollingNPC();

public:

	/** Cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

public:

//	~begin IInteractable interface 

	/** Performs an interaction triggered by another actor */
	virtual void Interaction(AActor* Interactor) override;

//	~end IInteractable interface

	/** Reactivates the NPC */
	void ResetDeactivation();
};
