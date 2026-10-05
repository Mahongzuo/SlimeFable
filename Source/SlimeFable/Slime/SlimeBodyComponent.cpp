// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeBodyComponent.h"
#include "SlimeUmbrellaComponent.h"
#include "SlimeAbilityComponent.h"
#include "Components/MeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/Float16.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "SlimeFable.h"
#include "Engine/GameInstance.h"
#include "Settings/SlimeGraphicsSettings.h"
#include "SlimeCharacterMovementComponent.h"
#include "CombatDamageable.h"
#include "SlimeCharacter.h"
#include "SlimeCombatComponent.h"
#include "SlimeDevourComponent.h"
#include "SlimeElementComponent.h"
#include "SlimeHealthComponent.h"
#include "SlimeHitProbe.h"
#include "Farm/SlimeElementReceiver.h"
#include "Hub/HomeBuild/SlimeHomeFluidPad.h"
#include "SlimeStatusComponent.h"
#include "UI/SlimeFloatingTextWidget.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "UObject/UObjectIterator.h"

using namespace SlimeSim;

static TAutoConsoleVariable<int32> CVarSlimeBodyVisualScaleOnly(
	TEXT("slime.BodyVisualScaleOnly"),
	0,
	TEXT("If 1, devour body scale only inflates the isosurface (no solver SizeScale)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarSlimeAmbientDebug(
	TEXT("slime.AmbientDebug"),
	0,
	TEXT("1 = slime skins show AmbientScale as grey and the body logs raw illuminance / sky / sun / scale once per second."),
	ECVF_Default);

namespace SlimeBodyPrivate
{
	static const FName ParamAmbientScale(TEXT("AmbientScale"));
	static const FName ParamAmbientDebug(TEXT("AmbientDebug"));

	/** Clearance kept between the capsule and a ceiling so the sweep does not re-hit it. */
	constexpr float CeilingSkin = 2.f;

	/** Lift for the horizontal probe so it measures walls, not the floor. */
	constexpr float ProbeGroundLift = 2.f;

	/** How far a squeeze value has to move before the delegate fires. */
	constexpr float SqueezeReportEpsilon = 0.02f;

	constexpr float NoCeilingZ = 1.e9f;

	/** Shell / bubble parameters shared by M_SlimeBody and M_SlimeBody_Spectral. */
	static const FName ParamShellCenter(TEXT("ShellCenter"));
	static const FName ParamShellAxes(TEXT("ShellAxes"));
	static const FName ParamShellForward(TEXT("ShellForward"));
	/** Jelly optics (slime_jelly_optics_hlsl.py): floor height for the contact band, key light for the highlight. */
	static const FName ParamContactFloorZ(TEXT("ContactFloorZ"));
	static const FName ParamKeyLightDir(TEXT("KeyLightDir"));
	/** Shot cluster ellipsoids (xyz = centre, a = radius); slot order = USlimeBodyComponent::GetShotSlotIds(). */
	static const FName ParamShotCenter[USlimeBodyComponent::MaxShotSlots] = {
		FName(TEXT("ShotCenter0")), FName(TEXT("ShotCenter1")), FName(TEXT("ShotCenter2")),
		FName(TEXT("ShotCenter3")), FName(TEXT("ShotCenter4")),
	};
	static const FName ParamBubbles[USlimeBodyComponent::MaxBubbles] = {
		FName(TEXT("Bubble0")), FName(TEXT("Bubble1")), FName(TEXT("Bubble2")), FName(TEXT("Bubble3")),
		FName(TEXT("Bubble4")), FName(TEXT("Bubble5")), FName(TEXT("Bubble6")), FName(TEXT("Bubble7")),
		FName(TEXT("Bubble8")), FName(TEXT("Bubble9")),
	};
	static const FName ParamBubbleBurst[USlimeBodyComponent::MaxBubbles] = {
		FName(TEXT("BubbleBurst0")), FName(TEXT("BubbleBurst1")), FName(TEXT("BubbleBurst2")), FName(TEXT("BubbleBurst3")),
		FName(TEXT("BubbleBurst4")), FName(TEXT("BubbleBurst5")), FName(TEXT("BubbleBurst6")), FName(TEXT("BubbleBurst7")),
		FName(TEXT("BubbleBurst8")), FName(TEXT("BubbleBurst9")),
	};

	/** Material parameter names of M_SlimeBody_Volumetric (create_slime_volumetric_material.py). */
	static const FName ParamDensityAtlas(TEXT("DensityAtlas"));
	static const FName ParamGridOrigin(TEXT("GridOrigin"));
	static const FName ParamGridDims(TEXT("GridDims"));
	static const FName ParamGridInfo(TEXT("GridInfo"));

	bool IsHomeFluidPadActor(const AActor* Actor)
	{
		for (const AActor* Cursor = Actor; Cursor; Cursor = Cursor->GetAttachParentActor())
		{
			if (Cursor->IsA(ASlimeHomeFluidPad::StaticClass()))
			{
				return true;
			}
		}
		return false;
	}

	bool OwnerLooksLikeNinjaLive(const AActor* Owner)
	{
		if (!Owner)
		{
			return false;
		}
		// Home fluid blueprints are children of NinjaLive_C with their own class names.
		for (const UClass* Class = Owner->GetClass(); Class; Class = Class->GetSuperClass())
		{
			if (Class->GetName().Contains(TEXT("NinjaLive"), ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return Owner->GetName().Contains(TEXT("NinjaLive"), ESearchCase::IgnoreCase)
			|| IsHomeFluidPadActor(Owner);
	}

	bool ComponentLooksLikeNinjaSimGeom(const UPrimitiveComponent* Component)
	{
		if (!Component)
		{
			return false;
		}
		const FString Name = Component->GetName();
		return Name.Contains(TEXT("TraceMesh"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("InteractionVolume"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("InteractionVol"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("ActivationVolume"), ESearchCase::IgnoreCase);
	}
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
	EnsureXRayOutlineMaterial();

	if (USlimeGraphicsSettings* Graphics = GetGraphicsSettings())
	{
		ApplyBodySkin(Graphics->GetBodySkin());
		BodySkinChangedHandle = Graphics->OnBodySkinChanged.AddUObject(this, &USlimeBodyComponent::ApplyBodySkin);
		ApplyBodyShape(Graphics->GetBodyShape());
		ShapeBlend = ShapeTarget;
		BodyShapeChangedHandle = Graphics->OnBodyShapeChanged.AddUObject(this, &USlimeBodyComponent::ApplyBodyShape);
	}

	if (Quality == ESlimeSimQuality::High)
	{
		SurfaceParams.CellSizeMultiplier = 0.64f;
		SurfaceParams.MaxGridDim = 48;
		SurfaceParams.MaxVertices = 12000;
		SurfaceParams.BlurPasses = 3;
	}

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

void USlimeBodyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
 for (auto& Pair:ShotContactDecals) if (Pair.Value) Pair.Value->DestroyComponent();
 ShotContactDecals.Reset();
 Solver.QueryGround=nullptr;
 Solver.QueryUmbrellaGround=nullptr;
 Solver.TraceSeparation=nullptr;
	if (BodySkinChangedHandle.IsValid())
	{
		if (USlimeGraphicsSettings* Graphics = GetGraphicsSettings())
		{
			Graphics->OnBodySkinChanged.Remove(BodySkinChangedHandle);
		}
		BodySkinChangedHandle.Reset();
	}
	if (BodyShapeChangedHandle.IsValid())
	{
		if (USlimeGraphicsSettings* Graphics = GetGraphicsSettings())
		{
			Graphics->OnBodyShapeChanged.Remove(BodyShapeChangedHandle);
		}
		BodyShapeChangedHandle.Reset();
	}
	ReleaseDensityAtlas();
	Super::EndPlay(EndPlayReason);
}

USlimeGraphicsSettings* USlimeBodyComponent::GetGraphicsSettings() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USlimeGraphicsSettings>() : nullptr;
}

void USlimeBodyComponent::ApplyBodyShape(ESlimeBodyShape Shape)
{
	ShapeTarget = Shape == ESlimeBodyShape::Dome ? 1.f : 0.f;
}

void USlimeBodyComponent::ApplyBodySkin(ESlimeBodySkin Skin)
{
	bool bWantVolumetric = false;
	if (Skin == ESlimeBodySkin::Luminous && !ResolvedLuminousMaterial)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeBodyComponent: luminous body material missing on '%s'; falling back to spectral."), *GetNameSafe(GetOwner()));
		Skin = ESlimeBodySkin::Spectral;
	}
	if (Skin == ESlimeBodySkin::Volumetric)
	{
		if (ResolvedVolumetricMaterial)
		{
			bWantVolumetric = true;
		}
		else
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("SlimeBodyComponent: volumetric body material missing on '%s'; falling back to the spectral skin."), *GetNameSafe(GetOwner()));
			Skin = ESlimeBodySkin::Spectral;
		}
	}

	UMaterialInterface* Wanted = ResolvedClassicMaterial;
	if (Skin == ESlimeBodySkin::Luminous)
	{
		Wanted = ResolvedLuminousMaterial;
	}
	else if (bWantVolumetric)
	{
		Wanted = ResolvedVolumetricMaterial;
	}
	else if (Skin == ESlimeBodySkin::Spectral)
	{
		if (ResolvedSpectralMaterial)
		{
			Wanted = ResolvedSpectralMaterial;
		}
		else
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("SlimeBodyComponent: spectral body material missing on '%s'; staying on the classic skin."), *GetNameSafe(GetOwner()));
		}
	}
	if (!Wanted)
	{
		return;
	}

	bLuminousSkinActive = Skin == ESlimeBodySkin::Luminous;
	// Only the volumetric skin pays for the density snapshot + GPU upload.
	bVolumetricActive = bWantVolumetric;
	Surface.SetCaptureBodyField(bVolumetricActive);
	if (bVolumetricActive)
	{
		EnsureDensityAtlas();
	}
	else
	{
		ReleaseDensityAtlas();
	}

	ResolvedMaterial = Wanted;
	if (SurfaceMesh && bMeshSectionCreated)
	{
		// The element component detects that slot 0 no longer holds its MID and rebuilds it with the current profile.
		SurfaceMesh->SetMaterial(0, ResolvedMaterial);
	}
}

void USlimeBodyComponent::EnsureDensityAtlas()
{
	if (!bVolumetricActive)
	{
		return;
	}

	// Tile edge = the largest grid we may ever be asked to upload. Spread mode can raise the
	// surface builder's MaxGridDim above SurfaceParams, so also honour the live field.
	int32 TileDim = FMath::Max(SurfaceParams.MaxGridDim, 4);
	const FSlimeBodyField& Field = Surface.GetBodyField();
	if (Field.IsValid())
	{
		TileDim = FMath::Max3(TileDim, Field.Dims.GetMax(), 4);
	}
	if (DensityAtlas && AtlasTileDim == TileDim)
	{
		return;
	}

	AtlasTileDim = TileDim;
	AtlasTilesX = FMath::Max(FMath::CeilToInt(FMath::Sqrt(float(TileDim))), 1);
	AtlasTilesY = FMath::Max(FMath::DivideAndRoundUp(TileDim, AtlasTilesX), 1);
	const int32 Width = AtlasTileDim * AtlasTilesX;
	const int32 Height = AtlasTileDim * AtlasTilesY;

	DensityAtlas = UTexture2D::CreateTransient(Width, Height, PF_R16F);
	if (!DensityAtlas)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeBodyComponent: could not create the %dx%d density atlas."), Width, Height);
		return;
	}
	DensityAtlas->SRGB = false;
	DensityAtlas->Filter = TF_Bilinear;
	DensityAtlas->AddressX = TA_Clamp;
	DensityAtlas->AddressY = TA_Clamp;
	DensityAtlas->CompressionSettings = TC_HDR;
	DensityAtlas->NeverStream = true;
	DensityAtlas->UpdateResource();

	AtlasRegion = FUpdateTextureRegion2D(0, 0, 0, 0, Width, Height);
	for (TArray<uint16>& Staging : AtlasStaging)
	{
		Staging.SetNumZeroed(Width * Height);
	}
	AtlasBoundMid.Reset();
	bFieldParamsValid = false;

	UE_LOG(LogSlimeFable, Log, TEXT("SlimeBodyComponent: density atlas %dx%d (tile %d, %dx%d slices)."),
		Width, Height, AtlasTileDim, AtlasTilesX, AtlasTilesY);
}

void USlimeBodyComponent::ReleaseDensityAtlas()
{
	DensityAtlas = nullptr;
	for (TArray<uint16>& Staging : AtlasStaging)
	{
		Staging.Empty();
	}
	AtlasTileDim = 0;
	AtlasTilesX = 0;
	AtlasTilesY = 0;
	AtlasBoundMid.Reset();
	bFieldParamsValid = false;
}

void USlimeBodyComponent::UploadBodyField()
{
	if (!bVolumetricActive || !SurfaceMesh)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(SlimeBody_UploadBodyField);

	const FSlimeBodyField& Field = Surface.GetBodyField();
	if (!Field.IsValid())
	{
		// No body cluster this frame: tell the shader to take the no-volume path.
		bFieldParamsValid = false;
		PushFieldParams(FVector::ZeroVector);
		return;
	}

	EnsureDensityAtlas();
	if (!DensityAtlas || AtlasStaging[0].Num() == 0)
	{
		return;
	}

	TArray<uint16>& Staging = AtlasStaging[AtlasStagingIndex];
	AtlasStagingIndex = (AtlasStagingIndex + 1) % UE_ARRAY_COUNT(AtlasStaging);

	const int32 Width = AtlasTileDim * AtlasTilesX;
	const FIntVector Dims = Field.Dims;
	const float* Src = Field.Density.GetData();
	uint16* Dst = Staging.GetData();
	for (int32 Z = 0; Z < Dims.Z; ++Z)
	{
		const int32 TileX = (Z % AtlasTilesX) * AtlasTileDim;
		const int32 TileY = (Z / AtlasTilesX) * AtlasTileDim;
		for (int32 Y = 0; Y < Dims.Y; ++Y)
		{
			uint16* Row = Dst + (TileY + Y) * Width + TileX;
			const float* SrcRow = Src + (Z * Dims.Y + Y) * Dims.X;
			for (int32 X = 0; X < Dims.X; ++X)
			{
				Row[X] = FFloat16(SrcRow[X]).Encoded;
			}
		}
	}

	// Staging buffers rotate, so nothing to free once the copy has been consumed.
	DensityAtlas->UpdateTextureRegions(
		0, 1, &AtlasRegion,
		Width * sizeof(uint16), sizeof(uint16),
		reinterpret_cast<uint8*>(Staging.GetData()),
		[](uint8*, const FUpdateTextureRegion2D*) {});

	FieldOrigin = Field.Origin;
	FieldDims = Dims;
	FieldCellSize = Field.CellSize;
	FieldIso = Field.Iso;
	bFieldParamsValid = true;
	PushFieldParams(FVector::ZeroVector);
}

