#include "WFC/WfcDungeonTile.h"

#include "Components/DecalComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "WFC/WfcTileSet.h"

namespace
{
	UStaticMesh* CubeMesh()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}

	void SetupCollision(UStaticMeshComponent* Mesh, bool bBlockCamera, bool bAffectNav)
	{
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetCollisionObjectType(ECC_WorldStatic);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCollisionResponseToAllChannels(ECR_Block);
		Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Mesh->SetCollisionResponseToChannel(ECC_Camera, bBlockCamera ? ECR_Block : ECR_Ignore);
		Mesh->SetCanEverAffectNavigation(bAffectNav);
		Mesh->SetReceivesDecals(true);
	}
}

AWFCDungeonTile::AWFCDungeonTile()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	SceneRoot->SetMobility(EComponentMobility::Movable);

	LampLight = CreateDefaultSubobject<URectLightComponent>(TEXT("LampLight"));
	LampLight->SetupAttachment(SceneRoot);
	LampLight->SetMobility(EComponentMobility::Movable);
	LampLight->SetVisibility(false);
	LampLight->SetIntensity(18.f);
	LampLight->SetAttenuationRadius(1100.f);
	LampLight->SetSourceWidth(220.f);
	LampLight->SetSourceHeight(90.f);
	LampLight->SetBarnDoorAngle(40.f);
	LampLight->SetBarnDoorLength(8.f);
	LampLight->SetUseTemperature(true);
	LampLight->SetTemperature(4200.f);
	LampLight->SetCastShadows(false);
	LampLight->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
}

void AWFCDungeonTile::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}
	RebuildVisuals();
}

float AWFCDungeonTile::DistanceToCell(int32 OtherX, int32 OtherY) const
{
	return static_cast<float>(FMath::Abs(CellX - OtherX) + FMath::Abs(CellY - OtherY));
}

void AWFCDungeonTile::Configure(
	const FWfcCellResult& Cell,
	const FWfcShellChoice& InShell,
	UWfcTileSet* InTileSet,
	int32 InWorldSeed,
	int32 InCellX,
	int32 InCellY,
	float InCellSize,
	float InDoorWidth,
	float InStandardHeight,
	float InTallHeight,
	float InDecalDensity,
	int32 InMaxFloorDecals,
	int32 InMaxWallDecals,
	bool bInBuildDecals,
	float InLightIntensityScale,
	const TArray<TSoftObjectPtr<UMaterialInterface>>& InFloorDecals,
	const TArray<TSoftObjectPtr<UMaterialInterface>>& InWallDecals)
{
	Result = Cell;
	Shell = InShell;
	SourceTileSet = InTileSet;
	WorldSeed = InWorldSeed;
	CellX = InCellX;
	CellY = InCellY;
	CellSize = InCellSize;
	DoorWidth = InDoorWidth;
	StandardHeight = InStandardHeight;
	TallHeight = InTallHeight;
	DecalDensity = InDecalDensity;
	MaxFloorDecals = InMaxFloorDecals;
	MaxWallDecals = InMaxWallDecals;
	bBuildDecals = bInBuildDecals;
	LightIntensityScale = InLightIntensityScale;
	FloorDecalMats = InFloorDecals;
	WallDecalMats = InWallDecals;
	RebuildVisuals();
}

void AWFCDungeonTile::RebuildVisuals()
{
	if (Result.TileIndex == INDEX_NONE)
	{
		return;
	}

	UWfcTileSet* TileSet = SourceTileSet.LoadSynchronous();
	RoomHeight = (Shell.Height == EWfcHeight::Tall) ? TallHeight : StandardHeight;
	ClearVisuals();
	BuildShell(TileSet);
	if (bBuildDecals)
	{
		BuildDecals(TileSet);
	}

	RefreshLamp();
}

