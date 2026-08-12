// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeTypes.h"

/**
 *  Position Based Fluids solver, trimmed down for a character sized blob.
 *
 *  Works directly in world space centimetres so there is no simulation-space conversion to
 *  get wrong. Cohesion comes from two cheap terms rather than a high resolution fluid:
 *  a spring on the centre of mass that follows the capsule anchor, and a membrane that pulls
 *  stray particles back inside RestRadius. Incompressibility from the density constraint is
 *  what makes the blob bulge sideways when a wall squeezes it.
 *
 *  Not a UObject: no reflection overhead on the hot loops. Owned by USlimeBodyComponent.
 */
class SLIMEFABLE_API FSlimeSolver
{
public:
	/** Builds the particle set as a dome resting on RestCenter's Z. */
	void Initialize(const FSlimeSolverParams& InParams, const FVector& RestCenter);

	/** Re-applies tunables. Reinitialises the particle set only if the budget changed. */
	void SetParams(const FSlimeSolverParams& InParams);

	const FSlimeSolverParams& GetParams() const { return Params; }

	/** Rebuilds the dome around RestCenter and clears every transient state. */
	void Reset(const FVector& RestCenter);

	void Step(float Dt);

	// ---- Per step inputs -------------------------------------------------------------

	/** Capsule anchor the centre of mass is sprung towards. */
	void SetAnchor(const FVector& InCenter, const FVector& InVelocity);

	/** Hard floor plane for the attached body. Particles never go below this. */
	void SetFloorZ(float InFloorZ) { FloorZ = float(InFloorZ); }

	/** Floor under the launched chunk COM. Ballistic particles use this instead of FloorZ. */
	void SetFragmentFloorZ(float InFloorZ) { FragmentFloorZ = float(InFloorZ); }

	/** Hard ceiling plane, or a large value when the sky is clear. */
	void SetCeilingZ(float InCeilingZ) { CeilingZ = float(InCeilingZ); }

	/** World primitives baked into solver space. Consumed by move. */
	void SetColliders(TArray<SlimeSim::FSlimeCollider>&& InColliders) { Colliders = MoveTemp(InColliders); }

	/** Pancake mode: widen the membrane and push outwards along the ground plane. */
	void SetSpread(bool bInSpread, float InSpreadRadius, float InSpreadPush, float InSpreadHalfHeight);

	/** Light air "duang" on double jump — far milder than landing squash. */
	void ApplyAirBounce();

	/** 0 = free, 1 = crushed. Stiffens cohesion so a gap cannot tear the body in half. */
	void SetSqueeze(float InAmount, const FVector& InFreeDirection);

	/** Recall passes through geometry so fragments never get stuck behind a wall. */
	void SetSkipWorldCollision(bool bInSkip) { bSkipWorldCollision = bInSkip; }

	/** Extra gravity multiplier, used while spreading. */
	void SetGravityScale(float InScale) { GravityScale = InScale; }

	/**
	 *  Impact response: kills upward spray and arms a short settle window with boosted
	 *  cohesion. Never loosens the hard tether.
	 */
	void ApplyLandingSquash(float ImpactSpeed);

	/** Seconds of landing settle remaining (driven down inside Step). */
	float GetLandingSettleRemaining() const { return LandingSettleRemaining; }

	// ---- Queries ---------------------------------------------------------------------

	const TArray<SlimeSim::FSlimeParticle>& GetParticles() const { return Particles; }

	/** Centre of mass of the attached body, ignoring fragments in flight. */
	FVector GetBodyCenter() const;

	/** Bounds over every particle, fragments included. */
	FBox GetBounds() const;

	/** Bounds over the attached body only. */
	FBox GetBodyBounds() const;

	/** Centre of mass of ballistic fragments only. Returns false when none are flying. */
	bool GetFragmentCenter(FVector& OutCenter) const;

	/** Bounds over ballistic fragments only. */
	FBox GetFragmentBounds() const;

	int32 GetNumBallistic() const { return NumBallistic; }

	bool HasFragments() const { return NumBallistic > 0; }

	/** Average penetration depth resolved last step; a cheap read on how hard it is pinched. */
	float GetContactLoad() const { return ContactLoad; }

	// ---- Abilities -------------------------------------------------------------------