void USlimeBodyComponent::PushFieldParams(const FVector& MeshOffset)
{
	using namespace SlimeBodyPrivate;

	UMaterialInstanceDynamic* Mid = SurfaceMesh ? Cast<UMaterialInstanceDynamic>(SurfaceMesh->GetMaterial(0)) : nullptr;
	if (!Mid)
	{
		return;
	}

	if (AtlasBoundMid.Get() != Mid)
	{
		// The element component rebuilds its MID on skin / quality changes; rebind the atlas then.
		if (DensityAtlas)
		{
			Mid->SetTextureParameterValue(ParamDensityAtlas, DensityAtlas);
		}
		AtlasBoundMid = Mid;
	}

	if (!bFieldParamsValid)
	{
		// GridInfo.x (cell size) == 0 is the shader's "no field" signal.
		Mid->SetVectorParameterValue(ParamGridInfo, FLinearColor(0.f, 0.f, 0.f, 0.f));
		return;
	}

	// The mesh slides by MeshOffset between rebuilds; move the grid with it so world positions still line up.
	const FVector Origin = FieldOrigin + MeshOffset;
	Mid->SetVectorParameterValue(ParamGridOrigin, FLinearColor(float(Origin.X), float(Origin.Y), float(Origin.Z), 0.f));
	Mid->SetVectorParameterValue(ParamGridDims, FLinearColor(float(FieldDims.X), float(FieldDims.Y), float(FieldDims.Z), float(AtlasTileDim)));
	Mid->SetVectorParameterValue(ParamGridInfo, FLinearColor(FieldCellSize, FieldIso, float(AtlasTilesX), float(AtlasTilesY)));
}

void USlimeBodyComponent::ResolveMaterial()
{
	ResolvedClassicMaterial = BodyMaterial;
	if (!ResolvedClassicMaterial && !BodyMaterialPath.IsNull())
	{
		ResolvedClassicMaterial = BodyMaterialPath.LoadSynchronous();
	}
	if (!ResolvedClassicMaterial)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeBodyComponent: no body material assigned on '%s'; the surface will use the engine default."), *GetNameSafe(GetOwner()));
	}
	if (!SpectralBodyMaterialPath.IsNull())
	{
		ResolvedSpectralMaterial = SpectralBodyMaterialPath.LoadSynchronous();
	}
	if (!VolumetricBodyMaterialPath.IsNull())
	{
		ResolvedVolumetricMaterial = VolumetricBodyMaterialPath.LoadSynchronous();
	}
	if (!LuminousBodyMaterialPath.IsNull())
	{
		ResolvedLuminousMaterial = LuminousBodyMaterialPath.LoadSynchronous();
	}
	ResolvedMaterial = ResolvedLuminousMaterial ? ResolvedLuminousMaterial
		: (ResolvedSpectralMaterial ? ResolvedSpectralMaterial : ResolvedClassicMaterial);
	bLuminousSkinActive = ResolvedMaterial && ResolvedMaterial == ResolvedLuminousMaterial;

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

	if (!XRayDepthProxyMaterialPath.IsNull())
	{
		ResolvedXRayDepthProxy = XRayDepthProxyMaterialPath.LoadSynchronous();
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

void USlimeBodyComponent::TickFragmentAttacks(float DeltaTime)
{
	if (!Solver.HasFragments() || FragmentAttackRadius <= KINDA_SMALL_NUMBER)
	{
		FragmentAttackCooldownRemaining.Reset();
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (bRecalling)
	{
		ClearShotTargets();
		return;
	}

	if (const USlimeDevourComponent* Devour = Owner->FindComponentByClass<USlimeDevourComponent>())
	{
		if (Devour->IsCombatLocked())
		{
			return;
		}
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	Solver.RefreshShotStates();
	const TArray<FSlimeSolver::FShotState>& Shots = Solver.GetShotStates();
	if (Shots.Num() == 0)
	{
		FragmentAttackCooldownRemaining.Reset();
		return;
	}

	TSet<uint8> LiveIds;
	for (const FSlimeSolver::FShotState& Shot : Shots)
	{
		LiveIds.Add(Shot.Id);
		float& Cd = FragmentAttackCooldownRemaining.FindOrAdd(Shot.Id);
		Cd = FMath::Max(Cd - DeltaTime, 0.f);
	}

	for (auto It = FragmentAttackCooldownRemaining.CreateIterator(); It; ++It)
	{
		if (!LiveIds.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}

	USlimeCombatComponent* CombatComp = Owner->FindComponentByClass<USlimeCombatComponent>();
	USlimeElementComponent* ElementComp = Owner->FindComponentByClass<USlimeElementComponent>();
	FSlimeSkillDef HitSkill;
	HitSkill.Slot = ESlimeSkillSlot::Combo1;
	HitSkill.Damage = 12.f;
	if (CombatComp)
	{
		const FSlimeElementKitData Kit = CombatComp->GetCurrentKit();
		if (const FSlimeSkillDef* Def = Kit.GetSkillSlot(ESlimeSkillSlot::Combo1))
		{
			HitSkill = *Def;
		}
	}
	const ESlimeElement Element = ElementComp ? ElementComp->CurrentElement : ESlimeElement::Physical;
	HitSkill.Element = Element;
	HitSkill.bAppliesElementAura = true;
	HitSkill.Damage = FMath::Max(HitSkill.Damage * FragmentAttackDamageScale, 0.f);

	const float Interval = FMath::Max(FragmentAttackInterval, 0.1f);
	const float ChaseSpeed = FMath::Max(FragmentChaseSpeed, 50.f);
	const float MeleeRangeSq = FMath::Square(FMath::Max(FragmentMeleeRange, 10.f));
	const float RadiusSq = FMath::Square(FragmentAttackRadius);
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SlimeFragmentAttack), false, Owner);
	QueryParams.AddIgnoredActor(Owner);

	for (const FSlimeSolver::FShotState& Shot : Shots)
	{
		if (Shot.Phase != FSlimeSolver::EShotPhase::Active) continue;
		// Clear leftover devour-style targets so BallisticLife is not refreshed forever.
		if (Solver.IsShotTargeted(Shot.Id))
		{
			ClearShotTarget(Shot.Id);
		}

		const FVector Center(Shot.Center);
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(
			Overlaps,
			Center,
			FQuat::Identity,
			ObjectParams,
			FCollisionShape::MakeSphere(FragmentAttackRadius),
			QueryParams);

		FCollisionObjectQueryParams WorldParams;
		WorldParams.AddObjectTypesToQuery(ECC_WorldStatic);
		WorldParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		TArray<FOverlapResult> WorldOverlaps;
		if (Shot.Id != 0)
		{
			World->OverlapMultiByObjectType(
				WorldOverlaps,
				Center,
				FQuat::Identity,
				WorldParams,
				FCollisionShape::MakeSphere(FMath::Max(FragmentMeleeRange, 80.f)),
				QueryParams);
		}
		for (const FOverlapResult& Overlap : WorldOverlaps)
		{
			AActor* Candidate = Overlap.GetActor();
			if (!Candidate || !Candidate->GetClass()->ImplementsInterface(USlimeElementReceiver::StaticClass()))
			{
				continue;
			}
			if (FVector::DistSquared(Center, Candidate->GetActorLocation()) > MeleeRangeSq)
			{
				continue;
			}
			float& ElementCd = FragmentAttackCooldownRemaining.FindOrAdd(Shot.Id);
			if (ElementCd <= KINDA_SMALL_NUMBER)
			{
				SlimeElementDelivery::NotifyActor(Candidate, Element, Owner, 1.f);
				ElementCd = Interval;
			}
			break;
		}

		AActor* BestTarget = nullptr;
		float BestDistSq = RadiusSq;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Candidate = Overlap.GetActor();
			if (!USlimeHitProbe::IsValidDamageTarget(Candidate) || !USlimeHitProbe::IsHostile(Owner, Candidate))
			{
				continue;
			}
			const float DistSq = FVector::DistSquared(Center, Candidate->GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				BestTarget = Candidate;
			}
		}

		if (!BestTarget)
		{
			continue;
		}

		const FVector EnemyLoc = BestTarget->GetActorLocation();
		FVector ChaseTarget = EnemyLoc;
		const float Fraction = LaunchFractionOverride > KINDA_SMALL_NUMBER ? LaunchFractionOverride : LaunchFraction;
		const float MiniR = Solver.GetScaledRestRadius() * FMath::Pow(FMath::Clamp(Fraction, 0.05f, 0.6f), 1.f / 3.f);
		const bool bHasShotFloor = Shot.FloorZ > -1.e8f;
		ChaseTarget.Z = bHasShotFloor ? (Shot.FloorZ + Solver.GetShotSupportHeight()) : Center.Z;
		if (Shot.bGrounded) Solver.SteerShot(Shot.Id, ChaseTarget, ChaseSpeed, DeltaTime, /*bKeepGrounded=*/true);

		const float HorizDistSq = FVector::DistSquaredXY(Center, EnemyLoc);
		float& Cd = FragmentAttackCooldownRemaining.FindOrAdd(Shot.Id);
		if (Cd > KINDA_SMALL_NUMBER || HorizDistSq > MeleeRangeSq)
		{
			continue;
		}

		float DamageAmount = HitSkill.Damage;
		if (CombatComp && DamageAmount > 0.f)
		{
			DamageAmount = CombatComp->ResolveOutgoingDamage(HitSkill);
		}
		if (DamageAmount <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector HitLoc = BestTarget->GetActorLocation();
		if (ICombatDamageable* Damageable = Cast<ICombatDamageable>(BestTarget))
		{
			Damageable->ApplyDamage(DamageAmount, Owner, HitLoc, FVector::ZeroVector);
		}
		else if (USlimeHealthComponent* Health = BestTarget->FindComponentByClass<USlimeHealthComponent>())
		{
			Health->ApplyDamage(DamageAmount, Owner, HitLoc, FVector::ZeroVector);
		}

		if (Cast<ASlimeCharacter>(Owner))
		{
			USlimeFloatingTextWidget::Spawn(
				BestTarget,
				HitLoc + FVector(0.f, 0.f, 40.f),
				FText::FromString(FString::Printf(TEXT("%.0f"), DamageAmount)),
				SlimeCombat::GetElementVfxColor(Element));
		}

		if (HitSkill.bAppliesElementAura)
		{
			if (USlimeStatusComponent* Status = BestTarget->FindComponentByClass<USlimeStatusComponent>())
			{
				Status->ApplyAura(Element, Owner);
			}
			SlimeElementDelivery::NotifyActor(BestTarget, Element, Owner, 1.f);
		}
		Cd = Interval;
	}
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
	// nobody can see is the easiest cost to delete. The screen-space silhouette disables
	// depth test, so it stays unoccluded and keeps its render time while the body is fully
	// behind a wall. That render time is what keeps the outline following the squeeze.
	const bool bSurfaceVisible = !SurfaceMesh || SurfaceMesh->WasRecentlyRendered(0.3f);
	const bool bXRayVisible = bScreenSpaceXRay && XRayMesh && XRayMesh->WasRecentlyRendered(0.3f);
	const bool bVisible = bSurfaceVisible || bXRayVisible;
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

	UpdateAmbientLight(DeltaTime);
	UpdateBubbleVisuals(DeltaTime);
	UpdateBubblesAndShellParams(DeltaTime);
	UpdateContactFootprint();
	UpdateVisualContactFootprint();
	UpdateContactDecal();
 UpdateShotContactDecals();

	TickFragmentAttacks(DeltaTime);
}

float USlimeBodyComponent::ComputeAmbientTarget(bool bLog) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 1.f;
	}

	const FVector Origin = GetShellCenter();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeAmbientTrace), false, GetOwner());
	Params.bTraceIntoSubComponents = false;

	float SkyOpen = 0.f;
	for (int32 i = 0; i < AmbientSkyRays; ++i)
	{
		SkyOpen += SkyRayOpen[i];
	}
	SkyOpen /= float(AmbientSkyRays);

	float Direct = 0.f;
	float SunVisLog = -1.f;
	float BestContribution = 0.f;
	FVector4f BestKeyLight(0.35f, 0.2f, 0.91f, 0.f);
	for (TObjectIterator<UDirectionalLightComponent> It; It; ++It)
	{
		const UDirectionalLightComponent* Light = *It;
		if (!Light || Light->GetWorld() != World || !Light->bAffectsWorld || !Light->IsVisible() || Light->Intensity <= 0.f)
		{
			continue;
		}
		const FVector ToLight = -Light->GetDirection();
		const float SinElev = float(ToLight.Z);
		if (SinElev <= 0.f)
		{
			continue;
		}
		const float Elev = 0.35f + 0.65f * FMath::SmoothStep(0.f, 0.5f, SinElev);
		const float Horizon = FMath::SmoothStep(0.f, 0.08f, SinElev);
		const float Color = Light->GetLightColor().GetLuminance();
		const float Contribution = Light->Intensity * Color * Elev * Horizon;
		if (Contribution <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		FHitResult Hit;
		const bool bBlocked = World->LineTraceSingleByChannel(
			Hit, Origin, Origin + ToLight * AmbientSunTraceLength, ECC_Visibility, Params);
		const float Vis = bBlocked ? 0.f : 1.f;
		SunVisLog = FMath::Max(SunVisLog, Vis);
		Direct += Contribution * Vis;
		if (Contribution > BestContribution)
		{
			BestContribution = Contribution;
			BestKeyLight = FVector4f(float(ToLight.X), float(ToLight.Y), float(ToLight.Z), Vis * Elev * Horizon);
		}
	}
	KeyLightDir = BestKeyLight;
	KeyLightContribution = BestContribution;

	float Sky = 0.f;
	for (TObjectIterator<USkyLightComponent> It; It; ++It)
	{
		const USkyLightComponent* SkyLight = *It;
		if (!SkyLight || SkyLight->GetWorld() != World || !SkyLight->bAffectsWorld || !SkyLight->IsVisible())
		{
			continue;
		}
		Sky += SkyLight->Intensity * SkyLight->GetLightColor().GetLuminance() * AmbientSkyWeight;
	}

	const float Raw = Direct + Sky * SkyOpen;
	const float Target = FMath::Clamp(Raw / FMath::Max(AmbientReference, 0.001f), AmbientFloor, 1.f);
	if (bLog)
	{
		UE_LOG(LogSlimeFable, Log, TEXT("SlimeAmbient %s: Raw=%.3f Direct=%.3f Sky=%.3f SkyOpen=%.2f SunVis=%.0f Target=%.3f Scale=%.3f (Reference=%.3f)"),
			*GetNameSafe(GetOwner()), Raw, Direct, Sky, SkyOpen, SunVisLog, Target, AmbientScale, AmbientReference);
	}
	return Target;
}

