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
	TSoftObjectPtr<UMaterialInterface> BodyMaterialPath =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Characters/Slime/Materials/M_SlimeBody.M_SlimeBody")));

	/** Opaque material on the hidden shadow-proxy mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Surface")
	TObjectPtr<UMaterialInterface> ShadowCasterMaterial;

	UPROPERTY(EditAnywhere, Category = "Slime|Surface")
	TSoftObjectPtr<UMaterialInterface> ShadowCasterMaterialPath =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Characters/Slime/Materials/M_SlimeShadowCaster.M_SlimeShadowCaster")));

	/** Translucent X-ray silhouette when occluded by world geometry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Surface")
	TObjectPtr<UMaterialInterface> XRayMaterial;

	UPROPERTY(EditAnywhere, Category = "Slime|Surface")
	TSoftObjectPtr<UMaterialInterface> XRayMaterialPath =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_SlimeXRay.M_SlimeXRay")));

	/** Solver steps per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Stepping", meta = (ClampMin = "20.0", ClampMax = "90.0"))
	float StepRate = 60.f;

	/** Surface rebuilds per second. Match StepRate so mesh resolution does not stutter against the solve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Stepping", meta = (ClampMin = "5.0", ClampMax = "90.0"))
	float SurfaceRate = 60.f;

	/** Guards against a spiral of death after a hitch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Stepping", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxStepsPerFrame = 2;

	// ---- World collision -------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Collision", meta = (ClampMin = "2", ClampMax = "48"))
	int32 MaxWorldColliders = 28;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Collision", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float ColliderRefreshInterval = 0.25f;

	/** Refresh early when the body has moved this far since the last gather, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Collision", meta = (ClampMin = "2.0"))
	float ColliderRefreshDistance = 35.f;

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
	float DefaultCapsuleHalfHeight = 20.f;

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

	/** When heavily squeezed with move input, crawl out of a pinch at this speed, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "20.0", ClampMax = "300.0"))
	float OozeSpeed = 100.f;

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
	float SpreadRadiusScale = 5.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.0"))
	float SpreadPush = 300.f;

	/** Half-thickness of the pancake disk, in cm. Thin sheet ~ SIM DomeHeight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.5", ClampMax = "12.0"))
	float SpreadHalfHeight = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float SpreadGravityScale = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float SpreadRecoverDuration = 0.45f;

	/** Extra XY splat while pancaked so the puddle stays one visual sheet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float SpreadSplatMultiplier = 2.0f;

	/** Vertical splat scale while pancaked (keeps the pie thin). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.25", ClampMax = "1.0"))
	float SpreadSplatZScale = 0.55f;

	/** Concentration multiplier while spread (keeps the centre filled). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float SpreadConcentrationScale = 1.15f;

	// ---- Landing ---------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Landing", meta = (ClampMin = "100.0"))
	float LandingSquashMinSpeed = 280.f;

	// ---- Launch and recall -----------------------------------------------------------

	/** Clone particle count as a fraction of the body particle budget (~30% mini slime). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "0.05", ClampMax = "0.6"))
	float LaunchFraction = 0.3f;

	/** Max simultaneous unrecovered clone shots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "1", ClampMax = "12"))
	int32 MaxActiveShots = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "0.5"))
	float FragmentLifetime = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "100.0"))
	float RecallPullSpeed = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "0.5"))
	float RecallTimeout = 5.f;

	/**
	 *  Distance from body COM at which flying chunks start soft-merge (metaball absorb).
	 *  0 = RestRadius * 1.6.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "0.0", ClampMax = "120.0"))
	float AbsorbMergeRadius = 0.f;

	/** Inside this radius (or after hold), clones finally destroy. 0 = RestRadius * 0.7. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float AbsorbCommitRadius = 0.f;

	/** Soft-merge duang window before clones are destroyed, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float MergeHoldDuration = 0.7f;

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

	void SetShadowMesh(UProceduralMeshComponent* InMesh) { ShadowMesh = InMesh; }

	void SetXRayMesh(UProceduralMeshComponent* InMesh) { XRayMesh = InMesh; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	UProceduralMeshComponent* GetSurfaceMesh() const { return SurfaceMesh; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	UProceduralMeshComponent* GetShadowMesh() const { return ShadowMesh; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	UProceduralMeshComponent* GetXRayMesh() const { return XRayMesh; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	UMaterialInterface* GetResolvedBodyMaterial() const { return ResolvedMaterial; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	UMaterialInterface* GetResolvedXRayMaterial() const { return ResolvedXRayMaterial; }

	/** Pancake mode. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetSpread(bool bInSpread);

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool IsSpreading() const { return bSpread; }

	/** Temporary MaxStepHeight used while walking onto short props. 0 restores the default. */
	void SetStepHeightBoost(float BoostedMaxStep);

	float GetDefaultStepHeight() const { return DefaultStepHeight; }

	/** Rebuilds the dome and clears every transient state. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void ResetBody();

	/** Spawns a cloned mini-slime shot without shrinking the body. Returns clone particle count. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	int32 LaunchChunk(const FVector& LaunchVelocity);

	int32 LaunchChunkAlongPath(const FSlimeLaunchPath& Path);

	/** Combat tendrils: short-lived clone blobs that peel then get recalled. */
	int32 LaunchTendril(const FVector& LaunchVelocity, float Fraction, float Life);

	/** Devour latch: clone a mini-slime that ignores the G-key shot cap. */
	int32 LaunchDevourShot(const FVector& LaunchVelocity, float Fraction, float Life, uint8& OutShotId);

	void SetShotTarget(uint8 ShotId, const FVector& Target, float PullSpeed);

	void ClearShotTargets();

	void AddIgnoreWorldShot(uint8 ShotId);
	void ClearIgnoreWorldShots();

	void SetRecallPullSpeedOverride(float Speed);
	void ClearRecallPullSpeedOverride();

	float GetEffectiveRecallPullSpeed() const;

	/** Instantly destroy every flying clone (used before a devour latch). */
	void ClearFragments();

	/** While > 0, FixedStep uses this instead of LaunchFraction (devour half-volume minis). 0 = off. */
	void SetLaunchFractionOverride(float Fraction);

	void ClearLaunchFractionOverride() { LaunchFractionOverride = 0.f; }

	FVector GetShotCenter(uint8 ShotId) const;

	UFUNCTION(BlueprintPure, Category = "Slime")
	float GetRecallPullSpeed() const { return RecallPullSpeed; }

	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetRecalling(bool bInRecalling);

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool IsRecalling() const { return bRecalling; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool HasFragments() const { return Solver.HasFragments(); }

	UFUNCTION(BlueprintPure, Category = "Slime")
	int32 GetActiveShotCount() const { return Solver.GetActiveShotCount(); }

	UFUNCTION(BlueprintPure, Category = "Slime")
	int32 GetMaxActiveShots() const { return MaxActiveShots; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	FVector GetBlobCenter() const { return Solver.GetBodyCenter(); }

	/** Particle COM plus visual-only Z lift so devour inner mesh tracks the inflated ball. */
	UFUNCTION(BlueprintPure, Category = "Slime")
	FVector GetVisualBlobCenter() const { return GetBlobCenter() + FVector(0.f, 0.f, VisualZLift); }

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool GetFragmentCenter(FVector& OutCenter) const { return Solver.GetFragmentCenter(OutCenter); }

	/** COM of each active ballistic mini-slime shot (refreshes shot cache). */
	UFUNCTION(BlueprintPure, Category = "Slime")
	void GetActiveShotCenters(TArray<FVector>& OutCenters) const;

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
	void SetCombatPose(const FSlimeCombatPoseState& Pose);

	UFUNCTION(BlueprintCallable, Category = "Slime")
	void ClearCombatPose();

	UFUNCTION(BlueprintCallable, Category = "Slime")
	void ApplyHitJolt();

	/**
	 *  Uniform blob scale (devour gulp). Capsule is left alone.
	 *  @param bIgnoreSqueeze 吞噬时勾上：不要被间隙挤压把 3x 压回 1x。
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetBodyScale(float NewScale, bool bIgnoreSqueeze = false);

	UFUNCTION(BlueprintPure, Category = "Slime")
	float GetBodyScale() const { return RequestedBodyScale; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	float GetAppliedBodyScale() const { return Solver.GetSizeScale(); }

	/** Extra walk-speed multiplier applied after squeeze. 1 = unchanged. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetExternalMoveSpeedScale(float Scale) { ExternalMoveSpeedScale = FMath::Clamp(Scale, 0.1f, 1.5f); }

	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetExternalJumpScale(float Scale) { ExternalJumpScale = FMath::Clamp(Scale, 0.1f, 1.5f); }

	/**
	 *  If true (or slime.BodyVisualScaleOnly=1), SetBodyScale only inflates the isosurface
	 *  and leaves particle positions at 1x.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Scale")
	bool bVisualOnlyBodyScale = false;

	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetQuality(ESlimeSimQuality InQuality);

	UFUNCTION(BlueprintPure, Category = "Slime")
	ESlimeSimQuality GetQuality() const { return Quality; }

	/** Re-applies params to the solver and surface builder after an edit. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void ApplyParams();

	/** Wall-cling hemisphere: Point/Normal of the stuck surface. Clears when bInCling is false. */
	void SetClingVisual(bool bInCling, const FVector& Point, const FVector& Normal);

	/** Skip ceiling height-squeeze for a short time after mantling onto a lip. */
	void SuppressHeightSqueeze(float Duration);

	/**
	 *  Stops the shadow proxy from casting. The proxy is hidden-in-game but casts anyway
	 *  (bCastHiddenShadow), so hiding the owning actor is not enough — and the surface rebuild
	 *  re-asserts the cast flags every frame, so the suppression has to live here.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetShadowCastSuppressed(bool bSuppressed);

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool IsShadowCastSuppressed() const { return bShadowCastSuppressed; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool IsClingingVisual() const { return bClingVisual; }

private:
	void FixedStep(float StepDelta);
	void SweepKinematicShots();
	void RefreshColliders();
	void UpdateFloor();
	void ProbeSqueeze(float DeltaTime);
	void TryOozeEscape(float DeltaTime);
	void ApplyCapsuleSize(float NewRadius, float NewHalfHeight);
	void UpdateAnchor();
	void RebuildSurface();
	void PushMeshSection();
	void UpdateMeshFollow();
	void UpdateQuality();
	void ResolveMaterial();
	FVector GetFootLocation() const;

	FSlimeSolver Solver;
	FSlimeSurfaceBuilder Surface;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> SurfaceMesh;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> ShadowMesh;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> XRayMesh;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UCapsuleComponent> OwnerCapsule;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ResolvedMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ResolvedShadowMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ResolvedXRayMaterial;

	float StepAccumulator = 0.f;
	float SurfaceAccumulator = 0.f;
	float ColliderTimer = 0.f;
	FVector LastColliderGatherCenter = FVector::ZeroVector;

	/** Body COM at the last surface rebuild; mesh slides by (CurrentCOM - this) between rebuilds. */
	FVector RebuildBodyCOM = FVector::ZeroVector;
	bool bHaveRebuildBodyCOM = false;

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
	float DefaultJumpZ = 620.f;
	float StepHeightBoost = 0.f;
	float HeightSqueezeSuppressRemaining = 0.f;
	float ExternalMoveSpeedScale = 1.f;
	float ExternalJumpScale = 1.f;
	float RequestedBodyScale = 1.f;
	float VisualZLift = 0.f;
	float LaunchFractionOverride = 0.f;
	float RecallPullSpeedOverride = 0.f;
	int32 SavedSurfaceMaxVertices = 9000;
	int32 SavedSurfaceMaxGridDim = 36;
	bool bEnlargedSurfaceBudget = false;
	bool bFreezeQualityLod = false;

	bool bSpread = false;
	bool bRecalling = false;
	bool bClingVisual = false;
	bool bShadowCastSuppressed = false;
	bool bMeshSectionCreated = false;
	bool bShadowMeshSectionCreated = false;
	bool bXRayMeshSectionCreated = false;
	bool bWarnedTruncation = false;

	FVector ClingPoint = FVector::ZeroVector;
	FVector ClingNormal = FVector::ForwardVector;

	ESlimeSimQuality Quality = ESlimeSimQuality::High;
};
