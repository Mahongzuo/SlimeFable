// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeSolver.h"

#include "Async/ParallelFor.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

using namespace SlimeSim;

namespace
{
	/** Cap on grid resolution per axis. Cell size grows instead when the blob spreads out. */
	constexpr int32 GMaxGridDim = 24;

	/** ParallelFor granularity: below this the dispatch costs more than the work. */
	constexpr int32 GParallelMinBatch = 48;

	FORCEINLINE int32 CellIndexOf(const FVector3f& Position, const FVector3f& Origin, float InvCell, const FIntVector& Dims)
	{
		const int32 X = FMath::Clamp(int32((Position.X - Origin.X) * InvCell), 0, Dims.X - 1);
		const int32 Y = FMath::Clamp(int32((Position.Y - Origin.Y) * InvCell), 0, Dims.Y - 1);
		const int32 Z = FMath::Clamp(int32((Position.Z - Origin.Z) * InvCell), 0, Dims.Z - 1);
		return X + Dims.X * (Y + Dims.Y * Z);
	}

	FORCEINLINE FIntVector CellCoordOf(const FVector3f& Position, const FVector3f& Origin, float InvCell, const FIntVector& Dims)
	{
		return FIntVector(
			FMath::Clamp(int32((Position.X - Origin.X) * InvCell), 0, Dims.X - 1),
			FMath::Clamp(int32((Position.Y - Origin.Y) * InvCell), 0, Dims.Y - 1),
			FMath::Clamp(int32((Position.Z - Origin.Z) * InvCell), 0, Dims.Z - 1));
	}
}

void FSlimeSolver::Initialize(const FSlimeSolverParams& InParams, const FVector& RestCenter)
{
	Params = InParams;
	RebuildDerived();
	BuildDome(RestCenter);
}

void FSlimeSolver::SetParams(const FSlimeSolverParams& InParams)
{
	const int32 PreviousCount = Params.NumParticles;
	const float PreviousSpacing = Params.ParticleSpacing;

	Params = InParams;
	RebuildDerived();

	const bool bLayoutChanged =
		PreviousCount != Params.NumParticles ||
		!FMath::IsNearlyEqual(PreviousSpacing, Params.ParticleSpacing);

	if (bLayoutChanged && Particles.Num() > 0)
	{
		BuildDome(GetBodyCenter());
	}
}

void FSlimeSolver::RebuildDerived()
{
	SmoothingRadius = FMath::Max(Params.GetSmoothingRadius(), 1.f);
	SmoothingRadiusSq = SmoothingRadius * SmoothingRadius;

	const float H3 = SmoothingRadius * SmoothingRadius * SmoothingRadius;
	const float H6 = H3 * H3;
	const float H9 = H6 * H3;

	Poly6Norm = 315.f / (64.f * PI * H9);
	SpikyGradNorm = 45.f / (PI * H6);

	RestDensity = FMath::Max(ComputeLatticeRestDensity(), KINDA_SMALL_NUMBER);

	const float DeltaQ = 0.2f * SmoothingRadius;
	ArtificialPressureDenom = FMath::Max(KernelPoly6(DeltaQ * DeltaQ, SmoothingRadiusSq, Poly6Norm), KINDA_SMALL_NUMBER);

	ContactRadius = Params.ParticleSpacing * 0.5f;
}

float FSlimeSolver::ComputeLatticeRestDensity() const
{
	// Density of an ideal infinite lattice at the rest spacing. Calibrating against the actual
	// initial layout instead would bake the dome's surface deficit into the target density.
	const float Spacing = FMath::Max(Params.ParticleSpacing, KINDA_SMALL_NUMBER);
	const int32 Range = FMath::CeilToInt(SmoothingRadius / Spacing);

	float Sum = 0.f;
	for (int32 Z = -Range; Z <= Range; ++Z)
	for (int32 Y = -Range; Y <= Range; ++Y)
	for (int32 X = -Range; X <= Range; ++X)
	{
		const float R2 = float(X * X + Y * Y + Z * Z) * Spacing * Spacing;
		Sum += KernelPoly6(R2, SmoothingRadiusSq, Poly6Norm);
	}
	return Sum;
}

void FSlimeSolver::BuildDome(const FVector& RestCenter)
{
	const int32 Count = FMath::Max(Params.NumParticles, 16);
	const float Spacing = FMath::Max(Params.ParticleSpacing, 1.f);
	const FVector3f Center(RestCenter);

	// Lay a lattice over a generous box, keep the points sitting on or above the floor plane,
	// then take the Count nearest to the centre. Gives an even packing without stacking
	// particles on top of each other, which a random fill would do.
	const int32 Range = FMath::CeilToInt((Params.RestRadius * 1.6f) / Spacing) + 1;

	struct FCandidate
	{
		FVector3f Offset;
		float DistSq;
	};

	TArray<FCandidate> Candidates;
	Candidates.Reserve((2 * Range + 1) * (2 * Range + 1) * (Range + 1));

	for (int32 Z = 0; Z <= 2 * Range; ++Z)
	for (int32 Y = -Range; Y <= Range; ++Y)
	for (int32 X = -Range; X <= Range; ++X)
	{
		// Squash vertically so the natural shape is a dome rather than a ball.
		const FVector3f Offset(X * Spacing, Y * Spacing, Z * Spacing * 0.92f);
		const FVector3f Weighted(Offset.X, Offset.Y, Offset.Z * 1.35f);
		Candidates.Add({ Offset, Weighted.SizeSquared() });
	}

	Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.DistSq < B.DistSq; });

	Particles.SetNum(Count);
	const int32 NumCore = FMath::Clamp(int32(Count * Params.CoreFraction), 1, Count);

	const float BaseZ = Center.Z - Params.RestRadius * 0.45f;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector3f Offset = Candidates.IsValidIndex(Index) ? Candidates[Index].Offset : FVector3f::ZeroVector;

		FSlimeParticle& Particle = Particles[Index];
		Particle.Position = FVector3f(Center.X + Offset.X, Center.Y + Offset.Y, BaseZ + Offset.Z + ContactRadius);
		Particle.PredictedPosition = Particle.Position;
		Particle.Velocity = FVector3f::ZeroVector;
		Particle.BallisticLife = 0.f;
		Particle.ShotId = 0;
		// Candidates are sorted by distance, so the first slice is the innermost shell.
		Particle.Flags = (Index < NumCore) ? PF_Core : PF_None;
	}

	Lambdas.SetNumUninitialized(Count, EAllowShrinking::No);
	ContactLoads.SetNumUninitialized(Count, EAllowShrinking::No);
	DeltaPositions.SetNumUninitialized(Count, EAllowShrinking::No);
	ContactNormals.SetNumUninitialized(Count, EAllowShrinking::No);
	ViscosityDelta.SetNumUninitialized(Count, EAllowShrinking::No);
	CellEntries.SetNumUninitialized(Count, EAllowShrinking::No);
	ParticleCell.SetNumUninitialized(Count, EAllowShrinking::No);

	NumBallistic = 0;
	ActiveShotCount = 0;
	NextShotId = 1;
	ContactLoad = 0.f;
	bSpread = false;
	SqueezeAmount = 0.f;
	GravityScale = 1.f;
	ShotStates.Reset();
	ShotFloorOverrides.Reset();
	ShotMergeElapsed.Reset();
	ShotImpactApplied.Reset();
}

void FSlimeSolver::EnsureScratchCapacity(int32 Count)
{
	if (Lambdas.Num() < Count)
	{
		Lambdas.SetNumUninitialized(Count, EAllowShrinking::No);
		ContactLoads.SetNumUninitialized(Count, EAllowShrinking::No);
		DeltaPositions.SetNumUninitialized(Count, EAllowShrinking::No);
		ContactNormals.SetNumUninitialized(Count, EAllowShrinking::No);
		ViscosityDelta.SetNumUninitialized(Count, EAllowShrinking::No);
		CellEntries.SetNumUninitialized(Count, EAllowShrinking::No);
		ParticleCell.SetNumUninitialized(Count, EAllowShrinking::No);
	}
}