	/**
	 *  Detaches the non-core particles closest to the aim direction and throws them.
	 *  Returns how many were launched, honouring MinRemaining.
	 */
	int32 LaunchChunk(const FVector& LaunchVelocity, float Fraction, int32 MinRemaining, float Life);

	/** Steers fragments home. Returns true once every one of them has rejoined. */
	bool RecallFragments(float Dt, const FVector& Target, float PullSpeed);

	/** Teleports every fragment back into the body. */
	void SnapFragmentsHome(const FVector& Target);

	/**
	 *  Clears ballistic on fragments whose COM is within MergeRadius of the body COM.
	 *  Returns how many particles were absorbed. Skipped while world collision is gated for recall.
	 */
	int32 AbsorbNearbyFragments(float MergeRadius);

private:
	void RebuildDerived();
	void BuildDome(const FVector& RestCenter);
	void BuildGrid();
	void SolveDensity();
	void ResolveCollisions();
	void ApplyViscosity();
	float ComputeLatticeRestDensity() const;

	/** Updates inertia axes from anchor velocity; returns unit forward (XY) or zero. */
	FVector3f UpdateInertiaShape(float Dt);

	/**
	 *  Hard-projects a non-ballistic particle into the current single-blob shell
	 *  (inertia ellipsoid, or flat disk while spread).
	 */
	void ClampToBodyShell(FVector3f& InOutPoint, const FVector3f& Center) const;

	/** Projects a point out of one primitive. Returns true on contact. */
	static bool ProjectOut(const SlimeSim::FSlimeCollider& Collider, float Skin, FVector3f& InOutPoint, FVector3f& OutNormal);

	FSlimeSolverParams Params;

	TArray<SlimeSim::FSlimeParticle> Particles;

	// Reused scratch buffers. Nothing here is allocated per step.
	TArray<float> Lambdas;
	TArray<float> ContactLoads;
	TArray<FVector3f> DeltaPositions;
	TArray<FVector3f> ContactNormals;
	TArray<FVector3f> ViscosityDelta;
	TArray<int32> CellStart;
	TArray<int32> CellCursor;
	TArray<int32> CellEntries;
	TArray<int32> ParticleCell;

	// Uniform grid over the current bounds. Cell size is never smaller than the kernel
	// radius, so the 3x3x3 neighbourhood always covers it.
	FVector3f GridOrigin = FVector3f::ZeroVector;
	FIntVector GridDims = FIntVector(1);
	float GridCellSize = 1.f;

	// Derived kernel constants.
	float SmoothingRadius = 1.f;
	float SmoothingRadiusSq = 1.f;
	float Poly6Norm = 1.f;
	float SpikyGradNorm = 1.f;
	float RestDensity = 1.f;
	float ArtificialPressureDenom = 1.f;
	float ContactRadius = 1.f;

	// Per step inputs.
	FVector3f AnchorCenter = FVector3f::ZeroVector;
	FVector3f AnchorVelocity = FVector3f::ZeroVector;
	FVector3f SqueezeFreeDirection = FVector3f::ZeroVector;
	float FloorZ = -1.e9f;
	float FragmentFloorZ = -1.e9f;
	float CeilingZ = 1.e9f;
	float SqueezeAmount = 0.f;
	float SpreadRadius = 0.f;
	float SpreadPush = 0.f;
	float SpreadHalfHeight = 2.5f;
	float GravityScale = 1.f;
	bool bSpread = false;
	bool bSkipWorldCollision = false;

	TArray<SlimeSim::FSlimeCollider> Colliders;

	int32 NumBallistic = 0;
	float ContactLoad = 0.f;

	/** Smoothed horizontal move direction for inertia deformation. */
	FVector3f InertiaForward = FVector3f::ForwardVector;
	float InertiaAmount = 0.f;
	FVector3f PrevAnchorVelocity = FVector3f::ZeroVector;

	/** Ellipsoid half-axes in the frame (Forward, Right, Up), cm. */
	FVector3f ShellAxes = FVector3f(27.f, 27.f, 27.f);
	/** Shift of the hard shell centre behind COM along -Forward (inertia trail). */
	float ShellBackShift = 0.f;

	float LandingSettleRemaining = 0.f;
	float LandingSettleDuration = 2.5f;
};
