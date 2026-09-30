#include "SlimeEncounterDirector.h"
#include "SlimeEncounterMember.h"
#include "Combat/SlimeDevourTarget.h"
#include "Combat/SlimeHealthComponent.h"
#include "Enemy/LyraShooterEnemy.h"
#include "Enemy/LyraXinShooterEnemy.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/EnemyFighter.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "SlimeFable.h"
#include "TimerManager.h"

ASlimeEncounterSpawnPoint::ASlimeEncounterSpawnPoint()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
#if WITH_EDITORONLY_DATA
	auto* Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("SpawnDirection"));
	if (Arrow) { Arrow->SetupAttachment(RootComponent); Arrow->ArrowColor = FColor::Green; }
#endif
}

ASlimeEncounterDirector::ASlimeEncounterDirector()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
	GroupLimits.Add(TEXT("Lyra"), 2);
}

void ASlimeEncounterDirector::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority() || !EnemyTable || EnemyTable->GetRowStruct() != FSlimeEncounterEnemyRow::StaticStruct()) return;
	for (TActorIterator<ASlimeEncounterSpawnPoint> It(GetWorld()); It; ++It) Points.Add(*It);
	Points.Sort([](const ASlimeEncounterSpawnPoint& A, const ASlimeEncounterSpawnPoint& B) { return A.PointId.LexicalLess(B.PointId); });
	TSet<FName> Ids;
	for (const ASlimeEncounterSpawnPoint* Point : Points)
	{
		if (Point->PointId.IsNone() || Ids.Contains(Point->PointId))
		{
			UE_LOG(LogSlimeFable, Error, TEXT("Encounter: missing/duplicate point ID; population disabled")); return;
		}
		Ids.Add(Point->PointId);
	}
	TArray<FSoftObjectPath> Paths;
	for (FName Name : EnemyTable->GetRowNames())
	{
		const auto* Row = EnemyTable->FindRow<FSlimeEncounterEnemyRow>(Name, TEXT("Encounter"));
		if (Row && Row->bEnabled && Row->Weight > 0.f && !Row->EnemyClass.IsNull()) Paths.AddUnique(Row->EnemyClass.ToSoftObjectPath());
	}
	if (Paths.IsEmpty() || Points.IsEmpty()) return;
	Preload = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
		FStreamableDelegate::CreateUObject(this, &ThisClass::FinishLoading));
}

void ASlimeEncounterDirector::FinishLoading()
{
	if (bEnding) return;
	for (FName Name : EnemyTable->GetRowNames())
	{
		const auto* Row = EnemyTable->FindRow<FSlimeEncounterEnemyRow>(Name, TEXT("Encounter"));
		UClass* Class = Row ? Row->EnemyClass.Get() : nullptr;
		if (Row && Row->bEnabled && Row->Weight > 0.f && Class && Class->ImplementsInterface(USlimeDevourTarget::StaticClass())) ValidSpecies.Add(Name);
	}
	ValidSpecies.Sort(FNameLexicalLess());
	if (ValidSpecies.IsEmpty()) return;
#if WITH_EDITOR
	if (FParse::Param(FCommandLine::Get(), TEXT("RuinEncounterVerify")))
	{
		SaveSlot += TEXT("_Verification");
		StartNewDay();
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			if (auto* PC = It->Get()) if (APawn* P = PC->GetPawn())
			{
				P->SetActorLocation(FVector(0, 0, 30000));
				P->SetActorTickEnabled(false);
				TInlineComponentArray<UActorComponent*> Components(P);
				for (auto* C : Components) C->SetComponentTickEnabled(false);
			}
	}
	else
#endif
	LoadState();
	bReady = true;
	GetWorldTimerManager().SetTimer(HeartbeatTimer, this, &ThisClass::Heartbeat, .5f, true);
	Heartbeat();
}

FString ASlimeEncounterDirector::ResolvedSlot() const
{
	return SaveSlot + (GetWorld()->WorldType == EWorldType::PIE ? TEXT("_PIE") : TEXT(""));
}

