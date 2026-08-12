// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeTypes.generated.h"

/** Simulation fidelity tier. Selected per frame from screen distance and visibility. */
UENUM(BlueprintType)
enum class ESlimeSimQuality : uint8
{
	High,
	Medium,
	Low
};

/**
 *  Tunable solver settings.
 *  Everything here is runtime data so particle budget and resolution can be changed
 *  from a Blueprint or a quality tier without recompiling.
 */
USTRUCT(BlueprintType)
struct SLIMEFABLE_API FSlimeSolverParams
{
	GENERATED_BODY()

	/** Particle budget. Solver cost scales close to linearly with this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "96", ClampMax = "768"))
	int32 NumParticles = 384;

	/** Rest spacing between particles, in cm. Drives kernel radius and overall blob volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "2.0", ClampMax = "12.0"))
	float ParticleSpacing = 4.6f;

	/** Kernel radius expressed as a multiple of ParticleSpacing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "1.6", ClampMax = "3.5"))
	float SmoothingMultiplier = 2.4f;

	/** Radius of the containment membrane the blob relaxes into, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "8.0", ClampMax = "120.0"))
	float RestRadius = 27.f;

	/** Density constraint relaxation iterations per fixed step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "1", ClampMax = "4"))
	int32 DensityIterations = 2;

	/** Gravity along Z, cm/s^2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver")
	float Gravity = -1400.f;

	/**
	 *  Natural frequency of the spring that carries the blob along with the capsule, in Hz.
	 *  Lower lags further behind and reads softer; above ~8 it stops looking like jelly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float AnchorFollowFrequency = 3.6f;

	/** Damping ratio for the anchor spring. 1 is critically damped, below that wobbles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "0.2", ClampMax = "2.0"))
	float AnchorDamping = 0.95f;

	/**
	 *  Membrane pulling stray particles back inside the rest radius, in cm/s^2 per cm of
	 *  overshoot. This is what keeps the body a blob instead of a puddle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "0.0", ClampMax = "6000.0"))
	float MembraneStiffness = 1200.f;

	/**
	 *  How far the membrane is allowed to stretch at full squeeze, as a fraction of RestRadius.
	 *  Without this the membrane fights the volume that a narrow gap displaces.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MembraneSqueezeStretch = 1.f;

	/** XSPH velocity blending, 0 = watery, 0.4 = very gooey. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "0.0", ClampMax = "0.6"))
	float Viscosity = 0.22f;

	/** Per second velocity damping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "0.0", ClampMax = "8.0"))
	float LinearDamping = 1.1f;

	/** Artificial pressure term, removes particle clustering and reads as surface tension. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "0.0", ClampMax = "0.4"))
	float SurfaceTension = 0.08f;

	/** Speed clamp, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "100.0"))
	float MaxSpeed = 1400.f;

	/** Tangential velocity kept when a particle is projected out of geometry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlideFriction = 0.72f;

	/** Bounce kept along the contact normal. Slime should barely bounce. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "0.0", ClampMax = "0.8"))
	float Restitution = 0.05f;

	/**
	 *  Collider projection passes per fixed step. A single pass gets ping-ponged between
	 *  two opposing walls, which shows up as jitter or a particle squirting through.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "1", ClampMax = "4"))
	int32 CollisionPasses = 2;

	/** Fraction of particles that stay bound to the core and can never be launched away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (ClampMin = "0.1", ClampMax = "0.9"))
	float CoreFraction = 0.35f;

	/**
	 *  Soft sticky pull toward the body centre (SIM Concentration). Applied inside the rest
	 *  radius and fading out to GripRadiusScale * RestRadius. Keeps the body one blob.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver|Cohesion", meta = (ClampMin = "0.0", ClampMax = "200.0"))
	float Concentration = 48.f;

	/** Outer sticky shell as a multiple of the current membrane radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver|Cohesion", meta = (ClampMin = "1.05", ClampMax = "2.0"))
	float GripRadiusScale = 1.35f;

	/** Restores dome height by pulling particles below the COM upward (0 while spread). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver|Cohesion", meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float UpwardRestore = 18.f;

	/** Max particle speed relative to the capsule anchor, cm/s. Kills impact spray. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver|Cohesion", meta = (ClampMin = "50.0", ClampMax = "2000.0"))
	float MaxRelSpeed = 420.f;

	/** Hard tether slack beyond the shape shell. Small on purpose: deform, never fragment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver|Cohesion", meta = (ClampMin = "1.0", ClampMax = "1.4"))
	float TetherSlack = 1.15f;

	/**
	 *  How strongly horizontal velocity stretches the body into an inertia ellipsoid.
	 *  0 = sphere, 1 = full front-squash / rear-trail at ReferenceWalkSpeed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver|Inertia", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InertiaStretch = 0.55f;

	/** How quickly the inertia shape tracks velocity changes (higher = snappier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver|Inertia", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float InertiaResponse = 10.f;

	/** Speed that maps to full inertia stretch, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver|Inertia", meta = (ClampMin = "50.0"))
	float ReferenceWalkSpeed = 420.f;

	/** Extra concentration while landing settle is active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver|Cohesion", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float LandingCohesionBoost = 1.8f;

	/** Kernel radius in cm. */
	FORCEINLINE float GetSmoothingRadius() const
	{
		return ParticleSpacing * SmoothingMultiplier;
	}
};