void FSlimeSolver::SetLaunchFraction(float Fraction)
{
	LaunchFractionCached = FMath::Clamp(Fraction, 0.05f, 0.6f);
	MiniMembraneRadius = Params.RestRadius * FMath::Pow(LaunchFractionCached, 1.f / 3.f);
}

void FSlimeSolver::SetShotFloorZ(uint8 ShotId, float InFloorZ)
{
	if (ShotId != 0)
	{
		ShotFloorOverrides.Add(ShotId, InFloorZ);
	}
}

void FSlimeSolver::ClearShotFloorOverrides()
{
	ShotFloorOverrides.Reset();
}

void FSlimeSolver::GetShotCenters(TArray<FVector>& OutCenters) const
{
	OutCenters.Reset();
	for (const FShotState& Shot : ShotStates)
	{
		OutCenters.Add(FVector(Shot.Center));
	}
}

void FSlimeSolver::RebuildShotStates()
{
	TMap<uint8, FShotState> Accumulators;
	for (const FSlimeParticle& Particle : Particles)
	{
		if (!Particle.IsBallistic() || Particle.ShotId == 0)
		{
			continue;
		}
		FShotState& Shot = Accumulators.FindOrAdd(Particle.ShotId);
		Shot.Id = Particle.ShotId;
		Shot.Center += Particle.Position;
		Shot.Velocity += Particle.Velocity;
		++Shot.Count;
	}

	ShotStates.Reset();
	ShotStates.Reserve(Accumulators.Num());
	for (TPair<uint8, FShotState>& Pair : Accumulators)
	{
		FShotState& Shot = Pair.Value;
		if (Shot.Count <= 0)
		{
			continue;
		}
		Shot.Center /= float(Shot.Count);
		Shot.Velocity /= float(Shot.Count);
		if (const float* FloorOverride = ShotFloorOverrides.Find(Shot.Id))
		{
			Shot.FloorZ = *FloorOverride;
		}
		else
		{
			Shot.FloorZ = FragmentFloorZ;
		}
		if (const float* MergeTime = ShotMergeElapsed.Find(Shot.Id))
		{
			Shot.MergeElapsed = *MergeTime;
		}
		Shot.bImpactApplied = ShotImpactApplied.Contains(Shot.Id);
		ShotStates.Add(Shot);
	}

	ActiveShotCount = ShotStates.Num();
	NumBallistic = 0;
	for (const FSlimeParticle& Particle : Particles)
	{
		if (Particle.IsBallistic())
		{
			++NumBallistic;
		}
	}
}

void FSlimeSolver::ClampToShotShell(FVector3f& InOutPoint, const FVector3f& ShotCenter) const
{
	// Dome ellipsoid (not a flat disc): XY full mini radius, Z slightly shorter, centre lifted
	// so the floor does not clip the blob into a pancake.
	const float RadiusXY = MiniMembraneRadius * Params.TetherSlack;
	const float RadiusZ = MiniMembraneRadius * 0.85f * Params.TetherSlack;
	const FVector3f DomeCenter = ShotCenter + FVector3f(0.f, 0.f, MiniMembraneRadius * 0.2f);
	FVector3f Local = InOutPoint - DomeCenter;
	const float NX = Local.X / FMath::Max(RadiusXY, KINDA_SMALL_NUMBER);
	const float NY = Local.Y / FMath::Max(RadiusXY, KINDA_SMALL_NUMBER);
	const float NZ = Local.Z / FMath::Max(RadiusZ, KINDA_SMALL_NUMBER);
	const float NormSq = NX * NX + NY * NY + NZ * NZ;
	if (NormSq > 1.f && NormSq > KINDA_SMALL_NUMBER)
	{
		const float Inv = 1.f / FMath::Sqrt(NormSq);
		InOutPoint = DomeCenter + FVector3f(Local.X * Inv, Local.Y * Inv, Local.Z * Inv);
	}
}

void FSlimeSolver::LiftShotCentersAboveFloor()
{
	const float MiniR = FMath::Max(MiniMembraneRadius, Params.ParticleSpacing * 2.f);
	const float MinComZOffset = MiniR * 0.45f;

	TMap<uint8, float> FloorByShot;
	for (const FShotState& Shot : ShotStates)
	{
		float Floor = FragmentFloorZ;
		if (const float* Override = ShotFloorOverrides.Find(Shot.Id))
		{
			Floor = *Override;
		}
		FloorByShot.Add(Shot.Id, Floor);
	}

	TMap<uint8, FVector3f> Centers;
	TMap<uint8, int32> Counts;
	for (const FSlimeParticle& Particle : Particles)
	{
		if (!Particle.IsBallistic() || Particle.ShotId == 0)
		{
			continue;
		}
		Centers.FindOrAdd(Particle.ShotId) += Particle.Position;
		Counts.FindOrAdd(Particle.ShotId) += 1;
	}

	for (TPair<uint8, FVector3f>& Pair : Centers)
	{
		const int32 Count = Counts.FindRef(Pair.Key);
		if (Count <= 0)
		{
			continue;
		}
		Pair.Value /= float(Count);
		const float Floor = FloorByShot.FindRef(Pair.Key);
		const float MinZ = Floor + MinComZOffset;
		if (Pair.Value.Z >= MinZ)
		{
			continue;
		}
		const float Lift = MinZ - Pair.Value.Z;
		for (FSlimeParticle& Particle : Particles)
		{
			if (Particle.IsBallistic() && Particle.ShotId == Pair.Key)
			{
				Particle.Position.Z += Lift;
				Particle.PredictedPosition.Z += Lift;
			}
		}
	}
}

bool FSlimeSolver::IsShotMerging(uint8 ShotId) const
{
	if (ShotId == 0)
	{
		return false;
	}
	if (const float* MergeTime = ShotMergeElapsed.Find(ShotId))
	{
		return *MergeTime >= 0.f;
	}
	return false;
}

void FSlimeSolver::GetMergingShotIds(TArray<uint8>& OutIds) const
{
	OutIds.Reset();
	for (const TPair<uint8, float>& Pair : ShotMergeElapsed)
	{
		if (Pair.Value >= 0.f)
		{
			OutIds.Add(Pair.Key);
		}
	}
}

void FSlimeSolver::Reset(const FVector& RestCenter)
{
	BuildDome(RestCenter);
	Colliders.Reset();
	bSkipWorldCollision = false;
	SpreadRadius = 0.f;
	SpreadPush = 0.f;
	AnchorCenter = FVector3f(RestCenter);
	AnchorVelocity = FVector3f::ZeroVector;
	PrevAnchorVelocity = FVector3f::ZeroVector;
	InertiaAmount = 0.f;
	ShellBackShift = 0.f;
	LandingSettleRemaining = 0.f;
	ShellAxes = FVector3f(Params.RestRadius);
}