void AWFCDungeonTile::RefreshLamp()
{
	if (!LampLight)
	{
		return;
	}

	LampLight->SetRelativeLocation(FVector(0.f, 0.f, RoomHeight - 18.f));
	LampLight->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	const bool bRoom = Result.Topology == EWfcTopology::Cross || Result.Topology == EWfcTopology::Tee;
	LampLight->SetIntensity((bRoom ? 22.f : 14.f) * FMath::Max(LightIntensityScale, 0.f));
	LampLight->SetCastShadows(false);

	const UWorld* World = GetWorld();
	const bool bEditor = World
		&& (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview);
	LampLight->SetVisibility(bEditor);
}

void AWFCDungeonTile::ApplyLampSettings(float Scale, bool bForceOn)
{
	LightIntensityScale = Scale;
	if (!LampLight)
	{
		return;
	}

	const bool bRoom = Result.Topology == EWfcTopology::Cross || Result.Topology == EWfcTopology::Tee;
	LampLight->SetIntensity((bRoom ? 22.f : 14.f) * FMath::Max(Scale, 0.f));
	if (bForceOn)
	{
		LampLight->SetVisibility(true);
		LampLight->SetCastShadows(false);
	}
}

void AWFCDungeonTile::UpdateLamp(int32 PlayerCellX, int32 PlayerCellY, int32 LightManhattan, int32 ShadowRank, int32 ShadowBudget)
{
	if (!LampLight)
	{
		return;
	}

	const bool bRoom = Result.Topology == EWfcTopology::Cross || Result.Topology == EWfcTopology::Tee;
	LampLight->SetIntensity((bRoom ? 22.f : 14.f) * FMath::Max(LightIntensityScale, 0.f));
	const int32 Dist = FMath::Abs(CellX - PlayerCellX) + FMath::Abs(CellY - PlayerCellY);
	const bool bOn = Dist <= LightManhattan;
	LampLight->SetVisibility(bOn);
	LampLight->SetCastShadows(bOn && ShadowRank >= 0 && ShadowRank < ShadowBudget);
}

void AWFCDungeonTile::ClearVisuals()
{
	for (UStaticMeshComponent* Piece : Pieces)
	{
		if (Piece)
		{
			RemoveInstanceComponent(Piece);
			Piece->DestroyComponent();
		}
	}
	Pieces.Reset();
	for (UDecalComponent* Decal : Decals)
	{
		if (Decal)
		{
			RemoveInstanceComponent(Decal);
			Decal->DestroyComponent();
		}
	}
	Decals.Reset();
}

FVector AWFCDungeonTile::DirNormal(int32 Dir) const
{
	const FIntPoint Off = WfcMath::DirToOffset(Dir);
	return FVector(static_cast<float>(Off.X), static_cast<float>(Off.Y), 0.f);
}

FVector AWFCDungeonTile::DirTangent(int32 Dir) const
{
	const FVector N = DirNormal(Dir);
	return FVector(-N.Y, N.X, 0.f);
}

float AWFCDungeonTile::ChannelWidth() const
{
	if (Result.Topology == EWfcTopology::Straight)
	{
		if (Shell.Preset == EWfcShellPreset::Narrow)
		{
			return 360.f;
		}
		if (Shell.Preset == EWfcShellPreset::Wide)
		{
			return 560.f;
		}
	}
	return CellSize;
}

UStaticMeshComponent* AWFCDungeonTile::AddBox(
	const FName& Name,
	const FVector& Center,
	const FVector& Size,
	UMaterialInterface* Material,
	bool bBlockCamera,
	bool bAffectNav)
{
	UStaticMesh* Mesh = CubeMesh();
	if (!Mesh || Size.GetMin() <= KINDA_SMALL_NUMBER)
	{
		return nullptr;
	}

	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, Name, RF_Transactional);
	Comp->SetStaticMesh(Mesh);
	Comp->SetupAttachment(SceneRoot);
	Comp->SetRelativeLocation(Center);
	Comp->SetRelativeScale3D(Size / 100.f);
	SetupCollision(Comp, bBlockCamera, bAffectNav);
	if (Material)
	{
		Comp->SetMaterial(0, Material);
	}
	Comp->RegisterComponent();
	AddInstanceComponent(Comp);
	Pieces.Add(Comp);
	return Comp;
}

