// Copyright Epic Games, Inc. All Rights Reserved.

#include "DayLevel/DayLevelSubsystem.h"
#include "SlimeFable.h"
#include "Quest/QuestSubsystem.h"
#include "Quest/QuestChapterGate.h"
#include "Hub/SlimeMuseumDayGate.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h"
#include "UObject/SoftObjectPath.h"

namespace DayLevelSubsystemPrivate
{
	static const TCHAR* RegistryObjectPath = TEXT("/Game/Data/DayLevels/DA_DayLevelRegistry.DA_DayLevelRegistry");
	static const TCHAR* MainMenuMapName = TEXT("/Game/Maps/Main");
	static const TCHAR* MuseumHubPath = TEXT("/Game/_Slime/Models/MapModel/TimeMuseum/Maps/L_TimeMuseum_Environment.L_TimeMuseum_Environment");

	TSoftObjectPtr<UWorld> StripPieWorld(TSoftObjectPtr<UWorld> Level)
	{
		if (Level.IsNull())
		{
			return Level;
		}
		const FString Clean = UWorld::RemovePIEPrefix(Level.ToSoftObjectPath().ToString());
		return TSoftObjectPtr<UWorld>(FSoftObjectPath(Clean));
	}
}

void UDayLevelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	DefaultRegistryPath = TSoftObjectPtr<UDayLevelRegistry>(FSoftObjectPath(DayLevelSubsystemPrivate::RegistryObjectPath));
	MuseumHubLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(DayLevelSubsystemPrivate::MuseumHubPath));
	LoadDefaultRegistry();
	SelectedDayId = GetTodayDayId().Id;
	WorldLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UDayLevelSubsystem::HandleWorldLoaded);
}

void UDayLevelSubsystem::LoadDefaultRegistry()
{
	if (Registry)
	{
		return;
	}

	UDayLevelRegistry* Loaded = DefaultRegistryPath.LoadSynchronous();
	if (!Loaded)
	{
		Loaded = LoadObject<UDayLevelRegistry>(nullptr, DayLevelSubsystemPrivate::RegistryObjectPath);
	}

	if (Loaded)
	{
		SetRegistry(Loaded);
		UE_LOG(LogSlimeFable, Log, TEXT("DayLevelSubsystem: Loaded registry with %d entries."), Loaded->Entries.Num());
	}
	else
	{
		UE_LOG(LogSlimeFable, Error, TEXT("DayLevelSubsystem: Failed to load %s"), DayLevelSubsystemPrivate::RegistryObjectPath);
	}
}

void UDayLevelSubsystem::SetRegistry(UDayLevelRegistry* InRegistry)
{
	Registry = InRegistry;
}

FDayId UDayLevelSubsystem::GetTodayDayId() const
{
	const FDateTime Now = FDateTime::Now();
	return MakeDayId(Now.GetMonth(), Now.GetDay());
}

FDayId UDayLevelSubsystem::MakeDayId(int32 Month, int32 Day)
{
	return FDayId(FString::Printf(TEXT("%02d%02d"), Month, Day));
}

FString UDayLevelSubsystem::MakeDayLevelPackagePath(int32 Month, int32 Day)
{
	const FString DayId = FString::Printf(TEXT("%02d%02d"), Month, Day);
	return FString::Printf(TEXT("/Game/Maps/Days/%02d/%s"), Month, *DayId);
}

bool UDayLevelSubsystem::GetLevelForDayId(FName DayId, TSoftObjectPtr<UWorld>& OutLevel) const
{
	if (!Registry)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("DayLevelSubsystem: Registry is not set."));
		return false;
	}

	FDayLevelEntry Entry;
	if (!Registry->FindEntry(DayId, Entry))
	{
		return false;
	}

	OutLevel = Entry.GetLevelSoftPtr();
	return !OutLevel.IsNull();
}

bool UDayLevelSubsystem::GetTodayLevel(TSoftObjectPtr<UWorld>& OutLevel) const
{
	return GetLevelForDayId(GetTodayDayId().Id, OutLevel);
}

FString UDayLevelSubsystem::GetSaveSlotKeyForDayId(FName DayId)
{
	return DayId.ToString();
}

FString UDayLevelSubsystem::GetTodayDisplayString() const
{
	const FDateTime Now = FDateTime::Now();
	const FDayId Today = GetTodayDayId();
	return FString::Printf(TEXT("%d月%d日 (%s)"), Now.GetMonth(), Now.GetDay(), *Today.ToString());
}

