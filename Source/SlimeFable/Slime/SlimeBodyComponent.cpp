// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeBodyComponent.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "SlimeFable.h"
#include "HAL/IConsoleManager.h"

using namespace SlimeSim;

static TAutoConsoleVariable<int32> CVarSlimeBodyVisualScaleOnly(
	TEXT("slime.BodyVisualScaleOnly"),
	0,
	TEXT("If 1, devour body scale only inflates the isosurface (no solver SizeScale)."),
	ECVF_Default);

namespace SlimeBodyPrivate
{
	/** Clearance kept between the capsule and a ceiling so the sweep does not re-hit it. */
	constexpr float CeilingSkin = 2.f;

	/** Lift for the horizontal probe so it measures walls, not the floor. */
	constexpr float ProbeGroundLift = 2.f;

	/** How far a squeeze value has to move before the delegate fires. */
	constexpr float SqueezeReportEpsilon = 0.02f;

	constexpr float NoCeilingZ = 1.e9f;
}

USlimeBodyComponent::USlimeBodyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Runs after movement so the anchor is read from the capsule's final position for the frame.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	bAutoActivate = true;
}

void USlimeBodyComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (OwnerCharacter)
	{
		OwnerCapsule = OwnerCharacter->GetCapsuleComponent();
		if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
		{
			DefaultStepHeight = Movement->MaxStepHeight;
			DefaultWalkSpeed = Movement->MaxWalkSpeed;
			DefaultJumpZ = Movement->JumpZVelocity;
		}
	}

	if (!SurfaceMesh)
	{
		SurfaceMesh = GetOwner() ? GetOwner()->FindComponentByClass<UProceduralMeshComponent>() : nullptr;
	}

	if (OwnerCapsule && bAdaptiveCapsule)
	{
		OwnerCapsule->SetCapsuleSize(DefaultCapsuleRadius, DefaultCapsuleHalfHeight, true);
	}

	ResolveMaterial();

	const FVector Foot = GetFootLocation();
	FloorZ = float(Foot.Z);
	Solver.Initialize(SolverParams, Foot + FVector(0.0, 0.0, SolverParams.RestRadius * AnchorHeightFraction));
	Surface.Configure(SurfaceParams, SolverParams.ParticleSpacing);

	LastColliderGatherCenter = Solver.GetBodyCenter();
	RefreshColliders();
	RebuildSurface();
}

void USlimeBodyComponent::ApplyParams()
{
	Solver.SetParams(SolverParams);
	Surface.Configure(SurfaceParams, SolverParams.ParticleSpacing);
	// The vertex budget defines the index buffer, so the section has to be recreated.
	bMeshSectionCreated = false;
	bShadowMeshSectionCreated = false;
	bXRayMeshSectionCreated = false;
}

void USlimeBodyComponent::ResolveMaterial()
{
	ResolvedMaterial = BodyMaterial;
	if (!ResolvedMaterial && !BodyMaterialPath.IsNull())
	{
		ResolvedMaterial = BodyMaterialPath.LoadSynchronous();
	}
	if (!ResolvedMaterial)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeBodyComponent: no body material assigned on '%s'; the surface will use the engine default."), *GetNameSafe(GetOwner()));
	}

	ResolvedShadowMaterial = ShadowCasterMaterial;
	if (!ResolvedShadowMaterial && !ShadowCasterMaterialPath.IsNull())
	{
		ResolvedShadowMaterial = ShadowCasterMaterialPath.LoadSynchronous();
	}

	ResolvedXRayMaterial = XRayMaterial;
	if (!ResolvedXRayMaterial && !XRayMaterialPath.IsNull())
	{
		ResolvedXRayMaterial = XRayMaterialPath.LoadSynchronous();
	}
}

FVector USlimeBodyComponent::GetFootLocation() const
{
	if (OwnerCapsule)
	{
		return OwnerCapsule->GetComponentLocation() - FVector(0.0, 0.0, OwnerCapsule->GetScaledCapsuleHalfHeight());
	}
	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

void USlimeBodyComponent::GetActiveShotCenters(TArray<FVector>& OutCenters) const
{
	const_cast<FSlimeSolver&>(Solver).RefreshShotStates();
	Solver.GetShotCenters(OutCenters);
}

FVector USlimeBodyComponent::GetShotCenter(uint8 ShotId) const
{
	const_cast<FSlimeSolver&>(Solver).RefreshShotStates();
	return Solver.GetShotCenterWorld(ShotId);
}

void USlimeBodyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Solver.GetParticles().Num() == 0)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(SlimeBody_Tick);

	UpdateQuality();

	const float StepDelta = 1.f / FMath::Max(StepRate, 1.f);
	StepAccumulator += DeltaTime;

	int32 Steps = 0;
	while (StepAccumulator >= StepDelta && Steps < MaxStepsPerFrame)
	{
		StepAccumulator -= StepDelta;
		++Steps;
		FixedStep(StepDelta);
	}
	if (StepAccumulator > StepDelta)
	{
		// Dropped time rather than letting the backlog grow without bound.
		StepAccumulator = 0.f;
	}

	// Off screen the simulation keeps running but the surface does not: rebuilding a mesh
	// nobody can see is the easiest cost to delete.
	const bool bVisible = !SurfaceMesh || SurfaceMesh->WasRecentlyRendered(0.3f);
	SurfaceAccumulator += DeltaTime;
	const float SurfaceDelta = 1.f / FMath::Max(SurfaceRate, 1.f);
	bool bRebuilt = false;
	if (SurfaceAccumulator >= SurfaceDelta)
	{
		SurfaceAccumulator = FMath::Fmod(SurfaceAccumulator, SurfaceDelta);
		if (bVisible)
		{
			RebuildSurface();
			bRebuilt = true;
		}
	}

	// Between rebuilds, slide the world-space mesh with the body COM so 60 Hz surfaces do not
	// freeze against a 120 Hz display. Skip while fragments fly — one mesh holds both clusters.
	if (!bRebuilt)
	{
		UpdateMeshFollow();
	}
}