void AWFCDungeonTile::AddClosedWall(int32 Dir, float Thickness, UMaterialInterface* WallMat, bool bAlcove)
{
	const float Half = CellSize * 0.5f;
	const FVector N = DirNormal(Dir);
	const FVector T = DirTangent(Dir);
	const float Channel = ChannelWidth();
	const float Along = (Result.Topology == EWfcTopology::Straight) ? Channel : CellSize;

	if (!bAlcove)
	{
		const FVector Center = N * (Half - Thickness * 0.5f) + FVector(0.f, 0.f, RoomHeight * 0.5f);
		const FVector Size = FVector::UpVector * RoomHeight
			+ N * Thickness
			+ T * Along;
		AddBox(*FString::Printf(TEXT("Wall_%d"), Dir), Center, Size.GetAbs(), WallMat, true, true);
		return;
	}

	const float Niche = 80.f;
	const float Opening = FMath::Min(220.f, Along * 0.45f);
	const float Side = FMath::Max((Along - Opening) * 0.5f, 20.f);
	const FVector Base = N * (Half - Thickness * 0.5f) + FVector(0.f, 0.f, RoomHeight * 0.5f);
	AddBox(*FString::Printf(TEXT("WallL_%d"), Dir),
		Base - T * (Opening * 0.5f + Side * 0.5f),
		(FVector::UpVector * RoomHeight + N * Thickness + T * Side).GetAbs(),
		WallMat, true, true);
	AddBox(*FString::Printf(TEXT("WallR_%d"), Dir),
		Base + T * (Opening * 0.5f + Side * 0.5f),
		(FVector::UpVector * RoomHeight + N * Thickness + T * Side).GetAbs(),
		WallMat, true, true);
	const FVector Back = N * (Half - Thickness - Niche * 0.5f) + FVector(0.f, 0.f, RoomHeight * 0.5f);
	AddBox(*FString::Printf(TEXT("AlcoveBack_%d"), Dir),
		Back,
		(FVector::UpVector * RoomHeight + N * Thickness + T * Opening).GetAbs(),
		WallMat, true, true);
	AddBox(*FString::Printf(TEXT("AlcoveRetL_%d"), Dir),
		N * (Half - Thickness - Niche * 0.5f) - T * (Opening * 0.5f) + FVector(0.f, 0.f, RoomHeight * 0.5f),
		(FVector::UpVector * RoomHeight + N * Niche + T * Thickness).GetAbs(),
		WallMat, true, true);
	AddBox(*FString::Printf(TEXT("AlcoveRetR_%d"), Dir),
		N * (Half - Thickness - Niche * 0.5f) + T * (Opening * 0.5f) + FVector(0.f, 0.f, RoomHeight * 0.5f),
		(FVector::UpVector * RoomHeight + N * Niche + T * Thickness).GetAbs(),
		WallMat, true, true);
}

void AWFCDungeonTile::AddOpenDoor(int32 Dir, UMaterialInterface* WallMat, UMaterialInterface* TrimMat)
{
	const float Half = CellSize * 0.5f;
	const float DoorH = 280.f;
	const FVector N = DirNormal(Dir);
	const FVector T = DirTangent(Dir);
	const float Along = (Result.Topology == EWfcTopology::Straight) ? ChannelWidth() : CellSize;
	const float Thick = 20.f;
	const float Jamb = FMath::Max((Along - DoorWidth) * 0.5f, 16.f);
	UMaterialInterface* Frame = TrimMat ? TrimMat : WallMat;
	const FVector WallCenter = N * (Half - Thick * 0.5f);

	AddBox(*FString::Printf(TEXT("JambL_%d"), Dir),
		WallCenter - T * (DoorWidth * 0.5f + Jamb * 0.5f) + FVector(0.f, 0.f, RoomHeight * 0.5f),
		(FVector::UpVector * RoomHeight + N * Thick + T * Jamb).GetAbs(),
		Frame, true, true);
	AddBox(*FString::Printf(TEXT("JambR_%d"), Dir),
		WallCenter + T * (DoorWidth * 0.5f + Jamb * 0.5f) + FVector(0.f, 0.f, RoomHeight * 0.5f),
		(FVector::UpVector * RoomHeight + N * Thick + T * Jamb).GetAbs(),
		Frame, true, true);

	const float LintelH = FMath::Max(RoomHeight - DoorH, 20.f);
	AddBox(*FString::Printf(TEXT("Lintel_%d"), Dir),
		WallCenter + FVector(0.f, 0.f, DoorH + LintelH * 0.5f),
		(FVector::UpVector * LintelH + N * Thick + T * DoorWidth).GetAbs(),
		Frame, true, true);
}

