#pragma once

#include "CoreMinimal.h"
#include "WFC/WfcTypes.h"

class SLIMEFABLE_API FWfcSolver
{
public:
	bool SolveRect(
		int32 OriginX,
		int32 OriginY,
		int32 Width,
		int32 Height,
		int32 WorldSeed,
		const TArray<FWfcTileDef>& Tiles,
		bool bSealBorder,
		TArray<FWfcCellResult>& OutCells,
		int32& OutRetries) const;

	bool SolveChunk(
		int32 ChunkX,
		int32 ChunkY,
		int32 ChunkSize,
		int32 WorldSeed,
		const TArray<FWfcTileDef>& Tiles,
		TArray<FWfcCellResult>& OutCells,
		int32& OutRetries) const;

private:
	struct FCellState
	{
		uint32 Possible = 0;
		int32 Collapsed = INDEX_NONE;
		int32 Count = 0;
	};

	bool SolveOnce(
		int32 OriginX,
		int32 OriginY,
		int32 Width,
		int32 Height,
		int32 WorldSeed,
		int32 AttemptSalt,
		const TArray<FWfcTileDef>& Tiles,
		bool bSealBorder,
		TArray<FWfcCellResult>& OutCells) const;

	void InitCells(
		int32 OriginX,
		int32 OriginY,
		int32 Width,
		int32 Height,
		int32 WorldSeed,
		const TArray<FWfcTileDef>& Tiles,
		bool bSealBorder,
		TArray<FCellState>& Cells) const;

	bool RestrictToMask(
		FCellState& Cell,
		uint32 Allowed,
		const TArray<FWfcTileDef>& Tiles) const;

	bool Propagate(
		int32 Width,
		int32 Height,
		const TArray<FWfcTileDef>& Tiles,
		TArray<FCellState>& Cells) const;

	int32 FindLowestEntropy(const TArray<FCellState>& Cells, FRandomStream& Stream) const;
	int32 PickTile(const FCellState& Cell, const TArray<FWfcTileDef>& Tiles, FRandomStream& Stream) const;
	void Collapse(FCellState& Cell, int32 TileIndex) const;
	void ForceFill(
		int32 OriginX,
		int32 OriginY,
		int32 Width,
		int32 Height,
		int32 WorldSeed,
		const TArray<FWfcTileDef>& Tiles,
		bool bSealBorder,
		TArray<FWfcCellResult>& OutCells) const;

	static int32 CellIndex(int32 LocalX, int32 LocalY, int32 Height)
	{
		return LocalX * Height + LocalY;
	}
};
