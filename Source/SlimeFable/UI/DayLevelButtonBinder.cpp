// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/DayLevelButtonBinder.h"
#include "UI/LevelSelectWidget.h"

void UDayLevelButtonBinder::HandleClicked()
{
	if (Owner)
	{
		Owner->HandleDaySlotClicked(DayId);
	}
}