void AWFCDungeonTile::AddPillars(UMaterialInterface* WallMat)
{
	const float Inset = 220.f;
	const float Size = 60.f;
	const TArray<FVector2D> Corners = {
		FVector2D(Inset, Inset),
		FVector2D(Inset, -Inset),
		FVector2D(-Inset, Inset),
		FVector2D(-Inset, -Inset),
	};

	int32 Count = 0;
	for (int32 i = 0; i < Corners.Num(); ++i)
	{
		const FVector2D C = Corners[i];
		bool bNearOpen = false;
		if (C.X > 0.f && WfcMath::SocketOpen(Result.SocketMask, WfcMath::DirNorth))
		{
			bNearOpen = Result.Topology == EWfcTopology::Tee;
		}
		if (C.X < 0.f && WfcMath::SocketOpen(Result.SocketMask, WfcMath::DirSouth))
		{
			bNearOpen = Result.Topology == EWfcTopology::Tee;
		}
		if (Result.Topology == EWfcTopology::Tee && bNearOpen && Count >= 2)
		{
			continue;
		}
		if (Result.Topology == EWfcTopology::Tee && i >= 2)
		{
			continue;
		}
		AddBox(*FString::Printf(TEXT("Pillar_%d"), i),
			FVector(C.X, C.Y, RoomHeight * 0.5f),
			FVector(Size, Size, RoomHeight),
			WallMat, true, true);
		++Count;
	}
}

void AWFCDungeonTile::AddInnerCornerFill(UMaterialInterface* WallMat, bool bColumn)
{
	FVector Acc = FVector::ZeroVector;
	int32 Closed = 0;
	for (int32 Dir = 0; Dir < 4; ++Dir)
	{
		if (!WfcMath::SocketOpen(Result.SocketMask, Dir))
		{
			Acc += DirNormal(Dir);
			++Closed;
		}
	}
	if (Closed == 0)
	{
		return;
	}
	Acc.Normalize();
	const float Dist = bColumn ? 180.f : 240.f;
	const FVector Center = Acc * Dist + FVector(0.f, 0.f, RoomHeight * 0.5f);
	const FVector Size = bColumn
		? FVector(70.f, 70.f, RoomHeight)
		: FVector(220.f, 220.f, RoomHeight);
	AddBox(bColumn ? TEXT("TurnColumn") : TEXT("TightFill"), Center, Size, WallMat, true, true);
}

void AWFCDungeonTile::AddApse(UMaterialInterface* WallMat)
{
	for (int32 Dir = 0; Dir < 4; ++Dir)
	{
		if (!WfcMath::SocketOpen(Result.SocketMask, Dir))
		{
			AddClosedWall(Dir, 20.f, WallMat, true);
			return;
		}
	}
}

