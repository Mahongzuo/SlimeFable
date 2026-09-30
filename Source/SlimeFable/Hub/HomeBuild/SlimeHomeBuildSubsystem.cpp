// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hub/HomeBuild/SlimeHomeBuildSubsystem.h"

#include "DayLevel/DayLevelSubsystem.h"
#include "Hub/NPC/SlimeNpcCollectionSubsystem.h"
#include "Hub/NPC/SlimeNpcCatalog.h"
#include "Engine/GameInstance.h"
#include "Farm/SlimeFarmSubsystem.h"
#include "Engine/World.h"
#include "Hub/HomeBuild/SlimeHomeBuildCatalog.h"
#include "Hub/HomeBuild/SlimeHomeBuildManager.h"
#include "Inventory/SlimeInventorySubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "SlimeFable.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

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
 // Keep the shared catalog asset untouched, including other agents' asset changes.
 if (Catalog) Catalog = DuplicateObject<USlimeHomeBuildCatalog>(Catalog, this);
 else Catalog = NewObject<USlimeHomeBuildCatalog>(this);
 if (auto* Collection = InWorld.GetGameInstance()->GetSubsystem<USlimeNpcCollectionSubsystem>())
  if (auto* Npcs = Collection->GetCatalog()) for (const auto& Species : Npcs->Species)
  {
   if (Species.Layers.IsEmpty()) continue;
   FSlimeHomeBuildEntry Entry;
   Entry.EntryId = USlimeNpcCatalog::EntryId(Species.SpeciesId);
   Entry.NpcSpeciesId = Species.SpeciesId;
   Entry.DisplayName = Species.DisplayName;
   Entry.Category = ESlimeHomeBuildCategory::NPC;
   Entry.Icon = Species.Icon;
   Entry.Description = FText::FromString(Species.bDamagesCrops ? TEXT("在放置点附近闲逛，会毁坏附近庄稼") : TEXT("在放置点附近闲逛"));
   Entry.HeightCm = Species.HalfHeight * 2.f;
   Catalog->Entries.Add(Entry);
  }
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

	// Runtime-only: do not resave the shared museum map to change navigation settings.
 for (TActorIterator<ANavigationData> It(&InWorld); It; ++It)
  if (FProperty* Property = FindFProperty<FProperty>(It->GetClass(), TEXT("RuntimeGeneration")))
   Property->ImportText_Direct(TEXT("Dynamic"), Property->ContainerPtrToValuePtr<void>(*It), *It, PPF_None);
 Load();
	const int32 Count = SaveGame ? SaveGame->Records.Num() : 0;
	UE_LOG(LogSlimeFable, Log, TEXT("[HomeBuild] restored %d pieces."), Count);
	if (Manager && SaveGame)
	{
		for (const FSlimeHomeBuildRecord& Record : SaveGame->Records)
		{
			if (!Record.NpcSpeciesId.IsNone()) PendingNpcs.Add(Record);
   else Manager->AddRecord(Record);
		}
	}
	if (auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&InWorld)) Nav->Build();
 if (!PendingNpcs.IsEmpty())
 {
  NpcRestoreStarted = InWorld.GetTimeSeconds();
  InWorld.GetTimerManager().SetTimer(NpcRestoreTimer, this, &ThisClass::RestoreNpcs, 1.f, true, 2.f);
 }
 if (bLoadedFromDisk && SaveGame)
	{
		TSet<FName> LiveHomePlots;
		LiveHomePlots.Reserve(SaveGame->Records.Num());
		for (const FSlimeHomeBuildRecord& Record : SaveGame->Records)
		{
			LiveHomePlots.Add(FName(*FString::Printf(TEXT("HomePlot_%d"), Record.Id)));
		}
		if (UGameInstance* GI = InWorld.GetGameInstance())
		{
			if (USlimeFarmSubsystem* Farm = GI->GetSubsystem<USlimeFarmSubsystem>())
			{
				Farm->PurgeOrphanHomePlots(LiveHomePlots);
			}
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
	if (const auto* Entry = Catalog ? Catalog->FindEntry(Record.EntryId) : nullptr)
 {
  if (!Entry->NpcSpeciesId.IsNone())
  {
   Record.NpcSpeciesId = Entry->NpcSpeciesId;
   Record.bFromBag = false;
   Record.bFluid = false;
   Record.UserScale = 1.f;
   Record.PitchDegrees = 0.f;
  }
 }
 if (!Record.NpcSpeciesId.IsNone() && !CanPlaceNpc(Record.NpcSpeciesId))
 {
  ScreenMessage(TEXT("NPC尚未收录或已达到放置上限"));
  return INDEX_NONE;
 }
 Record.Id = SaveGame->NextId++;
	SaveGame->Records.Add(Record);
	if (!Manager->AddRecord(Record))
	{
		SaveGame->Records.Pop();
		ScreenMessage(TEXT("放置失败：请检查地面、导航及外观配置"));
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
		bLoadedFromDisk = false;
		SaveGame = NewObject<USlimeHomeBuildSaveGame>(this);
		SaveGame->FluidVersion = 2;
		return;
	}
	bLoadedFromDisk = true;
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

int32 USlimeHomeBuildSubsystem::CountNpc(FName SpeciesId) const
{
 int32 Count = 0;
 if (SaveGame) for (const auto& Record : SaveGame->Records) if (Record.NpcSpeciesId == SpeciesId) ++Count;
 return Count;
}

bool USlimeHomeBuildSubsystem::CanPlaceNpc(FName SpeciesId) const
{
 auto* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
 auto* Collection = GI ? GI->GetSubsystem<USlimeNpcCollectionSubsystem>() : nullptr;
 auto* Npcs = Collection ? Collection->GetCatalog() : nullptr;
 const auto* Species = Npcs ? Npcs->Find(SpeciesId) : nullptr;
 return bMuseum && Species && !Species->Layers.IsEmpty() && Collection->IsUnlocked(SpeciesId) && CountNpc(SpeciesId) < Species->Limit();
}

void USlimeHomeBuildSubsystem::RestoreNpcs()
{
 auto* World = GetWorld();
 if (!World || !SaveGame || !Manager) return;
 const bool bTimeout = World->GetTimeSeconds() - NpcRestoreStarted > 60.f;
 if (!bTimeout && UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World)) return;
 for (int32 I = PendingNpcs.Num() - 1; I >= 0; --I)
 {
  const auto Record = PendingNpcs[I];
  if (!SaveGame->Records.ContainsByPredicate([&](const auto& R) { return R.Id == Record.Id; }))
  { PendingNpcs.RemoveAt(I); continue; }
  if (Manager->AddRecord(Record)) PendingNpcs.RemoveAt(I);
  else if (bTimeout)
  {
   // A blocked/removed anchor must not permanently consume the player's quota.
   SaveGame->Records.RemoveAll([&](const auto& R) { return R.Id == Record.Id; });
   PendingNpcs.RemoveAt(I);
   ScreenMessage(TEXT("部分NPC原位置无法恢复，已释放名额，可重新放置"));
   Save();
  }
 }
 if (PendingNpcs.IsEmpty()) World->GetTimerManager().ClearTimer(NpcRestoreTimer);
}
