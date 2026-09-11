#include "WFC/WfcDungeonGenerator.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/SlimeAudioPlay.h"
#include "SlimeFable.h"
#include "Sound/SoundBase.h"
#include "WFC/WfcDungeonTile.h"
#include "WFC/WfcTileSet.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

namespace
{
	const TCHAR* DefaultTileSetPath = TEXT("/Game/_Slime/WFC/Data/DA_WfcDungeonTiles.DA_WfcDungeonTiles");
	const TCHAR* DefaultFloorDecalPath = TEXT("/Game/ResearchMegaPack/ResearchFacility/Materials/MI_Decal_1.MI_Decal_1");
	const TCHAR* DefaultWallDecalPath = TEXT("/Game/ResearchMegaPack/ResearchFacility/Materials/MI_Decal_3.MI_Decal_3");

	bool IsEditorPreviewWorld(const UWorld* World)
	{
		return World && (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview);
	}
}

AWFCDungeonGenerator::AWFCDungeonGenerator()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	TileSet = TSoftObjectPtr<UWfcTileSet>(FSoftObjectPath(DefaultTileSetPath));

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);

	BgmComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("Bgm"));
	BgmComponent->SetupAttachment(Root);
	BgmComponent->bAutoActivate = false;
	BgmComponent->bIsUISound = false;
	BgmComponent->bAllowSpatialization = false;

	FloorDecals.Add(TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(DefaultFloorDecalPath)));
	WallDecals.Add(TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(DefaultWallDecalPath)));
}

void AWFCDungeonGenerator::BeginPlay()
{
	Super::BeginPlay();
	if (IConsoleVariable* PreExpose = IConsoleManager::Get().FindConsoleVariable(
			TEXT("r.EyeAdaptation.CachedLightingPreExposure")))
	{
		PreExpose->Set(0.f);
	}
	ResolveTileSet();
	CollectBakedTiles();
	if (BgmComponent)
	{
		BgmComponent->OnAudioFinished.AddDynamic(this, &AWFCDungeonGenerator::HandleBgmFinished);
	}
	StartBgm();
}

void AWFCDungeonGenerator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BgmComponent)
	{
		BgmComponent->OnAudioFinished.RemoveDynamic(this, &AWFCDungeonGenerator::HandleBgmFinished);
		BgmComponent->Stop();
	}
	LoadedTiles.Reset();
	Super::EndPlay(EndPlayReason);
}

void AWFCDungeonGenerator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (LoadedTiles.Num() == 0)
	{
		CollectBakedTiles();
	}
	APawn* Pawn = FindFollowPawn();
	const FVector Loc = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
	UpdateLights(WorldToCell(Loc));
}

UWfcTileSet* AWFCDungeonGenerator::ResolveTileSet()
{
	if (ResolvedTileSet)
	{
		return ResolvedTileSet;
	}
	if (!TileSet.IsNull())
	{
		ResolvedTileSet = TileSet.LoadSynchronous();
	}
	if (!ResolvedTileSet)
	{
		ResolvedTileSet = NewObject<UWfcTileSet>(this, TEXT("WfcTileSetRuntime"));
	}
	if (ResolvedTileSet)
	{
		ResolvedTileSet->EnsureDefaults();
	}
	return ResolvedTileSet;
}

FIntPoint AWFCDungeonGenerator::WorldToCell(const FVector& Location) const
{
	const float Size = FMath::Max(CellSize, 1.f);
	const FVector Local = Location - GetActorLocation();
	return FIntPoint(
		FMath::FloorToInt32((Local.X + Size * 0.5f) / Size),
		FMath::FloorToInt32((Local.Y + Size * 0.5f) / Size));
}

FVector AWFCDungeonGenerator::CellWorldLocation(int32 InCellX, int32 InCellY) const
{
	return GetActorLocation() + FVector(
		static_cast<float>(InCellX) * CellSize,
		static_cast<float>(InCellY) * CellSize,
		0.f);
}

APawn* AWFCDungeonGenerator::FindFollowPawn() const
{
	if (UWorld* World = GetWorld())
	{
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			return Pawn;
		}
	}
	return nullptr;
}

void AWFCDungeonGenerator::CollectBakedTiles()
{
	LoadedTiles.Reset();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(World, AWFCDungeonTile::StaticClass(), Found);
	for (AActor* Actor : Found)
	{
		if (AWFCDungeonTile* Tile = Cast<AWFCDungeonTile>(Actor))
		{
			LoadedTiles.Add(FIntPoint(Tile->GetCellX(), Tile->GetCellY()), Tile);
		}
	}
}

