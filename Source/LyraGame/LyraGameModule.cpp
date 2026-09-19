// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"


/**
 * FLyraGameModule
 */
class FLyraGameModule : public FDefaultGameModuleImpl
{
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

// SlimeFable remains the primary game module; Lyra ships as a second runtime module.
IMPLEMENT_GAME_MODULE(FLyraGameModule, LyraGame);
