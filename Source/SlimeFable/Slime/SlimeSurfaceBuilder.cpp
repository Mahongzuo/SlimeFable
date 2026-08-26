// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeSurfaceBuilder.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"

using namespace SlimeSim;

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
	TArray<uint8> NoMerging;
	Build(Particles, DegenerateAnchor, NoMerging);
}

void FSlimeSurfaceBuilder::Build(const TArray<FSlimeParticle>& Particles, const FVector& DegenerateAnchor, const TArray<uint8>& MergingShotIds, float InVisualZLift, float InClipFloorZ)
{
	if (!IsConfigured())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(SlimeSurface_Build);

	LiveVertexCount = 0;
	bTruncated = false;
	VisualZLift = FMath::Max(InVisualZLift, 0.f);
	ClipFloorZ = InClipFloorZ;
	ActiveMergingShots.Reset();
	for (const uint8 ShotId : MergingShotIds)
	{
		if (ShotId != 0)
		{
			ActiveMergingShots.Add(ShotId);
		}
	}

	const FVector Lift(0.f, 0.f, VisualZLift);
	FBox BodyBounds(ForceInit);
	TMap<uint8, FBox> ShotBounds;
	for (const FSlimeParticle& Particle : Particles)
	{
		if (Particle.IsBallistic())
		{
			if (Particle.ShotId == 0)
			{
				continue;
			}
			if (ActiveMergingShots.Contains(Particle.ShotId))
			{
				BodyBounds += FVector(Particle.Position) + Lift;
			}
			else
			{
				ShotBounds.FindOrAdd(Particle.ShotId) += FVector(Particle.Position);
			}
		}
		else
		{
			BodyBounds += FVector(Particle.Position) + Lift;
		}
	}

	// Reserve ~35% of the vertex budget for free-flying shots so a second Q chunk stays visible.
	const int32 VertexBudget = Vertices.Num();
	const int32 BodyVertexCap = ShotBounds.Num() > 0
		? FMath::Max((VertexBudget * 65) / 100, 3)
		: VertexBudget;

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
		FBox CoverBounds = SmoothedBodyBounds;
		CoverBounds += BodyBounds;
		BuildCluster(Particles, false, CoverBounds, 0);
		if (LiveVertexCount > BodyVertexCap)
		{
			LiveVertexCount = BodyVertexCap - (BodyVertexCap % 3);
			bTruncated = true;
		}
	}

	TArray<uint8> ShotIds;
	ShotBounds.GetKeys(ShotIds);
	ShotIds.Sort();
	for (const uint8 ShotId : ShotIds)
	{
		if (LiveVertexCount + 3 >= VertexBudget)
		{
			bTruncated = true;
			break;
		}
		const FBox* Bounds = ShotBounds.Find(ShotId);
		if (Bounds && Bounds->IsValid)
		{
			BuildCluster(Particles, true, *Bounds, ShotId);
		}
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

void FSlimeSurfaceBuilder::BuildCluster(const TArray<FSlimeParticle>& Particles, bool bBallisticSubset, const FBox& Bounds, uint8 ShotFilter)
{
	bClipFloorThisCluster = !bBallisticSubset && ClipFloorZ > -1.e8f;
	PrepareGrid(Bounds, !bBallisticSubset);
	SplatDensity(Particles, bBallisticSubset, ShotFilter, bBallisticSubset ? nullptr : &ActiveMergingShots);
	BlurDensity();
	ClipDensityBelowFloor();
	Triangulate();
}

void FSlimeSurfaceBuilder::PrepareGrid(const FBox& Bounds, bool bBodyCluster)
{
	// Particle AABB only covers centres. Expand by the anisotropic splat so the density
	// footprint (and therefore the iso surface) stays inside the grid.
	const FVector SplatExtent(SplatRadius, SplatRadius, SplatRadius * SplatZScale);
	const FBox Region = Bounds.ExpandBy(SplatExtent);

	// Empty shell outside the splat footprint for blur / iso closure — do not also add a
	// second fixed pad on top of the splat expand (that blew Dims toward MaxGridDim).
	const int32 EdgePad = FMath::Max(Params.BlurPasses + 1, 1);
	const int32 Usable = FMath::Max(Params.MaxGridDim - 2 * EdgePad - 1, 2);

	const float BaseCell = CellSize * (bBodyCluster ? FMath::Max(CellScale, 1.f) : 1.f);
	if (bBodyCluster && HeldRequiredCell < BaseCell)
	{
		HeldRequiredCell = BaseCell;
	}

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

	auto QuantizeOrigin = [](double MinCoord, float Cell) -> double
	{
		return FMath::FloorToDouble(MinCoord / Cell) * Cell;
	};

	// Lead on each axis: never lag into the volume (fixes -X holes). Trail with EMA only
	// when Desired is ahead — that does not inflate Dims the way min(smoothed, desired) did.
	auto LeadSnapOrigin = [&QuantizeOrigin](const FVector& Smoothed, const FVector& Desired, float Cell, float Ema) -> FVector
	{
		auto Axis = [&](double S, double D) -> double
		{
			if (D < S)
			{
				return D;
			}
			return QuantizeOrigin(FMath::Lerp(S, D, Ema), Cell);
		};
		return FVector(Axis(Smoothed.X, Desired.X), Axis(Smoothed.Y, Desired.Y), Axis(Smoothed.Z, Desired.Z));
	};

	FVector PaddedMin = Region.Min;
	FVector PaddedMax = Region.Max;
	FVector DesiredOrigin = FVector::ZeroVector;

	// If the settled span still exceeds MaxGridDim, grow the cell rather than Clamp-crop a face.
	for (int32 Attempt = 0; Attempt < 3; ++Attempt)
	{
		const FVector Pad(double(ActiveCell) * EdgePad);
		PaddedMin = Region.Min - Pad;
		PaddedMax = Region.Max + Pad;

		DesiredOrigin = FVector(
			QuantizeOrigin(PaddedMin.X, ActiveCell),
			QuantizeOrigin(PaddedMin.Y, ActiveCell),
			QuantizeOrigin(PaddedMin.Z, ActiveCell));

		if (bBodyCluster)
		{
			constexpr float OriginEma = 0.35f;
			constexpr float OriginSnap = 40.f;
			if (!bHaveSmoothedGridOrigin)
			{
				SmoothedGridOrigin = DesiredOrigin;
				bHaveSmoothedGridOrigin = true;
			}
			else if (Attempt == 0 && FVector::DistSquared(SmoothedGridOrigin, DesiredOrigin) > FMath::Square(OriginSnap))
			{
				SmoothedGridOrigin = DesiredOrigin;
			}
			else if (Attempt == 0)
			{
				SmoothedGridOrigin = LeadSnapOrigin(SmoothedGridOrigin, DesiredOrigin, ActiveCell, OriginEma);
			}
			else
			{
				// Cell grew: re-snap to the new lattice, still lead-snap so -axis stays covered.
				SmoothedGridOrigin = FVector(
					QuantizeOrigin(SmoothedGridOrigin.X, ActiveCell),
					QuantizeOrigin(SmoothedGridOrigin.Y, ActiveCell),
					QuantizeOrigin(SmoothedGridOrigin.Z, ActiveCell));
				SmoothedGridOrigin = LeadSnapOrigin(SmoothedGridOrigin, DesiredOrigin, ActiveCell, 1.f);
			}
			GridOrigin = SmoothedGridOrigin;
		}
		else
		{
			GridOrigin = DesiredOrigin;
		}

		const int32 NeedX = FMath::Max(FMath::CeilToInt((PaddedMax.X - GridOrigin.X) / ActiveCell) + 1, 4);
		const int32 NeedY = FMath::Max(FMath::CeilToInt((PaddedMax.Y - GridOrigin.Y) / ActiveCell) + 1, 4);
		const int32 NeedZ = FMath::Max(FMath::CeilToInt((PaddedMax.Z - GridOrigin.Z) / ActiveCell) + 1, 4);
		const int32 NeedMax = FMath::Max3(NeedX, NeedY, NeedZ);

		if (NeedMax <= Params.MaxGridDim)
		{
			Dims = FIntVector(NeedX, NeedY, NeedZ);
			break;
		}

		const double SpanX = PaddedMax.X - GridOrigin.X;
		const double SpanY = PaddedMax.Y - GridOrigin.Y;
		const double SpanZ = PaddedMax.Z - GridOrigin.Z;
		const double MaxSpan = FMath::Max3(SpanX, SpanY, SpanZ);
		// Slight pad so float Ceil does not push Need* over MaxGridDim and re-introduce a crop.
		const float FitCell = float(MaxSpan / double(FMath::Max(Params.MaxGridDim - 1, 1))) * 1.001f;
		ActiveCell = FMath::Max(ActiveCell, FitCell);
		if (bBodyCluster)
		{
			HeldRequiredCell = ActiveCell;
			RequiredGrowStreak = 0;
		}
	}

	// Final dims from the settled cell/origin (covers the last FitCell grow without a stale Need*).
	{
		const FVector Pad(double(ActiveCell) * EdgePad);
		PaddedMin = Region.Min - Pad;
		PaddedMax = Region.Max + Pad;
		DesiredOrigin = FVector(
			QuantizeOrigin(PaddedMin.X, ActiveCell),
			QuantizeOrigin(PaddedMin.Y, ActiveCell),
			QuantizeOrigin(PaddedMin.Z, ActiveCell));
		if (bBodyCluster)
		{
			SmoothedGridOrigin = FVector(
				QuantizeOrigin(SmoothedGridOrigin.X, ActiveCell),
				QuantizeOrigin(SmoothedGridOrigin.Y, ActiveCell),
				QuantizeOrigin(SmoothedGridOrigin.Z, ActiveCell));
			SmoothedGridOrigin = LeadSnapOrigin(SmoothedGridOrigin, DesiredOrigin, ActiveCell, 1.f);
			GridOrigin = SmoothedGridOrigin;
		}
		else
		{
			GridOrigin = DesiredOrigin;
		}
		Dims = FIntVector(
			FMath::Clamp(FMath::Max(FMath::CeilToInt((PaddedMax.X - GridOrigin.X) / ActiveCell) + 1, 4), 4, Params.MaxGridDim),
			FMath::Clamp(FMath::Max(FMath::CeilToInt((PaddedMax.Y - GridOrigin.Y) / ActiveCell) + 1, 4), 4, Params.MaxGridDim),
			FMath::Clamp(FMath::Max(FMath::CeilToInt((PaddedMax.Z - GridOrigin.Z) / ActiveCell) + 1, 4), 4, Params.MaxGridDim));
	}

	ActiveCellSize = ActiveCell;

	const int32 NumSamples = Dims.X * Dims.Y * Dims.Z;
	FMemory::Memzero(Density.GetData(), NumSamples * sizeof(float));

	TouchedMin = Dims;
	TouchedMax = FIntVector(-1);
}

void FSlimeSurfaceBuilder::SplatDensity(const TArray<FSlimeParticle>& Particles, bool bBallisticSubset, uint8 ShotFilter, const TSet<uint8>* MergingShots)
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
		if (bBallisticSubset)
		{
			if (!Particle.IsBallistic())
			{
				continue;
			}
			if (ShotFilter != 0 && Particle.ShotId != ShotFilter)
			{
				continue;
			}
		}
		else
		{
			// Body cluster: attached particles + any soft-merging clones.
			if (Particle.IsBallistic())
			{
				if (!MergingShots || !MergingShots->Contains(Particle.ShotId))
				{
					continue;
				}
			}
		}

		FVector WorldPos(Particle.Position);
		if (!bBallisticSubset)
		{
			WorldPos.Z += VisualZLift;
		}
		const FVector Local = WorldPos - GridOrigin;
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

void FSlimeSurfaceBuilder::ClipDensityBelowFloor()
{
	if (!bClipFloorThisCluster || ClipFloorZ <= -1.e8f || Dims.Z <= 0)
	{
		return;
	}

	constexpr float Eps = 0.25f;
	const float CutZ = ClipFloorZ + Eps;
	for (int32 Z = 0; Z < Dims.Z; ++Z)
	{
		const float WorldZ = GridOrigin.Z + float(Z) * ActiveCellSize;
		if (WorldZ >= CutZ)
		{
			continue;
		}
		for (int32 Y = 0; Y < Dims.Y; ++Y)
		{
			for (int32 X = 0; X < Dims.X; ++X)
			{
				Density[SampleIndex(X, Y, Z)] = 0.f;
			}
		}
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

			// Two Newton steps onto the iso surface. Central-difference |G| ≈ 2·dD/dCell, so
			// the cell step is 2·error/|G|. This flattens MC terraces that sunlight picks out.
			FVector3f P = Grid;
			FVector Grad = SampleGradient(P.X, P.Y, P.Z);
			FVector Normal = Grad.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
			for (int32 Iter = 0; Iter < 2; ++Iter)
			{
				const float GradLen = float(Grad.Size());
				if (GradLen <= KINDA_SMALL_NUMBER)
				{
					break;
				}
				const float Error = SampleTrilinear(P.X, P.Y, P.Z) - Threshold;
				const float StepCells = FMath::Clamp(2.f * Error / GradLen, -0.75f, 0.75f);
				P -= FVector3f(Normal) * StepCells;
				Grad = SampleGradient(P.X, P.Y, P.Z);
				Normal = Grad.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
			}

			Vertices[LiveVertexCount] = GridOrigin + FVector(P) * double(ActiveCellSize);
			Normals[LiveVertexCount] = Normal;

			++LiveVertexCount;
		}
	}
}
