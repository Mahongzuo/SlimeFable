#include "SlimeNpcVerification.h"
#include "SlimeNpcCatalog.h"
#include "SlimeNpcCollectionSubsystem.h"
#include "SlimeMuseumNpc.h"
#include "AIController.h"
#include "Combat/SlimeDevourTarget.h"
#include "Combat/SlimeHealthComponent.h"
#include "Combat/SlimeLockTarget.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DayLevel/DayLevelSubsystem.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Farm/SlimeFarmPlot.h"
#include "Farm/SlimeFarmSubsystem.h"
#include "Hub/HomeBuild/SlimeHomeBuildCatalog.h"
#include "Hub/HomeBuild/SlimeHomeBuildManager.h"
#include "Hub/HomeBuild/SlimeHomeBuildSubsystem.h"
#include "Inventory/SlimeInventorySubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "SlimeFable.h"

bool USlimeNpcVerification::ShouldCreateSubsystem(UObject* Outer) const
{
#if WITH_EDITOR
	return FParse::Param(FCommandLine::Get(), TEXT("NpcCollectionVerify"));
#else
	return false;
#endif
}

void USlimeNpcVerification::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Started = FPlatformTime::Seconds();
	if (FParse::Param(FCommandLine::Get(), TEXT("NpcReloadVerify"))) Phase = 20;
	Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick), 0.5f);
}

void USlimeNpcVerification::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(Handle);
	Super::Deinitialize();
}

void USlimeNpcVerification::Check(bool Result, const FString& Message)
{
	bFailed |= !Result;
	const FString Line = FString::Printf(TEXT("%s phase=%d %s\n"), Result ? TEXT("PASS") : TEXT("FAIL"), Phase, *Message);
	Report += Line;
	UE_LOG(LogSlimeFable, Display, TEXT("NpcVerify %s"), *Line);
}

void USlimeNpcVerification::Finish()
{
	Report += bFailed ? TEXT("FAILED\n") : TEXT("PASSED\n");
	FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("NpcVerification.txt")));
	FPlatformMisc::RequestExitWithStatus(false, bFailed ? 1 : 0);
}

void USlimeNpcVerification::KillSpecies(FName Id)
{
	auto* Collection = GetGameInstance()->GetSubsystem<USlimeNpcCollectionSubsystem>();
	const auto* S = Collection->GetCatalog()->Find(Id);
	UClass* Class = S ? S->EnemyClasses[0].LoadSynchronous() : nullptr;
	Check(Class != nullptr, TEXT("source class ") + Id.ToString());
	if (!Class) return;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Enemy = GetWorld()->SpawnActor<APawn>(Class, FVector(0, 0, 20000), FRotator::ZeroRotator, Params);
	auto* Target = SlimeDevourUtil::As(Enemy);
	auto* Health = Target ? Target->GetEnemyHealth() : nullptr;
	Check(Health != nullptr, TEXT("source health ") + Id.ToString());
	if (!Health) { if (Enemy) Enemy->Destroy(); return; }
	Check(!Collection->IsUnlocked(Id), TEXT("fresh lock ") + Id.ToString());
	// No false collection for an allied summoned/morph body.
	Health->CurrentHP = 0.f;
	Health->Team = ESlimeTeam::Player;
	Check(!Collection->ReportDefeat(Enemy) && !Collection->IsUnlocked(Id), TEXT("allied death excluded ") + Id.ToString());
	Health->Team = ESlimeTeam::Enemy;
	Health->ResetHP();
	Check(!Collection->ReportDefeat(Enemy), TEXT("living enemy excluded ") + Id.ToString());
	Health->InvulnerableUntil = -1.f;
	Health->ApplyDamage(100000000.f, UGameplayStatics::GetPlayerPawn(this, 0), Enemy->GetActorLocation(), FVector::ZeroVector);
	if (Id == TEXT("Xin"))
	{
		Check(Health->IsAlive() && !Collection->IsUnlocked(Id), TEXT("phase transition does not collect Xin"));
		Health->InvulnerableUntil = -1.f;
		Health->ApplyDamage(100000000.f, UGameplayStatics::GetPlayerPawn(this, 0), Enemy->GetActorLocation(), FVector::ZeroVector);
	}
	Check(Collection->IsUnlocked(Id), TEXT("final lethal hit collects ") + Id.ToString());
	Check(!Collection->ReportDefeat(Enemy), TEXT("repeat death idempotent ") + Id.ToString());
	if (auto* AI = Enemy->GetController()) { AI->UnPossess(); AI->Destroy(); }
	Enemy->Destroy();
}

