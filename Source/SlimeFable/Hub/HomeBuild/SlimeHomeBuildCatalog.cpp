// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hub/HomeBuild/SlimeHomeBuildCatalog.h"

const FSlimeHomeBuildEntry* USlimeHomeBuildCatalog::FindEntry(FName EntryId) const
{
	for (const FSlimeHomeBuildEntry& Entry : Entries)
	{
		if (Entry.EntryId == EntryId)
		{
			return &Entry;
		}
	}
	return nullptr;
}

USlimeHomeBuildCatalog* USlimeHomeBuildCatalog::LoadCatalog()
{
	USlimeHomeBuildCatalog* Catalog = LoadObject<USlimeHomeBuildCatalog>(
		nullptr,
		TEXT("/Game/_Slime/Hub/Build/DA_HomeBuildCatalog.DA_HomeBuildCatalog"));
	if (Catalog)
	{
		for (FSlimeHomeBuildEntry& Entry : Catalog->Entries)
		{
			// 6 m x 4 m on the 50 cm grid. The asset still says 4 x 4 until the catalog is rebuilt.
			if (Entry.EntryId == TEXT("Kub_FluidPool"))
			{
				Entry.FootprintX = 12;
				Entry.FootprintY = 8;
			}
		}
	}
	return Catalog;
}
