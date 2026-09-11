// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class APlayerController;

namespace SlimeCombatDetect
{
	/** True when a hostile or allied AI is chasing / attacking (not lock-on alone). */
	bool IsLocalCombatActive(APlayerController* PC);
}
