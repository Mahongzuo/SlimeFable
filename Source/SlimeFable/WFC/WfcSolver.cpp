#include "WFC/WfcSolver.h"
#include "SlimeFable.h"

namespace
{
	uint32 BitOf(int32 TileIndex)
	{
		return 1u << TileIndex;
	}

	int32 CountBits(uint32 Mask)
	{
		int32 Count = 0;
		while (Mask != 0)
		{
			Count += static_cast<int32>(Mask & 1u);
			Mask >>= 1;
		}
		return Count;
	}

	int32 CrossTileIndex(const TArray<FWfcTileDef>& Tiles)
	{
		for (int32 i = 0; i < Tiles.Num(); ++i)
		{
			if (Tiles[i].Topology == EWfcTopology::Cross)
			{
				return i;
			}
		}
		return Tiles.Num() > 0 ? 0 : INDEX_NONE;
	}

	void WriteResult(const FWfcTileDef& Def, int32 TileIndex, FWfcCellResult& Out)
	{
		Out.TileIndex = TileIndex;
		Out.Topology = Def.Topology;
		Out.Rotation90 = Def.Rotation90;
		Out.SocketMask = Def.SocketMask;
	}

	bool WantsOpen(
		int32 WorldSeed,
		int32 Wx,
		int32 Wy,
		int32 Dir,
		int32 OriginX,
		int32 OriginY,
		int32 Width,
		int32 Height,
		bool bSealBorder,
		bool bOnBorder)
	{
		if (!bOnBorder)
		{
			return true;
		}
		if (bSealBorder)
		{
			return false;
		}
		return WfcMath::BorderSocketOpen(WorldSeed, Wx, Wy, Dir, OriginX, OriginY, Width);
	}
}

bool FWfcSolver::SolveRect(
	int32 OriginX,
	int32 OriginY,
	int32 Width,
	int32 Height,
	int32 WorldSeed,
	const TArray<FWfcTileDef>& Tiles,
	bool bSealBorder,
	TArray<FWfcCellResult>& OutCells,
	int32& OutRetries) const
{
	OutRetries = 0;
	if (Tiles.Num() == 0 || Width <= 0 || Height <= 0 || Tiles.Num() > 32)
	{
		OutCells.Reset();
		return false;
	}

	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		if (SolveOnce(OriginX, OriginY, Width, Height, WorldSeed, Attempt, Tiles, bSealBorder, OutCells))
		{
			OutRetries = Attempt;
			return true;
		}
		++OutRetries;
	}

	UE_LOG(LogSlimeFable, Warning,
		TEXT("WFC rect (%d,%d %dx%d) contradicted 8 times — forcing border-safe fill"),
		OriginX, OriginY, Width, Height);
	ForceFill(OriginX, OriginY, Width, Height, WorldSeed, Tiles, bSealBorder, OutCells);
	return false;
}

bool FWfcSolver::SolveChunk(
	int32 ChunkX,
	int32 ChunkY,
	int32 ChunkSize,
	int32 WorldSeed,
	const TArray<FWfcTileDef>& Tiles,
	TArray<FWfcCellResult>& OutCells,
	int32& OutRetries) const
{
	const int32 Size = FMath::Max(ChunkSize, 1);
	return SolveRect(
		ChunkX * Size,
		ChunkY * Size,
		Size,
		Size,
		WorldSeed,
		Tiles,
		false,
		OutCells,
		OutRetries);
}