void ASlimeEncounterDirector::LoadState()
{
	State = Cast<USlimeEncounterSave>(UGameplayStatics::LoadGameFromSlot(ResolvedSlot(), 0));
	bool bValid = State && State->Version == 1 && State->Records.Num() == Points.Num()
		&& State->LocalDate == FDateTime::Now().ToString(TEXT("%Y-%m-%d"));
	if (bValid)
	{
		TSet<FGuid> Ids; TSet<FName> Occupied;
		for (const auto& R : State->Records)
		{
			bValid &= R.Id.IsValid() && !Ids.Contains(R.Id);
			Ids.Add(R.Id);
			if (!R.bPendingReplacement)
			{
				bValid &= ValidSpecies.Contains(R.Species) && !Occupied.Contains(R.PointId)
					&& Points.ContainsByPredicate([&](const auto& P) { return P->PointId == R.PointId; });
				Occupied.Add(R.PointId);
			}
		}
		for (FName Name : State->DailySpecies) bValid &= ValidSpecies.Contains(Name);
	}
	if (!bValid) { StartNewDay(); return; }
	KillLevel = FMath::Max(State->KillLevel, 0);
	// Reconcile tightened table quotas without increasing difficulty or crediting kills.
	TMap<FName, int32> Counts;
	for (auto& R : State->Records)
	{
		if (R.bPendingReplacement) continue;
		const FName Group = GetGroup(R.Species);
		const int32* Limit = GroupLimits.Find(Group);
		if (Limit && ++Counts.FindOrAdd(Group) > FMath::Max(0, *Limit))
		{
			R.bPendingReplacement = true; R.Species = NAME_None; R.PointId = NAME_None; R.DueUtcTicks = 0;
		}
	}
}

FName ASlimeEncounterDirector::GetGroup(FName Species) const
{
	const auto* Row = EnemyTable->FindRow<FSlimeEncounterEnemyRow>(Species, TEXT("Encounter"));
	if (!Row) return NAME_None;
	UClass* Class = Row->EnemyClass.Get();
	return Class && Class->IsChildOf(ALyraShooterEnemy::StaticClass()) ? FName(TEXT("Lyra")) : Row->LimitGroup;
}

bool ASlimeEncounterDirector::CanReserve(FName Species, const FGuid& Ignore) const
{
	const FName Group = GetGroup(Species);
	const int32* Limit = GroupLimits.Find(Group);
	if (!Limit) return true;
	int32 Count = 0;
	for (const auto& R : State->Records)
		if (R.Id != Ignore && !R.bPendingReplacement && GetGroup(R.Species) == Group) ++Count;
	// Include manually placed enemies and future external spawners, but not player morph bodies.
	if (Group == TEXT("Lyra")) for (TActorIterator<ALyraShooterEnemy> It(GetWorld()); It; ++It)
	{
		if (!It->IsPlayerControlled() && !It->IsMorphTarget() && It->GetEnemyHealth() && It->GetEnemyHealth()->IsAlive()
			&& !It->FindComponentByClass<USlimeEncounterMember>()) ++Count;
	}
	return Count < FMath::Max(0, *Limit);
}

FName ASlimeEncounterDirector::ChooseSpecies(const TArray<FName>& Pool, const FGuid& Ignore) const
{
	float Total = 0.f;
	for (FName N : Pool) if (CanReserve(N, Ignore)) Total += EnemyTable->FindRow<FSlimeEncounterEnemyRow>(N, TEXT("Encounter"))->Weight;
	if (Total <= 0.f) return NAME_None;
	float Pick = FMath::FRand() * Total;
	FName Last;
	for (FName N : Pool) if (CanReserve(N, Ignore))
	{
		Last = N;
		Pick -= EnemyTable->FindRow<FSlimeEncounterEnemyRow>(N, TEXT("Encounter"))->Weight;
		if (Pick <= 0.f) return N;
	}
	return Last;
}