void USlimeNpcVerification::TestPlacement()
{
	auto* World = GetWorld();
	auto* Home = World->GetSubsystem<USlimeHomeBuildSubsystem>();
	auto* Catalog = GetGameInstance()->GetSubsystem<USlimeNpcCollectionSubsystem>()->GetCatalog();
	auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	auto* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	Check(Home && Home->GetManager() && Nav && Player, TEXT("museum placement services"));
	if (!Home || !Home->GetManager() || !Nav || !Player) return;
	for (TActorIterator<ANavigationData> It(World); It; ++It)
		Check(It->GetRuntimeGenerationMode() == ERuntimeGenerationType::Dynamic, TEXT("museum dynamic navigation"));
	for (const auto& S : Catalog->Species)
	{
		if (S.Layers.IsEmpty()) continue;
		FSlimeHomeBuildRecord Last;
		for (int32 N = 0; N < S.Limit(); ++N)
		{
			int32 Id = INDEX_NONE;
			for (int32 Attempt = 0; Attempt < 200 && Id == INDEX_NONE; ++Attempt)
			{
				FNavLocation Point;
				if (!Nav->GetRandomReachablePointInRadius(Player->GetActorLocation(), 2400, Point)) continue;
				Last = FSlimeHomeBuildRecord();
				Last.EntryId = USlimeNpcCatalog::EntryId(S.SpeciesId);
				Last.AnchorX = FMath::FloorToInt(Point.Location.X / 50.f);
				Last.AnchorY = FMath::FloorToInt(Point.Location.Y / 50.f);
				Last.BaseZ = Point.Location.Z;
				FVector Center;
				if (!ASlimeMuseumNpc::ValidateLocation(World, S, FVector((Last.AnchorX + 0.5f)*50, (Last.AnchorY + 0.5f)*50, Last.BaseZ), Center)) continue;
				Id = Home->CommitRecord(Last);
			}
			Check(Id != INDEX_NONE, FString::Printf(TEXT("place %s %d"), *S.SpeciesId.ToString(), N+1));
		}
		Check(Home->CountNpc(S.SpeciesId) == S.Limit(), TEXT("correct quota ") + S.SpeciesId.ToString());
		Check(Home->CommitRecord(Last) == INDEX_NONE, TEXT("over limit rejected ") + S.SpeciesId.ToString());
	}
	ASlimeMuseumNpc* Pig = nullptr;
	for (TActorIterator<ASlimeMuseumNpc> It(World); It; ++It)
	{
		Check(!It->FindComponentByClass<USlimeHealthComponent>() && !Cast<ISlimeDevourTarget>(*It) && !Cast<ISlimeLockTarget>(*It) && !It->CanBeDamaged(), TEXT("peaceful interfaces ") + It->GetSpeciesId().ToString());
		if (It->GetSpeciesId() == TEXT("Pig")) Pig = *It;
	}
	if (Pig)
	{
		const int32 OldId = Pig->RecordId;
		FSlimeHomeBuildRecord Record;
		for (const auto& R : Home->GetManager()->GetRecords()) if (R.Id == OldId) Record = R;
		Pig->SetActorLocation(Pig->GetActorLocation()+FVector(0,0,300));
		FHitResult Hit(Pig, Pig->GetCapsuleComponent(), Pig->GetActorLocation(), FVector::UpVector);
		Check(Home->GetManager()->FindRecordAtHit(Hit) == OldId, TEXT("moving NPC clear hit resolves record"));
		Check(Home->RemoveRecord(OldId, true) && Home->CountNpc(TEXT("Pig")) == 4, TEXT("clear returns quota"));
		Check(Home->CommitRecord(Record) != INDEX_NONE && Home->CountNpc(TEXT("Pig")) == 5, TEXT("replace without another kill"));
	}
}

