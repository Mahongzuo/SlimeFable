// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "SlimeElementTypes.h"
#include "SlimeTrailComponent.generated.h"

class ACharacter;
class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class UProceduralMeshComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class USlimeBodyComponent;
class USlimeClingComponent;
class USlimeElementComponent;

UENUM(BlueprintType)
enum class ESlimeTrailStampKind : uint8
{
	None,
	Decal,
	Niagara
};

/** Per-element ground trail / body FX authoring. Soft refs are filled with project defaults. */
USTRUCT(BlueprintType)
struct SLIMEFABLE_API FSlimeTrailProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
	ESlimeElement Element = ESlimeElement::Water;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
	ESlimeTrailStampKind StampKind = ESlimeTrailStampKind::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail|Decal")
	TSoftObjectPtr<UMaterialInterface> DecalMaterial;

	/** Ground stamp Niagara (fire, gravel) or lightning main arcs (NS_TeslaCoil). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail|Niagara")
	TSoftObjectPtr<UNiagaraSystem> GroundNiagara;

	/** Body-attached looping FX (lightning wrap / fire underfoot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail|Attached")
	TSoftObjectPtr<UNiagaraSystem> AttachedNiagara;

	/** Optional mesh overlay while this element is active (lightning purple glow). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail|Attached")
	TSoftObjectPtr<UMaterialInterface> AttachedOverlayMaterial;

	/** Horizontal distance between stamps, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail", meta = (ClampMin = "5.0"))
	float SpawnDistance = 32.f;

	/** Decal diameter in cm, or ground Niagara uniform scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail", meta = (ClampMin = "0.01"))
	float StampSize = 48.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StampSizeJitter = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail", meta = (ClampMin = "0.05"))
	float StampLifetime = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaxStamps = 8;

	/** Horizontal speed below this does not stamp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail", meta = (ClampMin = "0.0"))
	float MinSpeed = 40.f;

	/** Lightning main-arc strike radius around the slime, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail|Lightning", meta = (ClampMin = "20.0"))
	float ArcRadius = 180.f;

	/** Relative scale of the attached body Niagara. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail|Attached")
	FVector AttachedScale = FVector(1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail|Attached")
	FVector AttachedOffset = FVector::ZeroVector;

	/** When true, ground Niagara uses User.PositionTarget (TeslaCoil-style arcs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail|Lightning")
	bool bGroundNiagaraIsArc = false;
};

/**
 *  Distance-stamped element trails: puddle decals, ground Niagara, and lightning body wrap.
 *  Subscribes to USlimeElementComponent; does not own element state.
 */
UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent, PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API USlimeTrailComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeTrailComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Trail")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Trail")
	bool bOnlyOnGround = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Trail")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Trail", meta = (ClampMin = "10.0"))
	float GroundTraceDistance = 120.f;

	/** Seconds at the end of a decal lifetime used to fade Fade → 0 (the puddle material shrinks and dries over it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Trail|Decal", meta = (ClampMin = "0.0",
		ToolTip = "水洼寿命末尾的干涸时长，秒，默认2.5；期间 Fade 1→0，材质边缘收缩、粗糙度上升、逐渐消失。"))
	float DecalFadeOutDuration = 2.5f;

	UPROPERTY(EditAnywhere, Category = "0_Config|Puddle", meta = (
		ToolTip = "水洼贴花按身体真实接触面（贴地/贴墙粒子拟合的椭圆）定尺寸和朝向。关了就用 Profiles 里的 StampSize 当直径。默认开。"))
	bool bPuddleFollowsContact = true;

	UPROPERTY(EditAnywhere, Category = "0_Config|Puddle", meta = (ClampMin = "0.5", ClampMax = "3.0", EditCondition = "bPuddleFollowsContact",
		ToolTip = "水洼相对真实接触椭圆的倍率，默认1.1；拖尾戳和身下常驻水洼都用它。1 = 正好贴合接触面。圆球形态用这个。"))
	float PuddleFootprintScale = 1.1f;

	UPROPERTY(EditAnywhere, Category = "0_Config|Puddle", meta = (ClampMin = "0.5", ClampMax = "3.0", EditCondition = "bPuddleFollowsContact",
		ToolTip = "Dome 形态下拖尾和水洼相对显示网格贴地轮廓的倍率，默认1。1 = 正好贴着钟形底边。圆球仍用上面的接触椭圆。"))
	float DomePuddleFootprintScale = 1.f;

	UPROPERTY(EditAnywhere, Category = "0_Config|Puddle", meta = (
		ToolTip = "身体正下方常驻一滩水洼：站着、摊开都不消失，离地或换元素后留在原地慢慢干涸。默认开。只对水/风/暗贴花元素生效。"))
	bool bBodyPuddle = true;

	UPROPERTY(EditAnywhere, Category = "0_Config|Puddle", meta = (ClampMin = "1.0", ClampMax = "3.0", EditCondition = "bBodyPuddle",
		ToolTip = "摊开时身下常驻水洼相对接触面再放大的倍率，默认1.15；用来盖住整张薄饼。1 = 不额外放大。"))
	float BodyPuddleSpreadScale = 1.15f;

	UPROPERTY(EditAnywhere, Category = "0_Config|Puddle", meta = (ClampMin = "1.0", ClampMax = "30.0", Units = "cm",
		ToolTip = "水洼贴花的投影深度（半厚），厘米，默认8；越小越不会投到旁边的侧墙。"))
	float BodyPuddleDepth = 8.f;

	/** StampSize is a diameter in cm. Decal Y/Z are half-extents, X is the projection depth. */
	static FVector DecalSizeFromDiameter(float DiameterCm, float DepthCm)
	{
		const float Half = FMath::Max(DiameterCm, 0.01f) * 0.5f;
		return FVector(FMath::Max(DepthCm, 1.f), Half, Half);
	}

	/** Contact half-axes (cm) times Scale become the decal Y/Z half-extents. */
	static FVector DecalSizeFromContact(const FVector2D& HalfAxesCm, float Scale, float DepthCm)
	{
		const float S = FMath::Max(Scale, 0.f);
		return FVector(FMath::Max(DepthCm, 1.f), FMath::Max(HalfAxesCm.X * S, 0.5f), FMath::Max(HalfAxesCm.Y * S, 0.5f));
	}

	/** Ball size and dome (mesh) size, blended by DomeWeight. Depth is shared. */
	static FVector DecalSizeForShape(
		const FVector2D& ParticleHalfAxes, float ParticleScale,
		const FVector2D& VisualHalfAxes, float VisualScale,
		float DomeWeight, float DepthCm)
	{
		const FVector Ball = DecalSizeFromContact(ParticleHalfAxes, ParticleScale, DepthCm);
		const FVector Dome = DecalSizeFromContact(VisualHalfAxes, VisualScale, DepthCm);
		return FMath::Lerp(Ball, Dome, FMath::Clamp(DomeWeight, 0.f, 1.f));
	}

	/** How far the puddle shader can push its edge past the nominal radius, as a box scale. */
	static float PuddleBoxMarginFrom(float EdgeNoise, float EdgeSoftness)
	{
		return 1.f + FMath::Max(EdgeNoise, 0.f) + FMath::Max(EdgeSoftness, 0.01f);
	}

	/**
	 *  Hidden skeletal mesh used as a sampling source for NS_Player_Electricity_Looping.
	 *  Defaults to the gallery mannequin simple mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Trail|Lightning")
	TSoftObjectPtr<USkeletalMesh> LightningSampleMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Trail|Lightning")
	FVector LightningSampleScale = FVector(0.22f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Trail|Lightning")
	FVector LightningSampleOffset = FVector(0.f, 0.f, -10.f);

	/** Keep Tesla arcs between the body and launched mini-slime shots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Trail|Lightning")
	bool bLinkArcsToShots = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Trail|Lightning", meta = (ClampMin = "0", ClampMax = "8"))
	int32 MaxShotArcs = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Trail")
	TArray<FSlimeTrailProfile> Profiles;

	/**
	 *  Bumped when default Profiles change. Mismatched instances are rebuilt from
	 *  MakeDefaultProfile so serialized BP CDOs pick up new tuning.
	 */
	UPROPERTY()
	int32 TrailDefaultsVersion = 0;

	static constexpr int32 CurrentTrailDefaultsVersion = 12;

	UPROPERTY(EditAnywhere, Category="0_Config|Trail", meta=(ToolTip="为子史莱姆真实落地和贴地移动生成当前元素轨迹；按半径缩小，共用现有印记数量和淡出，默认开启"))
 bool bShotGroundEffects = true;
 UFUNCTION(BlueprintPure, Category = "Slime|Trail")
	const FSlimeTrailProfile& GetProfile(ESlimeElement Element) const;