bool FWfcSolver::SolveOnce(
	int32 OriginX,
	int32 OriginY,
	int32 Width,
	int32 Height,
	int32 WorldSeed,
	int32 AttemptSalt,
	const TArray<FWfcTileDef>& Tiles,
	bool bSealBorder,
	TArray<FWfcCellResult>& OutCells) const
{
	TArray<FCellState> Cells;
	InitCells(OriginX, OriginY, Width, Height, WorldSeed, Tiles, bSealBorder, Cells);
	if (!Propagate(Width, Height, Tiles, Cells))
	{
		return false;
	}

	FRandomStream Stream(static_cast<int32>(
		WfcMath::CellHash(WorldSeed, OriginX, OriginY, 900 + AttemptSalt + Width * 17 + Height)));

	TArray<TArray<FCellState>> History;
	History.Reserve(Width * Height);
	int32 Backtracks = 0;
	const int32 BacktrackLimit = FMath::Clamp(Width * Height / 2, 32, 128);

	while (true)
	{
		bool bAllDone = true;
		for (const FCellState& Cell : Cells)
		{
			if (Cell.Collapsed == INDEX_NONE)
			{
				bAllDone = false;
				break;
			}
		}
		if (bAllDone)
		{
			OutCells.SetNum(Cells.Num());
			for (int32 i = 0; i < Cells.Num(); ++i)
			{
				WriteResult(Tiles[Cells[i].Collapsed], Cells[i].Collapsed, OutCells[i]);
			}
			return true;
		}

		const int32 Index = FindLowestEntropy(Cells, Stream);
		if (Index == INDEX_NONE)
		{
			return false;
		}

		History.Add(Cells);
		const int32 Picked = PickTile(Cells[Index], Tiles, Stream);
		if (Picked == INDEX_NONE)
		{
			return false;
		}
		Collapse(Cells[Index], Picked);
		if (Propagate(Width, Height, Tiles, Cells))
		{
			continue;
		}

		if (History.Num() == 0 || Backtracks >= BacktrackLimit)
		{
			return false;
		}

		Cells = History.Pop();
		Cells[Index].Possible &= ~BitOf(Picked);
		Cells[Index].Count = CountBits(Cells[Index].Possible);
		if (Cells[Index].Count == 0)
		{
			return false;
		}
		++Backtracks;
		if (!Propagate(Width, Height, Tiles, Cells))
		{
			return false;
		}
	}
}

void FWfcSolver::InitCells(
	int32 OriginX,
	int32 OriginY,
	int32 Width,
	int32 Height,
	int32 WorldSeed,
	const TArray<FWfcTileDef>& Tiles,
	bool bSealBorder,
	TArray<FCellState>& Cells) const
{
	const int32 Num = Width * Height;
	Cells.SetNum(Num);
	uint32 All = 0;
	for (int32 i = 0; i < Tiles.Num(); ++i)
	{
		All |= BitOf(i);
	}

	for (int32 lx = 0; lx < Width; ++lx)
	{
		for (int32 ly = 0; ly < Height; ++ly)
		{
			const int32 Index = CellIndex(lx, ly, Height);
			FCellState& Cell = Cells[Index];
			Cell.Possible = All;
			Cell.Collapsed = INDEX_NONE;
			Cell.Count = Tiles.Num();

			const int32 Wx = OriginX + lx;
			const int32 Wy = OriginY + ly;

			auto ConstrainDir = [&](int32 Dir, bool bOnBorder)
			{
				if (!bOnBorder)
				{
					return;
				}
				const bool bOpen = WantsOpen(
					WorldSeed, Wx, Wy, Dir, OriginX, OriginY, Width, Height, bSealBorder, true);
				uint32 Allowed = 0;
				for (int32 t = 0; t < Tiles.Num(); ++t)
				{
					if (WfcMath::SocketOpen(Tiles[t].SocketMask, Dir) == bOpen)
					{
						Allowed |= BitOf(t);
					}
				}
				RestrictToMask(Cell, Allowed, Tiles);
			};

			ConstrainDir(WfcMath::DirNorth, lx == Width - 1);
			ConstrainDir(WfcMath::DirSouth, lx == 0);
			ConstrainDir(WfcMath::DirEast, ly == Height - 1);
			ConstrainDir(WfcMath::DirWest, ly == 0);

			if (!bSealBorder && Wx == 0 && Wy == 0)
			{
				uint32 CrossOnly = 0;
				for (int32 t = 0; t < Tiles.Num(); ++t)
				{
					if (Tiles[t].Topology == EWfcTopology::Cross)
					{
						CrossOnly |= BitOf(t);
					}
				}
				if (CrossOnly != 0)
				{
					RestrictToMask(Cell, CrossOnly, Tiles);
				}
			}
		}
	}
}