void USlimeBodyComponent::UpdateAmbientLight(float DeltaTime)
{
	if (!bAmbientLightResponse)
	{
		AmbientScale = 1.f;
		AmbientTarget = 1.f;
		bAmbientPrimed = false;
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const bool bDebug = CVarSlimeAmbientDebug.GetValueOnGameThread() != 0;
	AmbientLogTimer += DeltaTime;
	const bool bLog = bDebug && AmbientLogTimer >= 1.f;
	if (bLog)
	{
		AmbientLogTimer = 0.f;
	}

	AmbientTimer -= DeltaTime;
	if (!bAmbientPrimed || AmbientTimer <= 0.f || bLog)
	{
		AmbientTimer = AmbientTraceInterval;
		const FVector Origin = GetShellCenter();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeAmbientTrace), false, GetOwner());

		// Straight up plus a ring at ~55 deg from zenith; unprimed bodies fire all rays at once.
		auto SkyDir = [](int32 Index)
		{
			if (Index == 0)
			{
				return FVector::UpVector;
			}
			const float Angle = 2.f * PI * float(Index - 1) / float(AmbientSkyRays - 1);
			return FVector(FMath::Cos(Angle) * 0.82f, FMath::Sin(Angle) * 0.82f, 0.57f).GetSafeNormal();
		};
		const int32 RaysNow = bAmbientPrimed ? 2 : AmbientSkyRays;
		for (int32 r = 0; r < RaysNow; ++r)
		{
			const int32 Index = bAmbientPrimed ? SkyRayCursor : r;
			FHitResult Hit;
			const bool bBlocked = World->LineTraceSingleByChannel(
				Hit, Origin, Origin + SkyDir(Index) * AmbientSkyTraceLength, ECC_Visibility, Params);
			SkyRayOpen[Index] = bBlocked ? 0.f : 1.f;
			if (bAmbientPrimed)
			{
				SkyRayCursor = (SkyRayCursor + 1) % AmbientSkyRays;
			}
		}

		AmbientTarget = ComputeAmbientTarget(bLog);
		if (!bAmbientPrimed)
		{
			AmbientScale = AmbientTarget;
			bAmbientPrimed = true;
		}
	}

	AmbientScale = FMath::FInterpTo(AmbientScale, AmbientTarget, DeltaTime, AmbientSmoothSpeed);
}

FVector USlimeBodyComponent::GetBubbleWorldPosition(int32 Index) const
{
	if (Index < 0 || Index >= MaxBubbles || !bBubblesInitialised)
	{
		return GetShellCenter();
	}
	return GetShellCenter() + BubbleOffset[Index];
}

void USlimeBodyComponent::RespawnBubble(int32 Index, bool bStagger)
{
	const float Angle = FMath::FRand() * 2.f * PI;
	const float Rad = FMath::FRandRange(0.08f, 0.36f);
	BubbleLateral[Index] = FVector2D(FMath::Cos(Angle) * Rad, FMath::Sin(Angle) * Rad);
	BubbleSpeed[Index] = FMath::FRandRange(0.10f, 0.18f);
	BubblePhase[Index] = bStagger ? (float(Index) / float(MaxBubbles)) : 0.f;
	BubbleBurst[Index] = 0.f;
	BubbleRestNorm[Index] = FVector(BubbleLateral[Index].X, BubbleLateral[Index].Y, -0.55);
}

void USlimeBodyComponent::UpdateBubblesAndShellParams(float DeltaTime)
{
	if (!bBubblesInitialised)
	{
		for (int32 i = 0; i < MaxBubbles; ++i)
		{
			BubbleOffset[i] = FVector::ZeroVector;
			BubbleVelocity[i] = FVector::ZeroVector;
			RespawnBubble(i, true);
		}
		bBubblesInitialised = true;
	}

	const FVector Axes = GetShellAxes();
	const FVector Fwd = GetInertiaForward().GetSafeNormal2D(KINDA_SMALL_NUMBER, FVector::ForwardVector);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd).GetSafeNormal();
	const FVector Up = FVector::UpVector;

	// Critically damped follower so bubbles land half a beat after the body stops / squashes.
	constexpr float Frequency = 1.6f;
	constexpr float Damping = 0.9f;
	constexpr float BurstSeconds = 0.25f;
	const float Omega = 2.f * PI * Frequency;
	const float K = Omega * Omega;
	const float C = 2.f * Damping * Omega;
	const float Dt = FMath::Clamp(DeltaTime, 0.f, 0.05f);
	const float MinR = FMath::Min(BubbleMinR, BubbleMaxR);
	const float MaxR = FMath::Max(BubbleMinR, BubbleMaxR);

	float Radii[MaxBubbles];
	const int32 ActiveBubbles = FMath::Clamp(BubbleCount, 0, MaxBubbles);
	for (int32 i = 0; i < MaxBubbles; ++i)
	{
		if (i >= ActiveBubbles)
		{
			Radii[i] = 0.f;
			continue;
		}
		if (BubbleBurst[i] > 0.f)
		{
			BubbleBurst[i] += Dt / BurstSeconds;
			if (BubbleBurst[i] >= 1.f)
			{
				RespawnBubble(i, false);
			}
		}
		else
		{
			BubblePhase[i] += BubbleSpeed[i] * Dt;
			if (BubblePhase[i] >= 1.f)
			{
				BubblePhase[i] = 1.f;
				BubbleBurst[i] = KINDA_SMALL_NUMBER;
			}
		}

		const float Phase = FMath::Clamp(BubblePhase[i], 0.f, 1.f);
		const float Z = FMath::Lerp(-0.55f, 0.70f, Phase);
		const float Sway = FMath::Sin(Phase * 4.2f + float(i) * 1.7f) * 0.05f;
		const float SwayB = FMath::Cos(Phase * 3.1f + float(i) * 2.1f) * 0.04f;
		BubbleRestNorm[i] = FVector(BubbleLateral[i].X + Sway, BubbleLateral[i].Y + SwayB, Z);

		const float RiseR = FMath::Lerp(MinR, MaxR, Phase);
		const float BurstR = MaxR * (1.f + BubbleBurst[i] * 1.8f);
		Radii[i] = FMath::Clamp(float(Axes.GetMin()) * (BubbleBurst[i] > 0.f ? BurstR : RiseR), 0.25f, 6.f);

		const FVector RestWorld =
			Fwd * (BubbleRestNorm[i].X * Axes.X * 0.72) +
			Right * (BubbleRestNorm[i].Y * Axes.Y * 0.72) +
			Up * (BubbleRestNorm[i].Z * Axes.Z * 0.72);

		const FVector Accel = (RestWorld - BubbleOffset[i]) * K - BubbleVelocity[i] * C;
		BubbleVelocity[i] += Accel * Dt;
		BubbleOffset[i] += BubbleVelocity[i] * Dt;

		const FVector Local(
			FVector::DotProduct(BubbleOffset[i], Fwd) / FMath::Max(Axes.X, 1.0),
			FVector::DotProduct(BubbleOffset[i], Right) / FMath::Max(Axes.Y, 1.0),
			FVector::DotProduct(BubbleOffset[i], Up) / FMath::Max(Axes.Z, 1.0));
		const double Len = Local.Size();
		constexpr double MaxNorm = 0.8;
		if (Len > MaxNorm)
		{
			const FVector Clamped = Local * (MaxNorm / Len);
			BubbleOffset[i] = Fwd * (Clamped.X * Axes.X) + Right * (Clamped.Y * Axes.Y) + Up * (Clamped.Z * Axes.Z);
			BubbleVelocity[i] *= 0.5;
		}
	}

	UMaterialInstanceDynamic* Mid = SurfaceMesh ? Cast<UMaterialInstanceDynamic>(SurfaceMesh->GetMaterial(0)) : nullptr;
	if (!Mid)
	{
		return;
	}
	const FVector Center = GetShellCenter();
	Mid->SetScalarParameterValue(TEXT("RearEyeStrength"), RearEyeStrength);
 Mid->SetScalarParameterValue(TEXT("EyeEmissionStrength"), EyeEmissionStrength);
 Mid->SetScalarParameterValue(TEXT("EyeReadabilityFloor"), EyeReadabilityFloor);
 Mid->SetScalarParameterValue(TEXT("BubbleSizeScaleMin"), BubbleSizeScaleMin);
 Mid->SetScalarParameterValue(TEXT("BubbleSizeScaleMax"), FMath::Max(BubbleSizeScaleMax, BubbleSizeScaleMin));
 Mid->SetScalarParameterValue(TEXT("BubbleEdgeStrength"), BubbleEdgeStrength);
 Mid->SetScalarParameterValue(TEXT("BubbleVisualRadiusScale"), BubbleVisualRadiusScale);
 Mid->SetScalarParameterValue(TEXT("DevourBubbleRadiusScale"), DevourBubbleRadiusScale);
    Mid->SetScalarParameterValue(TEXT("BubblePopSeconds"), FMath::Max(BubblePopSeconds, 0.01f));
    Mid->SetScalarParameterValue(TEXT("BubbleRimWidth"), FMath::Clamp(BubbleRimWidth, 0.005f, 0.25f));
 Mid->SetScalarParameterValue(TEXT("BubbleCoreStrength"), BubbleCoreStrength);
 Mid->SetScalarParameterValue(TEXT("FineBubbleCount"), FineBubbleCount);
 Mid->SetScalarParameterValue(TEXT("ExternalBodyBubbles"), BubbleVisualMesh ? 1.f : 0.f);
 Mid->SetScalarParameterValue(TEXT("FineBubbleMinRadius"), FineBubbleMinRadius);
 Mid->SetScalarParameterValue(TEXT("FineBubbleMaxRadius"), FMath::Max(FineBubbleMaxRadius, FineBubbleMinRadius));
 Mid->SetScalarParameterValue(TEXT("DevourBubbleCount"), DevourBubbleCount);
 Mid->SetScalarParameterValue(TEXT("DevourBubbleSeconds"), DevourBubbleSeconds);
 const float BubbleTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
 Mid->SetScalarParameterValue(TEXT("BubbleClock"), BubbleTime);
 UMeshComponent* DigestMesh = DigestBubbleSource.Get();
 const bool bDigestActive = IsValid(DigestMesh) && DigestMesh->IsVisible();
 Mid->SetScalarParameterValue(TEXT("DigestActive"), bDigestActive ? 1.f : 0.f);
 if (bDigestActive)
 {
  const FVector Rel = DigestMesh->Bounds.Origin-Center;
  const FVector Extent = DigestMesh->Bounds.BoxExtent/FMath::Max(DigestMesh->BoundsScale,0.01f);
  const FVector Local(FVector::DotProduct(Rel,Fwd)/FMath::Max(Axes.X,1.0), FVector::DotProduct(Rel,Right)/FMath::Max(Axes.Y,1.0), Rel.Z/FMath::Max(Axes.Z,1.0));
  const FVector LocalAxes(FVector::DotProduct(Extent,Fwd.GetAbs())/FMath::Max(Axes.X,1.0), FVector::DotProduct(Extent,Right.GetAbs())/FMath::Max(Axes.Y,1.0), Extent.Z/FMath::Max(Axes.Z,1.0));
  Mid->SetVectorParameterValue(TEXT("DigestCenter"),FLinearColor(Local));
  Mid->SetVectorParameterValue(TEXT("DigestAxes"),FLinearColor(LocalAxes));
 }

 for (int32 I=0; I<2; ++I)
 {
  Mid->SetVectorParameterValue(FName(*FString::Printf(TEXT("DevourBurstPosition%d"),I)), FLinearColor(DevourBursts[I].Local));
  Mid->SetScalarParameterValue(FName(*FString::Printf(TEXT("DevourBurstAge%d"),I)), BubbleTime-DevourBursts[I].Started);
  Mid->SetScalarParameterValue(FName(*FString::Printf(TEXT("DevourBurstSeed%d"),I)), DevourBursts[I].Seed);
 }
 Mid->SetScalarParameterValue(SlimeBodyPrivate::ParamAmbientScale, AmbientScale);
	Mid->SetVectorParameterValue(SlimeBodyPrivate::ParamKeyLightDir, FLinearColor(KeyLightDir.X, KeyLightDir.Y, KeyLightDir.Z, KeyLightDir.W));
	{
		// Ground contact band in the jelly skins; sentinel switches it off while airborne / clinging / spread.
		const float Hang = float(GetFootLocation().Z) - FloorZ;
		const bool bGrounded = FloorZ > -1.e8f && !bClingVisual && !bSpread && SpreadBlend <= 0.f
			&& Hang > -8.f && Hang < ContactFadeHeight;
		Mid->SetScalarParameterValue(SlimeBodyPrivate::ParamContactFloorZ, bGrounded ? FloorZ : -1.e9f);
	}
	Mid->SetScalarParameterValue(SlimeBodyPrivate::ParamAmbientDebug, CVarSlimeAmbientDebug.GetValueOnGameThread() != 0 ? 1.f : 0.f);
	Mid->SetVectorParameterValue(SlimeBodyPrivate::ParamShellCenter, FLinearColor(float(Center.X), float(Center.Y), float(Center.Z), 0.f));
	Mid->SetVectorParameterValue(SlimeBodyPrivate::ParamShellAxes, FLinearColor(float(Axes.X), float(Axes.Y), float(Axes.Z), 0.f));
	Mid->SetVectorParameterValue(SlimeBodyPrivate::ParamShellForward, FLinearColor(float(Fwd.X), float(Fwd.Y), float(Fwd.Z), 0.f));
	for (int32 i = 0; i < MaxBubbles; ++i)
	{
		const FVector P = Center + BubbleOffset[i];
		const float R = (i < ActiveBubbles) ? Radii[i] : 0.f;
		Mid->SetVectorParameterValue(SlimeBodyPrivate::ParamBubbles[i], FLinearColor(float(P.X), float(P.Y), float(P.Z), R));
		Mid->SetScalarParameterValue(SlimeBodyPrivate::ParamBubbleBurst[i], (i < ActiveBubbles) ? BubbleBurst[i] : 0.f);
	}

	// Shot cluster ellipsoids, indexed by the slot table from the last surface rebuild so the
	// vertex colour tag and ShotCenter{i} always refer to the same mini-slime. a = 0 disables the slot.
	Solver.RefreshShotStates();
	const TArray<FSlimeSolver::FShotState>& Shots = Solver.GetShotStates();
	const float MiniR = FMath::Max(Solver.GetMiniMembraneRadius(), 1.f);
	for (int32 Slot = 0; Slot < MaxShotSlots; ++Slot)
	{
		FLinearColor ShotParam(0.f, 0.f, 0.f, 0.f);
		if (ShotSlotIds.IsValidIndex(Slot))
		{
			const uint8 WantedId = ShotSlotIds[Slot];
			const FSlimeSolver::FShotState* Shot = Shots.FindByPredicate([WantedId](const FSlimeSolver::FShotState& S) { return S.Id == WantedId; });
			if (Shot)
			{
				ShotParam = FLinearColor(Shot->Center.X, Shot->Center.Y, Shot->Center.Z, MiniR);
			}
		}
		Mid->SetVectorParameterValue(SlimeBodyPrivate::ParamShotCenter[Slot], ShotParam);
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

	ShapeBlend = FMath::FInterpConstantTo(ShapeBlend, ShapeTarget, StepDelta, 1.f / FMath::Max(ShapeBlendTime, 0.01f));
	Solver.SetDomeShape(FMath::SmoothStep(0.f, 1.f, ShapeBlend), DomeWidthScale, DomeHeightScale, DomeUpwardRestoreScale);

	UpdateAnchor();

	// One squash amount drives shell height, radius, push, gravity and the vertical spring, so
	// pressing and releasing are the same motion run forwards and backwards.
	if (bSpread)
	{
		SpreadBlend = FMath::Min(SpreadBlend + StepDelta / FMath::Max(SpreadFlattenTime, 0.01f), 1.f);
	}
	else
	{
		SpreadBlend = FMath::Max(SpreadBlend - StepDelta / FMath::Max(SpreadRecoverDuration, 0.01f), 0.f);
	}
	const float Eased = FMath::SmoothStep(0.f, 1.f, SpreadBlend);
	const bool bSpreadActive = bSpread || SpreadBlend > 0.f;
	const bool bSpreadDrape = bSpreadFollowTerrain && bSpreadActive;

	const float HalfHeight = FMath::Max(SpreadHalfHeight, 0.5f);
	if (bSpreadActive)
	{
		const float Radius = FMath::Lerp(SolverParams.RestRadius, SolverParams.RestRadius * SpreadRadiusScale, Eased);
		Solver.SetSpread(true, Radius, bSpread ? SpreadPush * Eased : 0.f, HalfHeight);
		Solver.SetSpreadBlend(Eased);
		Solver.SetSpreadConcentrationScale(FMath::Lerp(1.f, SpreadConcentrationScale, Eased));
		Solver.SetSpreadLateralScale(SpreadLateralScale);
		Solver.SetGravityScale(FMath::Lerp(1.f, SpreadGravityScale, Eased));
	}
	else
	{
		Solver.SetSpread(false, 0.f, 0.f, HalfHeight);
		Solver.SetSpreadBlend(0.f);
		Solver.SetSpreadConcentrationScale(1.f);
		Solver.SetSpreadLateralScale(1.f);
		Solver.SetGravityScale(1.f);
	}

	if (bSpreadDrape)
	{
		UpdateGroundField(StepDelta);
		// Depth limit shrinks with the squash on release, drawing hanging goo back up.
		Solver.SetDrapeParams(SpreadDrapeDepth * Eased, SpreadDrapeTension, SpreadDrapeViscosity, SpreadMaxOverhang);
	}
	else
	{
		Solver.ClearGroundField();
		GroundFieldTimer = 0.f;
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
			Solver.RemoveUnprotectedShots();
			// Protected umbrella returns now own their motion; release global recall collision bypass.
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

	auto TraceFloorUnder = [this](const FVector& Origin, float ProxyRadius, float& OutZ, bool bIgnorePawns = true)
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return false;
		}

		FCollisionQueryParams Query(TEXT("SlimeFloor"), false, GetOwner());
		FCollisionResponseParams ResponseParams = FCollisionResponseParams::DefaultResponseParam;
		if (bIgnorePawns)
		{
			// Body + fragment floor must not treat enemy capsules as ground
			// (was lifting shots onto heads / clipping the blob against gunners).
			ResponseParams.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
		}
		const FVector Start = Origin + FVector(0.0, 0.0, 40.0);
		const FVector End = Origin - FVector(0.0, 0.0, 800.0);
		const float Radius = FMath::Max(ProxyRadius, 2.f);
		const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);

		// Prefer the first hit that is not FluidNinja TraceMesh / InteractionVolume —
		// those boards sit above the real ground and would ClipZ the whole blob away.
		TArray<FHitResult> Hits;
		if (World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn, Shape, Query, ResponseParams))
		{
			for (const FHitResult& Hit : Hits)
			{
				if (ShouldIgnoreFluidNinjaCollider(Hit.GetComponent()))
				{
					continue;
				}
				if (bIgnorePawns)
				{
					if (const AActor* HitActor = Hit.GetActor())
					{
						if (Cast<APawn>(HitActor) && HitActor != GetOwner())
						{
							continue;
						}
					}
				}
				OutZ = float(Hit.ImpactPoint.Z);
				return true;
			}
		}

		Hits.Reset();
		if (World->LineTraceMultiByChannel(Hits, Start, End, ECC_Pawn, Query, ResponseParams))
		{
			for (const FHitResult& Hit : Hits)
			{
				if (ShouldIgnoreFluidNinjaCollider(Hit.GetComponent()))
				{
					continue;
				}
				if (bIgnorePawns)
				{
					if (const AActor* HitActor = Hit.GetActor())
					{
						if (Cast<APawn>(HitActor) && HitActor != GetOwner())
						{
							continue;
						}
					}
				}
				OutZ = float(Hit.ImpactPoint.Z);
				return true;
			}
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
			UPrimitiveComponent* FloorComp = Movement->CurrentFloor.HitResult.GetComponent();
			const AActor* FloorActor = FloorComp ? FloorComp->GetOwner() : nullptr;
			const bool bFloorIsOtherPawn = FloorActor && Cast<APawn>(FloorActor) && FloorActor != GetOwner();
			if (FloorComp && !ShouldIgnoreFluidNinjaCollider(FloorComp) && !bFloorIsOtherPawn)
			{
				MovementFloorZ = float(Movement->CurrentFloor.HitResult.ImpactPoint.Z);
				FloorZ = MovementFloorZ;
				bBodyFloor = true;
			}
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
				&& Hang > 1.5f
				&& Hang <= MaxSnap)
			{
				if (USlimeCharacterMovementComponent* SlimeMovement = Cast<USlimeCharacterMovementComponent>(Movement))
				{
					SlimeMovement->QueueExternalCorrection(FVector(0.0, 0.0, double(-Hang)));
				}
				else
				{
					FHitResult Hit;
					Movement->SafeMoveUpdatedComponent(FVector(0.0, 0.0, double(-Hang)),
						OwnerCharacter->GetActorQuat(), true, Hit);
				}
			}
		}
	}

	FVector FragmentCenter;
	if (Solver.GetFragmentCenter(FragmentCenter))
	{
		const float ProxyR = FragmentProxyRadius > KINDA_SMALL_NUMBER
			? FragmentProxyRadius
			: SolverParams.RestRadius * 0.45f;
		if (!TraceFloorUnder(FragmentCenter, ProxyR, FragmentFloorZ, /*bIgnorePawns=*/true))
		{
			FragmentFloorZ = float(FragmentCenter.Z - 800.0);
		}
	}
	else
	{
		FragmentFloorZ = FloorZ;
	}

 Solver.QueryGround=[this](const FVector& Center,FHitResult& Hit) { return QueryShotGround(Center,Hit); };
 Solver.TraceSeparation=[this](const FVector& A,const FVector& B,float R,FHitResult& Hit) { return TraceShotWorld(Hit,A,B,R); };
 Solver.ClearShotFloorOverrides();
 Solver.RefreshShotStates();
 for (const FSlimeSolver::FShotState& Shot:Solver.GetShotStates())
 {
  FHitResult Hit;
  Solver.SetShotSupport(Shot.Id,QueryShotGround(FVector(Shot.Center),Hit) ? &Hit : nullptr);
 }

}

