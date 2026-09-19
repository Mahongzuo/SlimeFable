// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "LyraHotfixManager.generated.h"

/**
 * Installed-engine stub. Official Lyra subclasses UOnlineHotfixManager (Hotfix plugin,
 * not shipped with the launcher). RequestPatchAssetsFromIniFiles is a no-op here.
 */
UCLASS()
class ULyraHotfixManager : public UObject
{
	GENERATED_BODY()

public:
	ULyraHotfixManager();

	void RequestPatchAssetsFromIniFiles() {}
};