void ASlimeEncounterDirector::StartNewDay()
{
	for (auto& Pair : Live) if (IsValid(Pair.Value)) Pair.Value->Destroy();
	Live.Reset();
	State = NewObject<USlimeEncounterSave>(this);
	State->LocalDate = FDateTime::Now().ToString(TEXT("%Y-%m-%d"));
	KillLevel = 0;
	TArray<FName> Pool = ValidSpecies;
	// Ensure at least one unlimited species so all 15 slots can always be filled.
	TArray<FName> Unlimited;
	for (FName N : Pool) if (!GroupLimits.Contains(GetGroup(N))) Unlimited.Add(N);
	if (Unlimited.IsEmpty()) { UE_LOG(LogSlimeFable, Error, TEXT("Encounter table needs an unlimited species")); return; }
	FName First = ChooseSpecies(Unlimited);
	Pool.Remove(First);
	State->DailySpecies.Add(First);
	auto AddRecord = [&](FName N, int32 Point)
	{
		FSlimeEncounterRecord& R = State->Records.AddDefaulted_GetRef();
		R.Id = FGuid::NewGuid(); R.Species = N; R.PointId = Points[Point]->PointId;
	};
	AddRecord(First, 0);
	while (State->DailySpecies.Num() < FMath::Min(DailySpeciesCount, Points.Num()))
	{
		FName N = ChooseSpecies(Pool);
		if (N.IsNone()) break;
		Pool.Remove(N); State->DailySpecies.Add(N); AddRecord(N, State->Records.Num());
	}
	while (State->Records.Num() < Points.Num()) AddRecord(ChooseSpecies(State->DailySpecies), State->Records.Num());
	// Randomize which point receives each species, keeping stable point IDs for persistence.
	for (int32 I = State->Records.Num() - 1; I > 0; --I) Swap(State->Records[I].PointId, State->Records[FMath::RandRange(0, I)].PointId);
	bDirty = true;
	UE_LOG(LogSlimeFable, Log, TEXT("Encounter new day %s: %d species, %d population"), *State->LocalDate, State->DailySpecies.Num(), State->Records.Num());
}

float ASlimeEncounterDirector::PlayerDistance(const FVector& Position) const
{
	float Closest = TNumericLimits<float>::Max();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		if (const auto* PC = It->Get()) if (const APawn* P = PC->GetPawn()) Closest = FMath::Min(Closest, float(FVector::Dist(Position, P->GetActorLocation())));
	return Closest;
}

void ASlimeEncounterDirector::TrySpawn(FSlimeEncounterRecord& Record)
{
	if (Live.Contains(Record.Id) && IsValid(Live[Record.Id])) return;
	const bool bReplacement = Record.bPendingReplacement;
	if (bReplacement && !SlimeEncounterRules::IsDue(Record.DueUtcTicks, FDateTime::UtcNow().GetTicks())) return;
	FName Species = bReplacement ? ChooseSpecies(State->DailySpecies, Record.Id) : Record.Species;
	if (Species.IsNone() || !CanReserve(Species, Record.Id)) return;
	const auto* Row = EnemyTable->FindRow<FSlimeEncounterEnemyRow>(Species, TEXT("Encounter"));
	UClass* Class = Row ? Row->EnemyClass.Get() : nullptr;
	if (!Class) return;
	TArray<ASlimeEncounterSpawnPoint*> Available;
	for (ASlimeEncounterSpawnPoint* P : Points)
	{
		if (!bReplacement && P->PointId != Record.PointId) continue;
		const bool bOccupied = State->Records.ContainsByPredicate([&](const auto& R) { return R.Id != Record.Id && !R.bPendingReplacement && R.PointId == P->PointId; });
		if (!bOccupied && PlayerDistance(P->GetActorLocation()) >= SafeSpawnDistance) Available.Add(P);
	}
	while (!Available.IsEmpty())
	{
		const int32 Index = FMath::RandRange(0, Available.Num() - 1);
		auto* Point = Available[Index]; Available.RemoveAtSwap(Index);
		const auto* CDO = Class->GetDefaultObject<APawn>();
		const auto* Capsule = CDO->FindComponentByClass<UCapsuleComponent>();
		const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 50.f;
		const float Half = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 100.f;
		FNavLocation NavLoc;
		auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		if (!Nav || !Nav->ProjectPointToNavigation(Point->GetActorLocation(), NavLoc, FVector(100,100,200))) continue;
		FVector Location = NavLoc.Location + FVector(0,0,Half + 3.f);
		if (GetWorld()->OverlapBlockingTestByChannel(Location, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, Half))) continue;
		const FTransform Transform(Point->GetActorRotation(), Location);
		APawn* Pawn = GetWorld()->SpawnActorDeferred<APawn>(Class, Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
		if (!Pawn) continue;
		if (auto* Fighter = Cast<AEnemyFighter>(Pawn)) { Fighter->bPassive = false; Fighter->bHarmless = false; }
		UGameplayStatics::FinishSpawningActor(Pawn, Transform);
		if (!IsValid(Pawn)) continue;
		if (!Pawn->GetController()) Pawn->SpawnDefaultController();
		if (bReplacement) { Record.Level = KillLevel; Record.HealthFraction = 1.f; Record.HealthPhase = 1; }
		Record.Species = Species; Record.PointId = Point->PointId; Record.bPendingReplacement = false; Record.DueUtcTicks = 0;
		Live.Add(Record.Id, Pawn);
		auto* Member = NewObject<USlimeEncounterMember>(Pawn);
		Pawn->AddInstanceComponent(Member); Member->RegisterComponent();
		Member->Initialize(this, Record.Id, Row->HealthScale * SlimeEncounterRules::Scale(Record.Level, HealthPerKill),
			Row->DamageScale * SlimeEncounterRules::Scale(Record.Level, DamagePerKill), Record.HealthFraction, Record.HealthPhase);
		// Before first AI tick: distant Lyra must not become engaged simply by being instantiated.
		if (PlayerDistance(Pawn->GetActorLocation()) > WakeDistance) Member->SetSleeping(true);
		bDirty = true;
		UE_LOG(LogSlimeFable, Log, TEXT("Encounter spawn %s point=%s level=%d"), *Species.ToString(), *Point->PointId.ToString(), Record.Level);
		return;
	}
}