bool FWfcSolver::RestrictToMask(
	FCellState& Cell,
	uint32 Allowed,
	const TArray<FWfcTileDef>& Tiles) const
{
	Cell.Possible &= Allowed;
	Cell.Count = CountBits(Cell.Possible);
	if (Cell.Count == 1)
	{
		for (int32 t = 0; t < Tiles.Num(); ++t)
		{
			if (Cell.Possible & BitOf(t))
			{
				Cell.Collapsed = t;
				break;
			}
		}
	}
	return Cell.Count > 0;
}

bool FWfcSolver::Propagate(
	int32 Width,
	int32 Height,
	const TArray<FWfcTileDef>& Tiles,
	TArray<FCellState>& Cells) const
{
	TArray<int32> Stack;
	Stack.Reserve(Cells.Num());
	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		if (Cells[i].Count > 0)
		{
			Stack.Add(i);
		}
	}

	while (Stack.Num() > 0)
	{
		const int32 Index = Stack.Pop(EAllowShrinking::No);
		const int32 lx = Index / Height;
		const int32 ly = Index % Height;

		for (int32 Dir = 0; Dir < 4; ++Dir)
		{
			const FIntPoint Off = WfcMath::DirToOffset(Dir);
			const int32 Nlx = lx + Off.X;
			const int32 Nly = ly + Off.Y;
			if (Nlx < 0 || Nly < 0 || Nlx >= Width || Nly >= Height)
			{
				continue;
			}

			const int32 NIndex = CellIndex(Nlx, Nly, Height);
			FCellState& Neighbor = Cells[NIndex];

			bool bNeedOpen = false;
			bool bNeedClosed = false;
			for (int32 t = 0; t < Tiles.Num(); ++t)
			{
				if ((Cells[Index].Possible & BitOf(t)) == 0)
				{
					continue;
				}
				if (WfcMath::SocketOpen(Tiles[t].SocketMask, Dir))
				{
					bNeedOpen = true;
				}
				else
				{
					bNeedClosed = true;
				}
			}

			if (!bNeedOpen && !bNeedClosed)
			{
				return false;
			}

			const int32 Opp = WfcMath::OppositeDir(Dir);
			uint32 Allowed = 0;
			for (int32 t = 0; t < Tiles.Num(); ++t)
			{
				const bool bOpen = WfcMath::SocketOpen(Tiles[t].SocketMask, Opp);
				if ((bOpen && bNeedOpen) || (!bOpen && bNeedClosed))
				{
					Allowed |= BitOf(t);
				}
			}

			const uint32 Before = Neighbor.Possible;
			if (!RestrictToMask(Neighbor, Allowed, Tiles))
			{
				return false;
			}
			if (Neighbor.Possible != Before)
			{
				Stack.Add(NIndex);
			}
		}
	}
	return true;
}

int32 FWfcSolver::FindLowestEntropy(const TArray<FCellState>& Cells, FRandomStream& Stream) const
{
	int32 BestCount = TNumericLimits<int32>::Max();
	int32 BestIndex = INDEX_NONE;
	int32 Ties = 0;
	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		if (Cells[i].Collapsed != INDEX_NONE)
		{
			continue;
		}
		if (Cells[i].Count <= 0)
		{
			return INDEX_NONE;
		}
		if (Cells[i].Count < BestCount)
		{
			BestCount = Cells[i].Count;
			BestIndex = i;
			Ties = 1;
		}
		else if (Cells[i].Count == BestCount)
		{
			++Ties;
			if (Stream.RandRange(0, Ties - 1) == 0)
			{
				BestIndex = i;
			}
		}
	}
	return BestIndex;
}

