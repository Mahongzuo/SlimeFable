// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeCombatCatalog.h"

namespace
{
	TArray<FSlimeElementKitData>& BuiltinKits()
	{
		static TArray<FSlimeElementKitData> Kits;
		if (Kits.Num() == 0)
		{
			Kits.SetNum(SlimeElement::Count);
			for (int32 Index = 0; Index < SlimeElement::Count; ++Index)
			{
				Kits[Index] = SlimeCombat::MakeDefaultKit(SlimeElement::FromIndex(Index));
			}
		}
		return Kits;
	}
}

USlimeCombatCatalog::USlimeCombatCatalog()
{
	Kits.SetNum(SlimeElement::Count);
	for (int32 Index = 0; Index < SlimeElement::Count; ++Index)
	{
		Kits[Index] = SlimeCombat::MakeDefaultKit(SlimeElement::FromIndex(Index));
	}
}

FSlimeElementKitData USlimeCombatCatalog::GetKit(ESlimeElement Element) const
{
	for (const FSlimeElementKitData& Kit : Kits)
	{
		if (Kit.Element == Element && Kit.Combos.Num() >= 4)
		{
			return Kit;
		}
	}
	return GetBuiltinKit(Element);
}

const FSlimeElementKitData& USlimeCombatCatalog::GetBuiltinKit(ESlimeElement Element)
{
	return BuiltinKits()[SlimeElement::ToIndex(Element)];
}
