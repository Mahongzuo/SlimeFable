// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlimeCombatTypes.h"

namespace SlimeCombatVfx
{
	FORCEINLINE int32 ElementId(ESlimeElement Element)
	{
		return FMath::Clamp(static_cast<int32>(Element), 0, SlimeElement::Count - 1);
	}

	FORCEINLINE int32 ComboIndex(ESlimeSkillSlot Slot)
	{
		return FMath::Clamp(static_cast<int32>(Slot), 0, 3);
	}

	FORCEINLINE float SwingSign(ESlimeSkillSlot Slot)
	{
		return (ComboIndex(Slot) % 2) == 0 ? 1.f : -1.f;
	}

	FORCEINLINE float ComboScale(ESlimeSkillSlot Slot)
	{
		return Slot == ESlimeSkillSlot::Combo4 ? 1.35f : 1.f;
	}

	FORCEINLINE bool IsComboHitVfx(ESlimeSkillSlot Slot)
	{
		return SlimeCombat::IsComboSlot(Slot);
	}
}