/** Tunables for turning the particle set into a visible surface. */
USTRUCT(BlueprintType)
struct SLIMEFABLE_API FSlimeSurfaceParams
{
	GENERATED_BODY()

	/** Density grid cell size as a multiple of ParticleSpacing. Larger is cheaper and blobbier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0.5", ClampMax = "2.5"))
	float CellSizeMultiplier = 1.2f;

	/** Splat radius as a multiple of ParticleSpacing. Below ~1.5 the surface breaks into lumps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "1.2", ClampMax = "3.5"))
	float SplatRadiusMultiplier = 2.4f;

	/** Iso level on the normalised density field. Lower inflates the blob. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0.05", ClampMax = "0.9"))
	float IsoThreshold = 0.24f;

	/** Hard cap on grid samples per axis. Cell size grows rather than exceeding this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "8", ClampMax = "48"))
	int32 MaxGridDim = 24;

	/**
	 *  Vertex budget. Kept fixed so the render section can be updated in place instead of
	 *  recreated, which is where the reference implementation spent most of its time.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "300", ClampMax = "12000"))
	int32 MaxVertices = 9000;

	/** Separable 1-2-1 smoothing passes over the density field. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0", ClampMax = "3"))
	int32 BlurPasses = 1;
};

namespace SlimeSim
{
	/** Particle state flags. */
	enum EParticleFlags : uint8
	{
		PF_None = 0,
		/** Bound to the core: excluded from launches so the body can never be emptied out. */
		PF_Core = 1 << 0,
		/** Detached chunk in flight. Ignores the membrane and the anchor spring. */
		PF_Ballistic = 1 << 1
	};

	struct FSlimeParticle
	{
		FVector3f Position = FVector3f::ZeroVector;
		FVector3f PredictedPosition = FVector3f::ZeroVector;
		FVector3f Velocity = FVector3f::ZeroVector;
		float BallisticLife = 0.f;
		uint8 Flags = PF_None;

		FORCEINLINE bool IsCore() const { return (Flags & PF_Core) != 0; }
		FORCEINLINE bool IsBallistic() const { return (Flags & PF_Ballistic) != 0; }
	};

	enum class EColliderShape : uint8
	{
		Box,
		Sphere,
		Capsule
	};

	/**
	 *  A world collision primitive baked into solver space.
	 *  Only the three analytic shapes are kept; convex hulls are skipped on purpose because
	 *  half-space iteration is the single most expensive part of the reference implementation.
	 */
	struct FSlimeCollider
	{
		EColliderShape Shape = EColliderShape::Box;
		/** Centre in world space, cm. */
		FVector3f Center = FVector3f::ZeroVector;
		/** Rotation from shape local space to world. */
		FQuat4f Rotation = FQuat4f::Identity;
		/** Box half extent. */
		FVector3f HalfExtent = FVector3f::ZeroVector;
		/** Sphere and capsule radius. */
		float Radius = 0.f;
		/** Capsule half height of the cylindrical section, caps excluded. */
		float HalfHeight = 0.f;
		/** World space AABB used for broad phase rejection. */
		FBox3f Bounds = FBox3f(ForceInit);
	};

	/** Poly6 density kernel. Expects R2 = r^2 and H2 = h^2. */
	FORCEINLINE float KernelPoly6(float R2, float H2, float Poly6Norm)
	{
		if (R2 >= H2)
		{
			return 0.f;
		}
		const float D = H2 - R2;
		return Poly6Norm * D * D * D;
	}

	/** Spiky gradient magnitude, already negated so it points away from the neighbour. */
	FORCEINLINE float KernelSpikyGrad(float R, float H, float SpikyNorm)
	{
		if (R >= H || R <= KINDA_SMALL_NUMBER)
		{
			return 0.f;
		}
		const float D = H - R;
		return SpikyNorm * D * D;
	}

	FORCEINLINE FIntVector CellCoord(const FVector3f& Position, float InvCellSize)
	{
		return FIntVector(
			FMath::FloorToInt(Position.X * InvCellSize),
			FMath::FloorToInt(Position.Y * InvCellSize),
			FMath::FloorToInt(Position.Z * InvCellSize));
	}

	FORCEINLINE uint32 HashCell(const FIntVector& Coord, uint32 TableSize)
	{
		const uint32 H = uint32(Coord.X * 73856093) ^ uint32(Coord.Y * 19349663) ^ uint32(Coord.Z * 83492791);
		return H & (TableSize - 1);
	}
}
