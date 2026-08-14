// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeInventorySlotProxy.h"
#include "UI/SlimeInventoryWidget.h"

void USlimeInventorySlotProxy::HandleClicked()
{
	if (Owner)
	{
		Owner->HandleSlotClicked(ItemId);
	}
}
