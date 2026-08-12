// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeSolver.h"
#include "SlimeSurfaceBuilder.h"
#include "SlimeTypes.h"
#include "SlimeBodyComponent.generated.h"

class ACharacter;
class UCapsuleComponent;
class UMaterialInterface;
class UProceduralMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlimeSqueezeChanged, float, SqueezeAmount);

/**
 *  Drives the semi fluid body: fixed step simulation, world collider gathering, surface
 *  rebuild, and the adaptive capsule that lets the character actually enter a gap narrow
 *  enough to deform in.
 *
 *  Squeeze probing lives here rather than in its own component so it can reuse the collider
 *  set and floor trace this component already pays for.
 */
UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API USlimeBodyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeBodyComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---- Configuration ---------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Solver", meta = (ShowOnlyInnerProperties))
	FSlimeSolverParams SolverParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Surface", meta = (ShowOnlyInnerProperties))
	FSlimeSurfaceParams SurfaceParams;

	/** Material for the surface mesh. Falls back to BodyMaterialPath, then to a plain colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Surface")
	TObjectPtr<UMaterialInterface> BodyMaterial;

	UPROPERTY(EditAnywhere, Category = "Slime|Surface")
	TSoftObjectPtr<UMaterialInterface> BodyMaterialPath;

	/** Solver steps per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Stepping", meta = (ClampMin = "20.0", ClampMax = "90.0"))
	float StepRate = 40.f;

	/** Surface rebuilds per second. Match StepRate so mesh resolution does not stutter against the solve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Stepping", meta = (ClampMin = "5.0", ClampMax = "60.0"))
	float SurfaceRate = 40.f;

	/** Guards against a spiral of death after a hitch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Stepping", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxStepsPerFrame = 2;

	// ---- World collision -------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Collision", meta = (ClampMin = "2", ClampMax = "48"))
	int32 MaxWorldColliders = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Collision", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float ColliderRefreshInterval = 0.25f;

	/** Refresh early when the body has moved this far since the last gather, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Collision", meta = (ClampMin = "2.0"))
	float ColliderRefreshDistance = 20.f;

	/** Query box scale over the body bounds. Above 1 so walls enter the set before contact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Collision", meta = (ClampMin = "1.0", ClampMax = "3.0"))
	float ColliderQueryScale = 1.5f;

	/** Extra query padding around launched chunks so distant walls stay in the collider set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Collision", meta = (ClampMin = "4.0", ClampMax = "80.0"))
	float FragmentColliderRadius = 18.f;

	/** Analytic floor-proxy radius under the chunk COM (≈ RestRadius * 0.45 when left at default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Collision", meta = (ClampMin = "2.0", ClampMax = "40.0"))
	float FragmentProxyRadius = 12.f;

	// ---- Adaptive capsule and squeeze ------------------------------------------------

	/** Without this the movement component blocks the character out of any gap narrower than the capsule. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze")
	bool bAdaptiveCapsule = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "8.0"))
	float DefaultCapsuleRadius = 32.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "8.0"))
	float DefaultCapsuleHalfHeight = 26.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "4.0"))
	float MinCapsuleRadius = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "4.0"))
	float MinCapsuleHalfHeight = 6.f;

	/** Time constant while shrinking. Fast, so a gap does not stop the character dead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float ShrinkTime = 0.08f;

	/** Time constant while growing back. Slow, so the mouth of a gap does not chatter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "0.02", ClampMax = "2.0"))
	float RecoverTime = 0.3f;

	/** Probe this far along the velocity. Reacting on contact is already too late. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "0.0", ClampMax = "150.0"))
	float LookAheadDistance = 40.f;

	/** Walk speed multiplier at full squeeze. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float SqueezeSpeedScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "1", ClampMax = "6"))
	int32 RadiusProbeIterations = 3;

	/** Where the anchor sits above the feet, as a fraction of RestRadius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "0.1", ClampMax = "1.5"))
	float AnchorHeightFraction = 0.5f;

	/** Horizontal distance at which the capsule gets dragged back towards the blob. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "20.0"))
	float MaxAnchorDistance = 160.f;

	// ---- Pancake ---------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "1.0", ClampMax = "8.0"))
	float SpreadRadiusScale = 4.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.0"))
	float SpreadPush = 380.f;

	/** Half-thickness of the pancake disk, in cm. Thin sheet ~ SIM DomeHeight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.5", ClampMax = "12.0"))
	float SpreadHalfHeight = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float SpreadGravityScale = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float SpreadRecoverDuration = 0.45f;

	/** Extra splat while pancaked so the puddle stays one visual sheet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float SpreadSplatMultiplier = 2.8f;

	// ---- Landing ---------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Landing", meta = (ClampMin = "100.0"))
	float LandingSquashMinSpeed = 280.f;

	// ---- Launch and recall -----------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "0.05", ClampMax = "0.6"))
	float LaunchFraction = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "32"))
	int32 MinAttachedParticles = 96;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "0.5"))
	float FragmentLifetime = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "100.0"))
	float RecallPullSpeed = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "0.5"))
	float RecallTimeout = 5.f;

	/**
	 *  Distance from body COM at which flying chunks auto-merge back (metaball absorb).
	 *  0 = RestRadius * 1.6.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "0.0", ClampMax = "120.0"))
	float AbsorbMergeRadius = 0.f;

	// ---- Quality ---------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Quality")
	bool bAutoQuality = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Quality", meta = (ClampMin = "200.0"))
	float MediumQualityDistance = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Quality", meta = (ClampMin = "400.0"))
	float LowQualityDistance = 3500.f;

	/** Rebuild the particle set on tier changes. Only ever kicks in far from the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Quality")
	bool bQualityScalesParticleCount = true;

	// ---- Events ----------------------------------------------------------------------

	/** Fires when the squeeze amount moves meaningfully. Hook level hazards up here. */
	UPROPERTY(BlueprintAssignable, Category = "Slime|Squeeze")
	FOnSlimeSqueezeChanged OnSqueezeChanged;

	// ---- API -------------------------------------------------------------------------

	/** Assigned by the owning character in its constructor. */
	void SetSurfaceMesh(UProceduralMeshComponent* InMesh) { SurfaceMesh = InMesh; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	UProceduralMeshComponent* GetSurfaceMesh() const { return SurfaceMesh; }

	/** Pancake mode. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetSpread(bool bInSpread);

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool IsSpreading() const { return bSpread; }

	/** Rebuilds the dome and clears every transient state. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void ResetBody();

	/** Throws a chunk of non core particles. Returns how many left the body. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	int32 LaunchChunk(const FVector& LaunchVelocity);

	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetRecalling(bool bInRecalling);

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool IsRecalling() const { return bRecalling; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool HasFragments() const { return Solver.HasFragments(); }

	UFUNCTION(BlueprintPure, Category = "Slime")
	FVector GetBlobCenter() const { return Solver.GetBodyCenter(); }

	UFUNCTION(BlueprintPure, Category = "Slime")
	float GetSqueezeAmount() const { return SqueezeAmount; }

	/** Drives the capsule towards its minimum regardless of what the probes found. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetForcedSqueeze(float Amount) { ForcedSqueeze = FMath::Clamp(Amount, 0.f, 1.f); }

	/** Called from the character Landed event — squash + settle, never fragment. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void ApplyLandingSquash(float ImpactSpeed);

	/** Light double-jump "duang"; far milder than a landing squash. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void ApplyAirBounce();

	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetQuality(ESlimeSimQuality InQuality);

	UFUNCTION(BlueprintPure, Category = "Slime")
	ESlimeSimQuality GetQuality() const { return Quality; }

	/** Re-applies params to the solver and surface builder after an edit. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void ApplyParams();

private:
	void FixedStep(float StepDelta);
	void RefreshColliders();
	void UpdateFloor();
	void ProbeSqueeze(float DeltaTime);
	void ApplyCapsuleSize(float NewRadius, float NewHalfHeight);
	void UpdateAnchor();
	void RebuildSurface();
	void PushMeshSection();
	void UpdateQuality();
	void ResolveMaterial();
	FVector GetFootLocation() const;

	FSlimeSolver Solver;
	FSlimeSurfaceBuilder Surface;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> SurfaceMesh;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UCapsuleComponent> OwnerCapsule;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ResolvedMaterial;

	float StepAccumulator = 0.f;
	float SurfaceAccumulator = 0.f;
	float ColliderTimer = 0.f;
	FVector LastColliderGatherCenter = FVector::ZeroVector;

	float FloorZ = -1.e9f;
	float FragmentFloorZ = -1.e9f;
	float CeilingZ = 1.e9f;
	float SqueezeAmount = 0.f;
	float ReportedSqueeze = 0.f;
	float ForcedSqueeze = 0.f;
	float SpreadRecoverRemaining = 0.f;
	float RecallElapsed = 0.f;
	FVector SqueezeFreeDirection = FVector::UpVector;

	float DefaultStepHeight = 45.f;
	float DefaultWalkSpeed = 500.f;

	bool bSpread = false;
	bool bRecalling = false;
	bool bMeshSectionCreated = false;
	bool bWarnedTruncation = false;

	ESlimeSimQuality Quality = ESlimeSimQuality::High;
};
