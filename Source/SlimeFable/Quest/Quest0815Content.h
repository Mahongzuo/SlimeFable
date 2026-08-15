#pragma once

#include "CoreMinimal.h"

class UWorld;

struct FQuest0815Content
{
	static void SpawnForWorld(UWorld* World, FName ChapterId);
	static void EnterChapter(UWorld* World, FName ChapterId);
};
