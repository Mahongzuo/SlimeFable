#include "SlimeNpcCollectionSubsystem.h"
#include "SlimeNpcCatalog.h"
#include "Combat/SlimeHealthComponent.h"
#include "Combat/SlimeDevourTarget.h"
#include "DayLevel/DayLevelSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "SlimeFable.h"

namespace { const TCHAR* CollectionSlot = TEXT("NpcCollection"); }

void USlimeNpcCollectionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UGameplayStatics::DoesSaveGameExist(CollectionSlot, 0))
	{
		State = Cast<USlimeNpcCollectionSave>(UGameplayStatics::LoadGameFromSlot(CollectionSlot, 0));
		bLoadFailed = !State;
		if (bLoadFailed) UE_LOG(LogSlimeFable, Error, TEXT("NPC collection save failed to load; original file will not be overwritten."));
	}
	if (!State) State = NewObject<USlimeNpcCollectionSave>(this);
}

void USlimeNpcCollectionSubsystem::Deinitialize()
{
	Flush();
	Super::Deinitialize();
}

USlimeNpcCatalog* USlimeNpcCollectionSubsystem::GetCatalog()
{
	if (!Catalog) Catalog = USlimeNpcCatalog::Load();
	return Catalog;
}

bool USlimeNpcCollectionSubsystem::IsUnlocked(FName Id) const { return State && State->Unlocked.Contains(Id); }

bool USlimeNpcCollectionSubsystem::IsEligibleWorld(const UWorld* World) const
{
	if (!World || !World->IsGameWorld()) return false;
	FString Package = World->GetOutermost()->GetName();
	Package = UWorld::RemovePIEPrefix(Package);
	const FString MapName = UWorld::RemovePIEPrefix(World->GetMapName());
	if (Package.Contains(TEXT("/Sandbox/")) || Package.Contains(TEXT("Lab"))) return false;
	const auto* Day = GetGameInstance()->GetSubsystem<UDayLevelSubsystem>();
	if (!Day || Day->IsMuseumHubWorld(World)) return false;
	auto Matches = [&Package, &MapName](const FSoftObjectPath& Path)
	{
		return Path.GetLongPackageName() == Package || Path.GetAssetName() == MapName;
	};
	if (Catalog) for (const auto& Map : Catalog->ExplorationMaps)
		if (Matches(Map.ToSoftObjectPath())) return true;
	if (const auto* Registry = Day->GetRegistry()) for (const auto& Entry : Registry->Entries)
	{
		if (Matches(Entry.Level)) return true;
		for (const auto& Pair : Entry.SubLevels)
			if (Matches(Pair.Value.ToSoftObjectPath())) return true;
	}
	return false;
}

bool USlimeNpcCollectionSubsystem::ReportDefeat(AActor* Enemy)
{
	return TryCollect(Enemy, false);
}

bool USlimeNpcCollectionSubsystem::ReportDevour(AActor* Enemy)
{
	return TryCollect(Enemy, true);
}

bool USlimeNpcCollectionSubsystem::TryCollect(AActor* Enemy, bool bAllowLiving)
{
	const auto* Target = SlimeDevourUtil::As(Enemy);
	const auto* Health = Target ? Target->GetEnemyHealth() : nullptr;
	if (!Enemy || !Enemy->HasAuthority() || !Health || (!bAllowLiving && Health->IsAlive()) || Health->Team != ESlimeTeam::Enemy || Target->IsMorphTarget()) return false;
	if (!GetCatalog() || !IsEligibleWorld(Enemy->GetWorld())) return false;
	const auto* Species = Catalog->Match(Enemy->GetClass());
	if (!Species || Species->SpeciesId.IsNone() || !State) return false;
	if (IsUnlocked(Species->SpeciesId)) { Flush(); return false; }
	State->Unlocked.Add(Species->SpeciesId);
	bDirty = true;
	const bool bSaved = Flush();
	OnCollected.Broadcast(Species->SpeciesId);
	if (GEngine) GEngine->AddOnScreenDebugMessage(INDEX_NONE, 4.f, FColor(230, 213, 166),
		FString::Printf(TEXT("已收录：%s，可在博物馆放置%s"), *Species->DisplayName.ToString(), bSaved ? TEXT("") : TEXT("（存档失败）")));
	return true;
}

bool USlimeNpcCollectionSubsystem::Flush()
{
	if (!bDirty) return true;
	if (bLoadFailed || !State || !UGameplayStatics::SaveGameToSlot(State, CollectionSlot, 0))
	{
		UE_LOG(LogSlimeFable, Error, TEXT("NPC collection save failed; retaining pending unlocks in memory."));
		return false;
	}
	bDirty = false;
	return true;
}