void USlimeBodyComponent::FixedStep(float StepDelta)
{
	UpdateFloor();
	ProbeSqueeze(StepDelta);
	TryOozeEscape(StepDelta);

	ColliderTimer += StepDelta;
	const FVector Center = Solver.GetBodyCenter();
	FVector GatherWatch = Center;
	if (Solver.HasFragments())
	{
		FVector FragCenter;
		if (Solver.GetFragmentCenter(FragCenter))
		{
			GatherWatch = (Center + FragCenter) * 0.5f;
		}
	}
	const float RefreshInterval = Solver.HasFragments()
		? FMath::Min(ColliderRefreshInterval, 0.08f)
		: ColliderRefreshInterval;
	if (ColliderTimer >= RefreshInterval ||
		FVector::DistSquared(GatherWatch, LastColliderGatherCenter) > FMath::Square(ColliderRefreshDistance))
	{
		ColliderTimer = 0.f;
		RefreshColliders();
	}

	UpdateAnchor();

	// Pancake state, including the soft reform after the key is released.
	const float HalfHeight = FMath::Max(SpreadHalfHeight, 0.5f);
	if (bSpread)
	{
		Solver.SetSpread(true, SolverParams.RestRadius * SpreadRadiusScale, SpreadPush, HalfHeight);
		Solver.SetSpreadConcentrationScale(SpreadConcentrationScale);
		Solver.SetGravityScale(SpreadGravityScale);
	}
	else if (SpreadRecoverRemaining > 0.f)
	{
		SpreadRecoverRemaining = FMath::Max(SpreadRecoverRemaining - StepDelta, 0.f);
		const float Alpha = SpreadRecoverDuration > 0.f ? SpreadRecoverRemaining / SpreadRecoverDuration : 0.f;
		Solver.SetSpread(false, 0.f, 0.f, HalfHeight);
		Solver.SetSpreadConcentrationScale(1.f);
		Solver.SetGravityScale(FMath::Lerp(1.f, SpreadGravityScale, Alpha));
	}
	else
	{
		Solver.SetSpread(false, 0.f, 0.f, HalfHeight);
		Solver.SetSpreadConcentrationScale(1.f);
		Solver.SetGravityScale(1.f);
	}

	// Recall pulls fragments; soft-merge (after Step) finishes the duang before destroy.
	if (bRecalling)
	{
		RecallElapsed += StepDelta;
		Solver.SetSkipWorldCollision(true);
		const FVector Home = Solver.GetBodyCenter();
		Solver.RecallFragments(StepDelta, Home, GetEffectiveRecallPullSpeed());
		if (RecallElapsed >= RecallTimeout)
		{
			Solver.SnapFragmentsHome(Home);
			SetRecalling(false);
		}
	}
	else if (Solver.HasShotTargets())
	{
		Solver.SetSkipWorldCollision(true);
	}
	else
	{
		Solver.SetSkipWorldCollision(false);
	}

	Solver.SetFloorZ(FloorZ);
	Solver.SetFragmentFloorZ(FragmentFloorZ);
	Solver.SetCeilingZ(CeilingZ);
	Solver.SetSqueeze(SqueezeAmount, SqueezeFreeDirection);
	Solver.SetLaunchFraction(LaunchFractionOverride > KINDA_SMALL_NUMBER ? LaunchFractionOverride : LaunchFraction);
	if (bClingVisual && !bSpread)
	{
		Solver.SetClingPlane(true, ClingPoint, ClingNormal);
	}
	else
	{
		Solver.SetClingPlane(false, FVector::ZeroVector, FVector::UpVector);
	}
	Solver.Step(StepDelta);
	SweepKinematicShots();

	// Soft absorb AFTER step so contact/density can wobble before clones commit-destroy.
	if (Solver.HasFragments())
	{
		const float ApproachR = AbsorbMergeRadius > KINDA_SMALL_NUMBER
			? AbsorbMergeRadius
			: SolverParams.RestRadius * 1.6f;
		const float CommitR = AbsorbCommitRadius > KINDA_SMALL_NUMBER
			? AbsorbCommitRadius
			: SolverParams.RestRadius * 0.7f;
		Solver.UpdateSoftAbsorb(StepDelta, ApproachR, CommitR, MergeHoldDuration);
	}

	if (bRecalling && !Solver.HasFragments())
	{
		SetRecalling(false);
	}
}

void USlimeBodyComponent::UpdateFloor()
{
	const FVector Foot = GetFootLocation();

	auto TraceFloorUnder = [this](const FVector& Origin, float ProxyRadius, float& OutZ)
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return false;
		}

		FCollisionQueryParams Query(TEXT("SlimeFloor"), false, GetOwner());
		FHitResult Hit;
		const FVector Start = Origin + FVector(0.0, 0.0, 40.0);
		const FVector End = Origin - FVector(0.0, 0.0, 800.0);
		const float Radius = FMath::Max(ProxyRadius, 2.f);
		if (World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius), Query))
		{
			OutZ = float(Hit.ImpactPoint.Z);
			return true;
		}
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Pawn, Query))
		{
			OutZ = float(Hit.ImpactPoint.Z);
			return true;
		}
		return false;
	};

	bool bBodyFloor = false;
	float MovementFloorZ = 0.f;
	if (bClingVisual)
	{
		// Keep the leftover horizontal plane at the capsule so a far ground trace
		// cannot stretch the blob into a hanging sheet.
		FloorZ = float(Foot.Z - 8.0);
		bBodyFloor = true;
	}
	else if (OwnerCharacter)
	{
		const UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
		if (Movement && Movement->CurrentFloor.bBlockingHit)
		{
			MovementFloorZ = float(Movement->CurrentFloor.HitResult.ImpactPoint.Z);
			FloorZ = MovementFloorZ;
			bBodyFloor = true;
		}
	}
	if (!bBodyFloor)
	{
		if (!TraceFloorUnder(Foot, 4.f, FloorZ))
		{
			// Airborne over a void: keep the plane below the body so it does not clamp anything.
			FloorZ = float(Foot.Z - 400.0);
		}
	}
	else if (!bClingVisual && OwnerCharacter)
	{
		// Prefer the floor directly under the feet when CurrentFloor perched on a higher tread.
		float NearFloorZ = FloorZ;
		if (TraceFloorUnder(Foot, 4.f, NearFloorZ) && MovementFloorZ > NearFloorZ + 6.f)
		{
			FloorZ = NearFloorZ;
		}

		UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
		if (Movement && Movement->IsMovingOnGround())
		{
			FVector HorizVel = Movement->Velocity;
			HorizVel.Z = 0.0;
			const float Hang = float(Foot.Z) - FloorZ;
			const float MaxSnap = FMath::Max(Movement->MaxStepHeight, DefaultStepHeight);
			if (HorizVel.SizeSquared() < 400.0
				&& Movement->Velocity.Z > -50.f
				&& Hang > 4.f
				&& Hang <= MaxSnap)
			{
				OwnerCharacter->AddActorWorldOffset(FVector(0.0, 0.0, double(-Hang)), true);
			}
		}
	}

	FVector FragmentCenter;
	if (Solver.GetFragmentCenter(FragmentCenter))
	{
		const float ProxyR = FragmentProxyRadius > KINDA_SMALL_NUMBER
			? FragmentProxyRadius
			: SolverParams.RestRadius * 0.45f;
		if (!TraceFloorUnder(FragmentCenter, ProxyR, FragmentFloorZ))
		{
			FragmentFloorZ = float(FragmentCenter.Z - 800.0);
		}
	}
	else
	{
		FragmentFloorZ = FloorZ;
	}

	Solver.ClearShotFloorOverrides();
	Solver.RefreshShotStates();
	for (const FSlimeSolver::FShotState& Shot : Solver.GetShotStates())
	{
		const float ProxyR = FragmentProxyRadius > KINDA_SMALL_NUMBER
			? FragmentProxyRadius
			: SolverParams.RestRadius * 0.45f;
		float ShotFloor = FragmentFloorZ;
		if (!TraceFloorUnder(FVector(Shot.Center), ProxyR, ShotFloor))
		{
			ShotFloor = float(Shot.Center.Z - 800.0);
		}
		Solver.SetShotFloorZ(Shot.Id, ShotFloor);
	}
}

