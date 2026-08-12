// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeBodyComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "SlimeFable.h"

using namespace SlimeSim;

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
}

FVector USlimeBodyComponent::GetFootLocation() const
{
	if (OwnerCapsule)
	{
		return OwnerCapsule->GetComponentLocation() - FVector(0.0, 0.0, OwnerCapsule->GetScaledCapsuleHalfHeight());
	}
	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
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
	if (SurfaceAccumulator >= SurfaceDelta)
	{
		SurfaceAccumulator = FMath::Fmod(SurfaceAccumulator, SurfaceDelta);
		if (bVisible)
		{
			RebuildSurface();
		}
	}
}

void USlimeBodyComponent::FixedStep(float StepDelta)
{
	UpdateFloor();
	ProbeSqueeze(StepDelta);

	ColliderTimer += StepDelta;
	const FVector Center = Solver.GetBodyCenter();
	if (ColliderTimer >= ColliderRefreshInterval ||
		FVector::DistSquared(Center, LastColliderGatherCenter) > FMath::Square(ColliderRefreshDistance))
	{
		ColliderTimer = 0.f;
		RefreshColliders();
	}

	UpdateAnchor();

	// Pancake state, including the soft reform after the key is released.
	if (bSpread)
	{
		Solver.SetSpread(true, SolverParams.RestRadius * SpreadRadiusScale, SpreadPush);
		Solver.SetGravityScale(SpreadGravityScale);
	}
	else if (SpreadRecoverRemaining > 0.f)
	{
		SpreadRecoverRemaining = FMath::Max(SpreadRecoverRemaining - StepDelta, 0.f);
		const float Alpha = SpreadRecoverDuration > 0.f ? SpreadRecoverRemaining / SpreadRecoverDuration : 0.f;
		Solver.SetSpread(false, 0.f, 0.f);
		Solver.SetGravityScale(FMath::Lerp(1.f, SpreadGravityScale, Alpha));
	}
	else
	{
		Solver.SetSpread(false, 0.f, 0.f);
		Solver.SetGravityScale(1.f);
	}

	// Recall pulls fragments straight through geometry so none of them can get stranded.
	if (bRecalling)
	{
		RecallElapsed += StepDelta;
		Solver.SetSkipWorldCollision(true);
		const FVector Home = Solver.GetBodyCenter();
		if (Solver.RecallFragments(StepDelta, Home, RecallPullSpeed) || RecallElapsed >= RecallTimeout)
		{
			Solver.SnapFragmentsHome(Home);
			SetRecalling(false);
		}
	}
	else
	{
		Solver.SetSkipWorldCollision(false);
	}

	Solver.SetFloorZ(FloorZ);
	Solver.SetCeilingZ(CeilingZ);
	Solver.SetSqueeze(SqueezeAmount, SqueezeFreeDirection);
	Solver.Step(StepDelta);
}