void USlimeNpcVerification::TestCrops()
{
	auto* World = GetWorld();
	auto* Farm = GetGameInstance()->GetSubsystem<USlimeFarmSubsystem>();
	auto* Inv = GetGameInstance()->GetSubsystem<USlimeInventorySubsystem>();
	auto* Catalog = GetGameInstance()->GetSubsystem<USlimeNpcCollectionSubsystem>()->GetCatalog();
	TArray<USlimeCropDefinition*> Crops;
	Farm->GetAllCrops(Crops);
	Check(!Crops.IsEmpty(), TEXT("crop definitions available"));
	if (Crops.IsEmpty()) return;
	USlimeCropDefinition* Crop = Crops[0];
	for (auto* C : Crops) if (C->bRegrowAfterHarvest) { Crop = C; break; }
	const auto Before = Inv->GetEntries();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Plot = World->SpawnActor<ASlimeFarmPlot>(ASlimeFarmPlot::StaticClass(), FVector(0,0,20000), FRotator::ZeroRotator, Params);
	Check(Plot && Plot->PlantCrop(Crop), TEXT("plant test crop"));
	if (!Plot) return;
	FSlimeNpcSpecies S = *Catalog->Find(TEXT("Pig"));
	const FTransform Transform(FRotator::ZeroRotator, Plot->GetActorLocation() + FVector(150,0,88));
	auto* Pig = World->SpawnActorDeferred<ASlimeMuseumNpc>(ASlimeMuseumNpc::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	Check(Pig && Pig->Configure(S), TEXT("crop test pig configured"));
	if (!Pig) return;
	Pig->FinishSpawning(Transform);
	Pig->SetActorTickEnabled(false);
	FSlimeFarmPlotRecord Record;
	auto IsGrowing = [&]() { Farm->GetPlotRecord(Plot->PlotId, Record); return Record.State == ESlimeFarmPlotState::Growing; };
	Pig->UpdateCrops(1.5f);
	Check(IsGrowing(), TEXT("dwell threshold not yet reached"));
	// An actual collision obstacle between the animal and the crop edge.
	auto* Wall = World->SpawnActor<AActor>(AActor::StaticClass(), Plot->GetActorLocation() + FVector(125,0,40), FRotator::ZeroRotator, Params);
	auto* Box = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(10,200,100));
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->RegisterComponent();
	Wall->SetActorLocation(Plot->GetActorLocation() + FVector(125,0,40));
	Pig->UpdateCrops(4.f);
	Check(IsGrowing(), TEXT("wall/fence blocks crop damage"));
	Wall->Destroy();
	Pig->UpdateCrops(1.5f);
	Check(IsGrowing(), TEXT("obstacle reset continuous dwell"));
	Pig->SetActorLocation(Transform.GetLocation() + FVector(1000,0,0));
	Pig->UpdateCrops(4.f);
	Pig->SetActorLocation(Transform.GetLocation());
	Pig->UpdateCrops(1.5f);
	Check(IsGrowing(), TEXT("leaving range resets dwell"));
	UGameplayStatics::SetGamePaused(World, true);
	Pig->UpdateCrops(10.f);
	Check(IsGrowing(), TEXT("paused simulation cannot damage crops"));
	UGameplayStatics::SetGamePaused(World, false);
	Pig->UpdateCrops(1.6f);
	Farm->GetPlotRecord(Plot->PlotId, Record);
	Check(Record.State == ESlimeFarmPlotState::Empty && Record.CropId.IsNone() && !Record.bHarvestedLook, TEXT("destruction clears even regrowing crop"));
	Check(!Plot->DestroyCropByNpc(), TEXT("multiple animals cannot double destroy"));
	Check(Plot->PlantCrop(Crop), TEXT("land survives and replants"));
	Pig->UpdateCrops(5.f);
	Check(IsGrowing(), TEXT("animal cooldown prevents instant repeat"));
	Check(Before.Num() == Inv->GetEntries().Num(), TEXT("no new harvest items"));
	for (const auto& E : Before) Check(Inv->GetItemCount(E.ItemId) == E.Count, TEXT("inventory unchanged ") + E.ItemId.ToString());
	Pig->Destroy();
	Plot->MarkRemoved();
	Farm->RemovePlotRecord(Plot->PlotId);
	Plot->Destroy();
}

