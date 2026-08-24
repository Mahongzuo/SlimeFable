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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail", meta = (ClampMin = "1", ClampMax = "32"))
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
UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
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

	/** Seconds at the end of a decal lifetime used to fade Fade → 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Trail|Decal", meta = (ClampMin = "0.0"))
	float DecalFadeOutDuration = 0.45f;

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

	static constexpr int32 CurrentTrailDefaultsVersion = 10;

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
	void StampDecal(const FSlimeTrailProfile& Profile, const FVector& Location, const FVector& Normal, float Size);
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
	FVector LastStampLocation = FVector::ZeroVector;
	float DistanceAccumulator = 0.f;
	bool bHasLastStampLocation = false;
};
