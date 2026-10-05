// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
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
	enum class EShotPhase : uint8 { Separating, Flying, Active, Returning, Merging };

	struct FShotState
	{
		uint8 Id = 0;
		EShotPhase Phase = EShotPhase::Active;
		FVector3f LaunchDirection = FVector3f::ForwardVector;
		float Age = 0.f;
		float RemainingTime = 0.f;
		FVector AimTarget = FVector::ZeroVector, InitialVelocity = FVector::ZeroVector;
		float FlightGravity = 0.f, FlightTime = 0.f;
		FVector3f Center = FVector3f::ZeroVector;
		FVector3f Velocity = FVector3f::ZeroVector;
		float FloorZ = -1.e9f;
  FVector SupportPoint=FVector::ZeroVector, SupportNormal=FVector::UpVector;
  bool bHasSupport=false, bGrounded=false;
  bool bUmbrellaOpen=false, bKeepUntilMerged=false;
  uint32 LandingEvent=0;
		float MergeElapsed = -1.f;
		bool bImpactApplied = false;
		int32 Count = 0;
	};

	struct FKinematicShotMotion
	{
		uint8 Id = 0;
		FVector PrevCenter = FVector::ZeroVector;
		FVector Center = FVector::ZeroVector;
		float Radius = 18.f;
		bool bCannon = false;
	};

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

	/**
	 * Local ground heights used while spread (and during the recover after).
	 * Origin is the world XY of cell (0,0)'s min corner. A height <= -1e8 is a missed trace.
	 * Cells reachable from FootXY without dropping more than StepHeight are "supported";
	 * every other cell stores its nearest supported cell so the sheet can hang off an edge
	 * but stay attached to it. DrapeReferenceZ is the standing floor under the capsule.
	 */
	void SetGroundField(const FVector2f& InOrigin, float InCellSize, int32 InDimX, int32 InDimY, TArray<float>&& InHeights,
		float InDrapeReferenceZ, const FVector2f& FootXY, float InStepHeight);
	void ClearGroundField();
	bool HasGroundField() const { return bGroundField; }

	/**
	 * Goo overhang tuning, fed every step.
	 * MaxDepth: hard limit below the support edge. Tension (1/s^2) pulls hanging goo back up in
	 * proportion to its depth; Viscosity (1/s) damps it so it oozes. MaxOverhang: how far the sheet
	 * may reach horizontally past a supported cell.
	 */
	void SetDrapeParams(float InMaxDepth, float InTension, float InViscosity, float InMaxOverhang);

	/** 0 = dome, 1 = fully flattened. Blends shell height, vertical anchor and upward restore. */
	void SetSpreadBlend(float InBlend) { SpreadBlend = FMath::Clamp(InBlend, 0.f, 1.f); }

	/**
	 * Resting Dome silhouette (ground state only). Blend 0 = ball, 1 = dome: the shell's horizontal
	 * axes scale by WidthScale, the vertical by HeightScale, and upward restore by RestoreScale.
	 */
	void SetDomeShape(float InBlend, float InWidthScale, float InHeightScale, float InRestoreScale)
	{
		DomeBlend = FMath::Clamp(InBlend, 0.f, 1.f);
		DomeWidthScale = FMath::Max(InWidthScale, 0.5f);
		DomeHeightScale = FMath::Clamp(InHeightScale, 0.2f, 1.5f);
		DomeRestoreScale = FMath::Clamp(InRestoreScale, 0.f, 1.f);
	}

	/**
	 * Horizontal metric stretch for the density constraint while spread: the fluid's lateral rest
	 * spacing becomes ParticleSpacing * Scale, so the sheet thins into a wider single layer.
	 */
	void SetSpreadLateralScale(float InScale) { SpreadLateralScale = FMath::Clamp(InScale, 1.f, 4.f); }

	/** Fallback floor when a shot has no per-shot trace yet. */
	void SetFragmentFloorZ(float InFloorZ) { FragmentFloorZ = float(InFloorZ); }

	/** Per-shot floor plane for ballistic clones. */
	void SetShotFloorZ(uint8 ShotId, float InFloorZ);

	void ClearShotFloorOverrides();
 float GetShotSupportHeight() const { return FMath::Max(MiniMembraneRadius, Params.ParticleSpacing * 2.f) * 0.55f; }
 void SetShotSupport(uint8 Id, const FHitResult* Hit);
 TFunction<bool(const FVector&, FHitResult&)> QueryGround;
 TFunction<bool(const FVector&, FHitResult&)> QueryUmbrellaGround;
 void ConfigureUmbrellas(float Height,float Follow,float Fall,float MaxFall)
 { UmbrellaHeight=FMath::Max(Height,0.f); UmbrellaFollowSpeed=FMath::Max(Follow,10.f); UmbrellaFallSpeed=FMath::Max(Fall,30.f); UmbrellaMaxFallSpeed=FMath::Max(MaxFall,UmbrellaFallSpeed); }
 void RemoveUnprotectedShots();
 TFunction<bool(const FVector&, const FVector&, float, FHitResult&)> TraceSeparation;

	/** Hard ceiling plane, or a large value when the sky is clear. */
	void SetCeilingZ(float InCeilingZ) { CeilingZ = float(InCeilingZ); }

	/** World primitives baked into solver space. Consumed by move. */
	void SetColliders(TArray<SlimeSim::FSlimeCollider>&& InColliders) { Colliders = MoveTemp(InColliders); }

	/** Pancake mode: widen the membrane and push outwards along the ground plane. */
	void SetSpread(bool bInSpread, float InSpreadRadius, float InSpreadPush, float InSpreadHalfHeight);

	/**
	 *  Wall cling: treat the wall as a floor analogue so the blob sits as a mild hemisphere
	 *  on the surface. Not pancake spread.
	 */
	void SetClingPlane(bool bInCling, const FVector& InPoint, const FVector& InNormal);

	/** Light air "duang" on double jump — far milder than landing squash. */
	void ApplyAirBounce();

	/** 0 = free, 1 = crushed. Stiffens cohesion so a gap cannot tear the body in half. */
	void SetSqueeze(float InAmount, const FVector& InFreeDirection);

	/** Recall passes through geometry so fragments never get stuck behind a wall. */
	void SetSkipWorldCollision(bool bInSkip) { bSkipWorldCollision = bInSkip; }

	/** Extra gravity multiplier, used while spreading. */
	void SetGravityScale(float InScale) { GravityScale = InScale; }

	/** Mini-slime membrane radius from launch particle fraction (cbrt volume scale). */
	void SetLaunchFraction(float Fraction);

	/**
	 *  Impact response: kills upward spray and arms a short settle window with boosted
	 *  cohesion. Never loosens the hard tether.
	 */
	void ApplyLandingSquash(float ImpactSpeed);

	/** Seconds of landing settle remaining (driven down inside Step). */
	float GetLandingSettleRemaining() const { return LandingSettleRemaining; }

	/** Combat attack pose overlay. Cleared by the caller when the strike ends. */
	void SetCombatPose(const FSlimeCombatPoseState& InPose) { CombatPose = InPose; }

	const FSlimeCombatPoseState& GetCombatPose() const { return CombatPose; }

	/** Light jolt used for hit reactions — milder than a landing squash. */
	void ApplyHitJolt();

	/**
	 *  Uniform body scale without rebuilding the particle dome.
	 *  Scales attached (non-ballistic) particles about COM, then rebuilds derived kernel
	 *  lengths. Never writes Params.RestRadius / ParticleSpacing.
	 */
	void SetSizeScale(float NewScale);

	float GetSizeScale() const { return SizeScale; }

	/** RestRadius as used by the membrane / shell this step. */
	float GetScaledRestRadius() const { return Params.RestRadius * SizeScale; }

	/** Membrane radius of a launched mini-slime (cbrt of launch volume fraction). */
	float GetMiniMembraneRadius() const { return MiniMembraneRadius; }

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

	/** Snapshot of active clone shots (COM / counts). */
	const TArray<FShotState>& GetShotStates() const { return ShotStates; }

	/** Refresh shot COM cache (call before reading GetShotStates outside Step). */
	void RefreshShotStates() { RebuildShotStates(); }

	/** True while a clone shot is in the soft-merge absorb window. */
	bool IsShotMerging(uint8 ShotId) const;

	/** Fills OutIds with shot ids currently soft-merging. */
	void GetMergingShotIds(TArray<uint8>& OutIds) const;

	/** Fills OutCenters with each active shot COM. */
	void GetShotCenters(TArray<FVector>& OutCenters) const;

	FVector GetShotCenterWorld(uint8 ShotId) const;

	int32 GetNumBallistic() const { return NumBallistic; }

	bool HasFragments() const { return NumBallistic > 0; }

	int32 GetActiveShotCount() const { return ActiveShotCount; }

	/** Average penetration depth resolved last step; a cheap read on how hard it is pinched. */
	float GetContactLoad() const { return ContactLoad; }

	// ---- Shell readback (face / bubble visuals) -----------------------------------------

	/** Hard-shell ellipsoid half-axes in the (Forward, Right, Up) frame, cm. Includes squeeze / spread / landing / combat pose. */
	FVector3f GetShellAxes() const { return ShellAxes; }

	/** Smoothed horizontal move direction (unit XY). Forward axis of the shell frame. */
	FVector3f GetInertiaForward() const { return InertiaForward; }

	/** 0..1 how stretched the shell is along InertiaForward. */
	float GetInertiaAmount() const { return InertiaAmount; }

	/** Shell centre is shifted this far behind COM along -InertiaForward (inertia trail). */
	float GetShellBackShift() const { return ShellBackShift; }

	/** World-space centre of the hard shell (COM minus back shift). */
	FVector GetShellCenter() const
	{
		return GetBodyCenter() - FVector(InertiaForward) * double(ShellBackShift);
	}

	bool IsSpread() const { return bSpread; }
	bool IsClingPlane() const { return bCling; }

	// ---- Abilities -------------------------------------------------------------------

	/**
	 *  Clones ~Fraction of body particles into a ballistic mini-slime without shrinking the body.
	 *  Honours MaxActiveShots. Returns how many clone particles were spawned.
	 */
	int32 LaunchCannon(const FSlimeCannonLaunch& Cannon, float Fraction, float Life, int32 MaxShots);
	int32 LaunchChunk(const FVector& LaunchVelocity, float Fraction, float Life, int32 MaxActiveShots, const FSlimeLaunchPath* Path = nullptr, uint8* OutShotId = nullptr, bool bAutoReturn = false);

	/** Steer a flying clone toward a world point. Call every tick to keep it pinned. */
	void SetShotTarget(uint8 ShotId, const FVector& Target, float PullSpeed);

	void ClearShotTarget(uint8 ShotId);

	void ClearShotTargets();

	/**
	 *  One-shot chase nudge for G fragments: moves clones toward Target without writing
	 *  ShotTargets and without refreshing BallisticLife (so FragmentLifetime still expires).
	 */
	void SteerShot(uint8 ShotId, const FVector& Target, float Speed, float Dt, bool bKeepGrounded = false);

	void AddIgnoreWorldShot(uint8 ShotId);
	void ClearIgnoreWorldShot(uint8 ShotId);
	void ClearIgnoreWorldShots();
	bool IsIgnoreWorldShot(uint8 ShotId) const { return ShotId != 0 && IgnoreWorldShotIds.Contains(ShotId); }

	bool HasShotTargets() const { return ShotTargets.Num() > 0; }

	bool IsShotTargeted(uint8 ShotId) const { return ShotId != 0 && ShotTargets.Contains(ShotId); }

	void ClearKinematicPaths();
	void SetShotLifecycleParams(float Separation, float Return, float Hop) { SeparationSeconds = FMath::Max(Separation, 0.02f); ReturnSeconds = FMath::Max(Return, 0.1f); ReturnHopHeight = FMath::Max(Hop, 0.f); }
	void GetSeparatingShotIds(TArray<uint8>& OutIds) const;
	float GetShotSeparationProgress(uint8 Id) const;
	void GetKinematicShotMotions(TArray<FKinematicShotMotion>& OutMotions) const;
	void SnapKinematicShotTo(uint8 ShotId, const FVector& WorldPoint, const FHitResult* Support = nullptr);

	/** Steers fragments home. Returns true once every clone has entered soft-merge or been removed. */
	bool RecallFragments(float Dt, const FVector& Target, float PullSpeed);

	/** Hard-clears every flying clone (recall timeout). */
	void SnapFragmentsHome(const FVector& Target);

	/**
	 *  Soft-merge: approach → impact duang → hold → destroy.
	 *  Call AFTER Step so contact wobble can land. Returns particles destroyed this tick.
	 */
	int32 UpdateSoftAbsorb(float Dt, float ApproachRadius, float CommitRadius, float HoldDuration);

	/** Concentration multiplier applied while spread (body feeds this each step). */
	void SetSpreadConcentrationScale(float InScale) { SpreadConcentrationScale = FMath::Max(InScale, 0.1f); }