void AWFCDungeonGenerator::TagGenerated(AActor* Actor, const FString& Label) const
{
	if (!Actor)
	{
		return;
	}
	Actor->Tags.AddUnique(WfcTags::Generated());
#if WITH_EDITOR
	Actor->SetFolderPath(WfcTags::GeneratedFolder());
	if (!Label.IsEmpty())
	{
		Actor->SetActorLabel(Label, false);
	}
#endif
}

void AWFCDungeonGenerator::DestroyGeneratedActors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TSet<AActor*> Doomed;
	for (AActor* Actor : GeneratedActors)
	{
		if (IsValid(Actor))
		{
			Doomed.Add(Actor);
		}
	}

	TArray<AActor*> Tagged;
	UGameplayStatics::GetAllActorsWithTag(World, WfcTags::Generated(), Tagged);
	for (AActor* Actor : Tagged)
	{
		if (IsValid(Actor))
		{
			Doomed.Add(Actor);
		}
	}

	for (AActor* Actor : Doomed)
	{
		Actor->Destroy();
	}
	GeneratedActors.Reset();
	LoadedTiles.Reset();
}

void AWFCDungeonGenerator::ClearGenerated()
{
#if WITH_EDITOR
	FScopedTransaction Transaction(NSLOCTEXT("WFC", "ClearGenerated", "Clear WFC Generated"));
	Modify();
#endif
	DestroyGeneratedActors();
#if WITH_EDITOR
	if (UWorld* World = GetWorld())
	{
		World->MarkPackageDirty();
	}
#endif
}

void AWFCDungeonGenerator::RandomizeSeed()
{
#if WITH_EDITOR
	FScopedTransaction Transaction(NSLOCTEXT("WFC", "RandomizeSeed", "Randomize WFC Seed"));
	Modify();
#endif
	WorldSeed = FMath::RandRange(1, 999999);
}

void AWFCDungeonGenerator::ApplyLights()
{
#if WITH_EDITOR
	FScopedTransaction Transaction(NSLOCTEXT("WFC", "ApplyLights", "Apply WFC Lights"));
	Modify();
#endif
	CollectBakedTiles();
	const bool bEditor = IsEditorPreviewWorld(GetWorld());
	for (const auto& Pair : LoadedTiles)
	{
		if (Pair.Value)
		{
			Pair.Value->ApplyLampSettings(LightIntensityScale, bEditor);
		}
	}
#if WITH_EDITOR
	if (UWorld* World = GetWorld())
	{
		World->MarkPackageDirty();
	}
#endif
}

void AWFCDungeonGenerator::GenerateDungeon()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (World->WorldType == EWorldType::PIE)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("WFC: 请在编辑器视口点「生成」，不要在 PIE 里刷。"));
		return;
	}
	if (!IsEditorPreviewWorld(World) && World->HasBegunPlay())
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("WFC: 运行时不生成，只使用已烘焙的瓦片。"));
		return;
	}

	UWfcTileSet* Set = ResolveTileSet();
	if (!Set)
	{
		UE_LOG(LogSlimeFable, Error, TEXT("WFC: TileSet missing."));
		return;
	}

	const int32 Width = FMath::Clamp(MapCellsX, 4, 32);
	const int32 Height = FMath::Clamp(MapCellsY, 4, 32);

#if WITH_EDITOR
	FScopedTransaction Transaction(NSLOCTEXT("WFC", "GenerateDungeon", "Generate WFC Dungeon"));
	Modify();
#endif

	DestroyGeneratedActors();

	TArray<FWfcTileDef> Weighted = Set->ResolveTiles();
	WfcMath::ApplyLayoutChaos(Weighted, LayoutChaos);

	TArray<FWfcCellResult> Cells;
	int32 Retries = 0;
	const bool bClean = Solver.SolveRect(
		0, 0, Width, Height, WorldSeed, Weighted, true, Cells, Retries);
	UE_LOG(LogSlimeFable, Log, TEXT("WFC bake %dx%d seed=%d chaos=%.2f retries=%d clean=%d"),
		Width, Height, WorldSeed, LayoutChaos, Retries, bClean ? 1 : 0);

	SpawnRectTiles(Cells, Width, Height, Set);

#if WITH_EDITOR
	World->MarkPackageDirty();
#endif
}