void USlimeBodyComponent::UpdateFloor()
{
	const FVector Foot = GetFootLocation();

	if (OwnerCharacter)
	{
		const UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
		if (Movement && Movement->CurrentFloor.bBlockingHit)
		{
			FloorZ = float(Movement->CurrentFloor.HitResult.ImpactPoint.Z);
			return;
		}
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FCollisionQueryParams Query(TEXT("SlimeFloor"), false, GetOwner());
	FHitResult Hit;
	const FVector Start = Foot + FVector(0.0, 0.0, 20.0);
	const FVector End = Foot - FVector(0.0, 0.0, 400.0);
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Pawn, Query))
	{
		FloorZ = float(Hit.ImpactPoint.Z);
	}
	else
	{
		// Airborne over a void: keep the plane below the body so it does not clamp anything.
		FloorZ = float(Foot.Z - 400.0);
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

	const FBox BodyBounds = Solver.GetBodyBounds();
	const FVector Center = BodyBounds.IsValid ? BodyBounds.GetCenter() : GetFootLocation();
	const FVector Extent = (BodyBounds.IsValid ? BodyBounds.GetExtent() : FVector(SolverParams.RestRadius)) * ColliderQueryScale;
	LastColliderGatherCenter = Solver.GetBodyCenter();

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams Query(TEXT("SlimeColliders"), false, GetOwner());

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, FCollisionShape::MakeBox(FVector3f(Extent)), Query);

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
	if (FloorComponent)
	{
		Ordered.Add(FloorComponent);
	}
	for (const FOverlapResult& Overlap : Overlaps)
	{
		UPrimitiveComponent* Component = Overlap.GetComponent();
		if (Component && Component != FloorComponent && Component->IsCollisionEnabled())
		{
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
	float AvailableHeight = ProbeCeiling;
	{
		const float SphereRadius = FMath::Max(MinCapsuleRadius * 0.9f, 2.f);
		const FVector Start = Foot + FVector(0.0, 0.0, double(SphereRadius + ProbeGroundLift));
		const FVector End = Foot + FVector(0.0, 0.0, double(ProbeCeiling));
		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(SphereRadius), Query))
		{
			AvailableHeight = float(Hit.ImpactPoint.Z - Foot.Z);
		}
		CeilingZ = AvailableHeight < ProbeCeiling ? float(Foot.Z) + AvailableHeight : NoCeilingZ;
	}

	// ---- Narrow gap ------------------------------------------------------------------

	float FreeRadius = DefaultCapsuleRadius;
	{
		const float ProbeHalfHeight = MinCapsuleHalfHeight;
		FVector ProbeCenter = Foot + FVector(0.0, 0.0, double(ProbeHalfHeight + ProbeGroundLift));

		// Look ahead: by the time a wall is touching, the movement component has already
		// refused the move and the body would never get the chance to deform.
		FVector Heading = GetOwner()->GetVelocity();
		Heading.Z = 0.0;
		if (!Heading.IsNearlyZero())
		{
			ProbeCenter += Heading.GetSafeNormal() * double(LookAheadDistance);
		}

		auto IsBlocked = [World, &ProbeCenter, ProbeHalfHeight, &Query](float Radius)
		{
			return World->OverlapBlockingTestByChannel(
				ProbeCenter, FQuat::Identity, ECC_Pawn,
				FCollisionShape::MakeCapsule(Radius, ProbeHalfHeight), Query);
		};

		if (IsBlocked(DefaultCapsuleRadius))
		{
			float Low = MinCapsuleRadius;
			float High = DefaultCapsuleRadius;
			for (int32 Iteration = 0; Iteration < RadiusProbeIterations; ++Iteration)
			{
				const float Mid = (Low + High) * 0.5f;
				if (IsBlocked(Mid))
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
		// Growing into geometry would shove the character inside a wall, so only grow when
		// the larger shape actually fits.
		const FVector GrownCenter = Foot + FVector(0.0, 0.0, double(NewHalfHeight));
		if (World->OverlapBlockingTestByChannel(GrownCenter, FQuat::Identity, ECC_Pawn,
			FCollisionShape::MakeCapsule(NewRadius, NewHalfHeight), Query))
		{
			NewHalfHeight = CurrentHalfHeight;
			NewRadius = CurrentRadius;
		}
	}

	if (!FMath::IsNearlyEqual(NewHalfHeight, CurrentHalfHeight, 0.05f) || !FMath::IsNearlyEqual(NewRadius, CurrentRadius, 0.05f))
	{
		ApplyCapsuleSize(NewRadius, NewHalfHeight);
	}
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
		const float HeightRatio = NewHalfHeight / FMath::Max(DefaultCapsuleHalfHeight, KINDA_SMALL_NUMBER);
		Movement->MaxStepHeight = DefaultStepHeight * HeightRatio;
		Movement->MaxWalkSpeed = DefaultWalkSpeed * FMath::Lerp(1.f, SqueezeSpeedScale, SqueezeAmount);
	}
}

