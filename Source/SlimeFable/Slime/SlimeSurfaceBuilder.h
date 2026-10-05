// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeTypes.h"

/**
 *  Snapshot of the body cluster density grid taken right after it was triangulated, so the
 *  volumetric skin can ray march the exact field the mesh came from. World position of sample
 *  (x, y, z) is Origin + (x, y, z) * CellSize. Dims.X == 0 means no body this frame.
 */
struct FSlimeBodyField
{
	TArray<float> Density;
	FIntVector Dims = FIntVector::ZeroValue;
	FVector Origin = FVector::ZeroVector;
	float CellSize = 1.f;
	float Iso = 0.2f;

	bool IsValid() const { return Dims.X > 0 && Density.Num() >= Dims.X * Dims.Y * Dims.Z; }
};

/**
 *  Columns of the body density grid whose iso surface crosses the floor clip.
 *  Mean and the second moments are in world centimetres on the XY plane, so the trail can
 *  size a puddle from the mesh people actually see (bell flare included) rather than the particles.
 *  Count < 6 means there was no floor clip this build.
 */
struct FSlimeFloorFootprint
{
	int32 Count = 0;
	FVector2D Mean = FVector2D::ZeroVector;
	double Cxx = 0.0;
	double Cyy = 0.0;
	double Cxy = 0.0;
	float CellSize = 1.f;

	bool IsValid() const { return Count >= 6; }
};

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
	 *  ShotClipFloors clips each ballistic cluster at its own traced floor (sentinels omitted).
	 */
	void Build(const TArray<SlimeSim::FSlimeParticle>& Particles, const FVector& DegenerateAnchor, const TArray<uint8>& MergingShotIds, float InVisualZLift = 0.f, float InClipFloorZ = -1.e9f);
	void Build(const TArray<SlimeSim::FSlimeParticle>& Particles, const FVector& DegenerateAnchor, const TArray<uint8>& MergingShotIds, float InVisualZLift, float InClipFloorZ, const TMap<uint8, float>& InShotClipFloors);

	/**
	 *  Shader shot slots for the next Build: slot i (ShotId = Ids[i]) tags its cluster's vertices with
	 *  colour R = (i + 1) / 255; the body and any shot without a slot get R = 0. The body material uses
	 *  this to pick the per-cluster shell ellipsoid instead of the whole-component ObjectBounds.
	 */
	struct FVisualNeck { FVector Start, End; float Radius; uint8 ShotId; };
	void SetVisualNecks(const TArray<FVisualNeck>& Necks) { VisualNecks = Necks; }
	void SetConnectedShotRadius(float Radius) { ConnectedShotRadius = Radius; }
	void SetShotSlotIds(const TArray<uint8>& Ids) { ShotSlotIds = Ids; }

	/**
	 *  Ground skirt for the next Build: when the body cluster is clipped at a floor, density in the
	 *  slices within Height (cm) above it is dilated outwards by up to Spread (cm), fading to zero
	 *  at Height, so the iso surface flares into a meniscus. Zero disables it.
	 */
	void SetGroundSkirt(float InHeight, float InSpread) { SkirtHeight = FMath::Max(InHeight, 0.f); SkirtSpread = FMath::Max(InSpread, 0.f); }

	/**
	 *  Dome bell flare for the next Build: within HeightFraction of the body's height above the floor,
	 *  each slice is pushed out by Reach * (1-t)^Curve cm, so the base swings out from mid-body into a
	 *  bell. Tip rounds the contact edge. Reach 0 disables it.
	 */
	void SetBellFlare(float InHeightFraction, float InReach, float InCurve, float InTip)
	{
		FlareHeightFraction = FMath::Clamp(InHeightFraction, 0.f, 1.f);
		FlareReach = FMath::Max(InReach, 0.f);
		FlareCurve = FMath::Max(InCurve, 0.5f);
		FlareTip = FMath::Max(InTip, 0.f);
	}

	/**
	 *  Spread sheet for the next Build: the body cluster becomes a volume-conserving heightfield.
	 *  Each body particle carries Volume / N and spreads it with a normalised 2D kernel of radius
	 *  KernelRadius, so thickness h(x,y) is thick in the middle and thin at the rim. The sheet sits
	 *  on the weighted particle Z minus BaseOffset; samples thinner than MinThickness are outside.
	 *  Particles more than DrapeDepth below their column base keep ordinary 3D splats (drips over
	 *  edges). Blend lerps the ordinary field toward the sheet field; zero disables it.
	 */
	void SetSheetMode(float InBlend, float InVolume, float InKernelRadius, float InMinThickness, float InDrapeDepth, float InBaseOffset)
	{
		SheetBlend = FMath::Clamp(InBlend, 0.f, 1.f);
		SheetVolume = FMath::Max(InVolume, 0.f);
		SheetKernelRadius = FMath::Max(InKernelRadius, 0.5f);
		SheetMinThickness = FMath::Max(InMinThickness, 0.f);
		SheetDrapeDepth = FMath::Max(InDrapeDepth, 0.f);
		SheetBaseOffset = InBaseOffset;
	}

	/** World space positions, MaxVertices long. */
	const TArray<FVector>& GetVertices() const { return Vertices; }
	const TArray<FVector>& GetNormals() const { return Normals; }
	/** Per-vertex cluster id (see SetShotSlotIds), MaxVertices long. */
	const TArray<FLinearColor>& GetColors() const { return Colors; }

	/** Static 0..MaxVertices-1 soup, built once. */
	const TArray<int32>& GetIndices() const { return Indices; }

	int32 GetLiveVertexCount() const { return LiveVertexCount; }

	/** True when marching cubes wanted more room than the vertex budget allows. */
	bool WasTruncated() const { return bTruncated; }

	bool IsConfigured() const { return Indices.Num() > 0; }

	const FSlimeSurfaceParams& GetParams() const { return Params; }
	float GetParticleSpacing() const { return ParticleSpacing; }

	/** Off by default: only the volumetric skin pays for the extra density copy. */
	void SetCaptureBodyField(bool bCapture);
	bool IsCapturingBodyField() const { return bCaptureBodyField; }

	/** Body density snapshot from the last Build. Check IsValid(); empty when capture is off. */
	const FSlimeBodyField& GetBodyField() const { return BodyField; }

	/** Floor-crossing columns of the body cluster from the last Build. Invalid when nothing was clipped. */
	const FSlimeFloorFootprint& GetBodyFloorFootprint() const { return BodyFloorFootprint; }