void AWFCDungeonTile::BuildShell(UWfcTileSet* TileSet)
{
	UMaterialInterface* FloorMat = TileSet ? TileSet->ResolveFloor(Shell.Tint) : nullptr;
	UMaterialInterface* WallMat = TileSet ? TileSet->ResolveWall(Shell.Tint) : nullptr;
	UMaterialInterface* CeilingMat = TileSet ? TileSet->ResolveCeiling(Shell.Tint) : nullptr;
	UMaterialInterface* TrimMat = TileSet ? TileSet->ResolveTrim() : nullptr;
	UMaterialInterface* LampMat = TileSet ? TileSet->ResolveLampShade() : nullptr;

	const float Channel = ChannelWidth();
	const float FloorX = (Result.Topology == EWfcTopology::Straight) ? CellSize : CellSize;
	const float FloorY = (Result.Topology == EWfcTopology::Straight) ? Channel : CellSize;
	const bool bNS = WfcMath::SocketOpen(Result.SocketMask, WfcMath::DirNorth)
		&& WfcMath::SocketOpen(Result.SocketMask, WfcMath::DirSouth);
	const FVector FloorSize = (Result.Topology == EWfcTopology::Straight)
		? (bNS ? FVector(FloorX, FloorY, 20.f) : FVector(FloorY, FloorX, 20.f))
		: FVector(CellSize, CellSize, 20.f);

	AddBox(TEXT("Floor"), FVector(0.f, 0.f, -10.f), FloorSize, FloorMat, true, true);
	AddBox(TEXT("Ceiling"), FVector(0.f, 0.f, RoomHeight + 10.f), FloorSize, CeilingMat, false, false);

	const bool bAlcove = Shell.Preset == EWfcShellPreset::Alcove;
	const bool bApse = Shell.Preset == EWfcShellPreset::Apse;
	for (int32 Dir = 0; Dir < 4; ++Dir)
	{
		const bool bOpen = WfcMath::SocketOpen(Result.SocketMask, Dir);
		if (bOpen)
		{
			AddOpenDoor(Dir, WallMat, TrimMat);
			continue;
		}

		float Thick = 20.f;
		if (Result.Topology == EWfcTopology::Straight)
		{
			Thick = (CellSize - Channel) * 0.5f;
		}
		if (bApse)
		{
			continue;
		}
		AddClosedWall(Dir, Thick, WallMat, bAlcove);
	}

	if (bApse)
	{
		AddApse(WallMat);
	}
	if (Shell.Preset == EWfcShellPreset::Pillared)
	{
		AddPillars(WallMat);
	}
	if (Shell.Preset == EWfcShellPreset::Tight)
	{
		AddInnerCornerFill(WallMat, false);
	}
	if (Shell.Preset == EWfcShellPreset::ColumnTurn)
	{
		AddInnerCornerFill(WallMat, true);
	}

	UStaticMeshComponent* Shade = AddBox(
		TEXT("LampShade"),
		FVector(0.f, 0.f, RoomHeight - 6.f),
		FVector(220.f, 90.f, 6.f),
		LampMat ? LampMat : CeilingMat,
		false,
		false);
	if (Shade)
	{
		Shade->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Shade->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
}

void AWFCDungeonTile::PlaceDecal(UMaterialInterface* Mat, bool bWall, int32 Index, FRandomStream& Stream)
{
	if (!Mat)
	{
		return;
	}

	TArray<int32> ClosedDirs;
	if (bWall)
	{
		for (int32 Dir = 0; Dir < 4; ++Dir)
		{
			if (!WfcMath::SocketOpen(Result.SocketMask, Dir))
			{
				ClosedDirs.Add(Dir);
			}
		}
		if (ClosedDirs.Num() == 0)
		{
			return;
		}
	}

	UDecalComponent* Decal = NewObject<UDecalComponent>(this, *FString::Printf(TEXT("Decal_%d"), Index), RF_Transactional);
	Decal->SetupAttachment(SceneRoot);
	Decal->SetDecalMaterial(Mat);
	Decal->DecalSize = FVector(12.f, Stream.FRandRange(70.f, 140.f), Stream.FRandRange(70.f, 140.f));
	Decal->SetFadeScreenSize(0.001f);
	Decal->SetMobility(EComponentMobility::Movable);

	const float Half = CellSize * 0.5f;
	if (bWall)
	{
		const int32 Dir = ClosedDirs[Stream.RandRange(0, ClosedDirs.Num() - 1)];
		const FVector N = DirNormal(Dir);
		const FVector T = DirTangent(Dir);
		const FVector Loc = N * (Half - 22.f) + T * Stream.FRandRange(-180.f, 180.f)
			+ FVector(0.f, 0.f, Stream.FRandRange(60.f, RoomHeight - 80.f));
		Decal->SetRelativeLocation(Loc);
		Decal->SetRelativeRotation((-N).Rotation());
	}
	else
	{
		Decal->SetRelativeLocation(FVector(Stream.FRandRange(-200.f, 200.f), Stream.FRandRange(-200.f, 200.f), 2.f));
		Decal->SetRelativeRotation(FRotator(-90.f, Stream.FRandRange(0.f, 360.f), 0.f));
	}

	Decal->RegisterComponent();
	AddInstanceComponent(Decal);
	Decals.Add(Decal);
}

void AWFCDungeonTile::BuildDecals(UWfcTileSet* TileSet)
{
	if (DecalDensity <= 0.f)
	{
		return;
	}

	TArray<TSoftObjectPtr<UMaterialInterface>> FloorPool = FloorDecalMats;
	TArray<TSoftObjectPtr<UMaterialInterface>> WallPool = WallDecalMats;
	if (FloorPool.Num() == 0 && WallPool.Num() == 0 && TileSet)
	{
		const FWfcRoomDressing* Dress = TileSet->FindDressing(Result.Topology);
		const TArray<FWfcDecalEntry>* Entries = &TileSet->Decals;
		if (Dress && Dress->Decals.Num() > 0)
		{
			Entries = &Dress->Decals;
		}
		for (const FWfcDecalEntry& Entry : *Entries)
		{
			if (Entry.Material.IsNull())
			{
				continue;
			}
			if (Entry.bPreferWall)
			{
				WallPool.Add(Entry.Material);
			}
			else
			{
				FloorPool.Add(Entry.Material);
			}
		}
	}

	auto LoadedMats = [](const TArray<TSoftObjectPtr<UMaterialInterface>>& Pool) -> TArray<UMaterialInterface*>
	{
		TArray<UMaterialInterface*> Out;
		for (const TSoftObjectPtr<UMaterialInterface>& Soft : Pool)
		{
			if (UMaterialInterface* Mat = Soft.LoadSynchronous())
			{
				Out.Add(Mat);
			}
		}
		return Out;
	};

	const TArray<UMaterialInterface*> FloorMats = LoadedMats(FloorPool);
	const TArray<UMaterialInterface*> WallMats = LoadedMats(WallPool);
	if (FloorMats.Num() == 0 && WallMats.Num() == 0)
	{
		return;
	}

	FRandomStream Stream(static_cast<int32>(WfcMath::CellHash(WorldSeed, CellX, CellY, 55)));
	const int32 FloorMax = FMath::Clamp(FMath::RoundToInt(static_cast<float>(MaxFloorDecals) * DecalDensity), 0, 8);
	const int32 WallMax = FMath::Clamp(FMath::RoundToInt(static_cast<float>(MaxWallDecals) * DecalDensity), 0, 8);
	const int32 FloorCount = FloorMats.Num() > 0 ? Stream.RandRange(0, FloorMax) : 0;
	const int32 WallCount = WallMats.Num() > 0 ? Stream.RandRange(0, WallMax) : 0;

	int32 Index = 0;
	for (int32 i = 0; i < FloorCount; ++i)
	{
		PlaceDecal(FloorMats[Stream.RandRange(0, FloorMats.Num() - 1)], false, Index++, Stream);
	}
	for (int32 i = 0; i < WallCount; ++i)
	{
		PlaceDecal(WallMats[Stream.RandRange(0, WallMats.Num() - 1)], true, Index++, Stream);
	}
}
