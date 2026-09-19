// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeLyraQuickBarComponent.h"

#include "Inventory/LyraInventoryItemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SlimeLyraQuickBarComponent)

USlimeLyraQuickBarComponent::USlimeLyraQuickBarComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumSlots = 6;
}

int32 USlimeLyraQuickBarComponent::FindSlotOfItem(const ULyraInventoryItemInstance* Item) const
{
	if (!Item)
	{
		return INDEX_NONE;
	}
	const TArray<ULyraInventoryItemInstance*> Current = GetSlots();
	return Current.IndexOfByKey(Item);
}

void USlimeLyraQuickBarComponent::ClearAllSlots()
{
	// RemoveItemFromSlot on the active slot unequips through the pawn's EquipmentManager and resets the
	// active index; do that one first so no slot shuffle leaves EquippedItem dangling.
	const int32 Active = GetActiveSlotIndex();
	if (Active != INDEX_NONE)
	{
		RemoveItemFromSlot(Active);
	}
	const int32 Count = GetSlots().Num();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		RemoveItemFromSlot(Index);
	}
}