void UDayLevelSubsystem::GetEntriesForMonth(int32 Month, TArray<FDayLevelEntry>& OutEntries) const
{
	OutEntries.Reset();
	if (!Registry || Month < 1 || Month > 12)
	{
		return;
	}

	for (const FDayLevelEntry& Entry : Registry->Entries)
	{
		if (Entry.Month == Month)
		{
			OutEntries.Add(Entry);
		}
	}

	OutEntries.Sort([](const FDayLevelEntry& A, const FDayLevelEntry& B)
	{
		return A.Day < B.Day;
	});
}

bool UDayLevelSubsystem::TravelToDayId(const UObject* WorldContextObject, FName DayId)
{
	FDayLevelEntry Entry;
	if (bTravelPending || !Registry || !Registry->FindEntry(DayId, Entry)) return false;
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World) return false;
	PrepareDeparture(World);
	SelectedDayId = DayId;
	StoryChapterId = NAME_None;
	MuseumReturnTag = NAME_None;
	if (IsMuseumHubWorld(World))
	{
		Destination = EDayDestination::Museum;
		GetGameInstance()->GetSubsystem<UQuestSubsystem>()->RefreshMuseumDate(World);
		OnMuseumDateChanged.Broadcast();
		return true;
	}
	return OpenDestination(World, MuseumHubLevel, EDayDestination::Museum, NAME_None);
}

bool UDayLevelSubsystem::TravelToToday(const UObject* WorldContextObject)
{
	return TravelToDayId(WorldContextObject, GetTodayDayId().Id);
}

const TCHAR* UDayLevelSubsystem::OpenLevelSelectOption = TEXT("OpenLevelSelect");

bool UDayLevelSubsystem::TravelToMuseumHub(const UObject* WorldContextObject)
{
	if (IsMuseumHubWorld(WorldContextObject)) return false;
	return OpenDestination(WorldContextObject, MuseumHubLevel, EDayDestination::Museum, MuseumReturnTag);
}

bool UDayLevelSubsystem::IsMuseumHubWorld(const UObject* WorldContextObject) const
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World || MuseumHubLevel.IsNull())
	{
		return false;
	}
	const FString PackageName = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	return PackageName == MuseumHubLevel.ToSoftObjectPath().GetLongPackageName();
}

void UDayLevelSubsystem::TravelToMainMenu(const UObject* WorldContextObject)
{
	if (WorldContextObject && WorldContextObject->GetWorld()) PrepareDeparture(WorldContextObject->GetWorld());
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		UGameplayStatics::SetGamePaused(WorldContextObject, false);
		FInputModeUIOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}
	UGameplayStatics::OpenLevel(WorldContextObject, FName(DayLevelSubsystemPrivate::MainMenuMapName));
}

void UDayLevelSubsystem::TravelToLevelSelect(const UObject* WorldContextObject)
{
	if (WorldContextObject && WorldContextObject->GetWorld()) PrepareDeparture(WorldContextObject->GetWorld());
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		UGameplayStatics::SetGamePaused(WorldContextObject, false);
		FInputModeUIOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}
	const FString Options = FString::Printf(TEXT("%s=1"), OpenLevelSelectOption);
	UGameplayStatics::OpenLevel(WorldContextObject, FName(DayLevelSubsystemPrivate::MainMenuMapName), true, Options);
}

bool UDayLevelSubsystem::GetSubLevelForDayId(FName DayId, FName ChapterId, TSoftObjectPtr<UWorld>& OutLevel) const
{
	OutLevel.Reset();
	if (!Registry || ChapterId.IsNone())
	{
		return false;
	}

	FDayLevelEntry Entry;
	if (!Registry->FindEntry(DayId, Entry))
	{
		return false;
	}

	if (const TSoftObjectPtr<UWorld>* Found = Entry.SubLevels.Find(ChapterId))
	{
		OutLevel = *Found;
		return !OutLevel.IsNull();
	}
	return false;
}

bool UDayLevelSubsystem::TravelToSubLevel(const UObject* WorldContextObject, FName DayId, FName ChapterId)
{
	TSoftObjectPtr<UWorld> Level;
	if (!GetSubLevelForDayId(DayId, ChapterId, Level))
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("DayLevelSubsystem: No sub-level for DayId %s chapter %s"),
			*DayId.ToString(), *ChapterId.ToString());
		return false;
	}

	if (bTravelPending || !IsValidDestination(Level)) return false;
	SelectedDayId = DayId;
	StoryChapterId = ChapterId;
	FDayLevelEntry Entry;
	MuseumReturnTag = NAME_None;
	if (Registry && Registry->FindEntry(DayId, Entry))
	{
		const int32 Index = Entry.ChapterOrder.IndexOfByKey(ChapterId);
		if (Index != INDEX_NONE) MuseumReturnTag = FName(*FString::Printf(TEXT("MuseumYear_%d"), Index + 1));
	}
	return OpenDestination(WorldContextObject, Level, EDayDestination::Story, NAME_None);
}

void UDayLevelSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(WorldLoadedHandle);
	Super::Deinitialize();
}

bool UDayLevelSubsystem::IsValidDestination(TSoftObjectPtr<UWorld> Level)
{
	Level = DayLevelSubsystemPrivate::StripPieWorld(Level);
	return !Level.IsNull() && FPackageName::DoesPackageExist(Level.ToSoftObjectPath().GetLongPackageName());
}

void UDayLevelSubsystem::ReportTravelError(const FString& Message) const
{
	UE_LOG(LogSlimeFable, Error, TEXT("MuseumTravel: %s"), *Message);
	if (UQuestSubsystem* Quests = GetGameInstance()->GetSubsystem<UQuestSubsystem>())
		Quests->ShowCenterBanner(FText::FromString(TEXT("传送配置错误")), FText::FromString(Message), 6.f);
}

void UDayLevelSubsystem::PrepareDeparture(UWorld* World)
{
	for (TActorIterator<AQuestChapterGate> It(World); It; ++It) It->CancelPendingEnter();
	if (UQuestSubsystem* Quests = GetGameInstance()->GetSubsystem<UQuestSubsystem>()) Quests->SaveBeforeTravel();
	ASlimeMuseumDayGate::CloseOpenCalendar();
	UGameplayStatics::SetGamePaused(World, false);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
		PC->ResetIgnoreMoveInput();
		PC->ResetIgnoreLookInput();
	}
}

bool UDayLevelSubsystem::OpenDestination(const UObject* Context, TSoftObjectPtr<UWorld> Level, EDayDestination Role, FName ArrivalTag)
{
	UWorld* World = Context ? Context->GetWorld() : nullptr;
	if (!World || bTravelPending) return false;
	Level = DayLevelSubsystemPrivate::StripPieWorld(Level);
	if (!IsValidDestination(Level))
	{
		ReportTravelError(FString::Printf(TEXT("目的地图不存在：%s"), *Level.ToString()));
		return false;
	}
	PrepareDeparture(World);
	Destination = Role;
	PendingArrivalTag = ArrivalTag;
	DestinationPackage = Level.ToSoftObjectPath().GetLongPackageName();
	DepartureWorld = World;
	bTravelPending = true;
	const FString Options = Role == EDayDestination::Story
		? TEXT("game=/Script/SlimeFable.SlimePlayGameMode")
		: FString();
	UGameplayStatics::OpenLevelBySoftObjectPtr(World, Level, true, Options);
	return true;
}

bool UDayLevelSubsystem::TravelToMap(const UObject* Context, TSoftObjectPtr<UWorld> Level, FName ArrivalTag, FName ReturnTag)
{
	Level = DayLevelSubsystemPrivate::StripPieWorld(Level);
	if (!IsValidDestination(Level))
	{
		ReportTravelError(FString::Printf(TEXT("目的地图不存在：%s"), *Level.ToString()));
		return false;
	}
	if (bTravelPending) return false;
	if (IsMuseumHubWorld(Context)) MuseumReturnTag = ReturnTag;
	StoryChapterId = NAME_None;
	return OpenDestination(Context, Level, EDayDestination::Exploration, ArrivalTag);
}

FName UDayLevelSubsystem::GetArrivalTag(const UWorld* World) const
{
	return World && UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == DestinationPackage ? PendingArrivalTag : NAME_None;
}

void UDayLevelSubsystem::HandleWorldLoaded(UWorld* World)
{
	if (!World || World->GetGameInstance() != GetGameInstance() || !World->IsGameWorld()) return;
	bTravelPending = false;
	DepartureWorld.Reset();
	if (IsMuseumHubWorld(World)) Destination = EDayDestination::Museum;
	else if (UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) != DestinationPackage)
	{
		// Direct map start: derive legacy story identity, never import museum quests into Ruin.
		const FString Name = UWorld::RemovePIEPrefix(World->GetMapName());
		if (Name.StartsWith(TEXT("SL_")) && Name.Len() > 8)
		{
			SelectedDayId = FName(*Name.Mid(3, 4));
			StoryChapterId = FName(*Name.Mid(8));
			Destination = EDayDestination::Story;
		}
		else Destination = EDayDestination::Exploration;
	}
}