void USlimeBodyComponent::RefreshColliders()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(SlimeBody_RefreshColliders);

	FBox QueryBounds = Solver.GetBodyBounds();
	if (Solver.HasFragments())
	{
		const FBox FragmentBounds = Solver.GetFragmentBounds();
		if (FragmentBounds.IsValid)
		{
			if (QueryBounds.IsValid)
			{
				QueryBounds += FragmentBounds;
			}
			else
			{
				QueryBounds = FragmentBounds;
			}
			QueryBounds = QueryBounds.ExpandBy(FragmentColliderRadius);
		}
	}
	const FVector Center = QueryBounds.IsValid ? QueryBounds.GetCenter() : GetFootLocation();
	const FVector Extent = (QueryBounds.IsValid ? QueryBounds.GetExtent() : FVector(SolverParams.RestRadius)) * ColliderQueryScale;
	LastColliderGatherCenter = Center;

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams Query(TEXT("SlimeColliders"), false, GetOwner());

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, FCollisionShape::MakeBox(FVector3f(Extent)), Query);

	// Extra local queries under each flying shot so distant clones keep floor/wall colliders.
	const float ShotQueryExtent = FMath::Max(FragmentColliderRadius * 2.5f, SolverParams.RestRadius * 1.2f);
	for (const FSlimeSolver::FShotState& Shot : Solver.GetShotStates())
	{
		TArray<FOverlapResult> ShotOverlaps;
		World->OverlapMultiByObjectType(
			ShotOverlaps,
			FVector(Shot.Center),
			FQuat::Identity,
			ObjectParams,
			FCollisionShape::MakeBox(FVector3f(ShotQueryExtent)),
			Query);
		Overlaps.Append(ShotOverlaps);
	}

	// The floor under our feet must be in the set. The reference implementation calls this
	// out explicitly: miss it and the body sinks through slopes.
	UPrimitiveComponent* FloorComponent = nullptr;
	if (OwnerCharacter)
	{
		const UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
		if (Movement && Movement->CurrentFloor.bBlockingHit)
		{
			FloorComponent = Movement->CurrentFloor.HitResult.GetComponent();
		}
	}

	TArray<TWeakObjectPtr<UPrimitiveComponent>> Ordered;
	Ordered.Reserve(Overlaps.Num() + 1);
	TSet<UPrimitiveComponent*> Seen;
	if (FloorComponent)
	{
		Ordered.Add(FloorComponent);
		Seen.Add(FloorComponent);
	}
	for (const FOverlapResult& Overlap : Overlaps)
	{
		UPrimitiveComponent* Component = Overlap.GetComponent();
		if (Component && Component->IsCollisionEnabled() && !Seen.Contains(Component))
		{
			Seen.Add(Component);
			Ordered.Add(Component);
		}
	}

	TArray<FSlimeCollider> Gathered;
	Gathered.Reserve(MaxWorldColliders);

	const float Skin = SolverParams.ParticleSpacing;

	for (const TWeakObjectPtr<UPrimitiveComponent>& WeakComponent : Ordered)
	{
		if (Gathered.Num() >= MaxWorldColliders)
		{
			break;
		}

		UPrimitiveComponent* Component = WeakComponent.Get();
		if (!Component)
		{
			continue;
		}

		const UBodySetup* Setup = Component->GetBodySetup();
		if (!Setup)
		{
			continue;
		}

		const FTransform ComponentTM = Component->GetComponentTransform();
		const FVector Scale3D = ComponentTM.GetScale3D();
		const float RadialScale = float(FMath::Min(FMath::Abs(Scale3D.X), FMath::Abs(Scale3D.Y)));

		auto AddCollider = [&Gathered, Skin](FSlimeCollider&& Collider)
		{
			// Bounds are pre-expanded so the solver's broad phase is a single point test.
			const float Reach = FMath::Max3(Collider.HalfExtent.GetMax(), Collider.Radius + Collider.HalfHeight, 1.f) + Skin;
			Collider.Bounds = FBox3f(Collider.Center - FVector3f(Reach), Collider.Center + FVector3f(Reach));
			Gathered.Add(MoveTemp(Collider));
		};

		for (const FKBoxElem& Elem : Setup->AggGeom.BoxElems)
		{
			if (Gathered.Num() >= MaxWorldColliders)
			{
				break;
			}
			FSlimeCollider Collider;
			Collider.Shape = EColliderShape::Box;
			Collider.Center = FVector3f(ComponentTM.TransformPosition(Elem.Center));
			Collider.Rotation = FQuat4f(ComponentTM.GetRotation() * Elem.Rotation.Quaternion());
			Collider.HalfExtent = FVector3f(
				float(Elem.X * 0.5 * FMath::Abs(Scale3D.X)),
				float(Elem.Y * 0.5 * FMath::Abs(Scale3D.Y)),
				float(Elem.Z * 0.5 * FMath::Abs(Scale3D.Z)));
			AddCollider(MoveTemp(Collider));
		}

		for (const FKSphereElem& Elem : Setup->AggGeom.SphereElems)
		{
			if (Gathered.Num() >= MaxWorldColliders)
			{
				break;
			}
			FSlimeCollider Collider;
			Collider.Shape = EColliderShape::Sphere;
			Collider.Center = FVector3f(ComponentTM.TransformPosition(Elem.Center));
			Collider.Radius = float(Elem.Radius) * float(Scale3D.GetAbsMin());
			AddCollider(MoveTemp(Collider));
		}

		for (const FKSphylElem& Elem : Setup->AggGeom.SphylElems)
		{
			if (Gathered.Num() >= MaxWorldColliders)
			{
				break;
			}
			FSlimeCollider Collider;
			Collider.Shape = EColliderShape::Capsule;
			Collider.Center = FVector3f(ComponentTM.TransformPosition(Elem.Center));
			Collider.Rotation = FQuat4f(ComponentTM.GetRotation() * Elem.Rotation.Quaternion());
			Collider.Radius = float(Elem.Radius) * RadialScale;
			Collider.HalfHeight = float(Elem.Length * 0.5) * float(FMath::Abs(Scale3D.Z));
			AddCollider(MoveTemp(Collider));
		}

		// Convex hulls are skipped on purpose: half space iteration was the single most
		// expensive collision path in the reference implementation, and level geometry that
		// matters for squeezing is box shaped.
	}

	Solver.SetColliders(MoveTemp(Gathered));
}