void ASlimeEncounterDirector::ReportDefeat(FGuid Id)
{
	if (bEnding || !State) return;
	auto* R = State->Records.FindByPredicate([&](const auto& Entry) { return Entry.Id == Id; });
	if (!R || R->bPendingReplacement) return;
	R->bPendingReplacement = true; R->PointId = NAME_None; R->Species = NAME_None;
	R->DueUtcTicks = (FDateTime::UtcNow() + FTimespan::FromSeconds(RespawnSeconds)).GetTicks();
	KillLevel = FMath::Min(KillLevel + 1, 1000000);
	Live.Remove(Id);
	bDirty = true;
	SaveState();
	UE_LOG(LogSlimeFable, Log, TEXT("Encounter defeat: global level=%d, replacement in %.0fs"), KillLevel, RespawnSeconds);
}

bool ASlimeEncounterDirector::AnyEngaged() const
{
	for (const auto& Pair : Live) if (IsValid(Pair.Value))
		if (const auto* M = Pair.Value->FindComponentByClass<USlimeEncounterMember>()) if (M->IsEngaged()) return true;
	return false;
}

void ASlimeEncounterDirector::UpdatePresence()
{
	for (const auto& Pair : Live) if (IsValid(Pair.Value))
	{
		auto* M = Pair.Value->FindComponentByClass<USlimeEncounterMember>();
		if (!M) continue;
		const float D = PlayerDistance(Pair.Value->GetActorLocation());
		if (D <= WakeDistance) M->SetSleeping(false);
		else if (D > FMath::Max(SleepDistance, WakeDistance + 100.f) && !M->IsEngaged()) M->SetSleeping(true);
	}
}

void ASlimeEncounterDirector::Heartbeat()
{
	if (!bReady || bEnding || !State) return;
	if (State->LocalDate != FDateTime::Now().ToString(TEXT("%Y-%m-%d")) && !AnyEngaged()) StartNewDay();
	for (auto& R : State->Records) TrySpawn(R);
	UpdatePresence();
	if (bDirty || GetWorld()->GetTimeSeconds() - LastSaveTime >= 10.f) SaveState();
#if WITH_EDITOR
	if (FParse::Param(FCommandLine::Get(), TEXT("RuinEncounterVerify"))) VerifyRuntime();
#endif
}

void ASlimeEncounterDirector::SaveState()
{
	if (!State) return;
	State->KillLevel = KillLevel;
	for (auto& R : State->Records) if (const auto* P = Live.Find(R.Id)) if (IsValid(*P))
	{
		if (const auto* T = SlimeDevourUtil::As(*P)) if (const auto* H = T->GetEnemyHealth()) R.HealthFraction = H->GetHealthPercent();
		if (const auto* Xin = Cast<ALyraXinShooterEnemy>(*P)) R.HealthPhase = Xin->GetHealthPhase();
	}
	if (!UGameplayStatics::SaveGameToSlot(State, ResolvedSlot(), 0)) UE_LOG(LogSlimeFable, Warning, TEXT("Encounter save failed: %s"), *ResolvedSlot());
	bDirty = false; LastSaveTime = GetWorld()->GetTimeSeconds();
}

void ASlimeEncounterDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	bEnding = true;
	GetWorldTimerManager().ClearTimer(HeartbeatTimer);
	if (bReady) SaveState();
	if (Preload) { Preload->CancelHandle(); Preload.Reset(); }
	Super::EndPlay(Reason);
}

