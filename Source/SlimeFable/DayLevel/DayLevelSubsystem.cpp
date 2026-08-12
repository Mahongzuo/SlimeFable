// Copyright Epic Games, Inc. All Rights Reserved.

#include "DayLevel/DayLevelSubsystem.h"
#include "SlimeFable.h"
#include "Misc/DateTime.h"
#include "UObject/SoftObjectPath.h"

void UDayLevelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
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
