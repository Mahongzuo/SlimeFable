// Copyright Epic Games, Inc. All Rights Reserved.

#include "DayLevel/DayLevelTypes.h"

bool UDayLevelRegistry::FindEntry(FName InDayId, FDayLevelEntry& OutEntry) const
{
	for (const FDayLevelEntry& Entry : Entries)
	{
		if (Entry.DayId == InDayId)
		{
			OutEntry = Entry;
			return true;
		}
	}
	return false;
}

bool UDayLevelRegistry::FindEntryForMonthDay(int32 Month, int32 Day, FDayLevelEntry& OutEntry) const
{
	for (const FDayLevelEntry& Entry : Entries)
	{
		if (Entry.Month == Month && Entry.Day == Day)
		{
			OutEntry = Entry;
			return true;
		}
	}
	return false;
}