void USlimeBodyComponent::UpdateGroundField(float StepDelta)
{
	UWorld* World = GetWorld();
	const UCharacterMovementComponent* Movement = OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
	// Airborne (or no floor yet): keep the flat disk on the capsule. A sentinel floor far
	// below the feet would otherwise drop the whole sheet.
	if (!World || !Movement || !Movement->IsMovingOnGround() || FloorZ < -1.e8f)
	{
		Solver.ClearGroundField();
		GroundFieldTimer = 0.f;
		return;
	}

	GroundFieldTimer += StepDelta;
	const float CellWanted = FMath::Max(SpreadGroundCellSize, 4.f);
	const FVector Foot = GetFootLocation();
	const FVector Center(Foot.X, Foot.Y, Solver.GetBodyCenter().Z);
	const bool bMoved = (Center - LastGroundFieldCenter).SizeSquared2D() > FMath::Square(CellWanted * 0.5f);
	if (Solver.HasGroundField() && GroundFieldTimer < SpreadGroundRefreshInterval && !bMoved)
	{
		return;
	}
	GroundFieldTimer = 0.f;
	LastGroundFieldCenter = Center;

	const float CoverRadius = SolverParams.RestRadius * SpreadRadiusScale * FMath::Max(SolverParams.TetherSlack, 1.f);
	int32 Dim = FMath::CeilToInt((CoverRadius * 2.f) / CellWanted) + 1;
	float Cell = CellWanted;
	constexpr int32 MaxDim = 40;
	if (Dim > MaxDim)
	{
		Dim = MaxDim;
		Cell = (CoverRadius * 2.f) / float(FMath::Max(Dim - 1, 1));
	}
	Dim = FMath::Max(Dim, 3);

	const float OriginX = float(Center.X) - (float(Dim) * Cell) * 0.5f;
	const float OriginY = float(Center.Y) - (float(Dim) * Cell) * 0.5f;
	const float StartZ = FloorZ + FMath::Max(SpreadClimbHeight, 0.f);
	const float EndZ = FloorZ - FMath::Max(SpreadDrapeDepth, 0.f) - Cell;

	TArray<float> Heights;
	Heights.SetNumUninitialized(Dim * Dim);

	FCollisionQueryParams Query(TEXT("SlimeSpreadGround"), false, GetOwner());
	FCollisionResponseParams ResponseParams = FCollisionResponseParams::DefaultResponseParam;
	ResponseParams.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);

	TArray<FHitResult> Hits;
	Hits.Reserve(8);
	constexpr float MissZ = -1.e9f;

	for (int32 Y = 0; Y < Dim; ++Y)
	{
		const float YPos = OriginY + (float(Y) + 0.5f) * Cell;
		for (int32 X = 0; X < Dim; ++X)
		{
			const FVector Start(OriginX + (float(X) + 0.5f) * Cell, YPos, StartZ);
			const FVector End(Start.X, Start.Y, EndZ);
			float HitZ = MissZ;
			Hits.Reset();
			if (World->LineTraceMultiByChannel(Hits, Start, End, ECC_Pawn, Query, ResponseParams))
			{
				for (const FHitResult& Hit : Hits)
				{
					if (ShouldIgnoreFluidNinjaCollider(Hit.GetComponent()))
					{
						continue;
					}
					if (const AActor* HitActor = Hit.GetActor())
					{
						if (Cast<APawn>(HitActor) && HitActor != GetOwner())
						{
							continue;
						}
					}
					// Skip ceilings and the underside we exit when the ray started inside a volume.
					if (Hit.ImpactNormal.Z < 0.2f)
					{
						continue;
					}
					HitZ = float(Hit.ImpactPoint.Z);
					break;
				}
			}
			Heights[Y * Dim + X] = HitZ;
		}
	}

	Solver.SetGroundField(
		FVector2f(OriginX, OriginY), Cell, Dim, Dim, MoveTemp(Heights), FloorZ,
		FVector2f(float(Foot.X), float(Foot.Y)), SpreadStepHeight);
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
	if (FloorComponent && !ShouldIgnoreFluidNinjaCollider(FloorComponent))
	{
		Ordered.Add(FloorComponent);
		Seen.Add(FloorComponent);
	}
	for (const FOverlapResult& Overlap : Overlaps)
	{
		UPrimitiveComponent* Component = Overlap.GetComponent();
		if (Component && Component->IsCollisionEnabled() && !Seen.Contains(Component)
			&& !ShouldIgnoreFluidNinjaCollider(Component))
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
	// Nearest-blocking-hit semantics (HEAD): FluidNinja boards may be skipped, but another
	// Pawn stops the probe — do NOT look through enemies for a farther "ceiling".

	const float ProbeCeiling = DefaultCapsuleHalfHeight * 2.f + 40.f;
	const float StepCeilingIgnore = DefaultStepHeight + 8.f;
	float AvailableHeight = ProbeCeiling;
	{
		const float SphereRadius = FMath::Max(MinCapsuleRadius * 0.9f, 2.f);
		const FVector Start = Foot + FVector(0.0, 0.0, double(SphereRadius + ProbeGroundLift));
		const FVector End = Foot + FVector(0.0, 0.0, double(ProbeCeiling));
		TArray<FHitResult> Hits;
		if (World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(SphereRadius), Query))
		{
			for (const FHitResult& Hit : Hits)
			{
				if (ShouldIgnoreFluidNinjaCollider(Hit.GetComponent()))
				{
					continue;
				}
				if (const AActor* HitActor = Hit.GetActor())
				{
					if (Cast<APawn>(HitActor) && HitActor != GetOwner())
					{
						// Nearest solid is another pawn — same as SweepSingle hitting them: stop.
						break;
					}
				}
				if (float(Hit.ImpactNormal.GetSafeNormal().Z) <= -0.45f)
				{
					const float HitHeight = float(Hit.ImpactPoint.Z - Foot.Z);
					// Stair tread undersides sit within one step of the feet — not a real ceiling.
					if (HitHeight > StepCeilingIgnore)
					{
						AvailableHeight = HitHeight;
					}
				}
				break;
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

		auto IsBlockedAt = [this, World, ProbeHalfHeight, &Query](const FVector& Center, float Radius)
		{
			TArray<FOverlapResult> Overlaps;
			if (!World->OverlapMultiByChannel(
				Overlaps, Center, FQuat::Identity, ECC_Pawn,
				FCollisionShape::MakeCapsule(Radius, ProbeHalfHeight), Query))
			{
				return false;
			}
			for (const FOverlapResult& Overlap : Overlaps)
			{
				// Match HEAD OverlapBlockingTest, but skip FluidNinja sim boards.
				if (ShouldIgnoreFluidNinjaCollider(Overlap.GetComponent()))
				{
					continue;
				}
				return true;
			}
			return false;
		};

		auto IsStepRiserPinch = [&]() -> bool
		{
			if (HeadingDir.IsNearlyZero())
			{
				return false;
			}
			const FVector SweepStart = BaseCenter;
			const FVector SweepEnd = BaseCenter + HeadingDir * double(FMath::Max(LookAheadDistance, DefaultCapsuleRadius));
			TArray<FHitResult> RiserHits;
			if (!World->SweepMultiByChannel(
				RiserHits, SweepStart, SweepEnd, FQuat::Identity, ECC_Pawn,
				FCollisionShape::MakeSphere(FMath::Max(MinCapsuleRadius * 0.8f, 2.f)), Query))
			{
				return false;
			}
			for (const FHitResult& RiserHit : RiserHits)
			{
				if (ShouldIgnoreFluidNinjaCollider(RiserHit.GetComponent()))
				{
					continue;
				}
				const float NormalZ = float(RiserHit.ImpactNormal.GetSafeNormal().Z);
				if (FMath::Abs(NormalZ) > 0.35f)
				{
					return false;
				}
				const float HitAboveFoot = float(RiserHit.ImpactPoint.Z - Foot.Z);
				return HitAboveFoot >= -4.f && HitAboveFoot <= DefaultStepHeight + 8.f;
			}
			return false;
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
				if (!Comp || ShouldIgnoreFluidNinjaCollider(Comp))
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

bool USlimeBodyComponent::ShouldIgnoreFluidNinjaCollider(const UPrimitiveComponent* Component)
{
	using namespace SlimeBodyPrivate;
	if (!Component)
	{
		return false;
	}
	// The pad's AimQuery and anything spawned under it is never solid for the slime.
	if (Component->GetOwner() && Component->GetOwner()->IsA(ASlimeHomeFluidPad::StaticClass()))
	{
		return true;
	}
	if (!ComponentLooksLikeNinjaSimGeom(Component))
	{
		return false;
	}
	return OwnerLooksLikeNinjaLive(Component->GetOwner());
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

	if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
	{
		if (USlimeCharacterMovementComponent* SlimeMovement = Cast<USlimeCharacterMovementComponent>(Movement))
		{
			SlimeMovement->RequestCapsuleResize(NewRadius, NewHalfHeight);
		}
		else
		{
			const float PreviousHalfHeight = OwnerCapsule->GetUnscaledCapsuleHalfHeight();
			OwnerCapsule->SetCapsuleSize(NewRadius, NewHalfHeight, true);
			FHitResult Hit;
			Movement->SafeMoveUpdatedComponent(
				FVector(0.0, 0.0, double(NewHalfHeight - PreviousHalfHeight)),
				OwnerCharacter->GetActorQuat(), true, Hit);
		}
	}

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
		const float DomeAnchor = FMath::Lerp(1.f, DomeAnchorHeightScale, FMath::SmoothStep(0.f, 1.f, ShapeBlend));
		Anchor = Foot + FVector(0.0, 0.0, double(SolverParams.RestRadius * AnchorHeightFraction * DomeAnchor));
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
			if (USlimeCharacterMovementComponent* SlimeMovement = Cast<USlimeCharacterMovementComponent>(
				OwnerCharacter->GetCharacterMovement()))
			{
				SlimeMovement->QueueExternalCorrection(Correction);
			}
			else if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
			{
				FHitResult Hit;
				Movement->SafeMoveUpdatedComponent(Correction, OwnerCharacter->GetActorQuat(), true, Hit);
			}
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
	if (bSpread || SpreadBlend > 0.f || Solver.GetLandingSettleRemaining() > 0.f)
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
	// Draped fluid sits below the capsule floor. Clipping there would slice the sheet off.
	const bool bSpreadDrape = bSpread || SpreadBlend > 0.f;
	if (!bSpreadDrape && bVisualOnly && RequestedBodyScale > 1.05f && FloorZ > -1.e8f)
	{
		const float VisualR = SolverParams.RestRadius * RequestedBodyScale;
		VisualZLift = FMath::Max(0.f, VisualR - (RebuildBodyCOM.Z - FloorZ));
		ClipZ = FloorZ;
	}
	else if (!bSpreadDrape && !bClingVisual && FloorZ > -1.e8f)
	{
		const float Hang = float(GetFootLocation().Z) - FloorZ;
		if (Hang > -8.f && Hang < SolverParams.RestRadius)
		{
			// Splats reach below the floor and are cut flat there; any lift reads as hovering.
			ClipZ = FloorZ;
		}
	}

	TMap<uint8, float> ShotClips;
	Solver.RefreshShotStates();
	const float MiniR = FMath::Max(Solver.GetMiniMembraneRadius(), SolverParams.ParticleSpacing * 2.f);
	ShotSlotIds.Reset();
	for (const FSlimeSolver::FShotState& Shot : Solver.GetShotStates())
	{
		// Slot order = GetShotStates() order; the face / shell params and the vertex colours share this table.
		if (ShotSlotIds.Num() < MaxShotSlots && Shot.Id != 0)
		{
			ShotSlotIds.Add(Shot.Id);
		}
		if (!Shot.bHasSupport || (Shot.Phase != FSlimeSolver::EShotPhase::Active && Shot.Phase != FSlimeSolver::EShotPhase::Returning) || Shot.FloorZ <= -1.e8f)
		{
			continue;
		}
		const float ShotHang = Shot.Center.Z - Shot.FloorZ;
		if (ShotHang > -8.f && ShotHang < MiniR * 2.5f)
		{
			ShotClips.Add(Shot.Id, Shot.FloorZ);
		}
	}
	Surface.SetShotSlotIds(ShotSlotIds);
	Surface.SetConnectedShotRadius(Solver.GetMiniMembraneRadius());

	// The builder reserves 35% of the vertex budget for shots. Instead of letting that carve the top
	// off the body, grow the budget so the body's 65% share equals its normal budget. Held briefly
	// after the last fragment vanishes so rapid re-fires do not thrash the render sections.
	if (Solver.HasFragments())
	{
		LastFragmentSeenTime = GetWorld() ? float(GetWorld()->GetTimeSeconds()) : 0.f;
	}
	const float SinceFragments = GetWorld() ? float(GetWorld()->GetTimeSeconds()) - LastFragmentSeenTime : 1.e9f;
	if (SinceFragments < 0.5f)
	{
		const int32 Wanted = FMath::CeilToInt(float(ActiveSurface.MaxVertices) / 0.65f);
		ActiveSurface.MaxVertices = FMath::Clamp(Wanted, ActiveSurface.MaxVertices, FMath::Max(FragmentVertexBudgetCap, ActiveSurface.MaxVertices));
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
 TArray<uint8> SeparatingIds;
 Solver.GetSeparatingShotIds(SeparatingIds);
 TArray<FSlimeSurfaceBuilder::FVisualNeck> Necks;
 for (uint8 Id : SeparatingIds)
 {
  const FVector Center = Solver.GetShotCenterWorld(Id);
  const FVector Home = Solver.GetBodyCenter();
  const float Distance = FVector::Distance(Center,Home);
  if (Distance > Solver.GetScaledRestRadius()+Solver.GetMiniMembraneRadius()*1.8f) continue;
  MergingIds.AddUnique(Id);
  const FVector Direction = (Center-Home).GetSafeNormal();
  Necks.Add({Home+Direction*Solver.GetScaledRestRadius()*0.65f,Center, Solver.GetMiniMembraneRadius()*0.65f*(1.f-Solver.GetShotSeparationProgress(Id)),Id});
 }
 Surface.SetVisualNecks(Necks);
	{
		const float RimBlend = FMath::SmoothStep(0.f, 1.f, ShapeBlend);
		const bool bFlare = bDomeFlare && !bSpreadDrape && ClipZ > -1.e8f && RimBlend > 0.001f;
		// The old ground skirt and the bell flare would stack, so the skirt fades out as the dome comes in.
		const float SkirtScale = bFlare ? (1.f - RimBlend) : 1.f;
		Surface.SetGroundSkirt(bGroundSkirt ? SkirtHeight * SkirtScale : 0.f, bGroundSkirt ? SkirtSpread * SkirtScale : 0.f);
		Surface.SetBellFlare(bFlare ? DomeFlareHeightFraction : 0.f, bFlare ? DomeFlareReach * RimBlend : 0.f,
			DomeFlareCurve, bFlare ? DomeFlareTip * RimBlend : 0.f);
	}
	if (bSpreadSheet && bSpreadDrape)
	{
		const float SheetSpread = FMath::SmoothStep(0.f, 1.f, SpreadBlend);
		const float SheetSpacing = ConfigureSpacing * FMath::Lerp(1.f, SpreadLateralScale, SheetSpread);
		const float RestR = SolverParams.RestRadius * SurfaceScale;
		const float RestVolume = (4.f / 3.f) * PI * RestR * RestR * RestR;
		Surface.SetSheetMode(SheetSpread, RestVolume * SpreadSheetVolumeScale, SheetSpacing * SpreadSheetKernelScale,
			SpreadSheetMinThickness, SheetDrapeDepth, ConfigureSpacing * 0.5f);
	}
	else
	{
		Surface.SetSheetMode(0.f, 0.f, 1.f, 0.f, 0.f, 0.f);
	}
	Surface.Build(Solver.GetParticles(), Solver.GetBodyCenter(), MergingIds, VisualZLift, ClipZ, ShotClips);

	if (Surface.WasTruncated() && !bWarnedTruncation)
	{
		bWarnedTruncation = true;
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeBodyComponent: surface hit the %d vertex budget; raise SurfaceParams.MaxVertices or the cell size."), SurfaceParams.MaxVertices);
	}

	PushMeshSection();

	if (bVolumetricActive)
	{
		UploadBodyField();
	}
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
	if (bVolumetricActive && bFieldParamsValid)
	{
		PushFieldParams(Offset);
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
	// Cluster id in R (0 = body, (slot + 1) / 255 = shot slot). Stored raw: no sRGB conversion on either path.
	const TArray<FLinearColor>& ClusterColors = Surface.GetColors();

	// The sections are sized to the builder's vertex budget; when that changes (shots in flight,
	// body scale) the in-place update would mismatch, so all three sections are recreated.
	if (SectionVertexCount != Vertices.Num())
	{
		bMeshSectionCreated = false;
		bShadowMeshSectionCreated = false;
		bXRayMeshSectionCreated = false;
		bWarnedTruncation = false;
		SectionVertexCount = Vertices.Num();
	}

	if (!bMeshSectionCreated)
	{
		SurfaceMesh->ClearAllMeshSections();
		SurfaceMesh->CreateMeshSection_LinearColor(
			0, Vertices, Indices, Normals,
			NoUVs, ClusterColors, NoTangents, false, false);
		// Keep the element component's MID when the section is merely resized (shot budget); only
		// (re)assign the base material when slot 0 does not already derive from it.
		const UMaterialInstanceDynamic* CurrentMid = Cast<UMaterialInstanceDynamic>(SurfaceMesh->GetMaterial(0));
		if (ResolvedMaterial && !(CurrentMid && CurrentMid->Parent == ResolvedMaterial))
		{
			SurfaceMesh->SetMaterial(0, ResolvedMaterial);
		}
		bMeshSectionCreated = true;
	}
	else
	{
		// Vertex count is constant by design, so this is an in place update: no reallocation and
		// no collision cook, which is what made the reference implementation expensive.
		SurfaceMesh->UpdateMeshSection_LinearColor(0, Vertices, Normals, NoUVs, ClusterColors, NoTangents, false);
	}

	// CreateMeshSection can reset component shadow flags; keep the jelly casting-free.
	SurfaceMesh->SetCastShadow(false);
	SurfaceMesh->bCastDynamicShadow = false;
	SurfaceMesh->bCastVolumetricTranslucentShadow = false;
	SurfaceMesh->bCastContactShadow = false;
	SurfaceMesh->bReceiveMobileCSMShadows = false;

	// Opaque ground-shadow proxy: XY shrink + Z flatten into a bottom puck so VSM from the
	// caster cannot sweep dark bands across the translucent two-sided shell interior.
	// X-ray keeps a full-height 0.92 shell for occlusion silhouette outline.
	constexpr float ShadowProxyScale = 0.92f;
	constexpr float ShadowProxyHeightScale = 0.35f;
 // Unused budget vertices contain degenerate triangles. Never include them in bounds.
 struct FShadowCluster { FVector Sum=FVector::ZeroVector; double Bottom=DBL_MAX; int32 Count=0; };
 TMap<int32,FShadowCluster> Clusters;
 TSet<int32> ValidVertices;
 for (int32 T=0;T+2<Indices.Num();T+=3)
 {
  const int32 A=Indices[T],B=Indices[T+1],C=Indices[T+2];
  if (!Vertices.IsValidIndex(A)||!Vertices.IsValidIndex(B)||!Vertices.IsValidIndex(C)) continue;
  if (FVector::CrossProduct(Vertices[B]-Vertices[A],Vertices[C]-Vertices[A]).IsNearlyZero()) continue;
  ValidVertices.Add(A); ValidVertices.Add(B); ValidVertices.Add(C);
 }
 for (int32 I:ValidVertices)
 {
  const int32 Slot=ClusterColors.IsValidIndex(I) ? FMath::RoundToInt(ClusterColors[I].R*255.f) : 0;
  FShadowCluster& Cluster=Clusters.FindOrAdd(Slot);
  Cluster.Sum+=Vertices[I]; Cluster.Bottom=FMath::Min(Cluster.Bottom,Vertices[I].Z); ++Cluster.Count;
 }
 TArray<FVector> ShadowVertices=Vertices, XRayVertices=Vertices;
 for (int32 I:ValidVertices)
 {
  const int32 Slot=ClusterColors.IsValidIndex(I) ? FMath::RoundToInt(ClusterColors[I].R*255.f) : 0;
  const FShadowCluster& Cluster=Clusters.FindChecked(Slot);
  const FVector Center=Cluster.Sum/Cluster.Count;
  const FVector Scaled=Center+(Vertices[I]-Center)*ShadowProxyScale;
  const float XRayScale = bScreenSpaceXRay ? 1.f : ShadowProxyScale;
  XRayVertices[I]=Center+(Vertices[I]-Center)*XRayScale;
  ShadowVertices[I]=Scaled;
  ShadowVertices[I].Z=Cluster.Bottom+(Scaled.Z-Cluster.Bottom)*ShadowProxyHeightScale;
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
				0, XRayVertices, Indices, Normals,
				NoUVs, NoColors, NoTangents, false);
			if (ResolvedXRayMaterial)
			{
				XRayMesh->SetMaterial(0, ResolvedXRayMaterial);
			}
			bXRayMeshSectionCreated = true;
		}
		else
		{
			XRayMesh->UpdateMeshSection_LinearColor(0, XRayVertices, Normals, NoUVs, NoColors, NoTangents);
		}

		// Occlusion silhouette: Create/Update can reset flags; keep visible for wall reveal.
		XRayMesh->SetHiddenInGame(false);
		XRayMesh->SetVisibility(true);
		XRayMesh->SetCastShadow(false);
		XRayMesh->bCastDynamicShadow = false;
		XRayMesh->bCastVolumetricTranslucentShadow = false;
		XRayMesh->bCastContactShadow = false;
		// Screen-space mode owns the material (depth-test off, so it is not occlusion-culled, and still writes CustomDepth).
		ApplyXRayRenderMode();
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
		SurfaceParams.CellSizeMultiplier = 0.64f;
		SurfaceParams.MaxGridDim = 48;
		SurfaceParams.MaxVertices = 12000;
		SurfaceParams.BlurPasses = 3;
		break;

	case ESlimeSimQuality::Medium:
		StepRate = 40.f;
		SurfaceRate = 40.f;
		SolverParams.DensityIterations = 1;
		SurfaceParams.CellSizeMultiplier = 1.0f;
		SurfaceParams.MaxGridDim = 28;
		SurfaceParams.BlurPasses = 3;
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
		// Pancaking is a deliberate flatten, so drive the capsule down without waiting for
		// a probe to notice a ceiling.
		SetForcedSqueeze(1.f);
	}
	else
	{
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
 if (USlimeUmbrellaComponent* Umbrella=GetOwner()->FindComponentByClass<USlimeUmbrellaComponent>()) Umbrella->ResetUmbrellas();
 if (USlimeAbilityComponent* Ability=GetOwner()->FindComponentByClass<USlimeAbilityComponent>()) Ability->CancelLaunchAim();
	bSpread = false;
	bRecalling = false;
	bClingVisual = false;
	StepHeightBoost = 0.f;
	SpreadBlend = 0.f;
	GroundFieldTimer = 0.f;
	LastGroundFieldCenter = FVector::ZeroVector;
	bAmbientPrimed = false;
	AmbientTimer = 0.f;
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
	Solver.SetShotLifecycleParams(ShotSeparationSeconds, ShotReturnSeconds, ShotReturnHopHeight);
	const int32 Launched = Solver.LaunchChunk(LaunchVelocity, LaunchFraction, FragmentLifetime, MaxActiveShots, nullptr, nullptr, true);
	if (Launched > 0) SetRecalling(false);
	return Launched;
}

bool USlimeBodyComponent::CanLaunchCannon() const
{
	const_cast<FSlimeSolver &>(Solver).RefreshShotStates();
	return !bRecalling && Solver.GetActiveShotCount() < FMath::Max(MaxActiveShots, 1) &&
		   Solver.GetParticles().Num() > 0;
}
bool USlimeBodyComponent::TraceShotWorld(FHitResult& Hit, const FVector& Start, const FVector& End, float Radius, bool bIgnorePawns) const
{
 UWorld* World = GetWorld();
 if (!World) return false;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(SlimeShotWorld), false, GetOwner());
 // Repeat from the original segment after filtering a blocking helper. Multi-trace alone
 // stops at the first blocker and would miss the real wall behind that helper.
 for (int32 Attempt = 0; Attempt < 64; ++Attempt)
 {
  const bool Found = Radius > 0.f
   ? World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Radius), Query)
   : World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Query);
  if (!Found) return false;
  const AActor* Actor = Hit.GetActor();
  const bool OwnedHelper = Actor && !Actor->IsA<APawn>() &&
   (Actor->GetOwner() == GetOwner() || Actor->GetAttachParentActor() == GetOwner());
  if (ShouldIgnoreFluidNinjaCollider(Hit.GetComponent()) || OwnedHelper || (bIgnorePawns && Actor && Actor->IsA<APawn>()))
  {
   if (!Hit.GetComponent()) return true;
   Query.AddIgnoredComponent(Hit.GetComponent());
   continue;
  }
  return true;
 }
 return true; // Conservative on pathological stacks of helpers.
}

FVector USlimeBodyComponent::GetCannonMuzzle(const FSlimeCannonLaunch& Aim) const
{
 FBox BodyBounds(ForceInit);
 const auto& V = Surface.GetVertices();
 const auto& C = Surface.GetColors();
 const auto& I = Surface.GetIndices();
 for (int32 T = 0; T + 2 < I.Num(); T += 3)
 {
  if (!V.IsValidIndex(I[T]) || !V.IsValidIndex(I[T+1]) || !V.IsValidIndex(I[T+2])) continue;
  if (FVector::CrossProduct(V[I[T+1]]-V[I[T]], V[I[T+2]]-V[I[T]]).IsNearlyZero()) continue;
  for (int32 K=0; K<3; ++K)
   if (C.IsValidIndex(I[T+K]) && C[I[T+K]].R < 0.5f/255.f)
    BodyBounds += SurfaceMesh->GetComponentTransform().TransformPosition(V[I[T+K]]);
 }
 const FVector Home = GetBlobCenter();
 if (!BodyBounds.IsValid) BodyBounds = FBox(Home-FVector(Solver.GetScaledRestRadius()),Home+FVector(Solver.GetScaledRestRadius()));
 FVector Direction = (Aim.Target-Home).GetSafeNormal2D();
 if (Direction.IsNearlyZero()) Direction = GetOwner()->GetActorForwardVector().GetSafeNormal2D();
 const FVector Extent=BodyBounds.GetExtent();
 const double Reach=FMath::Abs(Direction.X)*Extent.X+FMath::Abs(Direction.Y)*Extent.Y;
 FVector Muzzle=BodyBounds.GetCenter()+Direction*(Reach+GetCannonRadius()+CannonClearance);
 Muzzle.Z=FMath::Max(Home.Z, BodyBounds.Min.Z+Extent.Z*1.45);
 return Muzzle;
}

bool USlimeBodyComponent::PrepareCannonLaunch(FSlimeCannonLaunch& Aim) const
{
 const FVector Home=GetBlobCenter();
 const FVector Muzzle=GetCannonMuzzle(Aim);
 FVector Start=Home;
 // Begin at the upper body, not at a low COM whose collision sphere overlaps the floor.
 Start.Z=Muzzle.Z;
 Aim.MuzzleOffset=Muzzle-Home;
 Aim.SeparationStartOffset=Start-Home;
 FHitResult Hit;
 return !TraceShotWorld(Hit, Start, Muzzle, GetCannonRadius()) &&
        !TraceShotWorld(Hit, Muzzle, Muzzle+FVector(0,0,0.01), GetCannonRadius());
}

bool USlimeBodyComponent::QueryShotGround(const FVector& Center, FHitResult& Hit) const
{
 const float Height=Solver.GetShotSupportHeight();
 return TraceShotWorld(Hit, Center+FVector(0,0,Height), Center-FVector(0,0,Height+30.f), 0.f, true)
  && !Hit.bStartPenetrating && Hit.ImpactNormal.Z >= 0.65f;
}

int32 USlimeBodyComponent::LaunchCannonShot(const FSlimeCannonLaunch &Cannon)
{
 FSlimeCannonLaunch Prepared=Cannon;
 if (!PrepareCannonLaunch(Prepared)) return 0;
	Solver.SetShotLifecycleParams(ShotSeparationSeconds, ShotReturnSeconds, ShotReturnHopHeight);
	const int32 Count = Solver.LaunchCannon(Prepared, LaunchFraction, FragmentLifetime, MaxActiveShots);
	if (Count)
		SetRecalling(false);
	return Count;
}

int32 USlimeBodyComponent::LaunchChunkAlongPath(const FSlimeLaunchPath& Path)
{
	const FVector Velocity = Path.bValid ? Path.LaunchVelocity : FVector::ZeroVector;
	Solver.SetShotLifecycleParams(ShotSeparationSeconds, ShotReturnSeconds, ShotReturnHopHeight);
	const int32 Launched = Solver.LaunchChunk(Velocity, LaunchFraction, FragmentLifetime, MaxActiveShots, &Path, nullptr, true);
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

void USlimeBodyComponent::ClearShotTarget(uint8 ShotId)
{
	Solver.ClearShotTarget(ShotId);
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
 UpdateShotContactDecals();
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

void USlimeBodyComponent::ApplyCannonImpact(uint8 ShotId, AActor *Target, const FVector &Location)
{
	AActor *Owner = GetOwner();
	if (!Target || !Owner)
		return;
	USlimeElementComponent *ElementComp = Owner->FindComponentByClass<USlimeElementComponent>();
	const ESlimeElement Element = ElementComp ? ElementComp->CurrentElement : ESlimeElement::Physical;
	// Environment receivers still receive cannon element contact.
	if (!USlimeHitProbe::IsValidDamageTarget(Target) || !USlimeHitProbe::IsHostile(Owner, Target))
	{
		SlimeElementDelivery::NotifyActor(Target, Element, Owner, 1.f);
		return;
	}
	float &Cd = FragmentAttackCooldownRemaining.FindOrAdd(ShotId);
	if (Cd > 0.f)
		return;
	USlimeCombatComponent *CombatComp = Owner->FindComponentByClass<USlimeCombatComponent>();
	FSlimeSkillDef HitSkill;
	HitSkill.Slot = ESlimeSkillSlot::Combo1;
	HitSkill.Damage = 12.f;
	if (CombatComp)
	{
		const FSlimeElementKitData Kit = CombatComp->GetCurrentKit();
		if (const FSlimeSkillDef *Def = Kit.GetSkillSlot(ESlimeSkillSlot::Combo1))
			HitSkill = *Def;
	}
	HitSkill.Element = Element;
	HitSkill.bAppliesElementAura = true;
	HitSkill.Damage = FMath::Max(HitSkill.Damage * FragmentAttackDamageScale, 0.f);
	const float Interval = FMath::Max(FragmentAttackInterval, 0.1f);
	float DamageAmount = HitSkill.Damage;
	if (CombatComp && DamageAmount > 0.f)
	{
		DamageAmount = CombatComp->ResolveOutgoingDamage(HitSkill);
	}
	if (DamageAmount <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector HitLoc = Location;
	if (ICombatDamageable *Damageable = Cast<ICombatDamageable>(Target))
	{
		Damageable->ApplyDamage(DamageAmount, Owner, HitLoc, FVector::ZeroVector);
	}
	else if (USlimeHealthComponent *Health = Target->FindComponentByClass<USlimeHealthComponent>())
	{
		Health->ApplyDamage(DamageAmount, Owner, HitLoc, FVector::ZeroVector);
	}

	if (Cast<ASlimeCharacter>(Owner))
	{
		USlimeFloatingTextWidget::Spawn(Target, HitLoc + FVector(0.f, 0.f, 40.f),
										FText::FromString(FString::Printf(TEXT("%.0f"), DamageAmount)),
										SlimeCombat::GetElementVfxColor(Element));
	}

	if (HitSkill.bAppliesElementAura)
	{
		if (USlimeStatusComponent *Status = Target->FindComponentByClass<USlimeStatusComponent>())
		{
			Status->ApplyAura(Element, Owner);
		}
		SlimeElementDelivery::NotifyActor(Target, Element, Owner, 1.f);
	}
	Cd = Interval;
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
		if (TraceShotWorld(Hit, Motion.PrevCenter, Motion.Center, Motion.Radius))
		{
   Solver.SnapKinematicShotTo(Motion.Id,Hit.Location,Hit.ImpactNormal.Z>=0.65f ? &Hit : nullptr);
   if (Motion.bCannon) ApplyCannonImpact(Motion.Id,Hit.GetActor(),Hit.ImpactPoint);
		}
	}
}

void USlimeBodyComponent::SetRecalling(bool bInRecalling)
{
	if (bInRecalling && !Solver.HasFragments())
	{
		return;
	}
	if (bInRecalling)
	{
		ClearShotTargets();
	}
	bRecalling = bInRecalling;
	RecallElapsed = 0.f;
	if (!bRecalling)
	{
		ClearRecallPullSpeedOverride();
	}
}

void USlimeBodyComponent::TriggerDevourBubbleBurst(const FVector& WorldPosition)
{
 const FVector Center = Solver.GetShellCenter();
 const FVector Axes(Solver.GetShellAxes());
 FVector Forward(Solver.GetInertiaForward());
 Forward = Forward.GetSafeNormal();
 const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
 const FVector Rel = WorldPosition-Center;
 FVector Local(FVector::DotProduct(Rel,Forward)/FMath::Max(Axes.X,1.0), FVector::DotProduct(Rel,Right)/FMath::Max(Axes.Y,1.0), Rel.Z/FMath::Max(Axes.Z,1.0));
 Local = Local.GetClampedToMaxSize(0.7);
 FBubbleBurst& Burst = DevourBursts[NextDevourBurst];
 Burst.Local = Local;
 Burst.Started = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
 Burst.Seed = FMath::FRandRange(1.f,1000.f);
 NextDevourBurst = (NextDevourBurst+1)%2;
}

void USlimeBodyComponent::SetDigestBubbleSource(UMeshComponent* Source)
{
 DigestBubbleSource = Source;
}

void USlimeBodyComponent::UpdateBubbleVisuals(float DeltaTime)
{
 APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this,0);
 if (!Camera || !SurfaceMesh) return;
 if (!BubbleVisualMesh)
 {
  UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Characters/Slime/Materials/M_SlimeBubbleVisual.M_SlimeBubbleVisual"));
  if (!Material) return;
  BubbleVisualMesh = NewObject<UProceduralMeshComponent>(GetOwner(),TEXT("SlimeBubbleVisual"),RF_Transient);
  BubbleVisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  BubbleVisualMesh->SetCastShadow(false);
  BubbleVisualMesh->SetCanEverAffectNavigation(false);
  BubbleVisualMesh->TranslucencySortPriority=SurfaceMesh->TranslucencySortPriority+10;
  BubbleVisualMesh->RegisterComponent();
  BubbleVisualMesh->SetMaterial(0,UMaterialInstanceDynamic::Create(Material,this));
 }
 BubbleVisualMesh->SetVisibility(SurfaceMesh->IsVisible());
 BubbleVisualMesh->SetHiddenInGame(SurfaceMesh->bHiddenInGame);
 UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(BubbleVisualMesh->GetMaterial(0));
 FLinearColor Tint(0.65f,0.85f,1.f);
 if (UMaterialInstanceDynamic* BubbleBodyMID = Cast<UMaterialInstanceDynamic>(SurfaceMesh->GetMaterial(0)))
  BubbleBodyMID->GetVectorParameterValue(FMaterialParameterInfo(TEXT("BaseColor")),Tint);
 Material->SetVectorParameterValue(TEXT("BubbleTint"),FMath::Lerp(Tint,FLinearColor::White,0.55f));
 Material->SetScalarParameterValue(TEXT("Brightness"),BubbleEdgeStrength);
 Material->SetScalarParameterValue(TEXT("RimWidth"),BubbleRimWidth);
 Material->SetScalarParameterValue(TEXT("CoreStrength"),BubbleCoreStrength);
 const bool bFizz = bFizzBubbles && bLuminousSkinActive;
 Material->SetScalarParameterValue(TEXT("FizzStyle"),bFizz ? 1.f : 0.f);
 const FVector Center = GetShellCenter();
 const FVector Axes(Solver.GetShellAxes());
 const FVector FizzFwd = FVector(Solver.GetInertiaForward()).GetSafeNormal2D(KINDA_SMALL_NUMBER,FVector::ForwardVector);
 const FVector FizzSide = FVector::CrossProduct(FVector::UpVector,FizzFwd).GetSafeNormal();
 const FVector Right = Camera->GetCameraRotation().RotateVector(FVector::RightVector);
 const FVector Up = Camera->GetCameraRotation().RotateVector(FVector::UpVector);
 TArray<FVector> V, N;
 TArray<int32> Indices;
 TArray<FVector2D> UV;
 TArray<FLinearColor> Colors;
 TArray<FProcMeshTangent> Tangents;
 auto Quad = [&](const FVector& P,float Radius,float Pop,float Fade,float Seed)
 {
  const int32 Start = V.Num();
  const float Size = Radius*(1.f+1.1f*Pop)*1.3f;
  for (int32 Corner=0; Corner<4; ++Corner)
  {
   const float X = (Corner==1 || Corner==2) ? 1.f : -1.f;
   const float Y = Corner>=2 ? 1.f : -1.f;
   V.Add(P+Right*Size*X+Up*Size*Y);
   UV.Add(FVector2D((X+1.f)*0.5f,(Y+1.f)*0.5f));
   Colors.Add(FLinearColor(Pop,Fade,Seed,1));
   N.Add(-Camera->GetCameraRotation().Vector());
  }
  Indices.Append({Start,Start+1,Start+2,Start,Start+2,Start+3});
 };
 const auto& Vertices = Surface.GetVertices();
 const auto& ClusterColors = Surface.GetColors();
 // Reuse the actual surface to accept interior spawn points and find each upper exit.
 // Missing/narrow columns are rejected; they must never become ground-level pop events.
 auto Column = [&](const FVector& P,float Radius,float& Bottom,float& Top)
 {
  float LowerHit=BIG_NUMBER, UpperHit=-BIG_NUMBER, UpperNormalZ=1.f;
  const FVector Offset=SurfaceMesh->GetComponentLocation();
  for (int32 I=0; I+2<Surface.GetLiveVertexCount(); I+=3)
  {
   if (ClusterColors.IsValidIndex(I) && ClusterColors[I].R>0.001f) continue;
   const FVector A=Vertices[I]+Offset, B=Vertices[I+1]+Offset, C=Vertices[I+2]+Offset;
   const double Den=(B.Y-C.Y)*(A.X-C.X)+(C.X-B.X)*(A.Y-C.Y);
   if (FMath::Abs(Den)<1.e-8) continue;
   const double U=((B.Y-C.Y)*(P.X-C.X)+(C.X-B.X)*(P.Y-C.Y))/Den;
   const double W=((C.Y-A.Y)*(P.X-C.X)+(A.X-C.X)*(P.Y-C.Y))/Den;
   if (U<0 || W<0 || U+W>1) continue;
   const float Z=U*A.Z+W*B.Z+(1-U-W)*C.Z;
   LowerHit=FMath::Min(LowerHit,Z);
   if (Z>UpperHit)
   {
    UpperHit=Z;
    UpperNormalZ=FMath::Abs(FVector::CrossProduct(B-A,C-A).GetSafeNormal().Z);
   }
  }
  if (UpperHit-LowerHit<Radius*6.f) return false;
  Bottom=LowerHit+Radius*2.f;
  Top=UpperHit-Radius/FMath::Max(UpperNormalZ,0.35f);
  return Top-Bottom>Radius*3.f;
 };
 const int32 Count=bFizz ? FMath::Clamp(FizzBubbleCount,0,96) : FMath::Clamp(FineBubbleCount,0,32);
 const int32 OldCount=VisibleBubbles.Num();
 VisibleBubbles.SetNum(Count);
 auto Respawn = [&](FVisibleBubble& B,bool Stagger)
 {
  B.bValid=false;
  B.Radius=FMath::FRandRange(FineBubbleMinRadius,FMath::Max(FineBubbleMinRadius,FineBubbleMaxRadius))*FMath::FRandRange(BubbleSizeScaleMin,FMath::Max(BubbleSizeScaleMin,BubbleSizeScaleMax))*FMath::Max(BubbleVisualRadiusScale,0.1f);
  B.Speed=FMath::FRandRange(0.13f,0.24f); B.Seed=FMath::FRand(); B.PopAge=-1;
  // Fizz: two rising columns on one flank (shell-normalised fwd / side), smaller and faster.
  const bool bMainColumn=FMath::FRand()<0.65f;
  const FVector2D ColumnXY=bMainColumn ? FVector2D(-0.12f,0.34f) : FVector2D(0.16f,0.24f);
  if (bFizz)
  {
   B.Radius*=FMath::Max(FizzRadiusScale,0.05f);
   B.Speed=FMath::FRandRange(0.28f,0.5f);
  }
  for (int32 Attempt=0; Attempt<8; ++Attempt)
  {
   const float Angle=FMath::FRand()*2.f*PI;
   const float Rad=Attempt==7 ? 0.f : FMath::Sqrt(FMath::FRand())*0.48f;
   B.Local=FVector(FMath::Cos(Angle)*Rad,FMath::Sin(Angle)*Rad,0.f);
   if (bFizz)
   {
    const float Pull=1.f-float(Attempt)/7.f;
    const float Jitter=FMath::Sqrt(FMath::FRand())*FMath::Max(FizzColumnSpread,0.f);
    const FVector2D XY=ColumnXY*Pull+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*Jitter;
    const FVector Flank=FizzFwd*XY.X+FizzSide*XY.Y;
    B.Local=FVector(Flank.X,Flank.Y,0.f);
   }
   float Bottom,Top;
   if (!Column(Center+B.Local*Axes,B.Radius,Bottom,Top)) continue;
   const float Z=FMath::Lerp(Bottom,Top,Stagger ? FMath::FRandRange(0.15f,0.8f) : 0.15f);
   B.Local.Z=(Z-Center.Z)/FMath::Max(Axes.Z,1.0);
   B.bValid=true;
   break;
  }
 };
 for (int32 I=0; I<Count; ++I)
 {
  FVisibleBubble& B=VisibleBubbles[I];
  if (I>=OldCount || !B.bValid) Respawn(B,true);
  if (!B.bValid) continue;
  if (B.PopAge>=0)
  {
   B.PopAge+=DeltaTime;
   if (B.PopAge>=FMath::Max(BubblePopSeconds,0.01f)) Respawn(B,false);
  }
  else
  {
   B.Local.Z+=B.Speed*DeltaTime;
   const FVector P=Center+B.Local*Axes;
   float Bottom,Top;
   if (!Column(P,B.Radius,Bottom,Top) || P.Z<Bottom)
    Respawn(B,false);
   else if (P.Z>=Top)
   {
    B.Local.Z=(Top-Center.Z)/FMath::Max(Axes.Z,1.0);
    B.PopAge=0;
   }
  }
  if (!B.bValid) continue;
  const float Pop=B.PopAge>=0 ? FMath::Clamp(B.PopAge/FMath::Max(BubblePopSeconds,0.01f),0.f,1.f) : 0.f;
  const float Growth=FMath::Clamp(0.92f+B.Local.Z*0.12f,0.82f,1.02f);
  Quad(Center+B.Local*Axes,B.Radius*Growth,Pop,B.PopAge>=0 ? FMath::Pow(1-Pop,1.5f) : 0.8f,B.Seed);
 }
 const float Time=GetWorld()->GetTimeSeconds();
 UMeshComponent* Source=DigestBubbleSource.Get();
 const bool Digest=IsValid(Source) && Source->IsVisible();
 const FVector Fwd=FVector(Solver.GetInertiaForward()).GetSafeNormal();
 const FVector Side=FVector::CrossProduct(FVector::UpVector,Fwd).GetSafeNormal();
 for (int32 I=0; I<FMath::Clamp(DevourBubbleCount,0,64); ++I)
 {
  FRandomStream Random(I*7919+713);
  const float Seed=Random.FRand();
  const FVector Direction=Random.VRand();
  const float Radius=Random.FRandRange(FineBubbleMinRadius,FMath::Max(FineBubbleMinRadius,FineBubbleMaxRadius))*Random.FRandRange(BubbleSizeScaleMin,FMath::Max(BubbleSizeScaleMin,BubbleSizeScaleMax))*FMath::Max(BubbleVisualRadiusScale,0.1f)*FMath::Max(DevourBubbleRadiusScale,0.1f);
  const float RiseSeconds=FMath::Max(DevourBubbleSeconds,0.1f)*Random.FRandRange(0.65f,1.0f);
  const float OverflowSeconds=Random.FRandRange(0.20f,0.35f);
  const float PopSeconds=FMath::Max(BubblePopSeconds,0.01f);
  const float CycleSeconds=RiseSeconds+OverflowSeconds+PopSeconds;
  float Age=FMath::Fmod(Time+Seed*CycleSeconds,CycleSeconds);
  FVector Origin;
  if (Digest)
   Origin=Source->Bounds.Origin+Direction*(Source->Bounds.BoxExtent/FMath::Max(Source->BoundsScale,0.01f))*0.75;
  else
  {
   const FBubbleBurst& Burst=DevourBursts[I%2];
   Age=Time-Burst.Started;
   if (Age<0 || Age>=CycleSeconds) continue;
   Origin=Center+Fwd*Burst.Local.X*Axes.X+Side*Burst.Local.Y*Axes.Y+FVector::UpVector*Burst.Local.Z*Axes.Z;
   Origin+=Direction*Radius*2.f;
  }
  // Re-centre emission points that fall outside a deformed body. Never emit from the floor.
  float Bottom=0,Top=0;
  bool bInterior=false;
  for (int32 Attempt=0; Attempt<6; ++Attempt)
  {
   if (Column(Origin,Radius,Bottom,Top)) { bInterior=true; break; }
   Origin.X=FMath::Lerp(Origin.X,Center.X,0.45);
   Origin.Y=FMath::Lerp(Origin.Y,Center.Y,0.45);
  }
  if (!bInterior) continue;
  const float Height=Top-Bottom;
  Origin.Z=FMath::Clamp(Origin.Z,Bottom+Height*0.18f,Top-Height*0.20f);
  const float Rise=FMath::Clamp(Age/RiseSeconds,0.f,1.f);
  FVector P=Origin;
  // Gentle horizontal drift forms a plume instead of an outward explosion on every side.
  P.X+=FMath::Sin(Rise*PI+Seed*2*PI)*Radius*1.5f*Rise;
  P.Y+=FMath::Cos(Rise*PI+Seed*2*PI)*Radius*1.5f*Rise;
  float DriftBottom,DriftTop;
  if (!Column(P,Radius,DriftBottom,DriftTop)) continue;
  Top=DriftTop;
  P.Z=FMath::Lerp(FMath::Min(Origin.Z,Top-Radius*2.f),Top,Rise);
  float Pop=0;
  const float FadeIn=FMath::Clamp(Age/0.12f,0.f,1.f);
  float Fade=FadeIn*0.75f;
  if (Age>=RiseSeconds)
  {
   const float OutsideAge=Age-RiseSeconds;
   const float Travel=FMath::Clamp(OutsideAge/OverflowSeconds,0.f,1.f);
   P.Z=Top+Travel*Height*FMath::Clamp(DevourBubbleOverflowHeight,0.02f,0.4f);
   if (OutsideAge>=OverflowSeconds)
   {
    Pop=FMath::Clamp((OutsideAge-OverflowSeconds)/PopSeconds,0.f,1.f);
    Fade*=FMath::Pow(1-Pop,1.5f);
   }
  }
  Quad(P,Radius*FMath::Lerp(0.85f,1.05f,Rise),Pop,Fade,Seed);
 }
 BubbleVisualMesh->CreateMeshSection_LinearColor(0,V,Indices,N,UV,Colors,Tangents,false,false);
}

FVector2D USlimeBodyComponent::FootprintHalfAxesFromMoments(const FSlimeFloorFootprint& Footprint, const FVector2D& MajorDir)
{
	FVector2D Major = MajorDir.GetSafeNormal();
	if (Major.IsNearlyZero())
	{
		Major = FVector2D(1.f, 0.f);
	}
	const FVector2D Minor(-Major.Y, Major.X);
	auto Variance = [&Footprint](const FVector2D& Axis) -> double
	{
		return FMath::Max(
			Axis.X * Axis.X * Footprint.Cxx + Axis.Y * Axis.Y * Footprint.Cyy + 2.0 * Axis.X * Axis.Y * Footprint.Cxy,
			0.0);
	};
	const float Pad = 0.5f * FMath::Max(Footprint.CellSize, 0.f);
	return FVector2D(
		float(2.0 * FMath::Sqrt(Variance(Major))) + Pad,
		float(2.0 * FMath::Sqrt(Variance(Minor))) + Pad);
}

float USlimeBodyComponent::GetDomeFootprintWeight() const
{
	if (!bVisualContactValid)
	{
		return 0.f;
	}
	const float Dome = FMath::SmoothStep(0.f, 1.f, ShapeBlend);
	const float Spread = FMath::SmoothStep(0.f, 1.f, SpreadBlend);
	return Dome * (1.f - Spread);
}

void USlimeBodyComponent::UpdateVisualContactFootprint()
{
	const FSlimeFloorFootprint& Footprint = Surface.GetBodyFloorFootprint();
	const bool bUsable = !bClingVisual && FloorZ > -1.e8f && Footprint.IsValid();
	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	const float Alpha = ContactSmoothing > 0.f ? 1.f - FMath::Exp(-Dt / ContactSmoothing) : 1.f;
	if (!bUsable)
	{
		bVisualContactValid = false;
		return;
	}

	FVector Major = ContactMajorDir;
	Major.Z = 0.f;
	if (!Major.Normalize())
	{
		Major = FVector::ForwardVector;
	}
	const FVector2D Target = FootprintHalfAxesFromMoments(Footprint, FVector2D(Major.X, Major.Y));
	if (!bVisualContactValid)
	{
		VisualContactHalfAxes = Target;
		bVisualContactValid = true;
	}
	else
	{
		VisualContactHalfAxes = FMath::Lerp(VisualContactHalfAxes, Target, Alpha);
	}
}

void USlimeBodyComponent::EnsureXRayOutlineMaterial()
{
	if (!bScreenSpaceXRay || XRayOutlineMID)
	{
		return;
	}
	UMaterialInterface* Parent = XRayOutlineMaterialPath.LoadSynchronous();
	if (!Parent)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeBodyComponent: X-ray outline material missing at %s"), *XRayOutlineMaterialPath.ToString());
		return;
	}
	XRayOutlineMID = UMaterialInstanceDynamic::Create(Parent, this);
	XRayOutlineMID->SetScalarParameterValue(TEXT("OcclusionBias"), XRayOcclusionBias);
	XRayOutlineMID->SetScalarParameterValue(TEXT("OcclusionRamp"), XRayOcclusionRamp);
}

void USlimeBodyComponent::SetXRayDepthSuppressed(bool bSuppressed)
{
	if (bXRayDepthSuppressed == bSuppressed)
	{
		return;
	}
	bXRayDepthSuppressed = bSuppressed;
	ApplyXRayRenderMode();
}

void USlimeBodyComponent::ApplyXRayRenderMode()
{
	if (!XRayMesh || XRayMesh->GetNumSections() == 0)
	{
		return;
	}
	if (bScreenSpaceXRay)
	{
		// Depth test off makes CanBeOccluded() false, so a fully hidden slime still writes CustomDepth.
		// Translucent custom-depth writes need AllowTranslucentCustomDepthWrites; the main pass stays off.
		if (!ResolvedXRayDepthProxy && !XRayDepthProxyMaterialPath.IsNull())
		{
			ResolvedXRayDepthProxy = XRayDepthProxyMaterialPath.LoadSynchronous();
		}
		UMaterialInterface* Proxy = ResolvedXRayDepthProxy;
		if (!Proxy)
		{
			if (!bLoggedMissingXRayDepthProxy)
			{
				bLoggedMissingXRayDepthProxy = true;
				UE_LOG(LogSlimeFable, Warning, TEXT("SlimeBodyComponent: X-ray depth proxy missing at %s; falling back to the default opaque material, so a fully hidden slime will be occlusion-culled."),
					*XRayDepthProxyMaterialPath.ToString());
			}
			Proxy = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		if (Proxy && XRayMesh->GetMaterial(0) != Proxy)
		{
			XRayMesh->SetMaterial(0, Proxy);
		}
		XRayMesh->SetRenderInMainPass(false);
		XRayMesh->SetRenderInDepthPass(false);
		XRayMesh->SetRenderCustomDepth(bXRayDepthSuppressed ? false : true);
		XRayMesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
		XRayMesh->SetCustomDepthStencilValue(1);
		return;
	}

	XRayMesh->SetRenderInMainPass(true);
	XRayMesh->SetRenderInDepthPass(true);
	XRayMesh->SetRenderCustomDepth(false);
	if (ResolvedXRayMaterial && XRayMesh->GetMaterial(0) != ResolvedXRayMaterial)
	{
		const UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(XRayMesh->GetMaterial(0));
		if (!(Mid && Mid->Parent == ResolvedXRayMaterial))
		{
			XRayMesh->SetMaterial(0, ResolvedXRayMaterial);
		}
	}
}

void USlimeBodyComponent::UpdateContactFootprint()
{
	// Contact plane: the wall while clinging, otherwise the floor under the body.
	FVector PlanePoint = FVector::ZeroVector;
	FVector PlaneNormal = FVector::UpVector;
	bool bHavePlane = false;
	if (bClingVisual)
	{
		PlaneNormal = ClingNormal.GetSafeNormal();
		PlanePoint = ClingPoint;
		bHavePlane = !PlaneNormal.IsNearlyZero();
	}
	else if (FloorZ > -1.e8f)
	{
		PlanePoint = FVector(0.0, 0.0, FloorZ);
		bHavePlane = true;
	}
	const bool bBodyVisible = !SurfaceMesh || (SurfaceMesh->IsVisible() && !SurfaceMesh->bHiddenInGame);

	// Fit an ellipse to the body particles lying within one contact band of the plane.
	// Runs while spread too, so the trail puddle can cover the whole sheet.
	int32 ContactCount = 0;
	FVector2D Mean = FVector2D::ZeroVector;
	FVector2D Axes = FVector2D::ZeroVector;
	FVector MajorDir = FVector::ForwardVector;
	FVector TangentU = FVector::ForwardVector;
	FVector TangentV = FVector::RightVector;
	if (bHavePlane && bBodyVisible)
	{
		TangentU = FMath::Abs(PlaneNormal.Z) > 0.9
			? FVector::ForwardVector
			: FVector::CrossProduct(FVector::UpVector, PlaneNormal).GetSafeNormal();
		TangentU = (TangentU - PlaneNormal * FVector::DotProduct(TangentU, PlaneNormal)).GetSafeNormal();
		TangentV = FVector::CrossProduct(PlaneNormal, TangentU);
		const float SizeScale = FMath::Max(Solver.GetSizeScale(), 0.05f);
		const double Band = double(SolverParams.ParticleSpacing * SizeScale * 1.7f);
		double SumU = 0.0, SumV = 0.0, SumUU = 0.0, SumVV = 0.0, SumUV = 0.0;
		for (const SlimeSim::FSlimeParticle& Particle : Solver.GetParticles())
		{
			if (Particle.IsBallistic() || Particle.ShotId != 0)
			{
				continue;
			}
			const FVector Rel = FVector(Particle.Position) - PlanePoint;
			const double Height = FVector::DotProduct(Rel, PlaneNormal);
			if (Height > Band || Height < -Band)
			{
				continue;
			}
			const double U = FVector::DotProduct(Rel, TangentU);
			const double V = FVector::DotProduct(Rel, TangentV);
			SumU += U; SumV += V; SumUU += U * U; SumVV += V * V; SumUV += U * V;
			++ContactCount;
		}
		if (ContactCount >= 6)
		{
			const double InvN = 1.0 / double(ContactCount);
			Mean = FVector2D(SumU * InvN, SumV * InvN);
			const double Cuu = FMath::Max(SumUU * InvN - Mean.X * Mean.X, 0.0);
			const double Cvv = FMath::Max(SumVV * InvN - Mean.Y * Mean.Y, 0.0);
			const double Cuv = SumUV * InvN - Mean.X * Mean.Y;
			const double Half = 0.5 * (Cuu + Cvv);
			const double Disc = FMath::Sqrt(FMath::Square(0.5 * (Cuu - Cvv)) + Cuv * Cuv);
			const double Angle = 0.5 * FMath::Atan2(2.0 * Cuv, Cuu - Cvv);
			// A uniformly filled disc of radius R has variance R^2/4 along any axis.
			Axes.X = 2.0 * FMath::Sqrt(FMath::Max(Half + Disc, 0.0)) + ContactFootprintPadding;
			Axes.Y = 2.0 * FMath::Sqrt(FMath::Max(Half - Disc, 0.0)) + ContactFootprintPadding;
			MajorDir = TangentU * FMath::Cos(Angle) + TangentV * FMath::Sin(Angle);
		}
	}

	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	const float Alpha = ContactSmoothing > 0.f ? 1.f - FMath::Exp(-Dt / ContactSmoothing) : 1.f;
	const float TargetFade = ContactCount >= 6 ? FMath::Clamp(float(ContactCount - 4) / 10.f, 0.f, 1.f) : 0.f;
	ContactFadeSmoothed = FMath::Lerp(ContactFadeSmoothed, TargetFade, Alpha);
	if (ContactCount >= 6)
	{
		const FVector Center = PlanePoint + TangentU * Mean.X + TangentV * Mean.Y;
		const FVector CenterOnPlane = bClingVisual
			? Center
			: FVector(Center.X, Center.Y, FloorZ);
		if (!bContactValid || FVector::DotProduct(ContactNormal, PlaneNormal) < 0.9)
		{
			ContactCenter = CenterOnPlane;
			ContactNormal = PlaneNormal;
			ContactMajorDir = MajorDir;
			ContactHalfAxes = Axes;
			bContactValid = true;
		}
		else
		{
			if (FVector::DotProduct(MajorDir, ContactMajorDir) < 0.0)
			{
				MajorDir = -MajorDir;
			}
			ContactCenter = FMath::Lerp(ContactCenter, CenterOnPlane, double(Alpha));
			ContactNormal = PlaneNormal;
			ContactMajorDir = FMath::Lerp(ContactMajorDir, MajorDir, double(Alpha)).GetSafeNormal();
			ContactHalfAxes = FMath::Lerp(ContactHalfAxes, Axes, double(Alpha));
		}
	}

	if (ContactFadeSmoothed <= 0.01f)
	{
		bContactValid = false;
	}
}

void USlimeBodyComponent::UpdateContactDecal()
{
	const bool bSpreading = bSpread || SpreadBlend > 0.f;
	const bool bShow = bContactDecal && bContactValid && !bSpreading && SurfaceMesh && GetOwner();
	if (!bShow)
	{
		if (ContactDecal)
		{
			ContactDecal->SetVisibility(false);
		}
		return;
	}
	if (!ContactDecal)
	{
		UMaterialInterface* Material = ContactDecalMaterialPath.LoadSynchronous();
		if (!Material)
		{
			return;
		}
		ContactDecal = NewObject<UDecalComponent>(GetOwner(), TEXT("SlimeContactDecal"), RF_Transient);
		ContactDecal->SetUsingAbsoluteLocation(true);
		ContactDecal->SetUsingAbsoluteRotation(true);
		ContactDecal->SetUsingAbsoluteScale(true);
		ContactDecal->SortOrder = -1;
		ContactDecal->RegisterComponent();
		ContactDecal->SetDecalMaterial(UMaterialInstanceDynamic::Create(Material, this));
	}
	ContactDecal->SetVisibility(true);

	const float Extent = FMath::Max(ContactShadowExtent, 1.f);
	// Decal X projects into the contact plane; Y follows the ellipse major axis.
	ContactDecal->SetWorldLocationAndRotation(ContactCenter,
		FRotationMatrix::MakeFromXY(-ContactNormal, ContactMajorDir).Rotator());
	const FVector NewSize(FMath::Max(ContactDepth, 1.f),
		FMath::Max(ContactHalfAxes.X, 1.0) * Extent,
		FMath::Max(ContactHalfAxes.Y, 1.0) * Extent);
	if (!ContactDecal->DecalSize.Equals(NewSize, 0.5))
	{
		ContactDecal->DecalSize = NewSize;
		ContactDecal->MarkRenderStateDirty();
	}

	if (UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(ContactDecal->GetDecalMaterial()))
	{
		FLinearColor Tint(0.3f, 0.8f, 0.4f, 1.f);
		if (UMaterialInstanceDynamic* BodyMid = Cast<UMaterialInstanceDynamic>(SurfaceMesh->GetMaterial(0)))
		{
			BodyMid->GetVectorParameterValue(FMaterialParameterInfo(TEXT("BaseColor")), Tint);
		}
		Mid->SetVectorParameterValue(TEXT("ContactColor"), Tint);
		Mid->SetScalarParameterValue(TEXT("ContactFade"), ContactFadeSmoothed);
		Mid->SetScalarParameterValue(TEXT("AmbientScale"), AmbientScale);
		// Fitted contact edge relative to the decal half extent: the dark core ends exactly there.
		Mid->SetScalarParameterValue(TEXT("FootprintRatio"), 1.f / Extent);
		Mid->SetScalarParameterValue(TEXT("CausticStrength"), ContactCausticStrength);
		Mid->SetVectorParameterValue(TEXT("PlanePoint"), FLinearColor(ContactCenter.X, ContactCenter.Y, ContactCenter.Z, 0.f));
		Mid->SetVectorParameterValue(TEXT("PlaneNormal"), FLinearColor(ContactNormal.X, ContactNormal.Y, ContactNormal.Z, 0.f));
	}
}

void USlimeBodyComponent::UpdateShotContactDecals()
{
 TSet<uint8> Visible;
 if (bShotContactDecals && bContactDecal && !bShadowCastSuppressed)
 for (const FSlimeSolver::FShotState& Shot:Solver.GetShotStates())
 {
  if ((bRecalling && !Shot.bKeepUntilMerged) || !Shot.bHasSupport || Shot.Phase==FSlimeSolver::EShotPhase::Separating || Shot.Phase==FSlimeSolver::EShotPhase::Merging) continue;
  const float Height=FMath::Max(0.f,Shot.Center.Z-Solver.GetShotSupportHeight()-float(Shot.SupportPoint.Z));
  const float Fade=1.f-FMath::Clamp(Height/FMath::Max(ContactFadeHeight,1.f),0.f,1.f);
  if (Fade<=0.f) continue;
  Visible.Add(Shot.Id);
  TObjectPtr<UDecalComponent>& Decal=ShotContactDecals.FindOrAdd(Shot.Id);
  if (!Decal)
  {
   UMaterialInterface* Material=ContactDecalMaterialPath.LoadSynchronous();
   if (!Material) continue;
   Decal=NewObject<UDecalComponent>(GetOwner(),NAME_None,RF_Transient);
   Decal->SetDecalMaterial(UMaterialInstanceDynamic::Create(Material,this));
   Decal->SortOrder=-1;
   Decal->RegisterComponent();
  }
  const float R=Solver.GetMiniMembraneRadius()*FMath::Max(ContactShadowExtent,1.f);
  Decal->DecalSize=FVector(FMath::Max(ContactDepth,1.f),R,R);
  Decal->SetWorldLocationAndRotation(Shot.SupportPoint,FRotationMatrix::MakeFromX(-Shot.SupportNormal).Rotator());
  Decal->MarkRenderStateDirty();
  if (UMaterialInstanceDynamic* Mid=Cast<UMaterialInstanceDynamic>(Decal->GetDecalMaterial()))
  {
   FLinearColor Tint(0.3f,0.8f,0.4f,1.f);
   if (SurfaceMesh) if (UMaterialInstanceDynamic* BodyMid=Cast<UMaterialInstanceDynamic>(SurfaceMesh->GetMaterial(0)))
    BodyMid->GetVectorParameterValue(FMaterialParameterInfo(TEXT("BaseColor")),Tint);
   Mid->SetVectorParameterValue(TEXT("ContactColor"),Tint);
   Mid->SetScalarParameterValue(TEXT("ContactFade"),Fade);
   Mid->SetScalarParameterValue(TEXT("AmbientScale"),AmbientScale);
   Mid->SetScalarParameterValue(TEXT("FootprintRatio"),1.f/FMath::Max(ContactShadowExtent,1.f));
   Mid->SetScalarParameterValue(TEXT("CausticStrength"),ContactCausticStrength);
   Mid->SetVectorParameterValue(TEXT("PlanePoint"),FLinearColor(Shot.SupportPoint.X,Shot.SupportPoint.Y,Shot.SupportPoint.Z,0));
   Mid->SetVectorParameterValue(TEXT("PlaneNormal"),FLinearColor(Shot.SupportNormal.X,Shot.SupportNormal.Y,Shot.SupportNormal.Z,0));
  }
 }
 for (auto It=ShotContactDecals.CreateIterator();It;++It)
  if (!Visible.Contains(It.Key())) { if (It.Value()) It.Value()->DestroyComponent(); It.RemoveCurrent(); }
}

void USlimeBodyComponent::ConfigureShotUmbrellas(float Height,float Follow,float Fall,float MaxFall)
{
 Solver.ConfigureUmbrellas(Height,Follow,Fall,MaxFall);
 Solver.QueryUmbrellaGround=[this](const FVector& Position,FHitResult& Hit)
 {
  return TraceShotWorld(Hit,Position+FVector(0,0,5),Position-FVector(0,0,20000),0.f,true)
   && !Hit.bStartPenetrating && Hit.ImpactNormal.Z>=0.65f;
 };
}