void USlimeBodyComponent::ProbeSqueeze(float DeltaTime)
{
	using namespace SlimeBodyPrivate;

	if (bClingVisual)
	{
		CeilingZ = NoCeilingZ;
		SqueezeAmount = 0.f;
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !OwnerCapsule)
	{
		CeilingZ = NoCeilingZ;
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(SlimeBody_ProbeSqueeze);

	const float CurrentRadius = OwnerCapsule->GetUnscaledCapsuleRadius();
	const float CurrentHalfHeight = OwnerCapsule->GetUnscaledCapsuleHalfHeight();
	const FVector Foot = GetFootLocation();

	FCollisionQueryParams Query(TEXT("SlimeSqueeze"), false, GetOwner());

	// ---- Low ceiling -----------------------------------------------------------------

	const float ProbeCeiling = DefaultCapsuleHalfHeight * 2.f + 40.f;
	const float StepCeilingIgnore = DefaultStepHeight + 8.f;
	float AvailableHeight = ProbeCeiling;
	{
		const float SphereRadius = FMath::Max(MinCapsuleRadius * 0.9f, 2.f);
		const FVector Start = Foot + FVector(0.0, 0.0, double(SphereRadius + ProbeGroundLift));
		const FVector End = Foot + FVector(0.0, 0.0, double(ProbeCeiling));
		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(SphereRadius), Query)
			&& float(Hit.ImpactNormal.GetSafeNormal().Z) <= -0.45f)
		{
			const float HitHeight = float(Hit.ImpactPoint.Z - Foot.Z);
			// Stair tread undersides sit within one step of the feet — not a real ceiling.
			if (HitHeight > StepCeilingIgnore)
			{
				AvailableHeight = HitHeight;
			}
		}
		CeilingZ = AvailableHeight < ProbeCeiling ? float(Foot.Z) + AvailableHeight : NoCeilingZ;
	}

	if (HeightSqueezeSuppressRemaining > 0.f)
	{
		HeightSqueezeSuppressRemaining = FMath::Max(HeightSqueezeSuppressRemaining - DeltaTime, 0.f);
		AvailableHeight = ProbeCeiling;
		CeilingZ = NoCeilingZ;
	}

	// ---- Narrow gap ------------------------------------------------------------------

	float FreeRadius = DefaultCapsuleRadius;
	{
		const float ProbeHalfHeight = MinCapsuleHalfHeight;
		const FVector BaseCenter = Foot + FVector(0.0, 0.0, double(ProbeHalfHeight + ProbeGroundLift));
		FVector ProbeCenter = BaseCenter;

		FVector Heading = GetOwner()->GetVelocity();
		Heading.Z = 0.0;
		if (Heading.IsNearlyZero() && OwnerCharacter)
		{
			if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
			{
				Heading = Movement->GetLastInputVector();
				Heading.Z = 0.0;
			}
		}
		const FVector HeadingDir = Heading.IsNearlyZero() ? FVector::ZeroVector : Heading.GetSafeNormal();
		if (!HeadingDir.IsNearlyZero())
		{
			ProbeCenter += HeadingDir * double(LookAheadDistance);
		}

		auto IsBlockedAt = [World, ProbeHalfHeight, &Query](const FVector& Center, float Radius)
		{
			return World->OverlapBlockingTestByChannel(
				Center, FQuat::Identity, ECC_Pawn,
				FCollisionShape::MakeCapsule(Radius, ProbeHalfHeight), Query);
		};

		auto IsStepRiserPinch = [&]() -> bool
		{
			if (HeadingDir.IsNearlyZero())
			{
				return false;
			}
			const FVector SweepStart = BaseCenter;
			const FVector SweepEnd = BaseCenter + HeadingDir * double(FMath::Max(LookAheadDistance, DefaultCapsuleRadius));
			FHitResult RiserHit;
			if (!World->SweepSingleByChannel(
				RiserHit, SweepStart, SweepEnd, FQuat::Identity, ECC_Pawn,
				FCollisionShape::MakeSphere(FMath::Max(MinCapsuleRadius * 0.8f, 2.f)), Query))
			{
				return false;
			}
			const float NormalZ = float(RiserHit.ImpactNormal.GetSafeNormal().Z);
			if (FMath::Abs(NormalZ) > 0.35f)
			{
				return false;
			}
			const float HitAboveFoot = float(RiserHit.ImpactPoint.Z - Foot.Z);
			return HitAboveFoot >= -4.f && HitAboveFoot <= DefaultStepHeight + 8.f;
		};

		// A solid wall / stair riser ahead is not a corridor. Only shrink radius for true side pinches.
		if (IsBlockedAt(ProbeCenter, DefaultCapsuleRadius) && IsBlockedAt(BaseCenter, DefaultCapsuleRadius)
			&& !IsStepRiserPinch())
		{
			float Low = MinCapsuleRadius;
			float High = DefaultCapsuleRadius;
			for (int32 Iteration = 0; Iteration < RadiusProbeIterations; ++Iteration)
			{
				const float Mid = (Low + High) * 0.5f;
				if (IsBlockedAt(ProbeCenter, Mid))
				{
					High = Mid;
				}
				else
				{
					Low = Mid;
				}
			}
			FreeRadius = Low;
		}
	}

	// ---- Targets ---------------------------------------------------------------------

	const float HeightRange = FMath::Max(DefaultCapsuleHalfHeight - MinCapsuleHalfHeight, KINDA_SMALL_NUMBER);
	const float RadiusRange = FMath::Max(DefaultCapsuleRadius - MinCapsuleRadius, KINDA_SMALL_NUMBER);

	float TargetHalfHeight = FMath::Clamp((AvailableHeight - CeilingSkin) * 0.5f, MinCapsuleHalfHeight, DefaultCapsuleHalfHeight);
	float TargetRadius = FMath::Clamp(FreeRadius, MinCapsuleRadius, DefaultCapsuleRadius);

	if (ForcedSqueeze > 0.f)
	{
		TargetHalfHeight = FMath::Lerp(TargetHalfHeight, MinCapsuleHalfHeight, ForcedSqueeze);
		TargetRadius = FMath::Lerp(TargetRadius, MinCapsuleRadius, ForcedSqueeze);
	}

	const float HeightSqueeze = 1.f - (TargetHalfHeight - MinCapsuleHalfHeight) / HeightRange;
	const float RadiusSqueeze = 1.f - (TargetRadius - MinCapsuleRadius) / RadiusRange;
	SqueezeAmount = FMath::Clamp(FMath::Max(HeightSqueeze, RadiusSqueeze), 0.f, 1.f);

	// Volume has to go somewhere: up when a ceiling presses down, along the gap when walls
	// pinch from the sides.
	if (HeightSqueeze >= RadiusSqueeze)
	{
		FVector Heading = GetOwner()->GetVelocity();
		Heading.Z = 0.0;
		SqueezeFreeDirection = Heading.IsNearlyZero() ? FVector::ZeroVector : Heading.GetSafeNormal();
	}
	else
	{
		SqueezeFreeDirection = FVector::UpVector;
	}

	if (FMath::Abs(SqueezeAmount - ReportedSqueeze) > SqueezeReportEpsilon)
	{
		ReportedSqueeze = SqueezeAmount;
		OnSqueezeChanged.Broadcast(SqueezeAmount);
	}

	if (!bAdaptiveCapsule)
	{
		return;
	}

	// ---- Drive the capsule -----------------------------------------------------------

	const bool bShrinking = TargetHalfHeight < CurrentHalfHeight || TargetRadius < CurrentRadius;
	const float Speed = 1.f / FMath::Max(bShrinking ? ShrinkTime : RecoverTime, 0.01f);
	float NewHalfHeight = FMath::FInterpTo(CurrentHalfHeight, TargetHalfHeight, DeltaTime, Speed);
	float NewRadius = FMath::FInterpTo(CurrentRadius, TargetRadius, DeltaTime, Speed);

	if (NewHalfHeight > CurrentHalfHeight || NewRadius > CurrentRadius)
	{
		const FVector GrownCenter = Foot + FVector(0.0, 0.0, double(NewHalfHeight));
		TArray<FOverlapResult> Overlaps;
		if (World->OverlapMultiByChannel(Overlaps, GrownCenter, FQuat::Identity, ECC_Pawn,
			FCollisionShape::MakeCapsule(NewRadius, NewHalfHeight), Query))
		{
			bool bBlockedByCeiling = false;
			for (const FOverlapResult& Overlap : Overlaps)
			{
				UPrimitiveComponent* Comp = Overlap.GetComponent();
				if (!Comp)
				{
					continue;
				}
				FVector Closest = GrownCenter;
				Comp->GetClosestPointOnCollision(GrownCenter, Closest);
				const FVector Away = (GrownCenter - Closest).GetSafeNormal();
				if (Away.Z <= -0.45f)
				{
					bBlockedByCeiling = true;
					break;
				}
			}
			if (bBlockedByCeiling)
			{
				NewHalfHeight = CurrentHalfHeight;
				NewRadius = CurrentRadius;
			}
		}
	}

	if (!FMath::IsNearlyEqual(NewHalfHeight, CurrentHalfHeight, 0.05f) || !FMath::IsNearlyEqual(NewRadius, CurrentRadius, 0.05f))
	{
		ApplyCapsuleSize(NewRadius, NewHalfHeight);
	}
}

