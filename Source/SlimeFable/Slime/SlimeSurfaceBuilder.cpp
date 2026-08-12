// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeSurfaceBuilder.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"

using namespace SlimeSim;

namespace
{
	/** Empty samples kept around the occupied region so the iso surface always closes. */
	constexpr int32 GGridPadding = 2;

	/** Cell size is allowed to grow this much before the grid clips instead. */
	constexpr float GMaxCellSizeScale = 2.f;
}

const FIntVector FSlimeSurfaceBuilder::CornerOffsets[8] =
{
	FIntVector(0, 0, 0),
	FIntVector(1, 0, 0),
	FIntVector(1, 1, 0),
	FIntVector(0, 1, 0),
	FIntVector(0, 0, 1),
	FIntVector(1, 0, 1),
	FIntVector(1, 1, 1),
	FIntVector(0, 1, 1),
};

const int32 FSlimeSurfaceBuilder::EdgeCorners[12][2] =
{
	{0, 1}, {1, 2}, {2, 3}, {0, 3},
	{4, 5}, {5, 6}, {6, 7}, {4, 7},
	{0, 4}, {1, 5}, {2, 6}, {3, 7},
};

// Standard marching cubes triangulation table (Paul Bourke).
const int32 FSlimeSurfaceBuilder::TriangleTable[256][16] =
{
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1},
	{3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1},
	{3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
	{3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1},
	{9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1},
	{1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1},
	{9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
	{2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1},
	{8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1},
	{9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
	{4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1},
	{3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1},
	{1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1},
	{4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1},
	{4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
	{9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1},
	{1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
	{5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
	{2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1},
	{9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
	{0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
	{2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1},
	{10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1},
	{4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1},
	{5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1},
	{5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1},
	{9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1},
	{0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1},
	{1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1},
	{10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1},
	{8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1},
	{2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1},
	{7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1},
	{9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1},
	{2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1},
	{11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1},
	{9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1},
	{5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
	{11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
	{11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
	{1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1},
	{9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1},
	{5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1},
	{2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
	{0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
	{5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1},
	{6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
	{0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1},
	{3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1},
	{6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1},
	{5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1},
	{1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
	{10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1},
	{6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1},
	{1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1},
	{8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1},
	{7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
	{3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
	{5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1},
	{0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1},
	{9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
	{8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1},
	{5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
	{0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
	{6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1},
	{10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1},
	{10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1},
	{8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1},
	{1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1},
	{3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1},
	{0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1},
	{10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1},
	{0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1},
	{3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1},
	{6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
	{9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1},
	{8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
	{3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1},
	{6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1},
	{0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1},
	{10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1},
	{10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1},
	{1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1},
	{2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
	{7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1},
	{7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1},
	{2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
	{1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
	{11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1},
	{8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
	{0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1},
	{7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
	{10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
	{2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
	{6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1},
	{7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1},
	{2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1},
	{1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1},
	{10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1},
	{10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1},
	{0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1},
	{7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1},
	{6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1},
	{8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1},
	{9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1},
	{6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1},
	{1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1},
	{4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1},
	{10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
	{8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1},
	{0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1},
	{1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1},
	{8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1},
	{10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1},
	{4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
	{10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
	{5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
	{11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1},
	{9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
	{6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1},
	{7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1},
	{3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
	{7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1},
	{9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1},
	{3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1},
	{6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
	{9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1},
	{1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
	{4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
	{7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1},
	{6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1},
	{3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1},
	{0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1},
	{6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1},
	{1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1},
	{0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
	{11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
	{6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1},
	{5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1},
	{9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1},
	{1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
	{1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
	{10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1},
	{0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1},
	{5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1},
	{10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1},
	{11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1},
	{0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1},
	{9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1},
	{7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
	{2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1},
	{8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1},
	{9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1},
	{9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
	{1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1},
	{9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1},
	{9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1},
	{5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1},
	{0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1},
	{10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
	{2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1},
	{0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
	{0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
	{9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1},
	{5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1},
	{3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
	{5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1},
	{8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1},
	{0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1},
	{9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
	{0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1},
	{1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1},
	{3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
	{4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1},
	{9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
	{11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1},
	{11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1},
	{2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1},
	{9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
	{3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
	{1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1},
	{4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1},
	{4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1},
	{0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
	{3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1},
	{3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1},
	{0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1},
	{9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1},
	{1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
};

void FSlimeSurfaceBuilder::Configure(const FSlimeSurfaceParams& InParams, float InParticleSpacing)
{
	const float NewSpacing = FMath::Max(InParticleSpacing, 0.5f);
	const bool bResetScale = !IsConfigured() || !FMath::IsNearlyEqual(ParticleSpacing, NewSpacing);

	Params = InParams;
	ParticleSpacing = NewSpacing;

	CellSize = FMath::Max(ParticleSpacing * Params.CellSizeMultiplier, 0.5f);
	SplatRadius = FMath::Max(ParticleSpacing * Params.SplatRadiusMultiplier, CellSize);
	SplatZScale = FMath::Clamp(Params.SplatZScale, 0.25f, 1.5f);
	// Preserve CellScale across per-frame Configure (spread splat tweaks). Reset only on first
	// configure or when particle spacing changes.
	if (bResetScale)
	{
		CellScale = 1.f;
		TruncationStreak = 0;
		HeldRequiredCell = CellSize;
		RequiredGrowStreak = 0;
		bHaveSmoothedBodyBounds = false;
		bHaveSmoothedGridOrigin = false;
	}

	// Field value a fully enclosed sample would reach at the rest packing density, so the
	// iso threshold means the same thing regardless of particle count or spacing.
	const float EffectiveSplatR = SplatRadius * FMath::Pow(SplatZScale, 1.f / 3.f);
	const float ParticlesPerVolume = 1.f / (ParticleSpacing * ParticleSpacing * ParticleSpacing);
	const float KernelIntegral = (64.f * PI * EffectiveSplatR * EffectiveSplatR * EffectiveSplatR) / 315.f;
	InvInteriorValue = 1.f / FMath::Max(ParticlesPerVolume * KernelIntegral, KINDA_SMALL_NUMBER);

	// The soup never shares vertices, so the index buffer is a constant and is built once.
	const int32 Budget = FMath::Max(Params.MaxVertices - (Params.MaxVertices % 3), 3);
	Vertices.SetNumUninitialized(Budget);
	Normals.SetNumUninitialized(Budget);
	if (Indices.Num() != Budget)
	{
		Indices.SetNumUninitialized(Budget);
		for (int32 Index = 0; Index < Budget; ++Index)
		{
			Indices[Index] = Index;
		}
	}

	const int32 MaxSamples = Params.MaxGridDim * Params.MaxGridDim * Params.MaxGridDim;
	Density.SetNumUninitialized(MaxSamples);
	DensityScratch.SetNumUninitialized(MaxSamples);

	LiveVertexCount = 0;
}

void FSlimeSurfaceBuilder::Build(const TArray<FSlimeParticle>& Particles, const FVector& DegenerateAnchor)
{
	if (!IsConfigured())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(SlimeSurface_Build);

	LiveVertexCount = 0;
	bTruncated = false;

	FBox BodyBounds(ForceInit);
	FBox FragmentBounds(ForceInit);
	for (const FSlimeParticle& Particle : Particles)
	{
		if (Particle.IsBallistic())
		{
			FragmentBounds += FVector(Particle.Position);
		}
		else
		{
			BodyBounds += FVector(Particle.Position);
		}
	}

	if (BodyBounds.IsValid)
	{
		constexpr float BoundsEma = 0.45f;
		constexpr float SnapDistance = 25.f;
		if (!bHaveSmoothedBodyBounds)
		{
			SmoothedBodyBounds = BodyBounds;
			bHaveSmoothedBodyBounds = true;
		}
		else
		{
			const float CenterDrift = float(FVector::Dist(SmoothedBodyBounds.GetCenter(), BodyBounds.GetCenter()));
			if (CenterDrift > SnapDistance)
			{
				SmoothedBodyBounds = BodyBounds;
			}
			else
			{
				SmoothedBodyBounds.Min = FMath::Lerp(SmoothedBodyBounds.Min, BodyBounds.Min, BoundsEma);
				SmoothedBodyBounds.Max = FMath::Lerp(SmoothedBodyBounds.Max, BodyBounds.Max, BoundsEma);
			}
		}
		// EMA damps grid origin jitter, but the cover volume must include every live particle
		// or splat drops at the lagging face and MC cuts a flat plane (especially on jump/move).
		FBox CoverBounds = SmoothedBodyBounds;
		CoverBounds += BodyBounds;
		BuildCluster(Particles, false, CoverBounds);
	}

	if (FragmentBounds.IsValid)
	{
		BuildCluster(Particles, true, FragmentBounds);
	}

	if (bTruncated)
	{
		++TruncationStreak;
		if (TruncationStreak >= 3)
		{
			CellScale = FMath::Min(CellScale * 1.25f, 3.f);
		}
	}
	else
	{
		TruncationStreak = 0;
		CellScale = FMath::Max(FMath::Lerp(CellScale, 1.f, 0.05f), 1.f);
	}

	for (int32 Index = LiveVertexCount; Index < Vertices.Num(); ++Index)
	{
		Vertices[Index] = DegenerateAnchor;
		Normals[Index] = FVector::UpVector;
	}
}

void FSlimeSurfaceBuilder::BuildCluster(const TArray<FSlimeParticle>& Particles, bool bBallisticSubset, const FBox& Bounds)
{
	PrepareGrid(Bounds, !bBallisticSubset);
	SplatDensity(Particles, bBallisticSubset);
	BlurDensity();
	Triangulate();
}

void FSlimeSurfaceBuilder::PrepareGrid(const FBox& Bounds, bool bBodyCluster)
{
	const int32 Usable = FMath::Max(Params.MaxGridDim - 2 * GGridPadding - 1, 2);
	const float BaseCell = CellSize * (bBodyCluster ? FMath::Max(CellScale, 1.f) : 1.f);
	if (bBodyCluster && HeldRequiredCell < BaseCell)
	{
		HeldRequiredCell = BaseCell;
	}

	FBox Region = Bounds;
	const FVector RegionSizeRaw = Region.GetSize();
	const float NeededCell = float(RegionSizeRaw.GetMax()) / float(Usable);

	float ActiveCell = BaseCell;
	if (bBodyCluster)
	{
		// Hysteresis: only grow RequiredCell after several consecutive frames that need it.
		if (NeededCell > HeldRequiredCell + 0.05f)
		{
			++RequiredGrowStreak;
			if (RequiredGrowStreak >= 3)
			{
				HeldRequiredCell = NeededCell;
				RequiredGrowStreak = 0;
			}
		}
		else
		{
			RequiredGrowStreak = 0;
			HeldRequiredCell = FMath::Max(FMath::Lerp(HeldRequiredCell, NeededCell, 0.05f), BaseCell);
		}
		ActiveCell = FMath::Max(BaseCell, HeldRequiredCell);
	}
	else
	{
		ActiveCell = FMath::Max(BaseCell, NeededCell);
	}

	ActiveCellSize = ActiveCell;

	// Prefer growing the cell over hard-cropping Region (cropping cuts horizontal planes).
	const double MaxSpan = double(Usable) * double(ActiveCell) * double(GMaxCellSizeScale);
	if (RegionSizeRaw.GetMax() > MaxSpan)
	{
		const float FitCell = float(RegionSizeRaw.GetMax()) / float(Usable);
		ActiveCell = FMath::Max(ActiveCell, FitCell);
		if (bBodyCluster)
		{
			HeldRequiredCell = ActiveCell;
			RequiredGrowStreak = 0;
		}
		ActiveCellSize = ActiveCell;
	}

	const FVector PaddedMin = Region.Min - FVector(double(ActiveCell) * GGridPadding);
	const FVector PaddedMax = Region.Max + FVector(double(ActiveCell) * GGridPadding);

	// Continuous cell snap (no block quantization) — block snapping caused walking flicker.
	auto QuantizeOrigin = [ActiveCell](double MinCoord) -> double
	{
		return FMath::FloorToDouble(MinCoord / ActiveCell) * ActiveCell;
	};

	FVector DesiredOrigin(
		QuantizeOrigin(PaddedMin.X),
		QuantizeOrigin(PaddedMin.Y),
		QuantizeOrigin(PaddedMin.Z));

	if (bBodyCluster)
	{
		constexpr float OriginEma = 0.35f;
		constexpr float OriginSnap = 40.f;
		if (!bHaveSmoothedGridOrigin)
		{
			SmoothedGridOrigin = DesiredOrigin;
			bHaveSmoothedGridOrigin = true;
		}
		else if (FVector::DistSquared(SmoothedGridOrigin, DesiredOrigin) > FMath::Square(OriginSnap))
		{
			SmoothedGridOrigin = DesiredOrigin;
		}
		else
		{
			SmoothedGridOrigin = FMath::Lerp(SmoothedGridOrigin, DesiredOrigin, OriginEma);
			// Re-snap after EMA so samples stay on the cell lattice.
			SmoothedGridOrigin = FVector(
				QuantizeOrigin(SmoothedGridOrigin.X),
				QuantizeOrigin(SmoothedGridOrigin.Y),
				QuantizeOrigin(SmoothedGridOrigin.Z));
		}
		GridOrigin = SmoothedGridOrigin;
	}
	else
	{
		GridOrigin = DesiredOrigin;
	}

	// Dims must span GridOrigin → padded max (not Region.Size alone), or snap clips the far side.
	Dims = FIntVector(
		FMath::Clamp(FMath::CeilToInt((PaddedMax.X - GridOrigin.X) / ActiveCell) + 1, 4, Params.MaxGridDim),
		FMath::Clamp(FMath::CeilToInt((PaddedMax.Y - GridOrigin.Y) / ActiveCell) + 1, 4, Params.MaxGridDim),
		FMath::Clamp(FMath::CeilToInt((PaddedMax.Z - GridOrigin.Z) / ActiveCell) + 1, 4, Params.MaxGridDim));

	const int32 NumSamples = Dims.X * Dims.Y * Dims.Z;
	FMemory::Memzero(Density.GetData(), NumSamples * sizeof(float));

	TouchedMin = Dims;
	TouchedMax = FIntVector(-1);
}

void FSlimeSurfaceBuilder::SplatDensity(const TArray<FSlimeParticle>& Particles, bool bBallisticSubset)
{
	const float InvCell = 1.f / ActiveCellSize;
	const float RadiusXY = SplatRadius;
	const float RadiusZ = SplatRadius * SplatZScale;
	const float InvRadiusXYSq = 1.f / FMath::Max(RadiusXY * RadiusXY, KINDA_SMALL_NUMBER);
	const float InvRadiusZSq = 1.f / FMath::Max(RadiusZ * RadiusZ, KINDA_SMALL_NUMBER);
	const int32 ReachXY = FMath::CeilToInt(RadiusXY * InvCell);
	const int32 ReachZ = FMath::CeilToInt(RadiusZ * InvCell);

	for (const FSlimeParticle& Particle : Particles)
	{
		if (Particle.IsBallistic() != bBallisticSubset)
		{
			continue;
		}

		const FVector Local = FVector(Particle.Position) - GridOrigin;
		const FIntVector Base(
			FMath::FloorToInt(Local.X * InvCell),
			FMath::FloorToInt(Local.Y * InvCell),
			FMath::FloorToInt(Local.Z * InvCell));

		const int32 MinX = FMath::Max(Base.X - ReachXY, 0);
		const int32 MaxX = FMath::Min(Base.X + ReachXY + 1, Dims.X - 1);
		const int32 MinY = FMath::Max(Base.Y - ReachXY, 0);
		const int32 MaxY = FMath::Min(Base.Y + ReachXY + 1, Dims.Y - 1);
		const int32 MinZ = FMath::Max(Base.Z - ReachZ, 0);
		const int32 MaxZ = FMath::Min(Base.Z + ReachZ + 1, Dims.Z - 1);

		if (MaxX < MinX || MaxY < MinY || MaxZ < MinZ)
		{
			continue;
		}

		for (int32 Z = MinZ; Z <= MaxZ; ++Z)
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const FVector Sample(X * ActiveCellSize, Y * ActiveCellSize, Z * ActiveCellSize);
			const FVector Delta = Sample - Local;
			// Anisotropic metaball: wider in XY, flatter in Z while pancaked.
			const float NormDistSq =
				float(Delta.X * Delta.X + Delta.Y * Delta.Y) * InvRadiusXYSq +
				float(Delta.Z * Delta.Z) * InvRadiusZSq;
			if (NormDistSq >= 1.f)
			{
				continue;
			}
			const float Falloff = 1.f - NormDistSq;
			Density[SampleIndex(X, Y, Z)] += Falloff * Falloff * Falloff;
		}

		TouchedMin.X = FMath::Min(TouchedMin.X, MinX);
		TouchedMin.Y = FMath::Min(TouchedMin.Y, MinY);
		TouchedMin.Z = FMath::Min(TouchedMin.Z, MinZ);
		TouchedMax.X = FMath::Max(TouchedMax.X, MaxX);
		TouchedMax.Y = FMath::Max(TouchedMax.Y, MaxY);
		TouchedMax.Z = FMath::Max(TouchedMax.Z, MaxZ);
	}

	const int32 NumSamples = Dims.X * Dims.Y * Dims.Z;
	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		Density[Index] *= InvInteriorValue;
	}
}

void FSlimeSurfaceBuilder::BlurDensity()
{
	if (Params.BlurPasses <= 0 || TouchedMax.X < TouchedMin.X)
	{
		return;
	}

	const int32 NumSamples = Dims.X * Dims.Y * Dims.Z;

	for (int32 Pass = 0; Pass < Params.BlurPasses; ++Pass)
	{
		// Separable 1-2-1: three cheap sweeps instead of a 27 tap kernel.
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			FMemory::Memcpy(DensityScratch.GetData(), Density.GetData(), NumSamples * sizeof(float));

			for (int32 Z = 0; Z < Dims.Z; ++Z)
			for (int32 Y = 0; Y < Dims.Y; ++Y)
			for (int32 X = 0; X < Dims.X; ++X)
			{
				const FIntVector Step(Axis == 0 ? 1 : 0, Axis == 1 ? 1 : 0, Axis == 2 ? 1 : 0);
				const int32 LowX = FMath::Clamp(X - Step.X, 0, Dims.X - 1);
				const int32 LowY = FMath::Clamp(Y - Step.Y, 0, Dims.Y - 1);
				const int32 LowZ = FMath::Clamp(Z - Step.Z, 0, Dims.Z - 1);
				const int32 HighX = FMath::Clamp(X + Step.X, 0, Dims.X - 1);
				const int32 HighY = FMath::Clamp(Y + Step.Y, 0, Dims.Y - 1);
				const int32 HighZ = FMath::Clamp(Z + Step.Z, 0, Dims.Z - 1);

				const float Low = DensityScratch[SampleIndex(LowX, LowY, LowZ)];
				const float Mid = DensityScratch[SampleIndex(X, Y, Z)];
				const float High = DensityScratch[SampleIndex(HighX, HighY, HighZ)];
				Density[SampleIndex(X, Y, Z)] = (Low + 2.f * Mid + High) * 0.25f;
			}
		}
	}
}

void FSlimeSurfaceBuilder::Triangulate()
{
	if (TouchedMax.X < TouchedMin.X)
	{
		return;
	}

	const float Threshold = Params.IsoThreshold;
	const int32 Budget = Vertices.Num();

	// Blur widens the footprint by one sample per pass, and marching cubes reads the cell's
	// far corner, so walk one extra sample out on each side.
	const int32 Slack = Params.BlurPasses + 1;
	const FIntVector From(
		FMath::Max(TouchedMin.X - Slack, 0),
		FMath::Max(TouchedMin.Y - Slack, 0),
		FMath::Max(TouchedMin.Z - Slack, 0));
	const FIntVector To(
		FMath::Min(TouchedMax.X + Slack, Dims.X - 2),
		FMath::Min(TouchedMax.Y + Slack, Dims.Y - 2),
		FMath::Min(TouchedMax.Z + Slack, Dims.Z - 2));

	float Corners[8];

	for (int32 Z = From.Z; Z <= To.Z; ++Z)
	for (int32 Y = From.Y; Y <= To.Y; ++Y)
	for (int32 X = From.X; X <= To.X; ++X)
	{
		int32 Mask = 0;
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FIntVector Offset = CornerOffsets[Corner];
			Corners[Corner] = Density[SampleIndex(X + Offset.X, Y + Offset.Y, Z + Offset.Z)];
			if (Corners[Corner] > Threshold)
			{
				Mask |= 1 << Corner;
			}
		}

		if (Mask == 0 || Mask == 255)
		{
			continue;
		}

		const int32* Line = TriangleTable[Mask];
		for (int32 Slot = 0; Slot < 15 && Line[Slot] >= 0; ++Slot)
		{
			if (LiveVertexCount >= Budget)
			{
				bTruncated = true;
				return;
			}

			const int32* Pair = EdgeCorners[Line[Slot]];
			const float Low = Corners[Pair[0]];
			const float High = Corners[Pair[1]];
			const float Denominator = High - Low;
			const float Alpha = FMath::Abs(Denominator) > SMALL_NUMBER
				? FMath::Clamp((Threshold - Low) / Denominator, 0.f, 1.f)
				: 0.5f;

			const FVector3f A(CornerOffsets[Pair[0]].X, CornerOffsets[Pair[0]].Y, CornerOffsets[Pair[0]].Z);
			const FVector3f B(CornerOffsets[Pair[1]].X, CornerOffsets[Pair[1]].Y, CornerOffsets[Pair[1]].Z);
			const FVector3f Grid = FVector3f(float(X), float(Y), float(Z)) + FMath::Lerp(A, B, Alpha);

			Vertices[LiveVertexCount] = GridOrigin + FVector(Grid) * double(ActiveCellSize);

			// Density rises inwards, so the outward normal is the negated gradient.
			const int32 Gx = X + FMath::RoundToInt(Grid.X - float(X));
			const int32 Gy = Y + FMath::RoundToInt(Grid.Y - float(Y));
			const int32 Gz = Z + FMath::RoundToInt(Grid.Z - float(Z));
			const FVector Gradient(
				SampleAt(Gx - 1, Gy, Gz) - SampleAt(Gx + 1, Gy, Gz),
				SampleAt(Gx, Gy - 1, Gz) - SampleAt(Gx, Gy + 1, Gz),
				SampleAt(Gx, Gy, Gz - 1) - SampleAt(Gx, Gy, Gz + 1));
			Normals[LiveVertexCount] = Gradient.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);

			++LiveVertexCount;
		}
	}
}
