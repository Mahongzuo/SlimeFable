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
	Colors.SetNumUninitialized(Budget);
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
	static const TMap<uint8, float> EmptyShotClips;
	Build(Particles, DegenerateAnchor, MergingShotIds, InVisualZLift, InClipFloorZ, EmptyShotClips);
}

void FSlimeSurfaceBuilder::Build(const TArray<FSlimeParticle>& Particles, const FVector& DegenerateAnchor, const TArray<uint8>& MergingShotIds, float InVisualZLift, float InClipFloorZ, const TMap<uint8, float>& InShotClipFloors)
{
	if (!IsConfigured())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(SlimeSurface_Build);

	LiveVertexCount = 0;
	bTruncated = false;
	// Invalidate up front: if no body cluster gets built this frame the GPU must not march a stale grid.
	BodyField.Dims = FIntVector::ZeroValue;
	BodyFloorFootprint = FSlimeFloorFootprint();
	VisualZLift = FMath::Max(InVisualZLift, 0.f);
	BodyClipFloorZ = InClipFloorZ;
	ClipFloorZ = InClipFloorZ;
	ShotClipFloors = InShotClipFloors;
	ConnectedBodyCenter = DegenerateAnchor;
 ConnectedShotCenters.Reset();
 TMap<uint8,int32> ConnectedCounts;
 for (const FSlimeParticle& Particle : Particles) if (Particle.IsBallistic() && MergingShotIds.Contains(Particle.ShotId))
 {
  ConnectedShotCenters.FindOrAdd(Particle.ShotId) += FVector(Particle.Position);
  ConnectedCounts.FindOrAdd(Particle.ShotId)++;
 }
 for (auto& Pair : ConnectedShotCenters) Pair.Value /= ConnectedCounts[Pair.Key];
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
		if (HasGroundSkirt() && BodyClipFloorZ > -1.e8f)
		{
			// Room for the flared base so the skirt never reaches the grid border.
			CoverBounds = CoverBounds.ExpandBy(FVector(SkirtSpread, SkirtSpread, 0.0));
		}
		if (HasBellFlare() && BodyClipFloorZ > -1.e8f)
		{
			CoverBounds = CoverBounds.ExpandBy(FVector(FlareReach, FlareReach, 0.0));
		}
		if (HasSheet())
		{
			// The 2D kernel reaches further than the splat, and the sheet top can rise above the particles.
			CoverBounds = CoverBounds.ExpandBy(FVector(SheetKernelRadius, SheetKernelRadius, SheetKernelRadius * 0.5f));
		}
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
		Colors[Index] = FLinearColor(0.f, 0.f, 0.f, 1.f);
	}
}

void FSlimeSurfaceBuilder::BuildCluster(const TArray<FSlimeParticle>& Particles, bool bBallisticSubset, const FBox& Bounds, uint8 ShotFilter)
{
	float ClusterClip = -1.e9f;
	if (bBallisticSubset)
	{
		if (const float* Found = ShotClipFloors.Find(ShotFilter))
		{
			ClusterClip = *Found;
		}
	}
	else
	{
		ClusterClip = BodyClipFloorZ;
	}
	ClipFloorZ = ClusterClip;
	bClipFloorThisCluster = ClipFloorZ > -1.e8f;
	bCollectFloorFootprint = !bBallisticSubset;

	// Cluster tag for the body material: 0 = body / unslotted shot, (slot + 1) / 255 = shot slot.
	CurrentClusterColor = FLinearColor(bBallisticSubset ? 1.f : 0.f, 0.f, 0.f, 1.f);
	if (bBallisticSubset)
	{
		const int32 Slot = ShotSlotIds.IndexOfByKey(ShotFilter);
		if (Slot != INDEX_NONE)
		{
			CurrentClusterColor.R = float(Slot + 1) / 255.f;
		}
	}

	PrepareGrid(Bounds, !bBallisticSubset);
	const bool bSheet = !bBallisticSubset && HasSheet();
	if (bSheet)
	{
		// Drape-only field first, parked in SheetDrapeField; then the ordinary field into Density.
		BuildSheetColumns(Particles);
		SplatMask = &SheetDrapeMask;
		SplatDensity(Particles, false, ShotFilter, &ActiveMergingShots);
		SplatMask = nullptr;
		BlurDensity();
		const int32 NumSamples = Dims.X * Dims.Y * Dims.Z;
		SheetDrapeField.SetNumUninitialized(NumSamples, EAllowShrinking::No);
		FMemory::Memcpy(SheetDrapeField.GetData(), Density.GetData(), NumSamples * sizeof(float));
		FMemory::Memzero(Density.GetData(), NumSamples * sizeof(float));
		if (SheetBlend < 0.999f)
		{
			SplatDensity(Particles, false, ShotFilter, &ActiveMergingShots);
			BlurDensity();
		}
		ApplySheetField();
	}
	else
	{
		SplatDensity(Particles, bBallisticSubset, ShotFilter, bBallisticSubset ? nullptr : &ActiveMergingShots);
		BlurDensity();
	}
	if (!bBallisticSubset && !bSheet)
	{
		ApplyGroundSkirt();
		ApplyBellFlare();
	}
	ClipDensityBelowFloor();
	Triangulate();

	// Fragment clusters reuse the Density buffer, so the body grid has to be copied out here.
	if (!bBallisticSubset && bCaptureBodyField)
	{
		CaptureBodyField();
	}
}