void USlimeBodyComponent::TryOozeEscape(float DeltaTime)
{
	if (bClingVisual || bSpread || SqueezeAmount < 0.55f || !OwnerCharacter || !OwnerCapsule)
	{
		return;
	}

	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	if (!Movement || Movement->IsFlying())
	{
		return;
	}

	FVector Dir = Movement->GetLastInputVector();
	Dir.Z = 0.0;
	if (Dir.IsNearlyZero())
	{
		Dir = Movement->GetPendingInputVector();
		Dir.Z = 0.0;
	}
	if (Dir.IsNearlyZero())
	{
		return;
	}

	// Only ooze when actually stuck — avoid paying SafeMove every squeezed walk frame.
	FVector HorizVel = Movement->Velocity;
	HorizVel.Z = 0.0;
	if (HorizVel.SizeSquared() > 2500.0)
	{
		return;
	}

	Dir = Dir.GetSafeNormal();

	// Prefer the squeeze free axis when it roughly agrees with input (crawl out of the pinch).
	FVector Free = SqueezeFreeDirection;
	Free.Z = 0.0;
	if (!Free.IsNearlyZero() && (Free.GetSafeNormal() | Dir) > 0.2f)
	{
		Dir = Free.GetSafeNormal();
	}

	Movement->MaxWalkSpeed = FMath::Max(Movement->MaxWalkSpeed, DefaultWalkSpeed * 0.35f);

	const float Radius = OwnerCapsule->GetScaledCapsuleRadius();
	const float Step = FMath::Min(OozeSpeed * DeltaTime, Radius);
	FHitResult Hit;
	Movement->SafeMoveUpdatedComponent(Dir * double(Step), OwnerCharacter->GetActorQuat(), true, Hit);
}

void USlimeBodyComponent::ApplyCapsuleSize(float NewRadius, float NewHalfHeight)
{
	if (!OwnerCapsule || !OwnerCharacter)
	{
		return;
	}

	const float PreviousHalfHeight = OwnerCapsule->GetUnscaledCapsuleHalfHeight();
	OwnerCapsule->SetCapsuleSize(NewRadius, NewHalfHeight, true);

	// SetCapsuleSize works around the centre, so shift the actor to keep the feet planted.
	OwnerCharacter->AddActorWorldOffset(FVector(0.0, 0.0, double(NewHalfHeight - PreviousHalfHeight)), false);

	if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
	{
		const float BaseStep = StepHeightBoost > KINDA_SMALL_NUMBER ? StepHeightBoost : DefaultStepHeight;
		Movement->MaxStepHeight = BaseStep;
		const float Scaled = DefaultWalkSpeed * FMath::Lerp(1.f, SqueezeSpeedScale, SqueezeAmount) * ExternalMoveSpeedScale;
		Movement->MaxWalkSpeed = SqueezeAmount >= 0.55f
			? FMath::Max(Scaled, DefaultWalkSpeed * 0.35f * ExternalMoveSpeedScale)
			: Scaled;
		Movement->JumpZVelocity = DefaultJumpZ * ExternalJumpScale;
	}
}

void USlimeBodyComponent::UpdateAnchor()
{
	if (!GetOwner())
	{
		return;
	}

	FVector Anchor;
	if (bClingVisual)
	{
		Anchor = ClingPoint + ClingNormal * double(SolverParams.RestRadius * AnchorHeightFraction);
	}
	else
	{
		const FVector Foot = GetFootLocation();
		Anchor = Foot + FVector(0.0, 0.0, double(SolverParams.RestRadius * AnchorHeightFraction));
	}
	Solver.SetAnchor(Anchor, GetOwner()->GetVelocity());

	// Rubber band: if the body ends up dragged a long way from the capsule, pull the capsule
	// back rather than letting the two drift apart forever.
	if (OwnerCharacter && !bClingVisual)
	{
		const FVector Center = Solver.GetBodyCenter();
		FVector Offset = Center - Anchor;
		Offset.Z = 0.0;
		const double Distance = Offset.Size();
		if (Distance > double(MaxAnchorDistance))
		{
			const FVector Correction = Offset.GetSafeNormal() * (Distance - double(MaxAnchorDistance));
			OwnerCharacter->AddActorWorldOffset(Correction, true);
		}
	}
}

void USlimeBodyComponent::ApplyLandingSquash(float ImpactSpeed)
{
	if (ImpactSpeed < LandingSquashMinSpeed)
	{
		return;
	}
	Solver.ApplyLandingSquash(ImpactSpeed);
}

void USlimeBodyComponent::ApplyAirBounce()
{
	Solver.ApplyAirBounce();
}

void USlimeBodyComponent::SetCombatPose(const FSlimeCombatPoseState& Pose)
{
	Solver.SetCombatPose(Pose);
}

void USlimeBodyComponent::ClearCombatPose()
{
	FSlimeCombatPoseState Empty;
	Solver.SetCombatPose(Empty);
}

void USlimeBodyComponent::ApplyHitJolt()
{
	Solver.ApplyHitJolt();
}

