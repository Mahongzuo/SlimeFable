#include "Quest/QuestChapterGate.h"
#include "Quest/QuestSubsystem.h"
#include "DayLevel/DayLevelSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

AQuestChapterGate::AQuestChapterGate()
{
	if (Mesh)
	{
		Mesh->SetWorldScale3D(FVector(0.7f, 0.35f, 1.4f));
	}
}

bool AQuestChapterGate::IsUnlocked() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	const UQuestSubsystem* Quests = GI ? GI->GetSubsystem<UQuestSubsystem>() : nullptr;
	const UDayQuestBook* Book = Quests ? Quests->GetBook() : nullptr;
	if (!Quests || !Book || TargetChapterId.IsNone())
	{
		return false;
	}
	if (Quests->IsChapterComplete(TargetChapterId))
	{
		return true;
	}

	for (int32 Index = 0; Index < Book->Chapters.Num(); ++Index)
	{
		if (Book->Chapters[Index].ChapterId != TargetChapterId)
		{
			continue;
		}
		if (Index == 0)
		{
			return true;
		}
		return Quests->IsChapterComplete(Book->Chapters[Index - 1].ChapterId);
	}
	return false;
}

FText AQuestChapterGate::GetInteractPromptVerb() const
{
	if (!IsUnlocked())
	{
		return FText::FromString(TEXT("未解锁"));
	}
	return FText::FromString(FString::Printf(TEXT("进入 %s"), *TargetChapterId.ToString()));
}

bool AQuestChapterGate::TryInteract(APawn* Interactor)
{
	if (!Interactor || !IsUnlocked())
	{
		return false;
	}

	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UQuestSubsystem* Quests = GI ? GI->GetSubsystem<UQuestSubsystem>() : nullptr;
	if (Quests && DayId == FName(TEXT("0815")))
	{
		Quests->Enter0815Chapter(TargetChapterId);
		return true;
	}

	UDayLevelSubsystem* Days = GI ? GI->GetSubsystem<UDayLevelSubsystem>() : nullptr;
	if (!Days)
	{
		return false;
	}

	TSoftObjectPtr<UWorld> SubLevel;
	if (Days->GetSubLevelForDayId(DayId, TargetChapterId, SubLevel) && !SubLevel.IsNull())
	{
		return Days->TravelToSubLevel(World, DayId, TargetChapterId);
	}
	return Days->TravelToDayId(World, DayId);
}