protected:
	void EnsureProfileDefaults();
	FSlimeTrailProfile MakeDefaultProfile(ESlimeElement Element) const;

	UFUNCTION()
	void HandleElementChanged(ESlimeElement NewElement, ESlimeElement PreviousElement);

	void RefreshAttachedEffects(ESlimeElement Element);
	void ClearAttachedEffects();
	void EnsureLightningSampleMesh();

	bool CanStamp(const FSlimeTrailProfile& Profile) const;
	bool ResolveStampLocation(FVector& OutLocation, FVector& OutNormal) const;
	const USlimeClingComponent* GetOwnerCling() const;
	bool IsOwnerClinging() const;
	void TryStamp(const FSlimeTrailProfile& Profile);
 void TickShotTrails(const FSlimeTrailProfile& Profile);
 struct FShotTrailState
 {
  FVector LastPosition=FVector::ZeroVector;
  float Distance=0.f;
  uint32 LandingEvent=0;
  bool bInitialized=false, bGrounded=false;
 };
 TMap<uint8,FShotTrailState> ShotTrailStates;
	void StampDecal(const FSlimeTrailProfile& Profile, const FVector& Location, const FVector& Normal, const FVector2D& HalfExtents, const FVector& MajorDir, float SpinRadians);
	float PuddleBoxMargin(const UMaterialInstanceDynamic* MID) const;
	void TickBodyPuddle(float DeltaTime);
	void ReleaseBodyPuddle(bool bLeaveDryingStamp, const FSlimeTrailProfile* LeaveProfile = nullptr);
	void LeavePeakPuddle(const FSlimeTrailProfile& Profile);
	void StampGroundNiagara(const FSlimeTrailProfile& Profile, const FVector& Location, const FVector& Normal, float Size);
	void StampLightningArc(const FSlimeTrailProfile& Profile);
	bool PickArcTarget(const FSlimeTrailProfile& Profile, FVector& OutTarget) const;
	/** Disables TeslaCoil Smoke emitter; keeps arcs only. */
	static void ConfigureTeslaArc(UNiagaraComponent* Niagara);
	/** Disables NS_Footstep_Fire footprint / mesh emitters; keeps flame only. */
	static void ConfigureFireFootstep(UNiagaraComponent* Niagara);

	void TickActiveStamps(float DeltaTime);
	void TickShotLinkArcs();
	void ClearShotLinkArcs();
	void RecycleOldestStamp();
	void UpdateClingFireFx();
	void ClearClingFireFx();

	UMaterialInterface* ResolveMaterial(const TSoftObjectPtr<UMaterialInterface>& Soft) const;
	UNiagaraSystem* ResolveNiagara(const TSoftObjectPtr<UNiagaraSystem>& Soft) const;
	USkeletalMesh* ResolveSampleMesh() const;

	struct FActiveStamp
	{
		TWeakObjectPtr<UDecalComponent> Decal;
		TWeakObjectPtr<UNiagaraComponent> Niagara;
		TObjectPtr<UMaterialInstanceDynamic> DecalMID;
		float Age = 0.f;
		float Lifetime = 1.f;
		bool bIsArc = false;
	};

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<USlimeElementComponent> ElementComponent;

	UPROPERTY(Transient)
	TObjectPtr<USlimeBodyComponent> BodyComponent;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> SurfaceMesh;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> LightningSampleComp;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> AttachedNiagaraComp;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ClingFireNiagaraComp;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ActiveOverlay;

	/** Persistent Tesla arcs linking body ↔ launched shots (not lifetime-pooled stamps). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> ShotLinkArcs;

	TArray<FActiveStamp> ActiveStamps;

	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> BodyPuddleDecal;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyPuddleMID;

	float BodyPuddleFade = 0.f;
	float BodyPuddleLostTime = 0.f;
	float BodyPuddlePeakArea = 0.f;
	FVector BodyPuddlePeakCenter = FVector::ZeroVector;
	FVector BodyPuddlePeakNormal = FVector::UpVector;
	FVector BodyPuddlePeakMajor = FVector::ForwardVector;
	FVector2D BodyPuddlePeakHalf = FVector2D::ZeroVector;
	/** Current resident half-axes before the shader box margin, so a released stamp is not scaled twice. */
	FVector2D BodyPuddleHalf = FVector2D::ZeroVector;
	bool bBodyPuddlePeakValid = false;

	FVector LastStampLocation = FVector::ZeroVector;
	float DistanceAccumulator = 0.f;
	bool bHasLastStampLocation = false;
};
