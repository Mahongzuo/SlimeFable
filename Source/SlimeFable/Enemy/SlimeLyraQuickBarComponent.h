// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Equipment/LyraQuickBarComponent.h"
#include "SlimeLyraQuickBarComponent.generated.h"

/**
 * Lyra quick bar for the morphed Lyra body. Lives on the PlayerController like Lyra's B_QuickBarComponent
 * (W_QuickBar / W_WeaponAmmoAndName look it up there), but ALyraShooterEnemy creates it on demand instead
 * of the ShooterCore experience, and the slot count follows the slime's 1-6 keys.
 */
UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API USlimeLyraQuickBarComponent : public ULyraQuickBarComponent
{
	GENERATED_BODY()

public:
	USlimeLyraQuickBarComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Only valid before RegisterComponent / BeginPlay (Lyra sizes the slot array in BeginPlay). */
	void ConfigureSlotCount(int32 InNumSlots) { NumSlots = FMath::Clamp(InNumSlots, 1, 9); }

	int32 GetNumSlots() const { return NumSlots; }

	/** Slot holding this item, or INDEX_NONE. */
	int32 FindSlotOfItem(const ULyraInventoryItemInstance* Item) const;

	/** Unequip + drop every slot reference. Call while the controller still owns the pawn. */
	void ClearAllSlots();

	/** Re-send the SlotsChanged / ActiveIndexChanged gameplay messages so HUD widgets created after the fact catch up. */
	void RebroadcastState()
	{
		OnRep_Slots();
		OnRep_ActiveSlotIndex();
	}
};