void FSlimeSurfaceBuilder::SetCaptureBodyField(bool bCapture)
{
	bCaptureBodyField = bCapture;
	if (!bCapture)
	{
		BodyField.Dims = FIntVector::ZeroValue;
		BodyField.Density.Empty();
	}
}

void FSlimeSurfaceBuilder::CaptureBodyField()
{
	const int32 NumSamples = Dims.X * Dims.Y * Dims.Z;
	if (NumSamples <= 0 || NumSamples > Density.Num())
	{
		BodyField.Dims = FIntVector::ZeroValue;
		return;
	}
	BodyField.Density.SetNumUninitialized(NumSamples, EAllowShrinking::No);
	FMemory::Memcpy(BodyField.Density.GetData(), Density.GetData(), NumSamples * sizeof(float));
	BodyField.Dims = Dims;
	BodyField.Origin = GridOrigin;
	BodyField.CellSize = ActiveCellSize;
	BodyField.Iso = Params.IsoThreshold;
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

	for (int32 ParticleIndex = 0; ParticleIndex < Particles.Num(); ++ParticleIndex)
	{
		const FSlimeParticle& Particle = Particles[ParticleIndex];
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
			if (SplatMask && (!SplatMask->IsValidIndex(ParticleIndex) || (*SplatMask)[ParticleIndex] == 0))
			{
				continue;
			}
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

	// Compact visual-only necks do not enter the physics particle set.
 if (!bBallisticSubset) for (const FVisualNeck& Neck : VisualNecks)
 {
  if (Neck.Radius < ActiveCellSize*0.2f) continue;
  const FVector Lift(0,0,VisualZLift);
  const FVector Start = Neck.Start+Lift, End = Neck.End+Lift, Segment = End-Start;
  const float Radius = FMath::Max(Neck.Radius, ActiveCellSize*0.5f);
  FIntVector Lo,Hi;
  for (int32 Axis=0;Axis<3;++Axis)
  {
   Lo[Axis] = FMath::Clamp(FMath::FloorToInt((FMath::Min(Start[Axis],End[Axis])-Radius-GridOrigin[Axis])/ActiveCellSize),0,Dims[Axis]-1);
   Hi[Axis] = FMath::Clamp(FMath::CeilToInt((FMath::Max(Start[Axis],End[Axis])+Radius-GridOrigin[Axis])/ActiveCellSize),0,Dims[Axis]-1);
  }
  for (int32 Z=Lo.Z;Z<=Hi.Z;++Z) for (int32 Y=Lo.Y;Y<=Hi.Y;++Y) for (int32 X=Lo.X;X<=Hi.X;++X)
  {
   const FVector Point = GridOrigin+FVector(X,Y,Z)*ActiveCellSize;
   const float T = FMath::Clamp(float(FVector::DotProduct(Point-Start,Segment)/FMath::Max(Segment.SizeSquared(),1.0)),0.f,1.f);
   const float Q = float(FVector::DistSquared(Point,Start+Segment*T))/(Radius*Radius);
   if (Q<1.f) Density[SampleIndex(X,Y,Z)] += FMath::Pow(1.f-Q,3.f) * (Params.IsoThreshold*1.8f/FMath::Max(InvInteriorValue,0.001f));
  }
  TouchedMin.X=FMath::Min(TouchedMin.X,Lo.X); TouchedMin.Y=FMath::Min(TouchedMin.Y,Lo.Y); TouchedMin.Z=FMath::Min(TouchedMin.Z,Lo.Z);
  TouchedMax.X=FMath::Max(TouchedMax.X,Hi.X); TouchedMax.Y=FMath::Max(TouchedMax.Y,Hi.Y); TouchedMax.Z=FMath::Max(TouchedMax.Z,Hi.Z);
 }
 const int32 NumSamples = Dims.X * Dims.Y * Dims.Z;
 for (int32 Index = 0; Index < NumSamples; ++Index)
 {
  Density[Index] *= InvInteriorValue;
	}
}

void FSlimeSurfaceBuilder::BuildSheetColumns(const TArray<FSlimeParticle>& Particles)
{
	const int32 NumColumns = Dims.X * Dims.Y;
	const float Cell = ActiveCellSize;
	const float InvCell = 1.f / Cell;
	const float Radius = FMath::Max(SheetKernelRadius, Cell);
	const float InvRadiusSq = 1.f / (Radius * Radius);
	const int32 Reach = FMath::CeilToInt(Radius * InvCell);
	const FVector Lift(0.f, 0.f, VisualZLift);

	SheetDrapeMask.SetNumZeroed(Particles.Num(), EAllowShrinking::No);
	FMemory::Memzero(SheetDrapeMask.GetData(), SheetDrapeMask.Num());

	TArray<int32> BodyIndices;
	BodyIndices.Reserve(Particles.Num());
	for (int32 Index = 0; Index < Particles.Num(); ++Index)
	{
		const FSlimeParticle& Particle = Particles[Index];
		if (Particle.IsBallistic())
		{
			if (ActiveMergingShots.Contains(Particle.ShotId))
			{
				SheetDrapeMask[Index] = 1;
			}
			continue;
		}
		BodyIndices.Add(Index);
	}

	auto Accumulate = [&](bool bSkipDrape, bool bHeight, float ParticleVolume)
	{
		SheetColumnWeight.SetNumUninitialized(NumColumns, EAllowShrinking::No);
		SheetColumnZ.SetNumUninitialized(NumColumns, EAllowShrinking::No);
		SheetColumnHeight.SetNumUninitialized(NumColumns, EAllowShrinking::No);
		FMemory::Memzero(SheetColumnWeight.GetData(), NumColumns * sizeof(float));
		FMemory::Memzero(SheetColumnZ.GetData(), NumColumns * sizeof(float));
		FMemory::Memzero(SheetColumnHeight.GetData(), NumColumns * sizeof(float));
		// Normalised 2D poly6: integrates to 1 over the disc, so the sheet holds exactly Volume.
		const float Norm = 4.f / (PI * Radius * Radius);
		for (const int32 Index : BodyIndices)
		{
			if (bSkipDrape && SheetDrapeMask[Index])
			{
				continue;
			}
			const FVector Local = FVector(Particles[Index].Position) + Lift - GridOrigin;
			const int32 Cx = FMath::RoundToInt(Local.X * InvCell);
			const int32 Cy = FMath::RoundToInt(Local.Y * InvCell);
			for (int32 Y = FMath::Max(Cy - Reach, 0); Y <= FMath::Min(Cy + Reach, Dims.Y - 1); ++Y)
			for (int32 X = FMath::Max(Cx - Reach, 0); X <= FMath::Min(Cx + Reach, Dims.X - 1); ++X)
			{
				const float Dx = float(X * Cell - Local.X);
				const float Dy = float(Y * Cell - Local.Y);
				const float Q = (Dx * Dx + Dy * Dy) * InvRadiusSq;
				if (Q >= 1.f)
				{
					continue;
				}
				const float W = (1.f - Q) * (1.f - Q) * (1.f - Q);
				const int32 Column = X + Dims.X * Y;
				SheetColumnWeight[Column] += W;
				SheetColumnZ[Column] += W * float(Local.Z);
				if (bHeight)
				{
					SheetColumnHeight[Column] += ParticleVolume * Norm * W;
				}
			}
		}
	};

	// Pass 1: provisional base from every body particle, used to spot drips hanging below it.
	Accumulate(false, false, 0.f);
	if (SheetDrapeDepth > 0.f)
	{
		for (const int32 Index : BodyIndices)
		{
			const FVector Local = FVector(Particles[Index].Position) + Lift - GridOrigin;
			const int32 X = FMath::Clamp(FMath::RoundToInt(Local.X * InvCell), 0, Dims.X - 1);
			const int32 Y = FMath::Clamp(FMath::RoundToInt(Local.Y * InvCell), 0, Dims.Y - 1);
			const int32 Column = X + Dims.X * Y;
			if (SheetColumnWeight[Column] > KINDA_SMALL_NUMBER &&
				Local.Z < SheetColumnZ[Column] / SheetColumnWeight[Column] - SheetDrapeDepth)
			{
				SheetDrapeMask[Index] = 1;
			}
		}
	}

	// Pass 2: the sheet proper. Drips keep their share of the volume, so v is Volume / all body particles.
	const float ParticleVolume = BodyIndices.Num() > 0 ? SheetVolume / float(BodyIndices.Num()) : 0.f;
	Accumulate(true, true, ParticleVolume);
}

void FSlimeSurfaceBuilder::ApplySheetField()
{
	const int32 NumSamples = Dims.X * Dims.Y * Dims.Z;
	if (NumSamples <= 0 || SheetDrapeField.Num() < NumSamples || SheetColumnWeight.Num() < Dims.X * Dims.Y)
	{
		return;
	}

	const float Cell = ActiveCellSize;
	const float Iso = Params.IsoThreshold;
	// One cell inside the surface reads Iso * 2; linear in Z so the iso crossing lands exactly on the sheet top.
	const float Slope = Iso / Cell;
	const float Blend = SheetBlend;
	const float Keep = 1.f - Blend;

	FIntVector Lo = TouchedMin;
	FIntVector Hi = TouchedMax;
	for (int32 Y = 0; Y < Dims.Y; ++Y)
	for (int32 X = 0; X < Dims.X; ++X)
	{
		const int32 Column = X + Dims.X * Y;
		const float Weight = SheetColumnWeight[Column];
		const float Thickness = SheetColumnHeight[Column];
		const bool bColumn = Weight > KINDA_SMALL_NUMBER && Thickness > 0.f;
		float Base = 0.f;
		float Top = 0.f;
		float Floor = 0.f;
		float Lateral = 0.f;
		if (bColumn)
		{
			Base = float(GridOrigin.Z) + SheetColumnZ[Column] / Weight - SheetBaseOffset;
			Top = Base + Thickness;
			Floor = Base - Cell;
			Lateral = Thickness - SheetMinThickness;
		}
		bool bTouched = false;
		for (int32 Z = 0; Z < Dims.Z; ++Z)
		{
			const int32 Sample = SampleIndex(X, Y, Z);
			float Sheet = 0.f;
			if (bColumn)
			{
				const float WorldZ = float(GridOrigin.Z) + float(Z) * Cell;
				const float Inside = FMath::Min3(WorldZ - Floor, Top - WorldZ, Lateral);
				Sheet = FMath::Max(Iso + Slope * Inside, 0.f);
			}
			const float Target = FMath::Max(SheetDrapeField[Sample], Sheet);
			const float Value = Density[Sample] * Keep + Target * Blend;
			Density[Sample] = Value;
			if (Sheet > 0.f)
			{
				bTouched = true;
				Lo.Z = FMath::Min(Lo.Z, Z);
				Hi.Z = FMath::Max(Hi.Z, Z);
			}
		}
		if (bTouched)
		{
			Lo.X = FMath::Min(Lo.X, X);
			Lo.Y = FMath::Min(Lo.Y, Y);
			Hi.X = FMath::Max(Hi.X, X);
			Hi.Y = FMath::Max(Hi.Y, Y);
		}
	}
	TouchedMin = FIntVector(FMath::Max(Lo.X - 1, 0), FMath::Max(Lo.Y - 1, 0), FMath::Max(Lo.Z - 1, 0));
	TouchedMax = FIntVector(FMath::Min(Hi.X + 1, Dims.X - 1), FMath::Min(Hi.Y + 1, Dims.Y - 1), FMath::Min(Hi.Z + 1, Dims.Z - 1));
}

static float SmoothMax(float A, float B, float K)
{
	if (K <= KINDA_SMALL_NUMBER)
	{
		return FMath::Max(A, B);
	}
	const float H = FMath::Max(K - FMath::Abs(A - B), 0.f) / K;
	return FMath::Max(A, B) + H * H * K * 0.25f;
}

void FSlimeSurfaceBuilder::ApplyBellFlare()
{
	if (!bClipFloorThisCluster || !HasBellFlare() || Dims.X <= 2 || Dims.Y <= 2 || Dims.Z <= 1)
	{
		return;
	}

	const float Cell = ActiveCellSize;
	const float Iso = Params.IsoThreshold;
	const float Slope = Iso / FMath::Max(Cell, 0.01f);
	const float Floor = ClipFloorZ;

	int32 TopZ = INDEX_NONE;
	for (int32 Z = Dims.Z - 1; Z >= 0; --Z)
	{
		const float WorldZ = float(GridOrigin.Z) + float(Z) * Cell;
		if (WorldZ < Floor)
		{
			break;
		}
		bool bHit = false;
		for (int32 Y = 0; Y < Dims.Y && !bHit; ++Y)
		{
			for (int32 X = 0; X < Dims.X; ++X)
			{
				if (Density[SampleIndex(X, Y, Z)] >= Iso)
				{
					bHit = true;
					break;
				}
			}
		}
		if (bHit)
		{
			TopZ = Z;
			break;
		}
	}
	if (TopZ == INDEX_NONE)
	{
		return;
	}

	const float BodyTop = float(GridOrigin.Z) + float(TopZ) * Cell;
	const float FlareHeight = FMath::Max(FlareHeightFraction * FMath::Max(BodyTop - Floor, Cell), Cell * 3.f);
	const float Reach = FlareReach;
	const float Curve = FMath::Max(FlareCurve, 0.5f);
	const float Tip = FMath::Min(FlareTip, Reach);
	const int32 ZMax = FMath::Min(FMath::CeilToInt((Floor + FlareHeight - float(GridOrigin.Z)) / Cell), Dims.Z - 1);
	if (ZMax < 0)
	{
		return;
	}

	const int32 Num = Dims.X * Dims.Y;
	TArray<float> OffX;
	TArray<float> OffY;
	TArray<uint8> Seeded;
	OffX.SetNumUninitialized(Num);
	OffY.SetNumUninitialized(Num);
	Seeded.SetNumUninitialized(Num);

	FIntVector Lo = TouchedMin;
	FIntVector Hi = TouchedMax;
	bool bDrew = false;

	auto Sample = [this](int32 X, int32 Y, int32 Z) -> float
	{
		if (X < 0 || Y < 0 || X >= Dims.X || Y >= Dims.Y)
		{
			return 0.f;
		}
		return Density[SampleIndex(X, Y, Z)];
	};

	for (int32 Z = 0; Z <= ZMax; ++Z)
	{
		const float WorldZ = float(GridOrigin.Z) + float(Z) * Cell;
		const float Above = WorldZ - Floor;
		if (Above > FlareHeight)
		{
			break;
		}
		if (Above < -Cell)
		{
			continue;
		}

		bool bAnyInside = false;
		const int32 DirX[4] = { 1, -1, 0, 0 };
		const int32 DirY[4] = { 0, 0, 1, -1 };
		for (int32 Y = 0; Y < Dims.Y; ++Y)
		{
			for (int32 X = 0; X < Dims.X; ++X)
			{
				const int32 Index = X + Dims.X * Y;
				const float Value = Sample(X, Y, Z);
				const bool bInside = Value >= Iso;
				bAnyInside |= bInside;
				float Best = 1.e6f;
				float BestX = 0.f;
				float BestY = 0.f;
				for (int32 Dir = 0; Dir < 4; ++Dir)
				{
					const float Neighbor = Sample(X + DirX[Dir], Y + DirY[Dir], Z);
					if (bInside == (Neighbor >= Iso))
					{
						continue;
					}
					const float Denom = FMath::Abs(Value - Neighbor);
					const float Along = FMath::Clamp(Denom > KINDA_SMALL_NUMBER ? FMath::Abs(Value - Iso) / Denom : 0.5f, 0.f, 1.f);
					const float Dist = Along * Cell;
					if (Dist < Best)
					{
						Best = Dist;
						BestX = float(DirX[Dir]) * Dist;
						BestY = float(DirY[Dir]) * Dist;
					}
				}
				Seeded[Index] = Best < 1.e5f ? 1 : 0;
				OffX[Index] = BestX;
				OffY[Index] = BestY;
			}
		}
		if (!bAnyInside)
		{
			continue;
		}

		auto Consider = [&](int32 X, int32 Y, int32 Nx, int32 Ny)
		{
			if (Nx < 0 || Ny < 0 || Nx >= Dims.X || Ny >= Dims.Y)
			{
				return;
			}
			const int32 Index = X + Dims.X * Y;
			const int32 Neighbor = Nx + Dims.X * Ny;
			if (!Seeded[Neighbor])
			{
				return;
			}
			// Offset is the vector from a cell to the nearest iso boundary.
			const float Dx = OffX[Neighbor] - float(X - Nx) * Cell;
			const float Dy = OffY[Neighbor] - float(Y - Ny) * Cell;
			const float LenSq = Dx * Dx + Dy * Dy;
			const float CurrentSq = Seeded[Index] ? OffX[Index] * OffX[Index] + OffY[Index] * OffY[Index] : 1.e12f;
			if (LenSq < CurrentSq)
			{
				OffX[Index] = Dx;
				OffY[Index] = Dy;
				Seeded[Index] = 1;
			}
		};

		for (int32 Y = 0; Y < Dims.Y; ++Y)
		{
			for (int32 X = 0; X < Dims.X; ++X)
			{
				Consider(X, Y, X - 1, Y);
				Consider(X, Y, X, Y - 1);
				Consider(X, Y, X - 1, Y - 1);
				Consider(X, Y, X + 1, Y - 1);
			}
		}
		for (int32 Y = Dims.Y - 1; Y >= 0; --Y)
		{
			for (int32 X = Dims.X - 1; X >= 0; --X)
			{
				Consider(X, Y, X + 1, Y);
				Consider(X, Y, X, Y + 1);
				Consider(X, Y, X + 1, Y + 1);
				Consider(X, Y, X - 1, Y + 1);
			}
		}

		const float HeightT = FMath::Clamp(Above / FlareHeight, 0.f, 1.f);
		float Spread = Reach * FMath::Pow(1.f - HeightT, Curve);
		if (Above < Tip && Tip > KINDA_SMALL_NUMBER)
		{
			const float Remaining = Tip - FMath::Max(Above, 0.f);
			Spread -= Tip - FMath::Sqrt(FMath::Max(Tip * Tip - Remaining * Remaining, 0.f));
		}
		Spread = FMath::Max(Spread, 0.f);
		const float Band = Spread + Cell;

		for (int32 Y = 0; Y < Dims.Y; ++Y)
		{
			for (int32 X = 0; X < Dims.X; ++X)
			{
				const int32 Index = X + Dims.X * Y;
				if (!Seeded[Index])
				{
					continue;
				}
				const float Len = FMath::Sqrt(OffX[Index] * OffX[Index] + OffY[Index] * OffY[Index]);
				const float Signed = Sample(X, Y, Z) >= Iso ? -Len : Len;
				if (Signed > Band || Signed < -Cell)
				{
					continue;
				}
				const float Field = Iso + Slope * (Spread - Signed);
				if (Field <= 0.f)
				{
					continue;
				}
				Density[SampleIndex(X, Y, Z)] = SmoothMax(Density[SampleIndex(X, Y, Z)], Field, 0.35f * Iso);
				Lo.X = FMath::Min(Lo.X, X);
				Lo.Y = FMath::Min(Lo.Y, Y);
				Lo.Z = FMath::Min(Lo.Z, Z);
				Hi.X = FMath::Max(Hi.X, X);
				Hi.Y = FMath::Max(Hi.Y, Y);
				Hi.Z = FMath::Max(Hi.Z, Z);
				bDrew = true;
			}
		}
	}

	if (bDrew)
	{
		TouchedMin = FIntVector(FMath::Max(Lo.X - 1, 0), FMath::Max(Lo.Y - 1, 0), FMath::Max(Lo.Z - 1, 0));
		TouchedMax = FIntVector(FMath::Min(Hi.X + 1, Dims.X - 1), FMath::Min(Hi.Y + 1, Dims.Y - 1), FMath::Min(Hi.Z + 1, Dims.Z - 1));
	}
}

void FSlimeSurfaceBuilder::ApplyGroundSkirt()
{
	if (!bClipFloorThisCluster || ClipFloorZ <= -1.e8f || !HasGroundSkirt() || Dims.Z <= 0)
	{
		return;
	}

	// At least 1.5 cells so a coarse grid still gets one interpolated slice of flare.
	const float Height = FMath::Max(SkirtHeight, ActiveCellSize * 1.5f);
	const float SpreadCells = SkirtSpread / FMath::Max(ActiveCellSize, 0.01f);
	const int32 Reach = FMath::Clamp(FMath::CeilToInt(SpreadCells), 1, 8);
	TArray<float> Slice;
	Slice.SetNumUninitialized(Dims.X * Dims.Y);
	bool bDilated = false;
	for (int32 Z = 0; Z < Dims.Z; ++Z)
	{
		const float WorldZ = GridOrigin.Z + float(Z) * ActiveCellSize;
		const float Above = WorldZ - ClipFloorZ;
		if (Above >= Height)
		{
			break;
		}
		if (Above < 0.25f)
		{
			// Zeroed by ClipDensityBelowFloor right after this.
			continue;
		}
		const float Weight = 1.f - FMath::Clamp(Above / Height, 0.f, 1.f);
		const float Radius = SpreadCells * Weight;
		if (Radius < 0.25f)
		{
			continue;
		}
		for (int32 Y = 0; Y < Dims.Y; ++Y)
		{
			for (int32 X = 0; X < Dims.X; ++X)
			{
				Slice[X + Dims.X * Y] = Density[SampleIndex(X, Y, Z)];
			}
		}
		// Outermost ring stays empty so marching cubes still closes the surface.
		for (int32 Y = 1; Y < Dims.Y - 1; ++Y)
		{
			for (int32 X = 1; X < Dims.X - 1; ++X)
			{
				float Best = Slice[X + Dims.X * Y];
				for (int32 Dy = -Reach; Dy <= Reach; ++Dy)
				{
					const int32 Ny = Y + Dy;
					if (Ny < 0 || Ny >= Dims.Y)
					{
						continue;
					}
					for (int32 Dx = -Reach; Dx <= Reach; ++Dx)
					{
						const int32 Nx = X + Dx;
						if (Nx < 0 || Nx >= Dims.X || (Dx == 0 && Dy == 0))
						{
							continue;
						}
						const float Dist = FMath::Sqrt(float(Dx * Dx + Dy * Dy));
						if (Dist > Radius)
						{
							continue;
						}
						// Smooth falloff so the flare meets the floor at a shallow contact angle.
						const float T = Dist / Radius;
						const float Falloff = 1.f - T * T * (3.f - 2.f * T);
						Best = FMath::Max(Best, Slice[Nx + Dims.X * Ny] * Falloff);
					}
				}
				Density[SampleIndex(X, Y, Z)] = Best;
			}
		}
		bDilated = true;
	}
	if (bDilated && TouchedMax.X >= TouchedMin.X)
	{
		TouchedMin.X = FMath::Max(TouchedMin.X - Reach, 1);
		TouchedMin.Y = FMath::Max(TouchedMin.Y - Reach, 1);
		TouchedMax.X = FMath::Min(TouchedMax.X + Reach, Dims.X - 2);
		TouchedMax.Y = FMath::Min(TouchedMax.Y + Reach, Dims.Y - 2);
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
	const float Cell = ActiveCellSize;
	const float Iso = Params.IsoThreshold;
	int32 Lowest = Dims.Z;
	int32 FloorCount = 0;
	double SumX = 0.0, SumY = 0.0, SumXX = 0.0, SumYY = 0.0, SumXY = 0.0;
	// Zeroing everything under the floor leaves the iso crossing up to one cell above it.
	// Instead, set the sample just below the floor so the crossing lands exactly on the floor.
	for (int32 Y = 0; Y < Dims.Y; ++Y)
	{
		for (int32 X = 0; X < Dims.X; ++X)
		{
			int32 Above = INDEX_NONE;
			for (int32 Z = 0; Z < Dims.Z; ++Z)
			{
				const float WorldZ = float(GridOrigin.Z) + float(Z) * Cell;
				if (WorldZ >= CutZ)
				{
					Above = Z;
					break;
				}
			}
			if (Above == INDEX_NONE)
			{
				for (int32 Z = 0; Z < Dims.Z; ++Z)
				{
					Density[SampleIndex(X, Y, Z)] = 0.f;
				}
				continue;
			}
			if (Above == 0)
			{
				continue;
			}
			const float AboveZ = float(GridOrigin.Z) + float(Above) * Cell;
			const float BelowZ = AboveZ - Cell;
			const float AboveValue = Density[SampleIndex(X, Y, Above)];
			float BelowValue = 0.f;
			if (AboveValue > Iso)
			{
				const float Span = FMath::Max(AboveZ - CutZ, 0.01f);
				BelowValue = FMath::Clamp(Iso - (AboveValue - Iso) * (CutZ - BelowZ) / Span, 0.f, Iso);
				// Same columns the iso surface actually sits on, after the bell flare has widened them.
				if (bCollectFloorFootprint)
				{
					const double WorldX = double(GridOrigin.X) + double(X) * double(Cell);
					const double WorldY = double(GridOrigin.Y) + double(Y) * double(Cell);
					SumX += WorldX;
					SumY += WorldY;
					SumXX += WorldX * WorldX;
					SumYY += WorldY * WorldY;
					SumXY += WorldX * WorldY;
					++FloorCount;
				}
			}
			Density[SampleIndex(X, Y, Above - 1)] = BelowValue;
			if (BelowValue > 0.f)
			{
				Lowest = FMath::Min(Lowest, Above - 1);
			}
			for (int32 Z = 0; Z < Above - 1; ++Z)
			{
				Density[SampleIndex(X, Y, Z)] = 0.f;
			}
		}
	}
	if (Lowest < Dims.Z)
	{
		TouchedMin.Z = FMath::Max(FMath::Min(TouchedMin.Z, Lowest), 0);
	}
	if (bCollectFloorFootprint && FloorCount > 0)
	{
		const double Inv = 1.0 / double(FloorCount);
		const double MeanX = SumX * Inv;
		const double MeanY = SumY * Inv;
		BodyFloorFootprint.Count = FloorCount;
		BodyFloorFootprint.Mean = FVector2D(MeanX, MeanY);
		BodyFloorFootprint.Cxx = FMath::Max(SumXX * Inv - MeanX * MeanX, 0.0);
		BodyFloorFootprint.Cyy = FMath::Max(SumYY * Inv - MeanY * MeanY, 0.0);
		BodyFloorFootprint.Cxy = SumXY * Inv - MeanX * MeanY;
		BodyFloorFootprint.CellSize = Cell;
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
			Colors[LiveVertexCount] = CurrentClusterColor;
			if (CurrentClusterColor.R == 0.f && Slot % 3 == 2)
			{
				// Keep one integer slot per triangle: interpolated tags could select an unrelated mini.
				const FVector Point = (Vertices[LiveVertexCount] + Vertices[LiveVertexCount - 1] + Vertices[LiveVertexCount - 2]) / 3.0 - FVector(0, 0, VisualZLift);
				double Best = FVector::DistSquared(Point, ConnectedBodyCenter);
				FLinearColor Tag = CurrentClusterColor;
				for (const auto& Connected : ConnectedShotCenters)
				{
					const double Distance = FVector::DistSquared(Point, Connected.Value);
					const int32 FaceSlot = ShotSlotIds.IndexOfByKey(Connected.Key);
					const FVector Outward=(Connected.Value-ConnectedBodyCenter).GetSafeNormal();
     const double Along=(Point-Connected.Value)|Outward;
     const bool bMiniLobe=Along>-ConnectedShotRadius*0.45f && Distance<FMath::Square(ConnectedShotRadius*1.5f);
     if (Distance < Best && FaceSlot != INDEX_NONE && bMiniLobe)
					{
						Best = Distance;
						Tag.R = float(FaceSlot + 1) / 255.f;
					}
				}
    // G masks faces on the visual neck only; R still selects the material shell.
    if (Tag.R==0.f)
    {
     for (const FVisualNeck& Neck : VisualNecks)
     {
      const FVector Segment=Neck.End-Neck.Start;
      const double T=FVector::DotProduct(Point-Neck.Start,Segment)/FMath::Max(Segment.SizeSquared(),1.0);
      if (T>0.1 && T<0.85 && FVector::DistSquared(Point,Neck.Start+Segment*T)<FMath::Square(Neck.Radius+ActiveCellSize)) Tag.G=1.f;
     }
    }
    Colors[LiveVertexCount] = Colors[LiveVertexCount - 1] = Colors[LiveVertexCount - 2] = Tag;
			}

			++LiveVertexCount;
		}
	}
}
