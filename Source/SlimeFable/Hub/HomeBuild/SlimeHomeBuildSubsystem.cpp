// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hub/HomeBuild/SlimeHomeBuildSubsystem.h"

#include "DayLevel/DayLevelSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Hub/HomeBuild/SlimeHomeBuildCatalog.h"
#include "Hub/HomeBuild/SlimeHomeBuildManager.h"
#include "Inventory/SlimeInventorySubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "SlimeFable.h"

const TCHAR* USlimeHomeBuildSubsystem::SaveSlot = TEXT("HomeBuild");

void USlimeHomeBuildSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bMuseum = IsMuseumWorld(InWorld);
	if (!bMuseum || !InWorld.IsGameWorld())
	{
		return;
	}

	Catalog = USlimeHomeBuildCatalog::LoadCatalog();
	if (!Catalog)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("[HomeBuild] catalog missing; bag placeables still work."));
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Manager = InWorld.SpawnActor<ASlimeHomeBuildManager>(ASlimeHomeBuildManager::StaticClass(), FTransform::Identity, Params);
	if (Manager)
	{
		Manager->SetCatalog(Catalog);
	}

	Load();
	const int32 Count = SaveGame ? SaveGame->Records.Num() : 0;
	UE_LOG(LogSlimeFable, Log, TEXT("[HomeBuild] restored %d pieces."), Count);
	if (Manager && SaveGame)
	{
		for (const FSlimeHomeBuildRecord& Record : SaveGame->Records)
		{
			Manager->AddRecord(Record);
		}
	}
}

int32 USlimeHomeBuildSubsystem::CommitRecord(FSlimeHomeBuildRecord Record)
{
	if (!bMuseum || !Manager || !SaveGame)
	{
		return INDEX_NONE;
	}
	if (SaveGame->Records.Num() >= ASlimeHomeBuildManager::MaxRecords)
	{
		ScreenMessage(TEXT("å®¶å­å·²ç»æ¾æ»¡äº"));
		return INDEX_NONE;
	}
	if (Record.bFluid && CountFluid() >= ASlimeHomeBuildManager::MaxFluidPads)
	{
		ScreenMessage(TEXT("æµä½å¹³å°æå¤ 6 å"));
		return INDEX_NONE;
	}
	Record.Id = SaveGame->NextId++;
	SaveGame->Records.Add(Record);
	if (!Manager->AddRecord(Record))
	{
		SaveGame->Records.Pop();
		return INDEX_NONE;
	}
	Save();
	return Record.Id;
}

bool USlimeHomeBuildSubsystem::RemoveRecord(int32 RecordId, bool bRefundBag)
{
	if (!SaveGame || !Manager)
	{
		return false;
	}
	const FSlimeHomeBuildRecord* Found = SaveGame->Records.FindByPredicate(
		[RecordId](const FSlimeHomeBuildRecord& Record) { return Record.Id == RecordId; });
	if (!Found)
	{
		return false;
	}
	if (bRefundBag && Found->bFromBag)
	{
		if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (USlimeInventorySubsystem* Inv = GI->GetSubsystem<USlimeInventorySubsystem>())
			{
				Inv->AddItem(Found->EntryId, 1);
			}
		}
	}
	Manager->RemoveRecord(RecordId);
	SaveGame->Records.RemoveAll([RecordId](const FSlimeHomeBuildRecord& Record) { return Record.Id == RecordId; });
	Save();
	return true;
}

void USlimeHomeBuildSubsystem::ForgetRecord(int32 RecordId)
{
	if (!bMuseum || !SaveGame)
	{
		return;
	}
	if (Manager)
	{
		Manager->RemoveRecord(RecordId);
	}
	SaveGame->Records.RemoveAll([RecordId](const FSlimeHomeBuildRecord& Record) { return Record.Id == RecordId; });
	Save();
}

int32 USlimeHomeBuildSubsystem::CountFluid() const
{
	if (!SaveGame)
	{
		return 0;
	}
	int32 Count = 0;
	for (const FSlimeHomeBuildRecord& Record : SaveGame->Records)
	{
		if (Record.bFluid)
		{
			++Count;
		}
	}
	return Count;
}

void USlimeHomeBuildSubsystem::ScreenMessage(const FString& Text) const
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2.5f, FColor::White, Text);
	}
	UE_LOG(LogSlimeFable, Log, TEXT("[HomeBuild] %s"), *Text);
}

void USlimeHomeBuildSubsystem::Load()
{
	SaveGame = Cast<USlimeHomeBuildSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
	if (!SaveGame)
	{
		SaveGame = NewObject<USlimeHomeBuildSaveGame>(this);
		SaveGame->FluidVersion = 2;
		return;
	}
	if (SaveGame->FluidVersion < 2)
	{
		const int32 Removed = SaveGame->Records.RemoveAll(
			[](const FSlimeHomeBuildRecord& Record) { return Record.bFluid; });
		SaveGame->FluidVersion = 2;
		Save();
		UE_LOG(LogSlimeFable, Log, TEXT("[HomeBuild] dropped %d old fluid pads."), Removed);
	}
	const int32 Rivers = SaveGame->Records.RemoveAll(
		[](const FSlimeHomeBuildRecord& Record) { return Record.EntryId == TEXT("Kub_FluidRiver"); });
	if (Rivers > 0)
	{
		Save();
		UE_LOG(LogSlimeFable, Log, TEXT("[HomeBuild] dropped %d river pads."), Rivers);
	}
}

void USlimeHomeBuildSubsystem::Save() const
{
	if (SaveGame)
	{
		UGameplayStatics::SaveGameToSlot(SaveGame, SaveSlot, 0);
	}
}

bool USlimeHomeBuildSubsystem::IsMuseumWorld(const UWorld& World) const
{
	const UGameInstance* GI = World.GetGameInstance();
	const UDayLevelSubsystem* Days = GI ? GI->GetSubsystem<UDayLevelSubsystem>() : nullptr;
	return Days && Days->IsMuseumHubWorld(&World);
}
