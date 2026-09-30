#include "DayLevel/MuseumPortalSubsystem.h"
#include "DayLevel/MuseumYearPortal.h"
#include "DayLevel/DayLevelSubsystem.h"
#include "Quest/QuestSubsystem.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "TimerManager.h"

void UMuseumPortalSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	UDayLevelSubsystem* Days = World.GetGameInstance()->GetSubsystem<UDayLevelSubsystem>();
	if (!Days || !Days->IsMuseumHubWorld(&World)) return;
	DateChangedHandle = Days->OnMuseumDateChanged.AddUObject(this, &UMuseumPortalSubsystem::Refresh);
	World.GetTimerManager().SetTimerForNextTick(this, &UMuseumPortalSubsystem::Refresh);
}

void UMuseumPortalSubsystem::Deinitialize()
{
	if (GetWorld() && GetWorld()->GetGameInstance())
		if (UDayLevelSubsystem* Days = GetWorld()->GetGameInstance()->GetSubsystem<UDayLevelSubsystem>())
			Days->OnMuseumDateChanged.Remove(DateChangedHandle);
	Super::Deinitialize();
}

bool UMuseumPortalSubsystem::ValidateLayout(const FDayLevelEntry& Entry, const TArray<int32>& Slots, FString& Error)
{
	TSet<int32> UniqueSlots;
	for (int32 Slot : Slots)
	{
		if (Slot < 1 || UniqueSlots.Contains(Slot))
		{
			Error = FString::Printf(TEXT("年份门位编号无效或重复：%d"), Slot);
			return false;
		}
		UniqueSlots.Add(Slot);
	}
	TSet<FName> UniqueChapters;
	for (int32 Index = 0; Index < Entry.ChapterOrder.Num(); ++Index)
	{
		const FName Chapter = Entry.ChapterOrder[Index];
		if (Chapter.IsNone() || UniqueChapters.Contains(Chapter) || !Entry.SubLevels.Contains(Chapter))
		{
			Error = FString::Printf(TEXT("日期 %s 的年份顺序存在重复、空项或缺少地图映射：%s"), *Entry.DayId.ToString(), *Chapter.ToString());
			return false;
		}
		UniqueChapters.Add(Chapter);
		if (!UniqueSlots.Contains(Index + 1))
		{
			Error = FString::Printf(TEXT("年份门位不足：当天有 %d 个故事，缺少 %d 号门，请补放带编号的年份门。"), Entry.ChapterOrder.Num(), Index + 1);
			return false;
		}
	}
	if (UniqueChapters.Num() != Entry.SubLevels.Num())
	{
		Error = TEXT("SubLevels 中有故事未列入 ChapterOrder，请显式配置全部年份顺序。");
		return false;
	}
	return true;
}

void UMuseumPortalSubsystem::Refresh()
{
	UWorld* World = GetWorld();
	UDayLevelSubsystem* Days = World->GetGameInstance()->GetSubsystem<UDayLevelSubsystem>();
	if (!Days || !Days->IsMuseumHubWorld(World)) return;
	UQuestSubsystem* Quests = World->GetGameInstance()->GetSubsystem<UQuestSubsystem>();
	Quests->BeginForWorld(World);
	Quests->CloseWeekSelect();
	TArray<AMuseumYearPortal*> Portals;
	TArray<int32> Slots;
	for (TActorIterator<AMuseumYearPortal> It(World); It; ++It)
	{
		It->SetPortalEnabled(false);
		Portals.Add(*It);
		Slots.Add(It->SlotNumber);
	}
	FDayLevelEntry Entry;
	if (!Days->GetRegistry() || !Days->GetRegistry()->FindEntry(Days->GetSelectedDayId(), Entry))
	{
		Days->ReportTravelError(TEXT("当前日期不在 Registry 中。"));
		return;
	}
	FString Error;
	if (!ValidateLayout(Entry, Slots, Error)) { Days->ReportTravelError(Error); return; }
	if (Entry.ChapterOrder.IsEmpty())
	{
		Quests->ShowCenterBanner(FText::FromString(TEXT("时光博物馆")), FText::FromString(TEXT("可以去野外探索或照料家园。")), 4.f);
		return;
	}
	for (AMuseumYearPortal* Portal : Portals)
	{
		if (!Entry.ChapterOrder.IsValidIndex(Portal->SlotNumber - 1)) continue;
		const FName Chapter = Entry.ChapterOrder[Portal->SlotNumber - 1];
		const TSoftObjectPtr<UWorld> Level = Entry.SubLevels.FindChecked(Chapter);
		if (!Days->IsValidDestination(Level))
		{
			Days->ReportTravelError(FString::Printf(TEXT("%s 的地图不存在：%s"), *Chapter.ToString(), *Level.ToString()));
			continue;
		}
		Portal->TargetChapterId = Chapter;
		Portal->DayId = Entry.DayId;
		Portal->bUseHostDayId = true;
		Portal->DestinationMode = EPortalDestination::Story;
		const FString ChapterText = Chapter.ToString();
		const bool bYearChapter = ChapterText.Len() == 4 && ChapterText.IsNumeric();
		if (bYearChapter)
		{
			Portal->DestinationLabel = FText::FromString(FString::Printf(TEXT("穿越到%s年"), *ChapterText));
		}
		else
		{
			Portal->DestinationLabel = FText::FromName(Chapter);
			const UDayQuestBook* Book = Quests->GetBook();
			if (const FQuestChapter* Story = Book ? Book->FindChapter(Chapter) : nullptr)
			{
				Portal->DestinationLabel = Story->Title;
			}
		}
		// Original 0812 is a compatibility map, not a fabricated quest chapter.
		if (Entry.DayId == FName(TEXT("0812")) && Chapter == FName(TEXT("Legacy0812")))
		{
			Portal->DestinationMode = EPortalDestination::Map;
			Portal->DestinationMap = Level;
			Portal->MuseumReturnTag = FName(*FString::Printf(TEXT("MuseumYear_%d"), Portal->SlotNumber));
			Portal->DestinationLabel = FText::FromString(TEXT("进入 0812 原有故事"));
		}
		Portal->SetPortalEnabled(true);
	}
}