private:
	void RebuildDerived();
	void BuildDome(const FVector& RestCenter);
	void EnsureScratchCapacity(int32 Count);
	void RemoveAllClones();
	void RemoveShotParticles(uint8 ShotId);
	void RecountActiveShots();
	void RebuildShotStates();
	void ApplyMergeImpact(const FShotState& Shot);
	void ClampToShotShell(FVector3f& InOutPoint, const FVector3f& ShotCenter) const;
	void LiftShotCentersAboveFloor();
	void AdvanceKinematicShots(float Dt);
	void AdvanceShotLifecycles(float Dt);
	void ApplyShotTargets(float Dt);
	void EndKinematicShot(uint8 ShotId);
	bool IsShotKinematic(uint8 ShotId) const;
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

	/** Nearest ground-field cell, or FloorZ when the field is off / the point is outside it. */
	float SampleGround(float X, float Y) const;

	/** Ground-field cell index for a world XY, clamped into the grid. INDEX_NONE when the field is off. */
	int32 GroundCellIndex(float X, float Y) const;

	/**
	 * Height step handling: a particle may not move sideways into a cell whose ground is more
	 * than StepHeight above its feet, when it came from a lower cell. Reverts XY in that case.
	 * Returns the ground under the final XY.
	 */
	float ResolveGroundStep(FVector3f& InOutPoint, const FVector3f& FromPoint) const;

	/** Keeps a point within MaxOverhang of its nearest supported cell. */
	void ClampToOverhang(FVector3f& InOutPoint) const;

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
	bool bGroundField = false;
	FVector2f GroundOrigin = FVector2f::ZeroVector;
	float GroundCellSize = 10.f;
	int32 GroundDimX = 0;
	int32 GroundDimY = 0;
	TArray<float> GroundHeights;
	/** Per cell: nearest supported cell centre (itself when supported), its ground height, and the distance to it. */
	TArray<FVector2f> SupportSeeds;
	TArray<float> SupportHeights;
	TArray<float> SupportDistances;
	float DrapeReferenceZ = 0.f;
	float DrapeDepth = 35.f;
	float DrapeTension = 140.f;
	float DrapeViscosity = 90.f;
	float DrapeMaxOverhang = 12.f;
	float GroundStepHeight = 8.f;
	float SpreadBlend = 0.f;
	float FragmentFloorZ = -1.e9f;
	float CeilingZ = 1.e9f;
	float SqueezeAmount = 0.f;
	float SpreadRadius = 0.f;
	float SpreadPush = 0.f;
	float SpreadHalfHeight = 2.5f;
	float SpreadConcentrationScale = 1.15f;
	float SpreadLateralScale = 1.f;
	float DomeBlend = 0.f;
	float DomeWidthScale = 1.4f;
	float DomeHeightScale = 0.55f;
	float DomeRestoreScale = 0.35f;
	float GravityScale = 1.f;
	float MiniMembraneRadius = 18.f;
	float LaunchFractionCached = 0.3f;
	bool bSpread = false;
	bool bSkipWorldCollision = false;
	bool bCling = false;
	FVector3f ClingPoint = FVector3f::ZeroVector;
	FVector3f ClingNormal = FVector3f::ForwardVector;

	TArray<SlimeSim::FSlimeCollider> Colliders;
	TArray<FShotState> ShotStates;
	TMap<uint8, float> ShotFloorOverrides;
 struct FShotSupport
 {
  FVector Point=FVector::ZeroVector, Normal=FVector::UpVector;
  bool bValid=false, bGrounded=false;
  uint32 LandingEvent=0;
 };
 TMap<uint8,FShotSupport> ShotSupports;
	/** Persist merge timers across RebuildShotStates. */
	TMap<uint8, float> ShotMergeElapsed;
	TSet<uint8> ShotImpactApplied;

	struct FShotPathFollow
	{
		FSlimeCannonLaunch Cannon;
		bool bCannon = false;
		FVector FlightStart = FVector::ZeroVector, InitialVelocity = FVector::ZeroVector;
		TArray<FVector> Points;
		float Duration = 0.f;
		float Elapsed = 0.f;
		FVector StartOffset = FVector::ZeroVector;
		FVector PrevCenter = FVector::ZeroVector;
		bool bActive = false;
	};

	static bool SampleShotPath(const FShotPathFollow& Follow, float Time, FVector& OutPos, FVector& OutVel);

	TMap<uint8, FShotPathFollow> ShotPaths;
	struct FShotLifecycle
	{
		float Age = 0.f, Life = 10.f, Separation = 0.22f, ReturnStart = 7.f;
		EShotPhase Phase = EShotPhase::Separating;
		FVector Direction = FVector::ForwardVector, LaunchVelocity = FVector::ZeroVector;
		FVector ReturnOrigin = FVector::ZeroVector;
  FVector SeparationOrigin=FVector::ZeroVector;
  FVector HopStart=FVector::ZeroVector, HopEnd=FVector::ZeroVector;
  int32 HopIndex=-1;
  bool bCancelled=false;
		bool bContactAbsorbArmed = false;
	};
	TMap<uint8, FShotLifecycle> ShotLifecycles;
 struct FShotUmbrellaReturn
 {
  bool bOpen=true;
  FVector ControlCenter=FVector::ZeroVector;
  float HopTime=0.f;
 };
 TMap<uint8,FShotUmbrellaReturn> UmbrellaReturns;
 TSet<uint8> UmbrellaEligibleShots;
 float UmbrellaHeight=120.f, UmbrellaFollowSpeed=420.f, UmbrellaFallSpeed=180.f, UmbrellaMaxFallSpeed=260.f;
 bool TryBeginUmbrellaReturn(uint8 Id);
 void AdvanceUmbrellaReturns(float Dt);
	float SeparationSeconds = 0.22f, ReturnSeconds = 3.f, ReturnHopHeight = 20.f;

	struct FShotTarget
	{
		FVector Location = FVector::ZeroVector;
		float PullSpeed = 2500.f;
	};

	TMap<uint8, FShotTarget> ShotTargets;
	TSet<uint8> IgnoreWorldShotIds;

	int32 NumBallistic = 0;
	int32 ActiveShotCount = 0;
	uint8 NextShotId = 1;
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

	FSlimeCombatPoseState CombatPose;

	/** Visual / membrane size multiplier. 1 = authored RestRadius. */
	float SizeScale = 1.f;
};