FString ASlimeEncounterDirector::GetEncounterDiagnostics() const
{
	int32 Sleeping = 0, LyraCount = 0, Pending = 0;
	for (const auto& Pair : Live) if (IsValid(Pair.Value))
	{
		if (const auto* M = Pair.Value->FindComponentByClass<USlimeEncounterMember>()) Sleeping += M->IsSleeping() ? 1 : 0;
		LyraCount += Pair.Value->IsA<ALyraShooterEnemy>() ? 1 : 0;
	}
	if (State) for (const auto& R : State->Records) Pending += R.bPendingReplacement ? 1 : 0;
	return FString::Printf(TEXT("ready=%d points=%d species=%d live=%d sleeping=%d lyra=%d pending=%d level=%d date=%s"),
		bReady, Points.Num(), State ? State->DailySpecies.Num() : 0, Live.Num(), Sleeping, LyraCount, Pending, KillLevel, State ? *State->LocalDate : TEXT("none"));
}

int32 ASlimeEncounterDirector::BuildValidatedSpawnPoints(int32 Count)
{
#if WITH_EDITOR
	if (GetWorld()->IsGameWorld() || Count <= 0) return 0;
	auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav) return 0;
	FVector Start(500,4500,100);
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It) { Start = It->GetActorLocation(); break; }
	TArray<FVector> Candidates;
	for (int32 X = -4250; X <= 4250; X += 500) for (int32 Y = -4250; Y <= 4250; Y += 500)
	{
		FNavLocation P;
		if (!Nav->ProjectPointToNavigation(FVector(X,Y,50), P, FVector(150,150,150))) continue;
		if (FVector::Dist2D(Start, P.Location) < 1500.f) continue;
		if (GetWorld()->OverlapBlockingTestByChannel(P.Location + FVector(0,0,123), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(80,120))) continue;
		const auto* Path = Nav->FindPathToLocationSynchronously(GetWorld(), Start, P.Location);
		if (!Path || !Path->IsValid() || Path->IsPartial()) continue;
		Candidates.Add(P.Location);
	}
	TArray<FVector> Chosen;
	while (Chosen.Num() < Count && Candidates.Num())
	{
		int32 Best = INDEX_NONE; float BestScore = -1.f;
		for (int32 I = 0; I < Candidates.Num(); ++I)
		{
			float Score = FVector::DistSquared2D(Start, Candidates[I]);
			for (const auto& P : Chosen) Score = FMath::Min(Score, float(FVector::DistSquared2D(P, Candidates[I])));
			if (Score > BestScore) { BestScore = Score; Best = I; }
		}
		if (Best == INDEX_NONE || BestScore < FMath::Square(1000.f)) break;
		Chosen.Add(Candidates[Best]); Candidates.RemoveAt(Best);
	}
	if (Chosen.Num() != Count) return 0; // No partial map mutation when geometry is unsuitable.
	TArray<ASlimeEncounterSpawnPoint*> Old;
	for (TActorIterator<ASlimeEncounterSpawnPoint> It(GetWorld()); It; ++It) if (It->ActorHasTag(TEXT("RuinEncounter"))) Old.Add(*It);
	for (auto* P : Old) P->Destroy();
	for (int32 I = 0; I < Chosen.Num(); ++I)
	{
		auto* P = GetWorld()->SpawnActor<ASlimeEncounterSpawnPoint>(Chosen[I], FRotator::ZeroRotator);
		P->PointId = FName(*FString::Printf(TEXT("Ruin_%02d"), I + 1));
		P->Tags.Add(TEXT("RuinEncounter")); P->SetActorLabel(P->PointId.ToString()); P->SetFolderPath(TEXT("Ruin/Encounters"));
	}
	return Chosen.Num();
#else
	return 0;
#endif
}

int32 ASlimeEncounterDirector::ValidateSpawnPoints() const
{
	int32 Valid = 0;
	auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav) return 0;
	FVector Start(500,4500,100);
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It) { Start = It->GetActorLocation(); break; }
	for (TActorIterator<ASlimeEncounterSpawnPoint> It(GetWorld()); It; ++It)
	{
		FNavLocation P;
		if (!Nav->ProjectPointToNavigation(It->GetActorLocation(), P, FVector(100,100,150))) continue;
		const auto* Path = Nav->FindPathToLocationSynchronously(GetWorld(), Start, P.Location);
		if (Path && Path->IsValid() && !Path->IsPartial()
			&& !GetWorld()->OverlapBlockingTestByChannel(P.Location + FVector(0,0,123), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(80,120))) ++Valid;
	}
	return Valid;
}