int32 FWfcSolver::PickTile(const FCellState& Cell, const TArray<FWfcTileDef>& Tiles, FRandomStream& Stream) const
{
	float Total = 0.f;
	for (int32 t = 0; t < Tiles.Num(); ++t)
	{
		if (Cell.Possible & BitOf(t))
		{
			Total += FMath::Max(Tiles[t].Weight, 0.f);
		}
	}
	if (Total <= 0.f)
	{
		for (int32 t = 0; t < Tiles.Num(); ++t)
		{
			if (Cell.Possible & BitOf(t))
			{
				return t;
			}
		}
		return INDEX_NONE;
	}

	float Roll = Stream.FRand() * Total;
	for (int32 t = 0; t < Tiles.Num(); ++t)
	{
		if ((Cell.Possible & BitOf(t)) == 0)
		{
			continue;
		}
		Roll -= FMath::Max(Tiles[t].Weight, 0.f);
		if (Roll <= 0.f)
		{
			return t;
		}
	}
	for (int32 t = Tiles.Num() - 1; t >= 0; --t)
	{
		if (Cell.Possible & BitOf(t))
		{
			return t;
		}
	}
	return INDEX_NONE;
}

void FWfcSolver::Collapse(FCellState& Cell, int32 TileIndex) const
{
	Cell.Possible = BitOf(TileIndex);
	Cell.Collapsed = TileIndex;
	Cell.Count = 1;
}

void FWfcSolver::ForceFill(
	int32 OriginX,
	int32 OriginY,
	int32 Width,
	int32 Height,
	int32 WorldSeed,
	const TArray<FWfcTileDef>& Tiles,
	bool bSealBorder,
	TArray<FWfcCellResult>& OutCells) const
{
	const int32 Cross = CrossTileIndex(Tiles);
	OutCells.SetNum(Width * Height);

	for (int32 lx = 0; lx < Width; ++lx)
	{
		for (int32 ly = 0; ly < Height; ++ly)
		{
			const int32 Wx = OriginX + lx;
			const int32 Wy = OriginY + ly;
			uint8 Wanted = 0;
			auto Want = [&](int32 Dir, bool bBorder)
			{
				if (WantsOpen(WorldSeed, Wx, Wy, Dir, OriginX, OriginY, Width, Height, bSealBorder, bBorder))
				{
					Wanted |= static_cast<uint8>(1 << Dir);
				}
			};
			Want(WfcMath::DirNorth, lx == Width - 1);
			Want(WfcMath::DirSouth, lx == 0);
			Want(WfcMath::DirEast, ly == Height - 1);
			Want(WfcMath::DirWest, ly == 0);
			if (!bSealBorder && Wx == 0 && Wy == 0)
			{
				Wanted = 0x0F;
			}

			int32 Best = Cross;
			for (int32 t = 0; t < Tiles.Num(); ++t)
			{
				const uint8 Mask = Tiles[t].SocketMask;
				const bool bBorderOk =
					(!((lx == Width - 1) && WfcMath::SocketOpen(Mask, WfcMath::DirNorth) != WfcMath::SocketOpen(Wanted, WfcMath::DirNorth)))
					&& (!((lx == 0) && WfcMath::SocketOpen(Mask, WfcMath::DirSouth) != WfcMath::SocketOpen(Wanted, WfcMath::DirSouth)))
					&& (!((ly == Height - 1) && WfcMath::SocketOpen(Mask, WfcMath::DirEast) != WfcMath::SocketOpen(Wanted, WfcMath::DirEast)))
					&& (!((ly == 0) && WfcMath::SocketOpen(Mask, WfcMath::DirWest) != WfcMath::SocketOpen(Wanted, WfcMath::DirWest)));
				if (bBorderOk)
				{
					Best = t;
					if (Tiles[t].Topology == EWfcTopology::Cross)
					{
						break;
					}
				}
			}
			if (Best == INDEX_NONE)
			{
				Best = Cross;
			}
			WriteResult(Tiles[Best], Best, OutCells[CellIndex(lx, ly, Height)]);
		}
	}
}