void USlimeBodyComponent::SetBodyScale(float NewScale, bool bIgnoreSqueeze)
{
	RequestedBodyScale = FMath::Max(NewScale, 0.05f);

	const float Squeeze = bIgnoreSqueeze ? 0.f : FMath::Clamp(SqueezeAmount, 0.f, 1.f);
	const float Attenuated = 1.f + (RequestedBodyScale - 1.f) * (1.f - Squeeze);
	const bool bVisualOnly = bVisualOnlyBodyScale || CVarSlimeBodyVisualScaleOnly.GetValueOnGameThread() != 0;
	const float SolverScale = bVisualOnly ? 1.f : Attenuated;
	const bool bWantBudget = RequestedBodyScale > 1.2f || Attenuated > 1.2f;

	if (bWantBudget && !bEnlargedSurfaceBudget)
	{
		SavedSurfaceMaxVertices = SurfaceParams.MaxVertices;
		SavedSurfaceMaxGridDim = SurfaceParams.MaxGridDim;
		SurfaceParams.MaxVertices = FMath::Min(16000, FMath::Max(SurfaceParams.MaxVertices, 14000));
		SurfaceParams.MaxGridDim = FMath::Min(64, FMath::Max(SurfaceParams.MaxGridDim, 56));
		bMeshSectionCreated = false;
		bShadowMeshSectionCreated = false;
		bXRayMeshSectionCreated = false;
		bEnlargedSurfaceBudget = true;
		bWarnedTruncation = false;
	}
	else if (!bWantBudget && bEnlargedSurfaceBudget)
	{
		SurfaceParams.MaxVertices = SavedSurfaceMaxVertices;
		SurfaceParams.MaxGridDim = SavedSurfaceMaxGridDim;
		bMeshSectionCreated = false;
		bShadowMeshSectionCreated = false;
		bXRayMeshSectionCreated = false;
		bEnlargedSurfaceBudget = false;
	}

	bFreezeQualityLod = SolverScale > 1.05f || Attenuated > 1.05f || RequestedBodyScale > 1.05f;
	Solver.SetSizeScale(SolverScale);
	if (RequestedBodyScale <= 1.05f)
	{
		VisualZLift = 0.f;
	}
}

void USlimeBodyComponent::RebuildSurface()
{
	if (!SurfaceMesh)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(SlimeBody_RebuildSurface);

	RebuildBodyCOM = Solver.GetBodyCenter();
	bHaveRebuildBodyCOM = true;
	SurfaceMesh->SetWorldLocation(FVector::ZeroVector);
	if (ShadowMesh)
	{
		ShadowMesh->SetWorldLocation(FVector::ZeroVector);
	}
	if (XRayMesh)
	{
		XRayMesh->SetWorldLocation(FVector::ZeroVector);
	}

	FSlimeSurfaceParams ActiveSurface = SurfaceParams;
	if (bSpread || Solver.GetLandingSettleRemaining() > 0.f)
	{
		// Thin connected sheet: wider XY splat, flatter Z, lower iso, lighter blur.
		ActiveSurface.SplatRadiusMultiplier = FMath::Max(ActiveSurface.SplatRadiusMultiplier, SpreadSplatMultiplier);
		ActiveSurface.SplatZScale = FMath::Min(ActiveSurface.SplatZScale, SpreadSplatZScale);
		ActiveSurface.IsoThreshold = FMath::Min(ActiveSurface.IsoThreshold, 0.17f);
		ActiveSurface.BlurPasses = FMath::Min(ActiveSurface.BlurPasses, 1);
		ActiveSurface.MaxGridDim = FMath::Max(ActiveSurface.MaxGridDim, 44);
		ActiveSurface.CellSizeMultiplier = FMath::Min(ActiveSurface.CellSizeMultiplier, 0.75f);
	}

	const float SurfaceScale = FMath::Max(
		bVisualOnlyBodyScale || CVarSlimeBodyVisualScaleOnly.GetValueOnGameThread() != 0
			? RequestedBodyScale
			: Solver.GetSizeScale(),
		0.05f);
	const bool bVisualOnly = bVisualOnlyBodyScale || CVarSlimeBodyVisualScaleOnly.GetValueOnGameThread() != 0;
	if (bVisualOnly && RequestedBodyScale > 1.f)
	{
		ActiveSurface.IsoThreshold = FMath::Max(0.08f, ActiveSurface.IsoThreshold / FMath::Max(RequestedBodyScale, 1.f));
	}

	VisualZLift = 0.f;
	float ClipZ = -1.e9f;
	if (bVisualOnly && RequestedBodyScale > 1.05f && FloorZ > -1.e8f)
	{
		const float VisualR = SolverParams.RestRadius * RequestedBodyScale;
		VisualZLift = FMath::Max(0.f, VisualR - (RebuildBodyCOM.Z - FloorZ));
		ClipZ = FloorZ;
	}

	const float ConfigureSpacing = SolverParams.ParticleSpacing * SurfaceScale;
	const bool bNeedConfigure = !Surface.IsConfigured()
		|| !FMath::IsNearlyEqual(Surface.GetParticleSpacing(), ConfigureSpacing)
		|| !FMath::IsNearlyEqual(Surface.GetParams().SplatRadiusMultiplier, ActiveSurface.SplatRadiusMultiplier)
		|| !FMath::IsNearlyEqual(Surface.GetParams().SplatZScale, ActiveSurface.SplatZScale)
		|| !FMath::IsNearlyEqual(Surface.GetParams().IsoThreshold, ActiveSurface.IsoThreshold)
		|| Surface.GetParams().MaxGridDim != ActiveSurface.MaxGridDim
		|| Surface.GetParams().MaxVertices != ActiveSurface.MaxVertices
		|| Surface.GetParams().BlurPasses != ActiveSurface.BlurPasses
		|| !FMath::IsNearlyEqual(Surface.GetParams().CellSizeMultiplier, ActiveSurface.CellSizeMultiplier);
	if (bNeedConfigure)
	{
		Surface.Configure(ActiveSurface, ConfigureSpacing);
	}

	TArray<uint8> MergingIds;
	Solver.GetMergingShotIds(MergingIds);
	Surface.Build(Solver.GetParticles(), Solver.GetBodyCenter(), MergingIds, VisualZLift, ClipZ);

	if (Surface.WasTruncated() && !bWarnedTruncation)
	{
		bWarnedTruncation = true;
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeBodyComponent: surface hit the %d vertex budget; raise SurfaceParams.MaxVertices or the cell size."), SurfaceParams.MaxVertices);
	}

	PushMeshSection();
}

void USlimeBodyComponent::UpdateMeshFollow()
{
	if (!bHaveRebuildBodyCOM || !SurfaceMesh || Solver.HasFragments())
	{
		return;
	}

	const FVector Offset = Solver.GetBodyCenter() - RebuildBodyCOM;
	SurfaceMesh->SetWorldLocation(Offset);
	if (ShadowMesh)
	{
		ShadowMesh->SetWorldLocation(Offset);
	}
	if (XRayMesh)
	{
		XRayMesh->SetWorldLocation(Offset);
	}
}