void USlimeNpcVerification::CheckRestored()
{
	auto* Home = GetWorld()->GetSubsystem<USlimeHomeBuildSubsystem>();
	auto* Collection = GetGameInstance()->GetSubsystem<USlimeNpcCollectionSubsystem>();
	for (const auto& S : Collection->GetCatalog()->Species)
	{
		if (S.Layers.IsEmpty()) continue;
		Check(Collection->IsUnlocked(S.SpeciesId), TEXT("persistent unlock ") + S.SpeciesId.ToString());
		int32 Live = 0;
		for (TActorIterator<ASlimeMuseumNpc> It(GetWorld()); It; ++It) if (It->GetSpeciesId() == S.SpeciesId) ++Live;
		Check(Home->CountNpc(S.SpeciesId) == S.Limit() && Live == S.Limit(), TEXT("restored NPC records and actors ") + S.SpeciesId.ToString());
	}
}

bool USlimeNpcVerification::Tick(float Dt)
{
	if (!FPaths::ProjectSavedDir().Contains(TEXT("RuntimeProfile"))) { Check(false, TEXT("requires isolated RuntimeProfile UserDir")); Finish(); return false; }
	if (FPlatformTime::Seconds() - Started > 600) { Check(false, TEXT("timeout")); Finish(); return false; }
	auto* World = GetWorld();
	if (!World || !World->IsGameWorld() || World->GetTimeSeconds() < 5.f) return true;
	auto* Days = GetGameInstance()->GetSubsystem<UDayLevelSubsystem>();
	auto* Collection = GetGameInstance()->GetSubsystem<USlimeNpcCollectionSubsystem>();
	if (Days->IsTravelPending()) return true;
	if (!Collection->GetCatalog()) { Check(false, TEXT("catalog missing")); Finish(); return false; }
	if (Phase == 0)
	{
		Check(Collection->IsEligibleWorld(World), TEXT("Ruin eligible"));
		for (const auto& S : Collection->GetCatalog()->Species)
			if (!S.Layers.IsEmpty() && S.SpeciesId != TEXT("Samurai")) KillSpecies(S.SpeciesId);
		UClass* RuinPig = LoadClass<APawn>(nullptr, TEXT("/Game/_Slime/Exploration/Ruin/Enemies/BP_Ruin_Pig.BP_Ruin_Pig_C"));
		const auto* Match = Collection->GetCatalog()->Match(RuinPig);
		Check(Match && Match->SpeciesId == TEXT("Pig"), TEXT("Ruin subclass shares Pig species"));
		Phase = 1;
		Check(Days->TravelToSubLevel(this, TEXT("0815"), TEXT("1945")), TEXT("travel to story"));
		return true;
	}
	if (Phase == 1)
	{
		Check(Collection->IsEligibleWorld(World), TEXT("year story eligible"));
		KillSpecies(TEXT("Samurai"));
		Phase = 2;
		Check(Days->TravelToMuseumHub(this), TEXT("travel to museum"));
		return true;
	}
	if (Phase == 2)
	{
		if (UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World)) return true;
		Check(!Collection->IsEligibleWorld(World), TEXT("museum excluded from collection"));
		TestPlacement();
		TestCrops();
		Phase = 3;
		// Reload map rather than just re-reading records in the same subsystem.
		UGameplayStatics::OpenLevel(this, FName(*Days->GetMuseumHubLevel().ToSoftObjectPath().GetLongPackageName()));
		return true;
	}
	if (Phase == 3 || Phase == 20)
	{
		if (World->GetTimeSeconds() < 12.f || UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World)) return true;
		CheckRestored();
		Finish();
		return false;
	}
	return true;
}