private:
	void BuildCluster(const TArray<SlimeSim::FSlimeParticle>& Particles, bool bBallisticSubset, const FBox& Bounds, uint8 ShotFilter = 0);
	void CaptureBodyField();
	void PrepareGrid(const FBox& Bounds, bool bBodyCluster);
	/** ShotFilter selects one flying shot; MergingShots (when non-null) are included in the body splat. */
	void SplatDensity(const TArray<SlimeSim::FSlimeParticle>& Particles, bool bBallisticSubset, uint8 ShotFilter, const TSet<uint8>* MergingShots);
	void BlurDensity();
	void ApplyGroundSkirt();
	void ApplyBellFlare();
	void ClipDensityBelowFloor();
	void Triangulate();
	/** Fills SheetDrapeMask and the per-column thickness / base sums for the body cluster. */
	void BuildSheetColumns(const TArray<SlimeSim::FSlimeParticle>& Particles);
	/** Density = lerp(Density, max(SheetDrapeField, heightfield), SheetBlend). */
	void ApplySheetField();

	bool HasGroundSkirt() const { return SkirtHeight > 0.f && SkirtSpread > 0.f; }
	bool HasBellFlare() const { return FlareReach > 0.05f && FlareHeightFraction > 0.01f; }
	bool HasSheet() const { return SheetBlend > 0.001f && SheetVolume > 0.f; }

	float SheetBlend = 0.f;
	float SheetVolume = 0.f;
	float SheetKernelRadius = 8.f;
	float SheetMinThickness = 0.3f;
	float SheetDrapeDepth = 6.f;
	float SheetBaseOffset = 0.f;
	/** Per particle: 1 = ordinary 3D splat (drape / merging shot), 0 = folded into the sheet. */
	TArray<uint8> SheetDrapeMask;
	/** When set, the body splat only draws particles whose mask entry is non-zero. */
	const TArray<uint8>* SplatMask = nullptr;
	TArray<float> SheetColumnWeight;
	TArray<float> SheetColumnZ;
	TArray<float> SheetColumnHeight;
	TArray<float> SheetDrapeField;

	TSet<uint8> ActiveMergingShots;
	TArray<FVisualNeck> VisualNecks;
	TMap<uint8, FVector> ConnectedShotCenters;
	float ConnectedShotRadius = 18.f;
	FVector ConnectedBodyCenter = FVector::ZeroVector;

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
	TArray<FLinearColor> Colors;
	TArray<int32> Indices;

	TArray<uint8> ShotSlotIds;
	/** Colour written for every vertex of the cluster currently being triangulated. */
	FLinearColor CurrentClusterColor = FLinearColor(0.f, 0.f, 0.f, 1.f);

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
	float BodyClipFloorZ = -1.e9f;
	bool bClipFloorThisCluster = false;
	/** Body cluster only: ClipDensityBelowFloor records the floor-crossing columns. */
	bool bCollectFloorFootprint = false;
	FSlimeFloorFootprint BodyFloorFootprint;
	TMap<uint8, float> ShotClipFloors;
	float SkirtHeight = 0.f;
	float SkirtSpread = 0.f;
	float FlareHeightFraction = 0.f;
	float FlareReach = 0.f;
	float FlareCurve = 2.f;
	float FlareTip = 0.f;

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

	FSlimeBodyField BodyField;
	bool bCaptureBodyField = false;

	static const int32 TriangleTable[256][16];
	static const int32 EdgeCorners[12][2];
	static const FIntVector CornerOffsets[8];
};