void USlimeBodyComponent::PushMeshSection()
{
	const TArray<FVector>& Vertices = Surface.GetVertices();
	if (Vertices.Num() == 0 || !SurfaceMesh)
	{
		return;
	}

	// The material drives itself from world position and normals, so UVs, colours and tangents
	// stay empty rather than being rebuilt every frame for nothing.
	const TArray<FVector2D> NoUVs;
	const TArray<FLinearColor> NoColors;
	const TArray<FProcMeshTangent> NoTangents;
	const TArray<FVector>& Normals = Surface.GetNormals();
	const TArray<int32>& Indices = Surface.GetIndices();

	if (!bMeshSectionCreated)
	{
		SurfaceMesh->ClearAllMeshSections();
		SurfaceMesh->CreateMeshSection_LinearColor(
			0, Vertices, Indices, Normals,
			NoUVs, NoColors, NoTangents, false);
		if (ResolvedMaterial)
		{
			SurfaceMesh->SetMaterial(0, ResolvedMaterial);
		}
		bMeshSectionCreated = true;
	}
	else
	{
		// Vertex count is constant by design, so this is an in place update: no reallocation and
		// no collision cook, which is what made the reference implementation expensive.
		SurfaceMesh->UpdateMeshSection_LinearColor(0, Vertices, Normals, NoUVs, NoColors, NoTangents);
	}

	// CreateMeshSection can reset component shadow flags; keep the jelly casting-free.
	SurfaceMesh->SetCastShadow(false);
	SurfaceMesh->bCastDynamicShadow = false;
	SurfaceMesh->bCastVolumetricTranslucentShadow = false;
	SurfaceMesh->bCastContactShadow = false;
	SurfaceMesh->bReceiveMobileCSMShadows = false;

	// Slightly shrink the opaque proxy / x-ray so edges don't stick to the translucent shell.
	constexpr float ShadowProxyScale = 0.92f;
	FVector ShadowCentroid = FVector::ZeroVector;
	for (const FVector& Vertex : Vertices)
	{
		ShadowCentroid += Vertex;
	}
	ShadowCentroid /= float(Vertices.Num());

	TArray<FVector> ShadowVertices;
	ShadowVertices.Reserve(Vertices.Num());
	for (const FVector& Vertex : Vertices)
	{
		ShadowVertices.Add(ShadowCentroid + (Vertex - ShadowCentroid) * ShadowProxyScale);
	}

	if (ShadowMesh)
	{
		if (!bShadowMeshSectionCreated)
		{
			ShadowMesh->ClearAllMeshSections();
			ShadowMesh->CreateMeshSection_LinearColor(
				0, ShadowVertices, Indices, Normals,
				NoUVs, NoColors, NoTangents, false);
			if (ResolvedShadowMaterial)
			{
				ShadowMesh->SetMaterial(0, ResolvedShadowMaterial);
			}
			bShadowMeshSectionCreated = true;
		}
		else
		{
			ShadowMesh->UpdateMeshSection_LinearColor(0, ShadowVertices, Normals, NoUVs, NoColors, NoTangents);
		}

		// Hidden from view; only casts a ground shadow. Create/Update can reset flags.
		ShadowMesh->SetHiddenInGame(true);
		ShadowMesh->SetVisibility(false);
		// Honour the suppression flag: while morphed the slime is parked out of the world and
		// must not keep stamping a shadow on the ground where the morph started.
		const bool bShouldCast = !bShadowCastSuppressed;
		ShadowMesh->bCastHiddenShadow = bShouldCast;
		ShadowMesh->SetCastShadow(bShouldCast);
		ShadowMesh->bCastDynamicShadow = bShouldCast;
		ShadowMesh->bCastVolumetricTranslucentShadow = false;
		ShadowMesh->bCastContactShadow = false;
	}

	if (XRayMesh)
	{
		if (!bXRayMeshSectionCreated)
		{
			XRayMesh->ClearAllMeshSections();
			XRayMesh->CreateMeshSection_LinearColor(
				0, ShadowVertices, Indices, Normals,
				NoUVs, NoColors, NoTangents, false);
			if (ResolvedXRayMaterial)
			{
				XRayMesh->SetMaterial(0, ResolvedXRayMaterial);
			}
			bXRayMeshSectionCreated = true;
		}
		else
		{
			XRayMesh->UpdateMeshSection_LinearColor(0, ShadowVertices, Normals, NoUVs, NoColors, NoTangents);
		}

		XRayMesh->SetHiddenInGame(false);
		XRayMesh->SetVisibility(true);
		XRayMesh->SetCastShadow(false);
		XRayMesh->bCastDynamicShadow = false;
		XRayMesh->bCastVolumetricTranslucentShadow = false;
		XRayMesh->bCastContactShadow = false;
		// Do not reassign material every rebuild — ElementComponent owns the MID.
	}
}

void USlimeBodyComponent::UpdateQuality()
{
	if (!bAutoQuality || bFreezeQualityLod)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Distance to the local view, which for the player's own slime is always the near tier.
	float DistanceSq = 0.f;
	if (const APlayerController* PlayerController = World->GetFirstPlayerController())
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
		DistanceSq = float(FVector::DistSquared(ViewLocation, Solver.GetBodyCenter()));
	}

	ESlimeSimQuality Desired = ESlimeSimQuality::High;
	if (DistanceSq > FMath::Square(LowQualityDistance))
	{
		Desired = ESlimeSimQuality::Low;
	}
	else if (DistanceSq > FMath::Square(MediumQualityDistance))
	{
		Desired = ESlimeSimQuality::Medium;
	}

	if (Desired != Quality)
	{
		SetQuality(Desired);
	}
}

void USlimeBodyComponent::SetQuality(ESlimeSimQuality InQuality)
{
	if (Quality == InQuality)
	{
		return;
	}

	const ESlimeSimQuality Previous = Quality;
	Quality = InQuality;

	switch (Quality)
	{
	case ESlimeSimQuality::High:
		StepRate = 60.f;
		SurfaceRate = 60.f;
		SolverParams.DensityIterations = 2;
		SurfaceParams.CellSizeMultiplier = 0.85f;
		SurfaceParams.MaxGridDim = 36;
		SurfaceParams.BlurPasses = 2;
		break;

	case ESlimeSimQuality::Medium:
		StepRate = 40.f;
		SurfaceRate = 40.f;
		SolverParams.DensityIterations = 1;
		SurfaceParams.CellSizeMultiplier = 1.0f;
		SurfaceParams.MaxGridDim = 28;
		SurfaceParams.BlurPasses = 2;
		break;

	case ESlimeSimQuality::Low:
		StepRate = 24.f;
		SurfaceRate = 24.f;
		SolverParams.DensityIterations = 1;
		SurfaceParams.CellSizeMultiplier = 1.2f;
		SurfaceParams.MaxGridDim = 14;
		SurfaceParams.BlurPasses = 0;
		break;
	}

	// Changing the particle budget rebuilds the dome, which pops. Only ever do that on the
	// low tier, which by definition is far enough away that nobody sees it.
	if (bQualityScalesParticleCount && (Quality == ESlimeSimQuality::Low || Previous == ESlimeSimQuality::Low))
	{
		SolverParams.NumParticles = Quality == ESlimeSimQuality::Low ? 128 : 384;
	}

	ApplyParams();
}

void USlimeBodyComponent::SetSpread(bool bInSpread)
{
	if (bSpread == bInSpread)
	{
		return;
	}

	bSpread = bInSpread;
	if (bSpread)
	{
		SpreadRecoverRemaining = 0.f;
		// Pancaking is a deliberate flatten, so drive the capsule down without waiting for
		// a probe to notice a ceiling.
		SetForcedSqueeze(1.f);
	}
	else
	{
		SpreadRecoverRemaining = SpreadRecoverDuration;
		SetForcedSqueeze(0.f);
	}
}

