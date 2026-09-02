// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class APlayerController;

namespace SlimeCombatDetect
{
	/** True when the local player is locked on or any enemy reports IsInCombat(). */
	bool IsLocalCombatActive(APlayerController* PC);
}
