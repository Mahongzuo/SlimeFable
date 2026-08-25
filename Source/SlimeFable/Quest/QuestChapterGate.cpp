#include "Quest/QuestChapterGate.h"
#include "Quest/QuestSubsystem.h"
#include "DayLevel/DayLevelTypes.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"

AQuestChapterGate::AQuestChapterGate()
{
	if (Mesh)
	{
		Mesh->SetRelativeScale3D(FVector(0.7f, 0.35f, 1.4f));
	}
}

void AQuestChapterGate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EnterDelayHandle);
	}
	bEnterPending = false;
	Super::EndPlay(EndPlayReason);
}

bool AQuestChapterGate::CanBeFocused() const
{
	return true;
}

bool AQuestChapterGate::IsUnlocked() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	const UQuestSubsystem* Quests = GI ? GI->GetSubsystem<UQuestSubsystem>() : nullptr;
	if (!Quests)
	{
		return false;
	}
	if (TargetChapterId == FName(TEXT("Hub")))
	{
		return Quests->IsCurrentChapterComplete();
	}
	return Quests->IsChapterUnlocked(TargetChapterId);
}

FText AQuestChapterGate::GetInteractPromptVerb() const
{
	if (!IsUnlocked())
	{
		return FText::FromString(TEXT("未解锁"));
	}
	if (TargetChapterId == FName(TEXT("Hub")))
	{
		return FText::FromString(TEXT("回大厅"));
	}
	return FText::FromString(FString::Printf(TEXT("进入 %s"), *TargetChapterId.ToString()));
}

FName AQuestChapterGate::ResolveTravelDayId() const
{
	if (bUseHostDayId)
	{
		const UWorld* World = GetWorld();
		const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		const UQuestSubsystem* Quests = GI ? GI->GetSubsystem<UQuestSubsystem>() : nullptr;
		if (Quests)
		{
			const FName Host = Quests->GetHostDayId();
			if (!Host.IsNone())
			{
				return Host;
			}
		}
	}
	return DayId;
}

FName AQuestChapterGate::ResolveOptionsDayId() const
{
	if (const UWorld* World = GetWorld())
	{
		FString Name = World->GetMapName();
		const FString Prefix = World->StreamingLevelsPrefix;
		if (!Prefix.IsEmpty() && Name.StartsWith(Prefix))
		{
			Name.RightChopInline(Prefix.Len());
		}
		Name = FPackageName::GetShortName(Name);
		if (Name.StartsWith(TEXT("SL_")) && Name.Len() >= 7)
		{
			return FName(*Name.Mid(3, 4));
		}
		if (Name.Len() == 4 && Name.IsNumeric())
		{
			return FName(*Name);
		}
	}
	return DayId;
}

TArray<FString> AQuestChapterGate::GetTargetChapterIdOptions() const
{
	TArray<FString> Options;
	const FName OptionsDayId = ResolveOptionsDayId();
	if (!OptionsDayId.IsNone())
	{
		if (const UDayLevelRegistry* Registry = LoadObject<UDayLevelRegistry>(
				nullptr, TEXT("/Game/Data/DayLevels/DA_DayLevelRegistry.DA_DayLevelRegistry")))
		{
			FDayLevelEntry Entry;
			if (Registry->FindEntry(OptionsDayId, Entry))
			{
				TArray<FName> Keys;
				Entry.SubLevels.GetKeys(Keys);
				Keys.Sort([](const FName& A, const FName& B)
				{
					return A.LexicalLess(B);
				});
				for (const FName& Key : Keys)
				{
					if (!Key.IsNone())
					{
						Options.AddUnique(Key.ToString());
					}
				}
			}
		}
	}
	Options.AddUnique(TEXT("Hub"));
	return Options;
}

FString AQuestChapterGate::ResolveEnterLabel() const
{
	if (TargetChapterId == FName(TEXT("Hub")))
	{
		return TEXT("大厅");
	}
	return TargetChapterId.ToString();
}

bool AQuestChapterGate::TryInteract(APawn* Interactor)
{
	return RequestEnter(Interactor);
}

bool AQuestChapterGate::RequestEnter(APawn* Interactor)
{
	if (!Interactor || TargetChapterId.IsNone())
	{
		return false;
	}
	if (bEnterPending)
	{
		return false;
	}

	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UQuestSubsystem* Quests = GI ? GI->GetSubsystem<UQuestSubsystem>() : nullptr;
	if (!Quests)
	{
		return false;
	}
	if (Quests->IsWeekSelectOpen())
	{
		return false;
	}

	const FName TravelDayId = ResolveTravelDayId();
	if (TargetChapterId == FName(TEXT("Hub")))
	{
		if (!Quests->IsCurrentChapterComplete())
		{
			Quests->ShowLockedChapterBanner(TargetChapterId);
			return false;
		}
	}
	else if (!Quests->IsChapterUnlocked(TargetChapterId))
	{
		Quests->ShowLockedChapterBanner(TargetChapterId);
		return false;
	}

	const float Delay = FMath::Max(0.f, EnterDelaySeconds);
	const FString Body = FString::Printf(TEXT("正在进入%s"), *ResolveEnterLabel());
	Quests->ShowCenterBanner(
		FText::FromString(TEXT("传送")),
		FText::FromString(Body),
		FMath::Max(Delay, 0.5f));

	PendingTravelDayId = TravelDayId;
	bEnterPending = true;

	if (Delay <= KINDA_SMALL_NUMBER)
	{
		FinishPendingEnter();
		return true;
	}

	World->GetTimerManager().ClearTimer(EnterDelayHandle);
	World->GetTimerManager().SetTimer(
		EnterDelayHandle,
		this,
		&AQuestChapterGate::FinishPendingEnter,
		Delay,
		false);
	return true;
}

void AQuestChapterGate::FinishPendingEnter()
{
	bEnterPending = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EnterDelayHandle);
	}

	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UQuestSubsystem* Quests = GI ? GI->GetSubsystem<UQuestSubsystem>() : nullptr;
	if (!Quests || TargetChapterId.IsNone())
	{
		return;
	}

	const FName TravelDayId = PendingTravelDayId.IsNone() ? ResolveTravelDayId() : PendingTravelDayId;
	PendingTravelDayId = NAME_None;

	if (TargetChapterId == FName(TEXT("Hub")))
	{
		Quests->TravelToHub(TravelDayId);
		return;
	}

	if (Quests->GetHighestWeek(TargetChapterId) <= 1)
	{
		Quests->TravelToChapter(TravelDayId, TargetChapterId, 1);
		return;
	}

	Quests->OpenWeekSelect(TravelDayId, TargetChapterId);
}
