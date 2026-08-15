#include "Quest/Quest0815Content.h"
#include "SlimeFable.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"

namespace
{
	FString ShortMapName(const UWorld* World)
	{
		if (!World)
		{
			return FString();
		}
		FString Name = World->GetMapName();
		const FString Prefix = World->StreamingLevelsPrefix;
		if (!Prefix.IsEmpty() && Name.StartsWith(Prefix))
		{
			Name.RightChopInline(Prefix.Len());
		}
		return FPackageName::GetShortName(Name);
	}

	const FName TagRuntime(TEXT("Quest.Runtime"));
	const FName TagYearLegacy(TEXT("Quest0815Year"));

	void ClearRuntimeActors(UWorld* World)
	{
		if (!World)
		{
			return;
		}

		TArray<AActor*> ToDestroy;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!IsValid(*It))
			{
				continue;
			}
			if (It->ActorHasTag(TagRuntime) || It->ActorHasTag(TagYearLegacy))
			{
				ToDestroy.Add(*It);
			}
		}
		for (AActor* Actor : ToDestroy)
		{
			Actor->Destroy();
		}
	}
}

void FQuest0815Content::EnterChapter(UWorld* World, FName ChapterId)
{
	if (!World)
	{
		return;
	}
	ClearRuntimeActors(World);
	UE_LOG(LogSlimeFable, Log, TEXT("Quest0815: Cleared runtime actors for chapter %s on %s"),
		*ChapterId.ToString(), *ShortMapName(World));
}

void FQuest0815Content::SpawnForWorld(UWorld* World, FName ChapterId)
{
	EnterChapter(World, ChapterId);
}