void AWFCDungeonGenerator::SpawnRectTiles(
	const TArray<FWfcCellResult>& Cells,
	int32 Width,
	int32 Height,
	UWfcTileSet* Set)
{
	UWorld* World = GetWorld();
	if (!World || !Set)
	{
		return;
	}

	for (int32 lx = 0; lx < Width; ++lx)
	{
		for (int32 ly = 0; ly < Height; ++ly)
		{
			const int32 Index = lx * Height + ly;
			if (!Cells.IsValidIndex(Index))
			{
				continue;
			}

			const FWfcCellResult& CellResult = Cells[Index];
			const FWfcShellChoice Shell = WfcMath::ChooseShell(
				CellResult.Topology,
				WorldSeed,
				lx,
				ly,
				bAllowTallRooms,
				Set->ShellPresets);

			FActorSpawnParameters Params;
			Params.Owner = nullptr;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.OverrideLevel = GetLevel();
			AWFCDungeonTile* Tile = World->SpawnActor<AWFCDungeonTile>(
				AWFCDungeonTile::StaticClass(),
				CellWorldLocation(lx, ly),
				FRotator::ZeroRotator,
				Params);
			if (!Tile)
			{
				continue;
			}

			Tile->Configure(
				CellResult,
				Shell,
				Set,
				WorldSeed,
				lx,
				ly,
				CellSize,
				DoorWidth,
				StandardHeight,
				TallHeight,
				DecalDensity,
				MaxFloorDecalsPerCell,
				MaxWallDecalsPerCell,
				true,
				LightIntensityScale,
				FloorDecals,
				WallDecals);
			TagGenerated(Tile, FString::Printf(TEXT("WFC_Tile_%d_%d"), lx, ly));
			GeneratedActors.Add(Tile);
			LoadedTiles.Add(FIntPoint(lx, ly), Tile);
			SpawnCellProps(CellResult, lx, ly, Set);
		}
	}
}

