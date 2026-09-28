// Copyright Epic Games, Inc. All Rights Reserved.

#include "Farm/SlimeCropTypes.h"

FName USlimeCropDefinition::ResolveYieldItemId() const
{
	if (!YieldItemId.IsNone())
	{
		return YieldItemId;
	}
	if (const USlimeConsumableDefinition* Def = YieldItem.Get())
	{
		return Def->ItemId;
	}
	return NAME_None;
}
