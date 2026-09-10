#include "WFC/WfcTypes.h"

void WfcMath::BuildStandardTiles(TArray<FWfcTileDef>& OutTiles)
{
	OutTiles.Reset();

	struct FCanon
	{
		EWfcTopology Topology;
		uint8 Mask;
		float Weight;
	};

	const FCanon Canons[] = {
		{EWfcTopology::DeadEnd, 0x01, 1.f},
		{EWfcTopology::Corner, 0x03, 3.f},
		{EWfcTopology::Straight, 0x05, 4.f},
		{EWfcTopology::Tee, 0x07, 5.f},
		{EWfcTopology::Cross, 0x0F, 6.f},
	};

	for (const FCanon& Canon : Canons)
	{
		const int32 RotCount = (Canon.Topology == EWfcTopology::Cross) ? 1 : 4;
		for (int32 Rot = 0; Rot < RotCount; ++Rot)
		{
			FWfcTileDef Def;
			Def.Topology = Canon.Topology;
			Def.Rotation90 = Rot;
			Def.SocketMask = RotateMask(Canon.Mask, Rot);
			Def.Weight = Canon.Weight;
			OutTiles.Add(Def);
		}
	}
}

bool WfcMath::EdgeOpen(int32 WorldSeed, int32 CellX, int32 CellY, int32 Dir)
{
	int32 KeyX = CellX;
	int32 KeyY = CellY;
	int32 KeyDir = Dir & 3;
	if (KeyDir == DirSouth)
	{
		KeyX = CellX - 1;
		KeyDir = DirNorth;
	}
	else if (KeyDir == DirWest)
	{
		KeyY = CellY - 1;
		KeyDir = DirEast;
	}
	return (CellHash(WorldSeed, KeyX, KeyY, 100 + KeyDir) % 8u) != 0u;
}

bool WfcMath::BorderSocketOpen(
	int32 WorldSeed,
	int32 CellX,
	int32 CellY,
	int32 Dir,
	int32 ChunkOriginX,
	int32 ChunkOriginY,
	int32 ChunkSize)
{
	const int32 Count = FMath::Max(ChunkSize, 1);
	TArray<bool, TInlineAllocator<16>> Raw;
	Raw.SetNum(Count);
	bool bAny = false;
	for (int32 i = 0; i < Count; ++i)
	{
		int32 Wx = CellX;
		int32 Wy = CellY;
		switch (Dir & 3)
		{
		case DirNorth:
			Wx = ChunkOriginX + Count - 1;
			Wy = ChunkOriginY + i;
			break;
		case DirSouth:
			Wx = ChunkOriginX;
			Wy = ChunkOriginY + i;
			break;
		case DirEast:
			Wx = ChunkOriginX + i;
			Wy = ChunkOriginY + Count - 1;
			break;
		default:
			Wx = ChunkOriginX + i;
			Wy = ChunkOriginY;
			break;
		}
		Raw[i] = EdgeOpen(WorldSeed, Wx, Wy, Dir);
		bAny |= Raw[i];
	}

	const int32 MyIndex = ((Dir & 3) == DirNorth || (Dir & 3) == DirSouth)
		? (CellY - ChunkOriginY)
		: (CellX - ChunkOriginX);
	if (MyIndex < 0 || MyIndex >= Count)
	{
		return EdgeOpen(WorldSeed, CellX, CellY, Dir);
	}
	if (!bAny)
	{
		return MyIndex == (Count / 2);
	}
	return Raw[MyIndex];
}

FWfcShellChoice WfcMath::ChooseShell(
	EWfcTopology Topology,
	int32 WorldSeed,
	int32 CellX,
	int32 CellY,
	bool bAllowTall,
	const TArray<FWfcShellPresetDef>& Presets)
{
	FRandomStream Stream(static_cast<int32>(CellHash(WorldSeed, CellX, CellY, 17)));
	FWfcShellChoice Choice;
	Choice.Tint = static_cast<EWfcTint>(Stream.RandRange(0, 2));
	Choice.Height = EWfcHeight::Standard;
	if (bAllowTall && Topology == EWfcTopology::Cross && Stream.FRand() < 0.25f)
	{
		Choice.Height = EWfcHeight::Tall;
	}

	TArray<int32> Matches;
	float Total = 0.f;
	for (int32 i = 0; i < Presets.Num(); ++i)
	{
		const FWfcShellPresetDef& Def = Presets[i];
		if (Def.Weight <= 0.f)
		{
			continue;
		}
		if (Def.AllowedTopologies.Num() > 0 && !Def.AllowedTopologies.Contains(Topology))
		{
			continue;
		}
		Matches.Add(i);
		Total += Def.Weight;
	}

	if (Matches.Num() > 0 && Total > 0.f)
	{
		float Roll = Stream.FRand() * Total;
		for (int32 Index : Matches)
		{
			Roll -= Presets[Index].Weight;
			if (Roll <= 0.f)
			{
				Choice.Preset = Presets[Index].Preset;
				return Choice;
			}
		}
		Choice.Preset = Presets[Matches.Last()].Preset;
		return Choice;
	}

	switch (Topology)
	{
	case EWfcTopology::Straight:
		Choice.Preset = Stream.FRand() < 0.5f ? EWfcShellPreset::Narrow : EWfcShellPreset::Wide;
		break;
	case EWfcTopology::Corner:
		Choice.Preset = Stream.FRand() < 0.5f ? EWfcShellPreset::Tight : EWfcShellPreset::ColumnTurn;
		break;
	case EWfcTopology::DeadEnd:
		Choice.Preset = Stream.FRand() < 0.5f ? EWfcShellPreset::Plain : EWfcShellPreset::Apse;
		break;
	case EWfcTopology::Tee:
	case EWfcTopology::Cross:
	default:
		{
			const float U = Stream.FRand();
			if (U < 0.34f)
			{
				Choice.Preset = EWfcShellPreset::Open;
			}
			else if (U < 0.67f)
			{
				Choice.Preset = EWfcShellPreset::Pillared;
			}
			else
			{
				Choice.Preset = EWfcShellPreset::Alcove;
			}
			break;
		}
	}
	return Choice;
}

void WfcMath::ApplyLayoutChaos(TArray<FWfcTileDef>& Tiles, float Chaos)
{
	const float T = FMath::Clamp(Chaos, 0.f, 1.f);
	for (FWfcTileDef& Def : Tiles)
	{
		float Orderly = 1.f;
		float Chaotic = 1.f;
		switch (Def.Topology)
		{
		case EWfcTopology::DeadEnd:
			Orderly = 0.35f;
			Chaotic = 3.6f;
			break;
		case EWfcTopology::Corner:
			Orderly = 1.4f;
			Chaotic = 0.7f;
			break;
		case EWfcTopology::Straight:
			Orderly = 1.6f;
			Chaotic = 0.55f;
			break;
		case EWfcTopology::Tee:
			Orderly = 0.9f;
			Chaotic = 1.15f;
			break;
		default:
			Orderly = 0.55f;
			Chaotic = 1.7f;
			break;
		}
		Def.Weight = FMath::Max(Def.Weight, 0.f) * FMath::Lerp(Orderly, Chaotic, T);
	}
}