void AWFCDungeonGenerator::SpawnCellProps(
	const FWfcCellResult& Cell,
	int32 InCellX,
	int32 InCellY,
	UWfcTileSet* Set)
{
	if (!bSpawnProps)
	{
		return;
	}
	(void)Set;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const TArray<FWfcPropEntry>* Pool = nullptr;
	int32 CountMin = 0;
	int32 CountMax = 0;
	if (Props.Num() > 0)
	{
		Pool = &Props;
		CountMin = PropCountMin;
		CountMax = PropCountMax;
	}
	if (!Pool || Pool->Num() == 0 || CountMax <= 0)
	{
		return;
	}

	TArray<int32> Valid;
	float Total = 0.f;
	for (int32 i = 0; i < Pool->Num(); ++i)
	{
		const FWfcPropEntry& Entry = (*Pool)[i];
		if (Entry.Weight <= 0.f)
		{
			continue;
		}
		if (Entry.AllowedTopologies.Num() > 0 && !Entry.AllowedTopologies.Contains(Cell.Topology))
		{
			continue;
		}
		if (Entry.ActorClass.IsNull() && Entry.StaticMesh.IsNull())
		{
			continue;
		}
		Valid.Add(i);
		Total += Entry.Weight;
	}
	if (Valid.Num() == 0 || Total <= 0.f)
	{
		return;
	}

	FRandomStream Stream(static_cast<int32>(WfcMath::CellHash(WorldSeed, InCellX, InCellY, 77)));
	const int32 Count = Stream.RandRange(CountMin, CountMax);
	const FVector Center = CellWorldLocation(InCellX, InCellY);

	auto Pick = [&]() -> const FWfcPropEntry*
	{
		float Roll = Stream.FRand() * Total;
		for (int32 Index : Valid)
		{
			Roll -= (*Pool)[Index].Weight;
			if (Roll <= 0.f)
			{
				return &(*Pool)[Index];
			}
		}
		return &(*Pool)[Valid.Last()];
	};

	for (int32 i = 0; i < Count; ++i)
	{
		const FWfcPropEntry* Entry = Pick();
		if (!Entry)
		{
			continue;
		}

		FVector Offset(
			Stream.FRandRange(-180.f, 180.f),
			Stream.FRandRange(-180.f, 180.f),
			20.f);
		for (int32 Dir = 0; Dir < 4; ++Dir)
		{
			if (!WfcMath::SocketOpen(Cell.SocketMask, Dir))
			{
				continue;
			}
			const FVector N = FVector(
				static_cast<float>(WfcMath::DirToOffset(Dir).X),
				static_cast<float>(WfcMath::DirToOffset(Dir).Y),
				0.f);
			const float Along = FVector::DotProduct(Offset, N);
			if (Along > CellSize * 0.22f)
			{
				Offset -= N * (Along - CellSize * 0.18f);
			}
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.OverrideLevel = GetLevel();
		AActor* Prop = nullptr;
		if (!Entry->ActorClass.IsNull())
		{
			if (UClass* Cls = Entry->ActorClass.LoadSynchronous())
			{
				Prop = World->SpawnActor<AActor>(Cls, Center + Offset, FRotator(0.f, Stream.FRandRange(0.f, 360.f), 0.f), Params);
			}
		}
		if (!Prop && !Entry->StaticMesh.IsNull())
		{
			if (UStaticMesh* Mesh = Entry->StaticMesh.LoadSynchronous())
			{
				AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(
					Center + Offset,
					FRotator(0.f, Stream.FRandRange(0.f, 360.f), 0.f),
					Params);
				if (MeshActor)
				{
					if (UStaticMeshComponent* Comp = MeshActor->GetStaticMeshComponent())
					{
						Comp->SetMobility(EComponentMobility::Movable);
						Comp->SetStaticMesh(Mesh);
						Comp->SetCanEverAffectNavigation(false);
						Comp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
					}
					MeshActor->SetActorScale3D(FVector(0.45f, 0.45f, 0.55f));
					Prop = MeshActor;
				}
			}
		}
		if (Prop)
		{
			TagGenerated(Prop, FString::Printf(TEXT("WFC_Prop_%d_%d_%d"), InCellX, InCellY, i));
			GeneratedActors.Add(Prop);
		}
	}
}

void AWFCDungeonGenerator::UpdateLights(const FIntPoint& PlayerCell)
{
	struct FLamp
	{
		AWFCDungeonTile* Tile = nullptr;
		int32 Dist = 0;
	};
	TArray<FLamp> Lamps;
	Lamps.Reserve(LoadedTiles.Num());
	for (const auto& Pair : LoadedTiles)
	{
		if (Pair.Value)
		{
			FLamp Lamp;
			Lamp.Tile = Pair.Value;
			Lamp.Dist = FMath::Abs(Pair.Key.X - PlayerCell.X) + FMath::Abs(Pair.Key.Y - PlayerCell.Y);
			Lamps.Add(Lamp);
		}
	}
	Lamps.Sort([](const FLamp& A, const FLamp& B) { return A.Dist < B.Dist; });
	for (int32 i = 0; i < Lamps.Num(); ++i)
	{
		Lamps[i].Tile->UpdateLamp(PlayerCell.X, PlayerCell.Y, LightManhattan, i, ShadowCastingLights);
	}
}

void AWFCDungeonGenerator::SetExploreBgmDucked(bool bDucked)
{
	if (bExploreBgmDucked == bDucked)
	{
		return;
	}
	bExploreBgmDucked = bDucked;
	if (!BgmComponent)
	{
		return;
	}

	const float Fade = 0.4f;
	if (bDucked)
	{
		BgmComponent->FadeOut(Fade, 0.f);
		return;
	}

	if (!BgmComponent->GetSound())
	{
		PlayBgmAt(BgmIndex);
		return;
	}

	const float TargetVol = SlimeAudioPlay::MusicMul(this);
	if (!BgmComponent->IsPlaying())
	{
		BgmComponent->SetVolumeMultiplier(0.f);
		BgmComponent->Play();
	}
	BgmComponent->FadeIn(Fade, TargetVol);
}

void AWFCDungeonGenerator::StartBgm()
{
	PlayBgmAt(0);
}

void AWFCDungeonGenerator::PlayBgmAt(int32 Index)
{
	if (!BgmComponent || BgmPlaylist.Num() == 0)
	{
		return;
	}

	BgmIndex = ((Index % BgmPlaylist.Num()) + BgmPlaylist.Num()) % BgmPlaylist.Num();
	USoundBase* Sound = BgmPlaylist[BgmIndex].LoadSynchronous();
	if (!Sound)
	{
		for (int32 Step = 1; Step < BgmPlaylist.Num(); ++Step)
		{
			const int32 Next = (BgmIndex + Step) % BgmPlaylist.Num();
			Sound = BgmPlaylist[Next].LoadSynchronous();
			if (Sound)
			{
				BgmIndex = Next;
				break;
			}
		}
	}
	if (!Sound)
	{
		return;
	}

	BgmComponent->Stop();
	BgmComponent->SetSound(Sound);
	BgmComponent->SetVolumeMultiplier(SlimeAudioPlay::MusicMul(this));
	BgmComponent->Play();
}

void AWFCDungeonGenerator::HandleBgmFinished()
{
	if (bExploreBgmDucked || BgmPlaylist.Num() == 0)
	{
		return;
	}
	PlayBgmAt(BgmIndex + 1);
}
