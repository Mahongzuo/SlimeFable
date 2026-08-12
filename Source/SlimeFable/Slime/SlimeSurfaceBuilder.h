// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeTypes.h"

/**
 *  Turns the particle set into a triangle soup with marching cubes.
 *
 *  Body and ballistic fragments each get their own grid so a distant Q chunk cannot
 *  coarsen or clip the main blob. Active cell size follows body bounds with hysteresis
 *  so walking does not flicker while spreads/jumps still fit the volume.
 *
 *  Output buffers are always exactly MaxVertices long. Unused slots collapse onto a single
 *  point so their triangles have zero area, which lets the render section be updated in place
 *  rather than recreated every time the topology changes.
 */
class SLIMEFABLE_API FSlimeSurfaceBuilder
{
public:
	/** Sizes the fixed buffers and caches derived constants. Call whenever params change. */
	void Configure(const FSlimeSurfaceParams& InParams, float InParticleSpacing);

	/** Rebuilds the surface (body cluster, then fragment cluster if any). */
	void Build(const TArray<SlimeSim::FSlimeParticle>& Particles, const FVector& DegenerateAnchor);

	/** World space positions, MaxVertices long. */
	const TArray<FVector>& GetVertices() const { return Vertices; }
	const TArray<FVector>& GetNormals() const { return Normals; }

	/** Static 0..MaxVertices-1 soup, built once. */
	const TArray<int32>& GetIndices() const { return Indices; }

	int32 GetLiveVertexCount() const { return LiveVertexCount; }

	/** True when marching cubes wanted more room than the vertex budget allows. */
	bool WasTruncated() const { return bTruncated; }

	bool IsConfigured() const { return Indices.Num() > 0; }

	const FSlimeSurfaceParams& GetParams() const { return Params; }
	float GetParticleSpacing() const { return ParticleSpacing; }

private:
	void BuildCluster(const TArray<SlimeSim::FSlimeParticle>& Particles, bool bBallisticSubset, const FBox& Bounds);
	void PrepareGrid(const FBox& Bounds, bool bBodyCluster);
	void SplatDensity(const TArray<SlimeSim::FSlimeParticle>& Particles, bool bBallisticSubset);
	void BlurDensity();
	void Triangulate();

	FORCEINLINE int32 SampleIndex(int32 X, int32 Y, int32 Z) const
	{
		return X + Dims.X * (Y + Dims.Y * Z);
	}

	FORCEINLINE float SampleAt(int32 X, int32 Y, int32 Z) const
	{
		X = FMath::Clamp(X, 0, Dims.X - 1);
		Y = FMath::Clamp(Y, 0, Dims.Y - 1);
		Z = FMath::Clamp(Z, 0, Dims.Z - 1);
		return Density[SampleIndex(X, Y, Z)];
	}

	FSlimeSurfaceParams Params;
	float ParticleSpacing = 4.6f;

	TArray<float> Density;
	TArray<float> DensityScratch;

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<int32> Indices;

	FVector GridOrigin = FVector::ZeroVector;
	FIntVector Dims = FIntVector(1);
	FIntVector TouchedMin = FIntVector(0);
	FIntVector TouchedMax = FIntVector(0);
	float CellSize = 4.6f;
	float ActiveCellSize = 4.6f;
	float SplatRadius = 8.5f;
	float InvInteriorValue = 1.f;

	/** Truncation coarsening multiplier (body cluster). */
	float CellScale = 1.f;
	int32 TruncationStreak = 0;

	/** Hysteresis on the AABB-driven cell requirement. */
	float HeldRequiredCell = 0.f;
	int32 RequiredGrowStreak = 0;

	/** EMA of body bounds; snaps on fast movement. */
	FBox SmoothedBodyBounds = FBox(ForceInit);
	bool bHaveSmoothedBodyBounds = false;

	int32 LiveVertexCount = 0;
	bool bTruncated = false;

	static const int32 TriangleTable[256][16];
	static const int32 EdgeCorners[12][2];
	static const FIntVector CornerOffsets[8];
};