void USlimeBodyComponent::SetStepHeightBoost(float BoostedMaxStep)
{
	StepHeightBoost = FMath::Max(BoostedMaxStep, 0.f);
	if (!OwnerCharacter || !OwnerCapsule)
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
	{
		const float BaseStep = StepHeightBoost > KINDA_SMALL_NUMBER ? StepHeightBoost : DefaultStepHeight;
		Movement->MaxStepHeight = BaseStep;
	}
}

void USlimeBodyComponent::SetClingVisual(bool bInCling, const FVector& Point, const FVector& Normal)
{
	bClingVisual = bInCling;
	ClingPoint = Point;
	ClingNormal = Normal.GetSafeNormal();
	if (ClingNormal.IsNearlyZero())
	{
		ClingNormal = FVector::ForwardVector;
		bClingVisual = false;
	}
}

void USlimeBodyComponent::SuppressHeightSqueeze(float Duration)
{
	HeightSqueezeSuppressRemaining = FMath::Max(HeightSqueezeSuppressRemaining, Duration);
}

void USlimeBodyComponent::SetShadowCastSuppressed(bool bSuppressed)
{
	if (bShadowCastSuppressed == bSuppressed)
	{
		return;
	}
	bShadowCastSuppressed = bSuppressed;

	// Apply immediately — the next surface rebuild would otherwise be a frame late, and if the
	// body has stopped rebuilding it would never arrive at all.
	if (ShadowMesh)
	{
		const bool bShouldCast = !bShadowCastSuppressed;
		ShadowMesh->bCastHiddenShadow = bShouldCast;
		ShadowMesh->SetCastShadow(bShouldCast);
		ShadowMesh->bCastDynamicShadow = bShouldCast;
		ShadowMesh->MarkRenderStateDirty();
	}
}

void USlimeBodyComponent::ResetBody()
{
	bSpread = false;
	bRecalling = false;
	bClingVisual = false;
	StepHeightBoost = 0.f;
	SpreadRecoverRemaining = 0.f;
	RecallElapsed = 0.f;
	ForcedSqueeze = 0.f;
	HeightSqueezeSuppressRemaining = 0.f;

	const FVector Foot = GetFootLocation();
	Solver.Reset(Foot + FVector(0.0, 0.0, double(SolverParams.RestRadius * AnchorHeightFraction)));

	if (OwnerCapsule && bAdaptiveCapsule)
	{
		ApplyCapsuleSize(DefaultCapsuleRadius, DefaultCapsuleHalfHeight);
	}

	RefreshColliders();
	RebuildSurface();
}

int32 USlimeBodyComponent::LaunchChunk(const FVector& LaunchVelocity)
{
	return LaunchTendril(LaunchVelocity, LaunchFraction, FragmentLifetime);
}

int32 USlimeBodyComponent::LaunchChunkAlongPath(const FSlimeLaunchPath& Path)
{
	const FVector Velocity = Path.bValid ? Path.LaunchVelocity : FVector::ZeroVector;
	const int32 Launched = Solver.LaunchChunk(Velocity, LaunchFraction, FragmentLifetime, MaxActiveShots, &Path);
	if (Launched > 0)
	{
		SetRecalling(false);
	}
	return Launched;
}

int32 USlimeBodyComponent::LaunchTendril(const FVector& LaunchVelocity, float Fraction, float Life)
{
	const int32 Launched = Solver.LaunchChunk(LaunchVelocity, Fraction, Life, MaxActiveShots);
	if (Launched > 0)
	{
		SetRecalling(false);
	}
	return Launched;
}

int32 USlimeBodyComponent::LaunchDevourShot(const FVector& LaunchVelocity, float Fraction, float Life, uint8& OutShotId)
{
	OutShotId = 0;
	constexpr int32 DevourShotCap = 8;
	const int32 Launched = Solver.LaunchChunk(LaunchVelocity, Fraction, Life, DevourShotCap, nullptr, &OutShotId);
	if (Launched > 0)
	{
		SetRecalling(false);
	}
	return Launched;
}

void USlimeBodyComponent::SetShotTarget(uint8 ShotId, const FVector& Target, float PullSpeed)
{
	Solver.SetShotTarget(ShotId, Target, PullSpeed);
}

void USlimeBodyComponent::ClearShotTargets()
{
	Solver.ClearShotTargets();
}

void USlimeBodyComponent::AddIgnoreWorldShot(uint8 ShotId)
{
	Solver.AddIgnoreWorldShot(ShotId);
}

void USlimeBodyComponent::ClearIgnoreWorldShots()
{
	Solver.ClearIgnoreWorldShots();
}

void USlimeBodyComponent::SetRecallPullSpeedOverride(float Speed)
{
	RecallPullSpeedOverride = Speed > KINDA_SMALL_NUMBER ? Speed : 0.f;
}

void USlimeBodyComponent::ClearRecallPullSpeedOverride()
{
	RecallPullSpeedOverride = 0.f;
}

float USlimeBodyComponent::GetEffectiveRecallPullSpeed() const
{
	return RecallPullSpeedOverride > KINDA_SMALL_NUMBER ? RecallPullSpeedOverride : RecallPullSpeed;
}

void USlimeBodyComponent::ClearFragments()
{
	Solver.SnapFragmentsHome(Solver.GetBodyCenter());
	SetRecalling(false);
}

void USlimeBodyComponent::SetLaunchFractionOverride(float Fraction)
{
	LaunchFractionOverride = Fraction > KINDA_SMALL_NUMBER ? FMath::Clamp(Fraction, 0.05f, 0.6f) : 0.f;
	if (LaunchFractionOverride > KINDA_SMALL_NUMBER)
	{
		Solver.SetLaunchFraction(LaunchFractionOverride);
	}
}

void USlimeBodyComponent::SweepKinematicShots()
{
	UWorld* World = GetWorld();
	if (!World || bRecalling)
	{
		return;
	}

	TArray<FSlimeSolver::FKinematicShotMotion> Motions;
	Solver.GetKinematicShotMotions(Motions);
	if (Motions.Num() == 0)
	{
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeLaunchFollow), false, GetOwner());
	for (const FSlimeSolver::FKinematicShotMotion& Motion : Motions)
	{
		if (FVector::DistSquared(Motion.PrevCenter, Motion.Center) < 1.f)
		{
			continue;
		}

		FHitResult Hit;
		const FCollisionShape Shape = FCollisionShape::MakeSphere(FMath::Max(Motion.Radius, 8.f));
		if (World->SweepSingleByChannel(Hit, Motion.PrevCenter, Motion.Center, FQuat::Identity, ECC_Visibility, Shape, Params))
		{
			const FVector Snap = Hit.ImpactPoint + Hit.ImpactNormal * Motion.Radius;
			Solver.SnapKinematicShotTo(Motion.Id, Snap);
		}
	}
}

void USlimeBodyComponent::SetRecalling(bool bInRecalling)
{
	if (bInRecalling && !Solver.HasFragments())
	{
		return;
	}
	bRecalling = bInRecalling;
	RecallElapsed = 0.f;
	if (!bRecalling)
	{
		ClearRecallPullSpeedOverride();
	}
}