void USlimeBodyComponent::UpdateAnchor()
{
	if (!GetOwner())
	{
		return;
	}

	const FVector Foot = GetFootLocation();
	const FVector Anchor = Foot + FVector(0.0, 0.0, double(SolverParams.RestRadius * AnchorHeightFraction));
	Solver.SetAnchor(Anchor, GetOwner()->GetVelocity());

	// Rubber band: if the body ends up dragged a long way from the capsule, pull the capsule
	// back rather than letting the two drift apart forever.
	if (OwnerCharacter)
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

void USlimeBodyComponent::RebuildSurface()
{
	if (!SurfaceMesh)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(SlimeBody_RebuildSurface);

	FSlimeSurfaceParams ActiveSurface = SurfaceParams;
	if (bSpread || Solver.GetLandingSettleRemaining() > 0.f)
	{
		// Keep the iso-surface one connected sheet while flattened or settling from a fall.
		ActiveSurface.SplatRadiusMultiplier = FMath::Max(ActiveSurface.SplatRadiusMultiplier, SpreadSplatMultiplier);
		ActiveSurface.IsoThreshold = FMath::Min(ActiveSurface.IsoThreshold, 0.22f);
	}
	Surface.Configure(ActiveSurface, SolverParams.ParticleSpacing);

	Surface.Build(Solver.GetParticles(), Solver.GetBodyCenter());

	if (Surface.WasTruncated() && !bWarnedTruncation)
	{
		bWarnedTruncation = true;
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeBodyComponent: surface hit the %d vertex budget; raise SurfaceParams.MaxVertices or the cell size."), SurfaceParams.MaxVertices);
	}

	PushMeshSection();
}

void USlimeBodyComponent::PushMeshSection()
{
	const TArray<FVector>& Vertices = Surface.GetVertices();
	if (Vertices.Num() == 0)
	{
		return;
	}

	// The material drives itself from world position and normals, so UVs, colours and tangents
	// stay empty rather than being rebuilt every frame for nothing.
	const TArray<FVector2D> NoUVs;
	const TArray<FLinearColor> NoColors;
	const TArray<FProcMeshTangent> NoTangents;

	if (!bMeshSectionCreated)
	{
		SurfaceMesh->ClearAllMeshSections();
		SurfaceMesh->CreateMeshSection_LinearColor(
			0, Vertices, Surface.GetIndices(), Surface.GetNormals(),
			NoUVs, NoColors, NoTangents, false);
		if (ResolvedMaterial)
		{
			SurfaceMesh->SetMaterial(0, ResolvedMaterial);
		}
		bMeshSectionCreated = true;
		return;
	}

	// Vertex count is constant by design, so this is an in place update: no reallocation and
	// no collision cook, which is what made the reference implementation expensive.
	SurfaceMesh->UpdateMeshSection_LinearColor(0, Vertices, Surface.GetNormals(), NoUVs, NoColors, NoTangents);
}

void USlimeBodyComponent::UpdateQuality()
{
	if (!bAutoQuality)
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
		StepRate = 40.f;
		SurfaceRate = 30.f;
		SolverParams.DensityIterations = 2;
		SurfaceParams.MaxGridDim = 24;
		SurfaceParams.BlurPasses = 1;
		break;

	case ESlimeSimQuality::Medium:
		StepRate = 30.f;
		SurfaceRate = 15.f;
		SolverParams.DensityIterations = 1;
		SurfaceParams.MaxGridDim = 18;
		SurfaceParams.BlurPasses = 1;
		break;

	case ESlimeSimQuality::Low:
		StepRate = 20.f;
		SurfaceRate = 8.f;
		SolverParams.DensityIterations = 1;
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

void USlimeBodyComponent::ResetBody()
{
	bSpread = false;
	bRecalling = false;
	SpreadRecoverRemaining = 0.f;
	RecallElapsed = 0.f;
	ForcedSqueeze = 0.f;

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
	const int32 Launched = Solver.LaunchChunk(LaunchVelocity, LaunchFraction, MinAttachedParticles, FragmentLifetime);
	if (Launched > 0)
	{
		SetRecalling(false);
	}
	return Launched;
}

void USlimeBodyComponent::SetRecalling(bool bInRecalling)
{
	if (bInRecalling && !Solver.HasFragments())
	{
		return;
	}
	bRecalling = bInRecalling;
	RecallElapsed = 0.f;
}
