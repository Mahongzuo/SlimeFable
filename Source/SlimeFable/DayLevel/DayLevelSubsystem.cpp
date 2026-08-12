// Copyright Epic Games, Inc. All Rights Reserved.

#include "DayLevel/DayLevelSubsystem.h"
#include "SlimeFable.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h"
#include "UObject/SoftObjectPath.h"

namespace DayLevelSubsystemPrivate
{
	static const TCHAR* RegistryObjectPath = TEXT("/Game/Data/DayLevels/DA_DayLevelRegistry.DA_DayLevelRegistry");
	static const TCHAR* MainMenuMapName = TEXT("/Game/Maps/Main");
}

void UDayLevelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	DefaultRegistryPath = TSoftObjectPtr<UDayLevelRegistry>(FSoftObjectPath(DayLevelSubsystemPrivate::RegistryObjectPath));
	LoadDefaultRegistry();
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
	TSoftObjectPtr<UWorld> Level;
	if (!GetLevelForDayId(DayId, Level))
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("DayLevelSubsystem: No level for DayId %s"), *DayId.ToString());
		return false;
	}

	UGameplayStatics::OpenLevelBySoftObjectPtr(WorldContextObject, Level);
	return true;
}

bool UDayLevelSubsystem::TravelToToday(const UObject* WorldContextObject)
{
	return TravelToDayId(WorldContextObject, GetTodayDayId().Id);
}

void UDayLevelSubsystem::TravelToMainMenu(const UObject* WorldContextObject)
{
	UGameplayStatics::OpenLevel(WorldContextObject, FName(DayLevelSubsystemPrivate::MainMenuMapName));
}