void FSlimeSolver::ApplyLandingSquash(float ImpactSpeed)
{
	const float Strength = FMath::Clamp((ImpactSpeed - 280.f) / 900.f, 0.f, 1.f);
	if (Strength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	LandingSettleRemaining = FMath::Max(LandingSettleRemaining, LandingSettleDuration * (0.55f + 0.45f * Strength));

	// Kill upward spray without loosening the shell — deform, never fragment.
	for (FSlimeParticle& Particle : Particles)
	{
		if (Particle.IsBallistic())
		{
			continue;
		}
		if (Particle.Velocity.Z > 0.f)
		{
			Particle.Velocity.Z *= FMath::Lerp(1.f, 0.05f, Strength);
		}
		// Mild radial push so the landing reads as a squash, still inside the tether.
		const FVector3f Center = FVector3f(GetBodyCenter());
		FVector3f Radial = Particle.Position - Center;
		Radial.Z = 0.f;
		const float Len = Radial.Size();
		if (Len > KINDA_SMALL_NUMBER)
		{
			Particle.Velocity += (Radial / Len) * (Strength * 80.f);
		}
	}
}

void FSlimeSolver::ApplyAirBounce()
{
	// Double-jump "duang": brief squash then springy recover, much milder than a landing.
	LandingSettleRemaining = FMath::Max(LandingSettleRemaining, 0.55f);
	const FVector3f Center = FVector3f(GetBodyCenter());
	for (FSlimeParticle& Particle : Particles)
	{
		if (Particle.IsBallistic())
		{
			continue;
		}
		FVector3f Offset = Particle.Position - Center;
		Offset.Z *= 0.55f;
		Particle.Position = Center + Offset;
		Particle.PredictedPosition = Particle.Position;
		Particle.Velocity.Z = FMath::Max(Particle.Velocity.Z, 0.f) + 120.f;
	}
}

FVector3f FSlimeSolver::UpdateInertiaShape(float Dt)
{
	FVector3f Horizontal(AnchorVelocity.X, AnchorVelocity.Y, 0.f);
	const float Speed = Horizontal.Size();
	const float TargetAmount = FMath::Clamp(Speed / FMath::Max(Params.ReferenceWalkSpeed, 1.f), 0.f, 1.f)
		* Params.InertiaStretch;

	// Extra punch on hard stops / sharp turns.
	FVector3f Accel = (AnchorVelocity - PrevAnchorVelocity) / FMath::Max(Dt, KINDA_SMALL_NUMBER);
	PrevAnchorVelocity = AnchorVelocity;
	FVector3f AccelXY(Accel.X, Accel.Y, 0.f);
	const float AccelBoost = FMath::Clamp(AccelXY.Size() / 4000.f, 0.f, 0.35f);

	const float Blend = 1.f - FMath::Exp(-Params.InertiaResponse * Dt);
	InertiaAmount = FMath::Lerp(InertiaAmount, FMath::Min(TargetAmount + AccelBoost, Params.InertiaStretch), Blend);

	if (Speed > 15.f)
	{
		const FVector3f Dir = Horizontal / Speed;
		InertiaForward = FMath::Lerp(InertiaForward, Dir, Blend).GetSafeNormal();
		if (InertiaForward.IsNearlyZero())
		{
			InertiaForward = Dir;
		}
	}

	return InertiaForward;
}

void FSlimeSolver::ClampToBodyShell(FVector3f& InOutPoint, const FVector3f& Center) const
{
	if (bSpread)
	{
		// Thin pancake disk: deform into a puddle, never leave the disk.
		const float Radius = FMath::Max(SpreadRadius, Params.RestRadius) * Params.TetherSlack;
		const float HalfH = FMath::Max(SpreadHalfHeight, 0.5f);
		FVector3f Local = InOutPoint - Center;
		FVector3f Radial(Local.X, Local.Y, 0.f);
		const float R = Radial.Size();
		if (R > Radius && R > KINDA_SMALL_NUMBER)
		{
			Radial *= Radius / R;
		}
		Local.X = Radial.X;
		Local.Y = Radial.Y;
		Local.Z = FMath::Clamp(Local.Z, -HalfH, HalfH);
		InOutPoint = Center + Local;
		return;
	}

	// Inertia ellipsoid in (Forward, Right, Up), centre shifted slightly rearward.
	FVector3f Forward = InertiaForward;
	Forward.Z = 0.f;
	if (Forward.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		Forward = FVector3f::ForwardVector;
	}
	else
	{
		Forward.Normalize();
	}
	const FVector3f Right = FVector3f::CrossProduct(FVector3f::UpVector, Forward).GetSafeNormal();
	const FVector3f Up = FVector3f::UpVector;
	const FVector3f ShellCenter = Center - Forward * ShellBackShift;

	FVector3f Local = InOutPoint - ShellCenter;
	const float Lf = Local | Forward;
	const float Lr = Local | Right;
	const float Lu = Local | Up;
	const float Ax = FMath::Max(ShellAxes.X, 1.f);
	const float Ay = FMath::Max(ShellAxes.Y, 1.f);
	const float Az = FMath::Max(ShellAxes.Z, 1.f);
	const float Score = (Lf * Lf) / (Ax * Ax) + (Lr * Lr) / (Ay * Ay) + (Lu * Lu) / (Az * Az);
	if (Score > 1.f && Score > KINDA_SMALL_NUMBER)
	{
		const float Scale = FMath::InvSqrt(Score);
		InOutPoint = ShellCenter + Forward * (Lf * Scale) + Right * (Lr * Scale) + Up * (Lu * Scale);
	}
}

void FSlimeSolver::SetAnchor(const FVector& InCenter, const FVector& InVelocity)
{
	AnchorCenter = FVector3f(InCenter);
	AnchorVelocity = FVector3f(InVelocity);
}

void FSlimeSolver::SetSpread(bool bInSpread, float InSpreadRadius, float InSpreadPush, float InSpreadHalfHeight)
{
	bSpread = bInSpread;
	SpreadRadius = InSpreadRadius;
	SpreadPush = InSpreadPush;
	SpreadHalfHeight = FMath::Max(InSpreadHalfHeight, 0.5f);
}

void FSlimeSolver::SetSqueeze(float InAmount, const FVector& InFreeDirection)
{
	SqueezeAmount = FMath::Clamp(InAmount, 0.f, 1.f);
	SqueezeFreeDirection = FVector3f(InFreeDirection);
}

FVector FSlimeSolver::GetBodyCenter() const
{
	FVector3f Sum = FVector3f::ZeroVector;
	int32 Count = 0;
	for (const FSlimeParticle& Particle : Particles)
	{
		if (!Particle.IsBallistic())
		{
			Sum += Particle.Position;
			++Count;
		}
	}
	return Count > 0 ? FVector(Sum / float(Count)) : FVector(AnchorCenter);
}

FBox FSlimeSolver::GetBounds() const
{
	FBox Box(ForceInit);
	for (const FSlimeParticle& Particle : Particles)
	{
		Box += FVector(Particle.Position);
	}
	return Box.ExpandBy(ContactRadius);
}

FBox FSlimeSolver::GetBodyBounds() const
{
	FBox Box(ForceInit);
	for (const FSlimeParticle& Particle : Particles)
	{
		if (!Particle.IsBallistic())
		{
			Box += FVector(Particle.Position);
		}
	}
	return Box.ExpandBy(ContactRadius);
}

bool FSlimeSolver::GetFragmentCenter(FVector& OutCenter) const
{
	FVector3f Sum = FVector3f::ZeroVector;
	int32 Count = 0;
	for (const FSlimeParticle& Particle : Particles)
	{
		if (Particle.IsBallistic())
		{
			Sum += Particle.Position;
			++Count;
		}
	}
	if (Count <= 0)
	{
		return false;
	}
	OutCenter = FVector(Sum / float(Count));
	return true;
}

FBox FSlimeSolver::GetFragmentBounds() const
{
	FBox Box(ForceInit);
	for (const FSlimeParticle& Particle : Particles)
	{
		if (Particle.IsBallistic())
		{
			Box += FVector(Particle.Position);
		}
	}
	return Box.IsValid ? Box.ExpandBy(ContactRadius) : Box;
}

void FSlimeSolver::Step(float Dt)
{
	const int32 Count = Particles.Num();
	if (Count == 0 || Dt <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	EnsureScratchCapacity(Count);
	RebuildShotStates();
	LiftShotCentersAboveFloor();
	RebuildShotStates();

	TRACE_CPUPROFILER_EVENT_SCOPE(SlimeSolver_Step);

	if (LandingSettleRemaining > 0.f)
	{
		LandingSettleRemaining = FMath::Max(LandingSettleRemaining - Dt, 0.f);
	}

	// ---- External forces --------------------------------------------------------------

	FVector3f BodyCenter = FVector3f::ZeroVector;
	FVector3f BodyVelocity = FVector3f::ZeroVector;
	int32 BodyCount = 0;
	for (const FSlimeParticle& Particle : Particles)
	{
		if (!Particle.IsBallistic())
		{
			BodyCenter += Particle.Position;
			BodyVelocity += Particle.Velocity;
			++BodyCount;
		}
	}
	if (BodyCount > 0)
	{
		BodyCenter /= float(BodyCount);
		BodyVelocity /= float(BodyCount);
	}
	else
	{
		BodyCenter = AnchorCenter;
	}

	UpdateInertiaShape(Dt);

	// A gap displaces volume, so let the membrane stretch instead of fighting it.
	const float MembraneRadius = bSpread
		? FMath::Max(SpreadRadius, Params.RestRadius)
		: Params.RestRadius * (1.f + SqueezeAmount * Params.MembraneSqueezeStretch);

	// Build the single-blob hard shell (ellipsoid or flat disk). Never larger than needed.
	const float Slack = Params.TetherSlack;
	if (bSpread)
	{
		ShellAxes = FVector3f(MembraneRadius * Slack, MembraneRadius * Slack, FMath::Max(Params.ParticleSpacing * 1.25f, 3.f));
		ShellBackShift = 0.f;
	}
	else
	{
		const float Base = MembraneRadius * Slack;
		const float Stretch = InertiaAmount;
		// Along move: elongate; across / up: squash — inertia trail, still one blob.
		ShellAxes.X = Base * (1.f + 0.35f * Stretch);
		ShellAxes.Y = Base * (1.f - 0.22f * Stretch);
		ShellAxes.Z = Base * (1.f - 0.18f * Stretch);
		ShellBackShift = Base * 0.18f * Stretch;
		if (SqueezeAmount > 0.05f && !SqueezeFreeDirection.IsNearlyZero())
		{
			// Gap squeeze further flattens the shell along the blocked axes.
			ShellAxes.Y *= FMath::Lerp(1.f, 0.65f, SqueezeAmount);
			ShellAxes.Z *= FMath::Lerp(1.f, 0.55f, SqueezeAmount);
			ShellAxes.X *= FMath::Lerp(1.f, 1.2f, SqueezeAmount);
		}
	}

	// Critically damped follower on the centre of mass.
	const float Omega = 2.f * PI * FMath::Max(Params.AnchorFollowFrequency, 0.1f);
	const float SpringK = Omega * Omega;
	const float SpringC = 2.f * Params.AnchorDamping * Omega;
	const FVector3f AnchorAccel = (AnchorCenter - BodyCenter) * SpringK - (BodyVelocity - AnchorVelocity) * SpringC;

	const float MembraneK = Params.MembraneStiffness * (1.f + SqueezeAmount * 1.5f);
	const float SettleBoost = LandingSettleRemaining > 0.f ? Params.LandingCohesionBoost : 1.f;
	// Keep the centre filled while spread (no doughnut): boost concentration, do not weaken it.
	const float Concentration = Params.Concentration * SettleBoost * (bSpread ? SpreadConcentrationScale : 1.f);
	const float GripRadius = MembraneRadius * Params.GripRadiusScale;
	const float UpwardRestore = bSpread ? 0.f : Params.UpwardRestore * SettleBoost;
	const float Gravity = Params.Gravity * GravityScale;
	const float DampingFactor = FMath::Exp(-Params.LinearDamping * Dt);
	const float MiniRadius = FMath::Max(MiniMembraneRadius, Params.ParticleSpacing * 2.f);
	const float MiniGrip = MiniRadius * Params.GripRadiusScale;
	const float MiniConcentration = Params.Concentration * 1.1f;
	const float MiniMembraneK = Params.MembraneStiffness;

	// Shot COM lookup for ballistic cohesion (copied out of ParallelFor for thread safety).
	TMap<uint8, FVector3f> ShotCenters;
	ShotCenters.Reserve(ShotStates.Num());
	for (const FShotState& Shot : ShotStates)
	{
		ShotCenters.Add(Shot.Id, Shot.Center);
	}

	ParallelFor(Count, [this, Dt, &AnchorAccel, &BodyCenter, MembraneRadius, MembraneK, GripRadius, Concentration, UpwardRestore, Gravity, DampingFactor, MiniRadius, MiniGrip, MiniConcentration, MiniMembraneK, &ShotCenters](int32 Index)
	{
		FSlimeParticle& Particle = Particles[Index];
		FVector3f Accel(0.f, 0.f, Gravity);

		if (!Particle.IsBallistic())
		{
			Accel += AnchorAccel;

			const FVector3f Offset = Particle.Position - BodyCenter;
			const float Distance = Offset.Size();
			if (Distance > KINDA_SMALL_NUMBER)
			{
				const FVector3f ToCenter = -Offset / Distance;

				// Soft sticky jelly (SIM): full stick inside rest radius, fade to grip shell.
				float Stick = 0.f;
				if (Distance < MembraneRadius)
				{
					Stick = 1.f;
				}
				else if (Distance < GripRadius)
				{
					const float T = (Distance - MembraneRadius) / FMath::Max(GripRadius - MembraneRadius, KINDA_SMALL_NUMBER);
					Stick = (1.f - T) * (1.f - T);
				}
				Accel += ToCenter * (Concentration * Stick * FMath::Min(Distance, MembraneRadius));

				if (Distance > MembraneRadius)
				{
					Accel += ToCenter * ((Distance - MembraneRadius) * MembraneK);
				}

				if (UpwardRestore > 0.f && Offset.Z < 0.f)
				{
					Accel.Z += UpwardRestore * (-Offset.Z);
				}
			}

			if (bSpread && SpreadPush > 0.f)
			{
				// Edge-only push: centre stays dense; outer ring expands into a pancake.
				FVector3f Radial(Offset.X, Offset.Y, 0.f);
				const float RadialLength = Radial.Size();
				const float InnerR = MembraneRadius * 0.65f;
				const float OuterR = MembraneRadius * 0.92f;
				if (RadialLength > InnerR && RadialLength < OuterR)
				{
					const float Edge = 1.f - ((RadialLength - InnerR) / FMath::Max(OuterR - InnerR, KINDA_SMALL_NUMBER));
					Accel += (Radial / RadialLength) * (SpreadPush * Edge);
				}
			}
			else if (SqueezeAmount > 0.05f && !SqueezeFreeDirection.IsNearlyZero())
			{
				Accel += SqueezeFreeDirection * (SqueezeAmount * 260.f);
			}
		}
		else if (Particle.ShotId != 0)
		{
			// Mini-slime cohesion: dome membrane around the shot COM (never pancake).
			if (const FVector3f* ShotCenter = ShotCenters.Find(Particle.ShotId))
			{
				const FVector3f Offset = Particle.Position - *ShotCenter;
				const float Distance = Offset.Size();
				if (Distance > KINDA_SMALL_NUMBER)
				{
					const FVector3f ToCenter = -Offset / Distance;
					float Stick = 0.f;
					if (Distance < MiniRadius)
					{
						Stick = 1.f;
					}
					else if (Distance < MiniGrip)
					{
						const float T = (Distance - MiniRadius) / FMath::Max(MiniGrip - MiniRadius, KINDA_SMALL_NUMBER);
						Stick = (1.f - T) * (1.f - T);
					}
					Accel += ToCenter * (MiniConcentration * Stick * FMath::Min(Distance, MiniRadius));
					if (Distance > MiniRadius)
					{
						Accel += ToCenter * ((Distance - MiniRadius) * MiniMembraneK);
					}
				}
				// Keep the dome height — without this the floor clips the blob into a pancake.
				if (Offset.Z < 0.f)
				{
					Accel.Z += Params.UpwardRestore * (-Offset.Z);
				}
			}
		}

		Particle.Velocity = (Particle.Velocity + Accel * Dt) * DampingFactor;
		Particle.PredictedPosition = Particle.Position + Particle.Velocity * Dt;
	}, Count < GParallelMinBatch ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);

	// ---- Density constraint -----------------------------------------------------------

	BuildGrid();
	for (int32 Iteration = 0; Iteration < Params.DensityIterations; ++Iteration)
	{
		SolveDensity();
	}

	// ---- Collision --------------------------------------------------------------------

	ResolveCollisions();

	// Recompute COM after collision projections, then hard-clamp into the single-blob shell.
	// Ballistic clones clamp into their own mini shell so they stay one blob in flight.
	{
		FVector3f PostCenter = FVector3f::ZeroVector;
		int32 PostCount = 0;
		for (const FSlimeParticle& Particle : Particles)
		{
			if (!Particle.IsBallistic())
			{
				PostCenter += Particle.PredictedPosition;
				++PostCount;
			}
		}
		if (PostCount > 0)
		{
			PostCenter /= float(PostCount);
		}
		else
		{
			PostCenter = BodyCenter;
		}

		TMap<uint8, FVector3f> ShotPredictedCenters;
		TMap<uint8, int32> ShotPredictedCounts;
		for (const FSlimeParticle& Particle : Particles)
		{
			if (!Particle.IsBallistic() || Particle.ShotId == 0)
			{
				continue;
			}
			ShotPredictedCenters.FindOrAdd(Particle.ShotId) += Particle.PredictedPosition;
			ShotPredictedCounts.FindOrAdd(Particle.ShotId) += 1;
		}
		for (TPair<uint8, FVector3f>& Pair : ShotPredictedCenters)
		{
			const int32 ShotCount = ShotPredictedCounts.FindRef(Pair.Key);
			if (ShotCount > 0)
			{
				Pair.Value /= float(ShotCount);
			}
		}

		ParallelFor(Count, [this, PostCenter, &ShotPredictedCenters](int32 Index)
		{
			FSlimeParticle& Particle = Particles[Index];
			if (Particle.IsBallistic())
			{
				if (const FVector3f* ShotCenter = ShotPredictedCenters.Find(Particle.ShotId))
				{
					ClampToShotShell(Particle.PredictedPosition, *ShotCenter);
				}
				return;
			}
			ClampToBodyShell(Particle.PredictedPosition, PostCenter);
		}, Count < GParallelMinBatch ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);

		// Re-lift after floor/shell so the dome COM cannot sink into the ground plane.
		LiftShotCentersAboveFloor();
	}

	// ---- Velocity update --------------------------------------------------------------

	const float InvDt = 1.f / Dt;
	const float MaxSpeed = Params.MaxSpeed;
	const float MaxRel = Params.MaxRelSpeed;
	const float Friction = Params.SlideFriction;
	const float Restitution = Params.Restitution;
	const FVector3f AnchorVel = AnchorVelocity;

	ParallelFor(Count, [this, InvDt, MaxSpeed, MaxRel, Friction, Restitution, AnchorVel](int32 Index)
	{
		FSlimeParticle& Particle = Particles[Index];
		FVector3f Velocity = (Particle.PredictedPosition - Particle.Position) * InvDt;

		const FVector3f Contact = ContactNormals[Index];
		if (!Contact.IsNearlyZero())
		{
			const FVector3f Normal = Contact.GetSafeNormal();
			const float Along = Velocity | Normal;
			if (Along < 0.f)
			{
				const FVector3f Tangent = Velocity - Normal * Along;
				Velocity = Tangent * Friction - Normal * (Along * Restitution);
			}
		}

		if (!Particle.IsBallistic())
		{
			FVector3f Rel = Velocity - AnchorVel;
			const float RelSpeed = Rel.Size();
			if (RelSpeed > MaxRel)
			{
				Velocity = AnchorVel + Rel * (MaxRel / RelSpeed);
			}
		}

		const float Speed = Velocity.Size();
		if (Speed > MaxSpeed)
		{
			Velocity *= MaxSpeed / Speed;
		}

		Particle.Velocity = Velocity;
		Particle.Position = Particle.PredictedPosition;
	}, Count < GParallelMinBatch ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);

	ApplyViscosity();

	// ---- Fragment lifetime ------------------------------------------------------------

	if (NumBallistic > 0)
	{
		bool bRemovedAny = false;
		TSet<uint8> TouchedShots;
		for (int32 Index = Particles.Num() - 1; Index >= 0; --Index)
		{
			FSlimeParticle& Particle = Particles[Index];
			if (!Particle.IsBallistic())
			{
				continue;
			}
			Particle.BallisticLife -= Dt;
			if (Particle.BallisticLife > 0.f)
			{
				continue;
			}

			TouchedShots.Add(Particle.ShotId);
			if (Particle.IsClone())
			{
				Particles.RemoveAtSwap(Index, 1, EAllowShrinking::No);
				bRemovedAny = true;
			}
			else
			{
				Particle.BallisticLife = 0.f;
				Particle.Flags &= ~(PF_Ballistic | PF_Clone);
				Particle.ShotId = 0;
			}
		}

		if (bRemovedAny)
		{
			for (const uint8 ShotId : TouchedShots)
			{
				bool bStillAlive = false;
				for (const FSlimeParticle& Particle : Particles)
				{
					if (Particle.IsBallistic() && Particle.ShotId == ShotId)
					{
						bStillAlive = true;
						break;
					}
				}
				if (!bStillAlive)
				{
					ShotMergeElapsed.Remove(ShotId);
					ShotImpactApplied.Remove(ShotId);
					ShotFloorOverrides.Remove(ShotId);
				}
			}
			RebuildShotStates();
			EnsureScratchCapacity(Particles.Num());
		}
		else
		{
			RecountActiveShots();
		}
	}
}

void FSlimeSolver::BuildGrid()
{
	const int32 Count = Particles.Num();

	// Body bounds drive resolution. Nearby merging clones join the hash so body↔clone
	// density can squeeze; distant shots stay on membrane+shell only (avoids coarsening).
	FBox3f Bounds(ForceInit);
	bool bAnyBody = false;
	FVector3f BodyCenterAccum = FVector3f::ZeroVector;
	int32 BodyCount = 0;
	for (const FSlimeParticle& Particle : Particles)
	{
		if (Particle.IsBallistic())
		{
			continue;
		}
		Bounds += Particle.PredictedPosition;
		BodyCenterAccum += Particle.PredictedPosition;
		++BodyCount;
		bAnyBody = true;
	}

	const FVector3f ApproxBodyCenter = BodyCount > 0 ? (BodyCenterAccum / float(BodyCount)) : AnchorCenter;
	const float NearSq = FMath::Square(Params.RestRadius * 5.f);
	for (const FSlimeParticle& Particle : Particles)
	{
		if (!Particle.IsBallistic())
		{
			continue;
		}
		if (FVector3f::DistSquared(Particle.PredictedPosition, ApproxBodyCenter) <= NearSq)
		{
			Bounds += Particle.PredictedPosition;
		}
	}

	if (!bAnyBody)
	{
		Bounds.Init();
		for (const FSlimeParticle& Particle : Particles)
		{
			Bounds += Particle.PredictedPosition;
		}
	}
	Bounds = Bounds.ExpandBy(SmoothingRadius);

	const FVector3f Size = Bounds.GetSize();
	// Never below the kernel radius, otherwise the 3x3x3 walk would miss neighbours.
	// Fragments flying far away only make the cells coarser, never the search wrong.
	GridCellSize = FMath::Max(SmoothingRadius, Size.GetMax() / float(GMaxGridDim));
	GridOrigin = Bounds.Min;

	const float InvCell = 1.f / GridCellSize;
	GridDims = FIntVector(
		FMath::Clamp(FMath::CeilToInt(Size.X * InvCell), 1, GMaxGridDim),
		FMath::Clamp(FMath::CeilToInt(Size.Y * InvCell), 1, GMaxGridDim),
		FMath::Clamp(FMath::CeilToInt(Size.Z * InvCell), 1, GMaxGridDim));

	const int32 NumCells = GridDims.X * GridDims.Y * GridDims.Z;
	CellStart.SetNumUninitialized(NumCells + 1, EAllowShrinking::No);
	FMemory::Memzero(CellStart.GetData(), (NumCells + 1) * sizeof(int32));

	// Counting sort into flat buckets: no hashing, no per frame allocation.
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 Cell = CellIndexOf(Particles[Index].PredictedPosition, GridOrigin, InvCell, GridDims);
		ParticleCell[Index] = Cell;
		++CellStart[Cell + 1];
	}
	for (int32 Cell = 0; Cell < NumCells; ++Cell)
	{
		CellStart[Cell + 1] += CellStart[Cell];
	}

	CellCursor.SetNumUninitialized(NumCells, EAllowShrinking::No);
	FMemory::Memcpy(CellCursor.GetData(), CellStart.GetData(), NumCells * sizeof(int32));

	for (int32 Index = 0; Index < Count; ++Index)
	{
		CellEntries[CellCursor[ParticleCell[Index]]++] = Index;
	}
}

void FSlimeSolver::SolveDensity()
{
	const int32 Count = Particles.Num();
	const float InvCell = 1.f / GridCellSize;
	const float InvRestDensity = 1.f / RestDensity;
	const EParallelForFlags Flags = Count < GParallelMinBatch ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None;

	auto Couples = [this](const FSlimeParticle& A, const FSlimeParticle& B) -> bool
	{
		const bool bA = A.IsBallistic();
		const bool bB = B.IsBallistic();
		if (bA == bB)
		{
			return !bA || A.ShotId == B.ShotId;
		}
		// Soft-merge: body <-> merging shot share one fluid so the iso surface can fuse.
		const uint8 ShotId = bA ? A.ShotId : B.ShotId;
		return IsShotMerging(ShotId);
	};

	ParallelFor(Count, [this, InvCell, InvRestDensity, &Couples](int32 Index)
	{
		const FSlimeParticle& Self = Particles[Index];
		const FVector3f Pi = Self.PredictedPosition;
		const FIntVector Base = CellCoordOf(Pi, GridOrigin, InvCell, GridDims);

		float Density = 0.f;
		FVector3f GradSelf = FVector3f::ZeroVector;
		float SumGradSq = 0.f;

		for (int32 Z = FMath::Max(Base.Z - 1, 0); Z <= FMath::Min(Base.Z + 1, GridDims.Z - 1); ++Z)
		for (int32 Y = FMath::Max(Base.Y - 1, 0); Y <= FMath::Min(Base.Y + 1, GridDims.Y - 1); ++Y)
		for (int32 X = FMath::Max(Base.X - 1, 0); X <= FMath::Min(Base.X + 1, GridDims.X - 1); ++X)
		{
			const int32 Cell = X + GridDims.X * (Y + GridDims.Y * Z);
			for (int32 Entry = CellStart[Cell]; Entry < CellStart[Cell + 1]; ++Entry)
			{
				const int32 Other = CellEntries[Entry];
				const FSlimeParticle& OtherP = Particles[Other];
				if (!Couples(Self, OtherP))
				{
					continue;
				}

				const FVector3f Delta = Pi - OtherP.PredictedPosition;
				const float DistSq = Delta.SizeSquared();
				if (DistSq >= SmoothingRadiusSq)
				{
					continue;
				}

				Density += KernelPoly6(DistSq, SmoothingRadiusSq, Poly6Norm);

				if (Other == Index || DistSq <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				const float Dist = FMath::Sqrt(DistSq);
				const float GradMag = -KernelSpikyGrad(Dist, SmoothingRadius, SpikyGradNorm);
				const FVector3f GradC = Delta * (GradMag / Dist) * InvRestDensity;
				GradSelf += GradC;
				SumGradSq += GradC.SizeSquared();
			}
		}

		SumGradSq += GradSelf.SizeSquared();

		// Allow stronger under-density pull so sparse islands collapse back into one blob.
		const float Constraint = FMath::Max(Density * InvRestDensity - 1.f, -0.8f);
		Lambdas[Index] = FMath::Clamp(-Constraint / (SumGradSq + 1.e-4f), -6.f, 6.f);
	}, Flags);

	const float SurfaceTension = Params.SurfaceTension;

	ParallelFor(Count, [this, InvCell, InvRestDensity, SurfaceTension, &Couples](int32 Index)
	{
		const FSlimeParticle& Self = Particles[Index];
		const FVector3f Pi = Self.PredictedPosition;
		const FIntVector Base = CellCoordOf(Pi, GridOrigin, InvCell, GridDims);
		const float LambdaI = Lambdas[Index];

		FVector3f Delta = FVector3f::ZeroVector;

		for (int32 Z = FMath::Max(Base.Z - 1, 0); Z <= FMath::Min(Base.Z + 1, GridDims.Z - 1); ++Z)
		for (int32 Y = FMath::Max(Base.Y - 1, 0); Y <= FMath::Min(Base.Y + 1, GridDims.Y - 1); ++Y)
		for (int32 X = FMath::Max(Base.X - 1, 0); X <= FMath::Min(Base.X + 1, GridDims.X - 1); ++X)
		{
			const int32 Cell = X + GridDims.X * (Y + GridDims.Y * Z);
			for (int32 Entry = CellStart[Cell]; Entry < CellStart[Cell + 1]; ++Entry)
			{
				const int32 Other = CellEntries[Entry];
				if (Other == Index)
				{
					continue;
				}

				const FSlimeParticle& OtherP = Particles[Other];
				if (!Couples(Self, OtherP))
				{
					continue;
				}

				const FVector3f Offset = Pi - OtherP.PredictedPosition;
				const float DistSq = Offset.SizeSquared();
				if (DistSq >= SmoothingRadiusSq || DistSq <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				const float Dist = FMath::Sqrt(DistSq);
				const float GradMag = -KernelSpikyGrad(Dist, SmoothingRadius, SpikyGradNorm);

				const float Ratio = KernelPoly6(DistSq, SmoothingRadiusSq, Poly6Norm) / ArtificialPressureDenom;
				const float RatioSq = Ratio * Ratio;
				const float Correction = -SurfaceTension * RatioSq * RatioSq;

				Delta += Offset * (GradMag / Dist) * (LambdaI + Lambdas[Other] + Correction);
			}
		}

		DeltaPositions[Index] = Delta * InvRestDensity;
	}, Flags);

	ParallelFor(Count, [this](int32 Index)
	{
		Particles[Index].PredictedPosition += DeltaPositions[Index];
	}, Flags);
}

bool FSlimeSolver::ProjectOut(const FSlimeCollider& Collider, float Skin, FVector3f& InOutPoint, FVector3f& OutNormal)
{
	switch (Collider.Shape)
	{
	case EColliderShape::Sphere:
	{
		const FVector3f Delta = InOutPoint - Collider.Center;
		const float Distance = Delta.Size();
		const float Target = Collider.Radius + Skin;
		if (Distance >= Target)
		{
			return false;
		}
		OutNormal = Distance > KINDA_SMALL_NUMBER ? Delta / Distance : FVector3f::UpVector;
		InOutPoint = Collider.Center + OutNormal * Target;
		return true;
	}

	case EColliderShape::Capsule:
	{
		const FVector3f Local = Collider.Rotation.UnrotateVector(InOutPoint - Collider.Center);
		const float ClampedZ = FMath::Clamp(Local.Z, -Collider.HalfHeight, Collider.HalfHeight);
		const FVector3f OnAxis(0.f, 0.f, ClampedZ);
		const FVector3f Delta = Local - OnAxis;
		const float Distance = Delta.Size();
		const float Target = Collider.Radius + Skin;
		if (Distance >= Target)
		{
			return false;
		}
		const FVector3f LocalNormal = Distance > KINDA_SMALL_NUMBER ? Delta / Distance : FVector3f::UpVector;
		OutNormal = Collider.Rotation.RotateVector(LocalNormal);
		InOutPoint = Collider.Center + Collider.Rotation.RotateVector(OnAxis + LocalNormal * Target);
		return true;
	}

	case EColliderShape::Box:
	default:
	{
		const FVector3f Local = Collider.Rotation.UnrotateVector(InOutPoint - Collider.Center);
		const FVector3f Extent = Collider.HalfExtent + FVector3f(Skin);

		if (FMath::Abs(Local.X) >= Extent.X || FMath::Abs(Local.Y) >= Extent.Y || FMath::Abs(Local.Z) >= Extent.Z)
		{
			return false;
		}

		// Inside: leave through the face with the least penetration.
		const FVector3f Penetration = Extent - FVector3f(FMath::Abs(Local.X), FMath::Abs(Local.Y), FMath::Abs(Local.Z));
		FVector3f LocalNormal = FVector3f::ZeroVector;
		FVector3f LocalPoint = Local;

		if (Penetration.X <= Penetration.Y && Penetration.X <= Penetration.Z)
		{
			const float Sign = Local.X >= 0.f ? 1.f : -1.f;
			LocalNormal = FVector3f(Sign, 0.f, 0.f);
			LocalPoint.X = Sign * Extent.X;
		}
		else if (Penetration.Y <= Penetration.Z)
		{
			const float Sign = Local.Y >= 0.f ? 1.f : -1.f;
			LocalNormal = FVector3f(0.f, Sign, 0.f);
			LocalPoint.Y = Sign * Extent.Y;
		}
		else
		{
			const float Sign = Local.Z >= 0.f ? 1.f : -1.f;
			LocalNormal = FVector3f(0.f, 0.f, Sign);
			LocalPoint.Z = Sign * Extent.Z;
		}

		OutNormal = Collider.Rotation.RotateVector(LocalNormal);
		InOutPoint = Collider.Center + Collider.Rotation.RotateVector(LocalPoint);
		return true;
	}
	}
}

void FSlimeSolver::ResolveCollisions()
{
	const int32 Count = Particles.Num();
	const EParallelForFlags Flags = Count < GParallelMinBatch ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None;

	FMemory::Memzero(ContactNormals.GetData(), Count * sizeof(FVector3f));

	const int32 Passes = FMath::Max(Params.CollisionPasses, 1);
	const float LocalContactRadius = ContactRadius;

	FMemory::Memzero(ContactLoads.GetData(), Count * sizeof(float));

	for (int32 Pass = 0; Pass < Passes; ++Pass)
	{
		ParallelFor(Count, [this, LocalContactRadius](int32 Index)
		{
			FSlimeParticle& Particle = Particles[Index];
			FVector3f Point = Particle.PredictedPosition;
			FVector3f Accumulated = FVector3f::ZeroVector;
			float Load = 0.f;

			if (!bSkipWorldCollision)
			{
				for (const FSlimeCollider& Collider : Colliders)
				{
					if (!Collider.Bounds.IsInsideOrOn(Point))
					{
						continue;
					}
					FVector3f Normal;
					const FVector3f Before = Point;
					if (ProjectOut(Collider, LocalContactRadius, Point, Normal))
					{
						Accumulated += Normal;
						Load += (Point - Before).Size();
					}
				}
			}

			// Body uses the capsule floor; each clone shot uses its own traced floor.
			float PlaneZ = FloorZ;
			if (Particle.IsBallistic())
			{
				PlaneZ = FragmentFloorZ;
				if (const float* ShotFloor = ShotFloorOverrides.Find(Particle.ShotId))
				{
					PlaneZ = *ShotFloor;
				}
			}
			if (Point.Z - LocalContactRadius < PlaneZ)
			{
				Load += PlaneZ + LocalContactRadius - Point.Z;
				Point.Z = PlaneZ + LocalContactRadius;
				Accumulated += FVector3f::UpVector;
			}

			if (Point.Z + LocalContactRadius > CeilingZ)
			{
				Load += Point.Z + LocalContactRadius - CeilingZ;
				Point.Z = CeilingZ - LocalContactRadius;
				Accumulated -= FVector3f::UpVector;
			}

			Particle.PredictedPosition = Point;
			ContactNormals[Index] += Accumulated;
			ContactLoads[Index] += Load;
		}, Flags);
	}

	float TotalLoad = 0.f;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		TotalLoad += ContactLoads[Index];
	}
	ContactLoad = Count > 0 ? TotalLoad / float(Count) : 0.f;
}

void FSlimeSolver::ApplyViscosity()
{
	if (Params.Viscosity <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const int32 Count = Particles.Num();
	const float InvCell = 1.f / GridCellSize;
	const float Strength = Params.Viscosity;
	const EParallelForFlags Flags = Count < GParallelMinBatch ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None;

	ParallelFor(Count, [this, InvCell, Strength](int32 Index)
	{
		const FVector3f Pi = Particles[Index].Position;
		const FVector3f Vi = Particles[Index].Velocity;
		const FIntVector Base = CellCoordOf(Pi, GridOrigin, InvCell, GridDims);

		FVector3f Accumulated = FVector3f::ZeroVector;
		float WeightSum = 0.f;

		for (int32 Z = FMath::Max(Base.Z - 1, 0); Z <= FMath::Min(Base.Z + 1, GridDims.Z - 1); ++Z)
		for (int32 Y = FMath::Max(Base.Y - 1, 0); Y <= FMath::Min(Base.Y + 1, GridDims.Y - 1); ++Y)
		for (int32 X = FMath::Max(Base.X - 1, 0); X <= FMath::Min(Base.X + 1, GridDims.X - 1); ++X)
		{
			const int32 Cell = X + GridDims.X * (Y + GridDims.Y * Z);
			for (int32 Entry = CellStart[Cell]; Entry < CellStart[Cell + 1]; ++Entry)
			{
				const int32 Other = CellEntries[Entry];
				if (Other == Index)
				{
					continue;
				}
				const float DistSq = (Pi - Particles[Other].Position).SizeSquared();
				if (DistSq >= SmoothingRadiusSq)
				{
					continue;
				}
				const float Weight = KernelPoly6(DistSq, SmoothingRadiusSq, Poly6Norm);
				Accumulated += (Particles[Other].Velocity - Vi) * Weight;
				WeightSum += Weight;
			}
		}

		ViscosityDelta[Index] = WeightSum > KINDA_SMALL_NUMBER ? Accumulated * (Strength / WeightSum) : FVector3f::ZeroVector;
	}, Flags);

	ParallelFor(Count, [this](int32 Index)
	{
		Particles[Index].Velocity += ViscosityDelta[Index];
	}, Flags);
}

void FSlimeSolver::RecountActiveShots()
{
	RebuildShotStates();
}

void FSlimeSolver::RemoveShotParticles(uint8 ShotId)
{
	for (int32 Index = Particles.Num() - 1; Index >= 0; --Index)
	{
		const FSlimeParticle& Particle = Particles[Index];
		if (Particle.IsBallistic() && Particle.ShotId == ShotId)
		{
			Particles.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}
	ShotMergeElapsed.Remove(ShotId);
	ShotImpactApplied.Remove(ShotId);
	ShotFloorOverrides.Remove(ShotId);
	RebuildShotStates();
	EnsureScratchCapacity(Particles.Num());
}

void FSlimeSolver::RemoveAllClones()
{
	for (int32 Index = Particles.Num() - 1; Index >= 0; --Index)
	{
		const FSlimeParticle& Particle = Particles[Index];
		if (Particle.IsClone() || Particle.IsBallistic())
		{
			Particles.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}
	NumBallistic = 0;
	ActiveShotCount = 0;
	ShotStates.Reset();
	ShotMergeElapsed.Reset();
	ShotImpactApplied.Reset();
	ShotFloorOverrides.Reset();
	EnsureScratchCapacity(Particles.Num());
}

void FSlimeSolver::ApplyMergeImpact(const FShotState& Shot)
{
	const int32 BodyCount = Params.NumParticles;
	if (BodyCount <= 0 || Shot.Count <= 0)
	{
		return;
	}

	const float ShotMass = float(Shot.Count);
	const float BodyMass = float(BodyCount);
	const float ImpulseScale = ShotMass / (ShotMass + BodyMass);
	const FVector3f DeltaV = Shot.Velocity * ImpulseScale;

	for (int32 Index = 0; Index < Particles.Num(); ++Index)
	{
		FSlimeParticle& Particle = Particles[Index];
		if (Particle.IsBallistic())
		{
			continue;
		}
		Particle.Velocity += DeltaV;
	}

	// Body duang: longer settle so the fused blob visibly swells then recovers.
	LandingSettleRemaining = FMath::Max(LandingSettleRemaining, 1.1f);
}

int32 FSlimeSolver::LaunchChunk(const FVector& LaunchVelocity, float Fraction, float Life, int32 MaxActiveShots)
{
	SetLaunchFraction(Fraction);

	const int32 BodyCount = Params.NumParticles;
	if (BodyCount <= 0 || Particles.Num() < BodyCount)
	{
		return 0;
	}

	RebuildShotStates();
	if (ActiveShotCount >= FMath::Max(MaxActiveShots, 1))
	{
		return 0;
	}

	const FVector3f Center(GetBodyCenter());
	const FVector3f Direction = FVector3f(LaunchVelocity).GetSafeNormal();

	struct FPick
	{
		int32 Index;
		float Score;
	};

	TArray<FPick> Picks;
	Picks.Reserve(BodyCount);

	// Templates are attached body particles only (never peel them).
	for (int32 Index = 0; Index < BodyCount && Index < Particles.Num(); ++Index)
	{
		const FSlimeParticle& Particle = Particles[Index];
		if (Particle.IsCore() || Particle.IsBallistic() || Particle.IsClone())
		{
			continue;
		}
		Picks.Add({ Index, (Particle.Position - Center) | Direction });
	}

	const int32 Budget = FMath::FloorToInt(float(BodyCount) * FMath::Clamp(Fraction, 0.05f, 0.6f));
	if (Budget <= 0 || Picks.Num() == 0)
	{
		return 0;
	}

	Picks.Sort([](const FPick& A, const FPick& B) { return A.Score > B.Score; });

	const int32 Launched = FMath::Min(Budget, Picks.Num());
	const int32 MaxClonePool = FMath::Max(Budget * FMath::Max(MaxActiveShots, 1), Budget);
	const int32 CurrentClones = Particles.Num() - BodyCount;
	if (CurrentClones + Launched > MaxClonePool)
	{
		return 0;
	}

	const uint8 ShotId = NextShotId;
	NextShotId = (NextShotId == 255) ? 1 : uint8(NextShotId + 1);

	const FVector3f LaunchVel(LaunchVelocity);
	const FVector3f Away = Direction.IsNearlyZero() ? FVector3f::ForwardVector : Direction;

	Particles.Reserve(Particles.Num() + Launched);
	for (int32 I = 0; I < Launched; ++I)
	{
		const FSlimeParticle& Template = Particles[Picks[I].Index];
		FSlimeParticle Clone = Template;
		Clone.Flags = PF_Ballistic | PF_Clone;
		Clone.ShotId = ShotId;
		Clone.BallisticLife = Life;
		Clone.Velocity = LaunchVel;
		// Peel clear of the absorb radius so the chunk is not immediately reabsorbed.
		Clone.Position = Template.Position + Away * (Params.RestRadius * 2.4f);
		Clone.PredictedPosition = Clone.Position;
		Particles.Add(Clone);
	}

	EnsureScratchCapacity(Particles.Num());
	ShotMergeElapsed.Remove(ShotId);
	ShotImpactApplied.Remove(ShotId);
	RebuildShotStates();
	return Launched;
}

int32 FSlimeSolver::UpdateSoftAbsorb(float Dt, float ApproachRadius, float CommitRadius, float HoldDuration)
{
	if (NumBallistic <= 0 || ApproachRadius <= KINDA_SMALL_NUMBER)
	{
		return 0;
	}

	RebuildShotStates();

	const FVector BodyCenter = GetBodyCenter();
	const float ApproachSq = FMath::Square(ApproachRadius);
	const float SurfaceStandoff = Params.RestRadius * 0.85f;
	const float CommitR = FMath::Max(CommitRadius, Params.RestRadius * 0.55f);
	const float CommitSq = FMath::Square(CommitR);
	const float Hold = FMath::Max(HoldDuration, 0.1f);
	const FVector3f Home(BodyCenter);
	const FVector3f HomeVel(AnchorVelocity);

	TArray<uint8> CommitShots;
	CommitShots.Reserve(ShotStates.Num());

	for (FShotState& Shot : ShotStates)
	{
		const float DistSq = FVector3f::DistSquared(Shot.Center, Home);
		float& MergeTime = ShotMergeElapsed.FindOrAdd(Shot.Id, -1.f);

		if (DistSq > ApproachSq)
		{
			if (MergeTime >= 0.f && DistSq > ApproachSq * 1.35f)
			{
				MergeTime = -1.f;
				ShotImpactApplied.Remove(Shot.Id);
			}
			continue;
		}

		if (MergeTime < 0.f)
		{
			MergeTime = 0.f;
		}
		else
		{
			MergeTime += Dt;
		}

		if (!ShotImpactApplied.Contains(Shot.Id))
		{
			ApplyMergeImpact(Shot);
			ShotImpactApplied.Add(Shot.Id);
		}

		// Surface-seeking pull: approach the body skin, do not lerp into the COM
		// (that reads as "sinking inside" instead of metaball edge fusion).
		const float PullBlend = FMath::Clamp(MergeTime / Hold, 0.f, 1.f);
		const float PullStrength = FMath::Lerp(120.f, 280.f, PullBlend);
		for (FSlimeParticle& Particle : Particles)
		{
			if (!Particle.IsBallistic() || Particle.ShotId != Shot.Id)
			{
				continue;
			}
			const FVector3f FromHome = Particle.Position - Home;
			const float Dist = FromHome.Size();
			FVector3f Target = Home;
			if (Dist > KINDA_SMALL_NUMBER)
			{
				Target = Home + (FromHome / Dist) * SurfaceStandoff;
				const FVector3f ToTarget = Target - Particle.Position;
				const float TargetDist = ToTarget.Size();
				if (TargetDist > KINDA_SMALL_NUMBER)
				{
					Particle.Velocity = FMath::Lerp(
						Particle.Velocity,
						HomeVel + (ToTarget / TargetDist) * PullStrength,
						0.22f);
					// Very weak position nudge — keep the blob outside so iso surfaces can fuse.
					Particle.Position = FMath::Lerp(Particle.Position, Target, 0.02f * Dt * 60.f);
					Particle.PredictedPosition = Particle.Position;
				}
			}
			Particle.BallisticLife = FMath::Max(Particle.BallisticLife, Hold + 0.25f);
		}

		// Commit only after a full hold AND the shot has reached the surface band.
		const bool bHeldLongEnough = MergeTime >= Hold;
		const bool bAtSurface = DistSq <= CommitSq;
		if (bHeldLongEnough && bAtSurface)
		{
			CommitShots.Add(Shot.Id);
		}
		else if (bHeldLongEnough && DistSq <= FMath::Square(ApproachRadius * 0.55f))
		{
			// Fallback if COM never quite enters CommitR (heavy push from density).
			CommitShots.Add(Shot.Id);
		}
	}

	int32 Absorbed = 0;
	for (const uint8 ShotId : CommitShots)
	{
		const int32 Before = Particles.Num();
		RemoveShotParticles(ShotId);
		Absorbed += FMath::Max(Before - Particles.Num(), 0);
	}

	if (Absorbed > 0)
	{
		RebuildShotStates();
	}
	return Absorbed;
}

bool FSlimeSolver::RecallFragments(float Dt, const FVector& Target, float PullSpeed)
{
	if (NumBallistic <= 0)
	{
		return true;
	}

	const FVector3f Home(Target);
	const float ArriveRadius = Params.RestRadius * 0.8f;
	const float ArriveSq = FMath::Square(ArriveRadius);
	int32 StillOut = 0;

	RebuildShotStates();

	for (FSlimeParticle& Particle : Particles)
	{
		if (!Particle.IsBallistic())
		{
			continue;
		}

		const FVector3f Delta = Home - Particle.Position;
		const float Distance = Delta.Size();
		if (Distance <= ArriveRadius)
		{
			// Enter soft-merge instead of instant destroy so duang can play.
			float& MergeTime = ShotMergeElapsed.FindOrAdd(Particle.ShotId, -1.f);
			if (MergeTime < 0.f)
			{
				MergeTime = 0.f;
			}
			Particle.BallisticLife = FMath::Max(Particle.BallisticLife, 0.6f);
			continue;
		}

		const float Speed = PullSpeed * FMath::Min(1.f, Distance / 100.f + 0.35f);
		Particle.Velocity = (Delta / Distance) * Speed;
		Particle.BallisticLife = FMath::Max(Particle.BallisticLife, Dt * 4.f);
		++StillOut;
	}

	// Kick impact once a shot crosses arrive radius.
	for (const FShotState& Shot : ShotStates)
	{
		if (FVector3f::DistSquared(Shot.Center, Home) > ArriveSq)
		{
			continue;
		}
		float& MergeTime = ShotMergeElapsed.FindOrAdd(Shot.Id, -1.f);
		if (MergeTime < 0.f)
		{
			MergeTime = 0.f;
		}
		if (!ShotImpactApplied.Contains(Shot.Id))
		{
			ApplyMergeImpact(Shot);
			ShotImpactApplied.Add(Shot.Id);
		}
	}

	RebuildShotStates();
	return ShotStates.Num() == 0;
}

void FSlimeSolver::SnapFragmentsHome(const FVector& Target)
{
	(void)Target;
	RemoveAllClones();
}
