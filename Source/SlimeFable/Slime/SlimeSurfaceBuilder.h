// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeTypes.h"

/**
 *  Turns the particle set into a triangle soup with marching cubes.
 *
 *  Body and ballistic fragments each get their own grid so a distant Q chunk cannot
 *  coarsen or clip the main blob. Particle AABBs are expanded by splat reach plus a thin
 *  blur shell. Body grid-origin lead-snaps on the negative axes (avoids walk holes) and
 *  EMA-trails only when Desired is ahead so Dims stay small. Cell size follows bounds with
 *  hysteresis; if the span would exceed MaxGridDim the cell grows instead of cropping.
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

	/** Rebuilds the surface (body cluster, then free-flying fragment clusters). */
	void Build(const TArray<SlimeSim::FSlimeParticle>& Particles, const FVector& DegenerateAnchor);

	/**
	 *  Same as Build, but ShotIds in MergingShotIds are splatted into the body density field
	 *  (metaball fusion) instead of getting their own cluster.
	 *  VisualZLift raises body splats so a visual-only scale stays glued to the floor.
	 *  ClipFloorZ (world Z) zeros density below the plane after blur; pass a very low value to skip.
	 */
	void Build(const TArray<SlimeSim::FSlimeParticle>& Particles, const FVector& DegenerateAnchor, const TArray<uint8>& MergingShotIds, float InVisualZLift = 0.f, float InClipFloorZ = -1.e9f);

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
	void BuildCluster(const TArray<SlimeSim::FSlimeParticle>& Particles, bool bBallisticSubset, const FBox& Bounds, uint8 ShotFilter = 0);
	void PrepareGrid(const FBox& Bounds, bool bBodyCluster);
	/** ShotFilter selects one flying shot; MergingShots (when non-null) are included in the body splat. */
	void SplatDensity(const TArray<SlimeSim::FSlimeParticle>& Particles, bool bBallisticSubset, uint8 ShotFilter, const TSet<uint8>* MergingShots);
	void BlurDensity();
	void ClipDensityBelowFloor();
	void Triangulate();

	TSet<uint8> ActiveMergingShots;

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

	/** Density at a fractional grid coordinate. Avoids the stair-step normals of nearest-cell sampling. */
	FORCEINLINE float SampleTrilinear(float X, float Y, float Z) const
	{
		const int32 X0 = FMath::FloorToInt(X);
		const int32 Y0 = FMath::FloorToInt(Y);
		const int32 Z0 = FMath::FloorToInt(Z);
		const float Tx = X - float(X0);
		const float Ty = Y - float(Y0);
		const float Tz = Z - float(Z0);

		const float C000 = SampleAt(X0, Y0, Z0);
		const float C100 = SampleAt(X0 + 1, Y0, Z0);
		const float C010 = SampleAt(X0, Y0 + 1, Z0);
		const float C110 = SampleAt(X0 + 1, Y0 + 1, Z0);
		const float C001 = SampleAt(X0, Y0, Z0 + 1);
		const float C101 = SampleAt(X0 + 1, Y0, Z0 + 1);
		const float C011 = SampleAt(X0, Y0 + 1, Z0 + 1);
		const float C111 = SampleAt(X0 + 1, Y0 + 1, Z0 + 1);

		const float C00 = FMath::Lerp(C000, C100, Tx);
		const float C10 = FMath::Lerp(C010, C110, Tx);
		const float C01 = FMath::Lerp(C001, C101, Tx);
		const float C11 = FMath::Lerp(C011, C111, Tx);
		return FMath::Lerp(FMath::Lerp(C00, C10, Ty), FMath::Lerp(C01, C11, Ty), Tz);
	}

	/** Central-difference gradient in grid cells. Density rises inwards, so this points outward. */
	FORCEINLINE FVector SampleGradient(float X, float Y, float Z) const
	{
		return FVector(
			SampleTrilinear(X - 1.f, Y, Z) - SampleTrilinear(X + 1.f, Y, Z),
			SampleTrilinear(X, Y - 1.f, Z) - SampleTrilinear(X, Y + 1.f, Z),
			SampleTrilinear(X, Y, Z - 1.f) - SampleTrilinear(X, Y, Z + 1.f));
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
	float SplatZScale = 1.f;
	float InvInteriorValue = 1.f;
	float VisualZLift = 0.f;
	float ClipFloorZ = -1.e9f;
	bool bClipFloorThisCluster = false;

	/** Truncation coarsening multiplier (body cluster). */
	float CellScale = 1.f;
	int32 TruncationStreak = 0;

	/** Hysteresis on the AABB-driven cell requirement. */
	float HeldRequiredCell = 0.f;
	int32 RequiredGrowStreak = 0;

	/** EMA of body bounds; snaps on fast movement. */
	FBox SmoothedBodyBounds = FBox(ForceInit);
	bool bHaveSmoothedBodyBounds = false;

	/** EMA of body grid origin to damp cell-boundary flicker. */
	FVector SmoothedGridOrigin = FVector::ZeroVector;
	bool bHaveSmoothedGridOrigin = false;

	int32 LiveVertexCount = 0;
	bool bTruncated = false;

	static const int32 TriangleTable[256][16];
	static const int32 EdgeCorners[12][2];
	static const FIntVector CornerOffsets[8];
};
