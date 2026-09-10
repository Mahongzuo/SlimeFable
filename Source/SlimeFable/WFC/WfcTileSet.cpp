#include "WFC/WfcTileSet.h"
#include <initializer_list>

namespace
{
	UMaterialInterface* LoadTint(
		const TArray<TSoftObjectPtr<UMaterialInterface>>& List,
		EWfcTint Tint)
	{
		const int32 Index = static_cast<int32>(Tint);
		if (List.IsValidIndex(Index) && !List[Index].IsNull())
		{
			return List[Index].LoadSynchronous();
		}
		for (const TSoftObjectPtr<UMaterialInterface>& Soft : List)
		{
			if (!Soft.IsNull())
			{
				return Soft.LoadSynchronous();
			}
		}
		return nullptr;
	}

	void AddShell(TArray<FWfcShellPresetDef>& Out, FName Name, EWfcShellPreset Preset, float Weight, std::initializer_list<EWfcTopology> Tops)
	{
		FWfcShellPresetDef Def;
		Def.Name = Name;
		Def.Preset = Preset;
		Def.Weight = Weight;
		for (EWfcTopology Top : Tops)
		{
			Def.AllowedTopologies.Add(Top);
		}
		Out.Add(Def);
	}
}

const TArray<FWfcTileDef>& UWfcTileSet::ResolveTiles() const
{
	if (Tiles.Num() > 0)
	{
		return Tiles;
	}
	if (CachedDefaultTiles.Num() == 0)
	{
		WfcMath::BuildStandardTiles(CachedDefaultTiles);
	}
	return CachedDefaultTiles;
}

void UWfcTileSet::EnsureDefaults()
{
	if (Tiles.Num() == 0)
	{
		WfcMath::BuildStandardTiles(Tiles);
	}
	if (ShellPresets.Num() == 0)
	{
		AddShell(ShellPresets, TEXT("Narrow"), EWfcShellPreset::Narrow, 1.f, {EWfcTopology::Straight});
		AddShell(ShellPresets, TEXT("Wide"), EWfcShellPreset::Wide, 1.f, {EWfcTopology::Straight});
		AddShell(ShellPresets, TEXT("Tight"), EWfcShellPreset::Tight, 1.f, {EWfcTopology::Corner});
		AddShell(ShellPresets, TEXT("ColumnTurn"), EWfcShellPreset::ColumnTurn, 1.f, {EWfcTopology::Corner});
		AddShell(ShellPresets, TEXT("Open"), EWfcShellPreset::Open, 1.f, {EWfcTopology::Tee, EWfcTopology::Cross});
		AddShell(ShellPresets, TEXT("Pillared"), EWfcShellPreset::Pillared, 1.f, {EWfcTopology::Tee, EWfcTopology::Cross});
		AddShell(ShellPresets, TEXT("Alcove"), EWfcShellPreset::Alcove, 1.f, {EWfcTopology::Tee, EWfcTopology::Cross});
		AddShell(ShellPresets, TEXT("Plain"), EWfcShellPreset::Plain, 1.f, {EWfcTopology::DeadEnd});
		AddShell(ShellPresets, TEXT("Apse"), EWfcShellPreset::Apse, 1.f, {EWfcTopology::DeadEnd});
	}
	if (RoomDressings.Num() == 0)
	{
		auto AddDress = [this](EWfcTopology Topology, int32 DMin, int32 DMax, int32 PMin, int32 PMax)
		{
			FWfcRoomDressing Dress;
			Dress.Topology = Topology;
			Dress.DecalCountMin = DMin;
			Dress.DecalCountMax = DMax;
			Dress.PropCountMin = PMin;
			Dress.PropCountMax = PMax;
			RoomDressings.Add(Dress);
		};
		AddDress(EWfcTopology::DeadEnd, 0, 2, 0, 1);
		AddDress(EWfcTopology::Corner, 1, 2, 0, 1);
		AddDress(EWfcTopology::Straight, 0, 1, 0, 0);
		AddDress(EWfcTopology::Tee, 1, 3, 0, 1);
		AddDress(EWfcTopology::Cross, 2, 3, 1, 2);
	}
}

const FWfcRoomDressing* UWfcTileSet::FindDressing(EWfcTopology Topology) const
{
	for (const FWfcRoomDressing& Dress : RoomDressings)
	{
		if (Dress.Topology == Topology)
		{
			return &Dress;
		}
	}
	return nullptr;
}

UMaterialInterface* UWfcTileSet::ResolveFloor(EWfcTint Tint) const
{
	return LoadTint(FloorByTint, Tint);
}

UMaterialInterface* UWfcTileSet::ResolveWall(EWfcTint Tint) const
{
	return LoadTint(WallByTint, Tint);
}

UMaterialInterface* UWfcTileSet::ResolveCeiling(EWfcTint Tint) const
{
	return LoadTint(CeilingByTint, Tint);
}

UMaterialInterface* UWfcTileSet::ResolveTrim() const
{
	return TrimMaterial.IsNull() ? nullptr : TrimMaterial.LoadSynchronous();
}

UMaterialInterface* UWfcTileSet::ResolveLampShade() const
{
	return LampShadeMaterial.IsNull() ? nullptr : LampShadeMaterial.LoadSynchronous();
}
