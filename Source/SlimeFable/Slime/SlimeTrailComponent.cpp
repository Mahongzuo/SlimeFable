// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeTrailComponent.h"

#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ProceduralMeshComponent.h"
#include "SlimeBodyComponent.h"
#include "SlimeCharacter.h"
#include "SlimeClingComponent.h"
#include "SlimeElementComponent.h"
#include "SlimeFable.h"

namespace SlimeTrailDefaults
{
	static const FSoftObjectPath WaterDecal(TEXT("/Game/Characters/Slime/Materials/MI_SlimeTrail_Water.MI_SlimeTrail_Water"));
	static const FSoftObjectPath DarkDecal(TEXT("/Game/Characters/Slime/Materials/MI_SlimeTrail_Dark.MI_SlimeTrail_Dark"));
	static const FSoftObjectPath WindDecal(TEXT("/Game/Characters/Slime/Materials/MI_SlimeTrail_Wind.MI_SlimeTrail_Wind"));
	static const FSoftObjectPath FireNiagara(TEXT("/Game/NiagaraExamples/FX_Footstep/NS_Footstep_Fire.NS_Footstep_Fire"));
	static const FSoftObjectPath FireOverlay(TEXT("/Game/Characters/Slime/Materials/MI_Slime_FireOverlay.MI_Slime_FireOverlay"));
	static const FSoftObjectPath PhysicalNiagara(TEXT("/Game/NiagaraExamples/FX_Footstep/NS_Footstep_Gravel.NS_Footstep_Gravel"));
	static const FSoftObjectPath TeslaArc(TEXT("/Game/Characters/Slime/FX/NS_SlimeTeslaArc.NS_SlimeTeslaArc"));
	static const FSoftObjectPath ElectricityWrap(TEXT("/Game/NiagaraExamples/FX_Player/NS_Player_Electricity_Looping.NS_Player_Electricity_Looping"));
	static const FSoftObjectPath Overlay(TEXT("/Game/NiagaraExamples/Materials/MI_Mesh_Overlay_TeslaCoil_Player.MI_Mesh_Overlay_TeslaCoil_Player"));
	static const FSoftObjectPath SampleMesh(TEXT("/Game/NiagaraExamples/Gallery/SkeletalMesh/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));

	static const FName FadeParam(TEXT("Fade"));
	static const FName PositionTarget(TEXT("PositionTarget"));
	static const FName SkeletalMeshUser(TEXT("SkeletalMesh"));
	static const FName TeslaSmokeEmitter(TEXT("Smoke"));
	static const FName TeslaSmokeColor(TEXT("Smoke Color"));
}

USlimeTrailComponent::USlimeTrailComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;
	LightningSampleMesh = TSoftObjectPtr<USkeletalMesh>(SlimeTrailDefaults::SampleMesh);
	EnsureProfileDefaults();
}

void USlimeTrailComponent::EnsureProfileDefaults()
{
	const bool bNeedsRebuild =
		TrailDefaultsVersion != CurrentTrailDefaultsVersion || Profiles.Num() < SlimeElement::Count;

	if (!bNeedsRebuild)
	{
		return;
	}

	Profiles.Reset();
	Profiles.Reserve(SlimeElement::Count);
	for (int32 Index = 0; Index < SlimeElement::Count; ++Index)
	{
		Profiles.Add(MakeDefaultProfile(SlimeElement::FromIndex(Index)));
	}
	TrailDefaultsVersion = CurrentTrailDefaultsVersion;
}

FSlimeTrailProfile USlimeTrailComponent::MakeDefaultProfile(ESlimeElement Element) const
{
	FSlimeTrailProfile Profile;
	Profile.Element = Element;
	Profile.AttachedScale = FVector(1.f);
	Profile.AttachedOffset = FVector::ZeroVector;
	Profile.StampSizeJitter = 0.2f;
	Profile.MinSpeed = 40.f;
	Profile.ArcRadius = 180.f;
	Profile.bGroundNiagaraIsArc = false;

	switch (Element)
	{
	case ESlimeElement::Water:
		Profile.StampKind = ESlimeTrailStampKind::Decal;
		Profile.DecalMaterial = TSoftObjectPtr<UMaterialInterface>(SlimeTrailDefaults::WaterDecal);
		Profile.SpawnDistance = 16.f;
		Profile.StampSize = 48.f;
		Profile.StampLifetime = 3.f;
		Profile.MaxStamps = 16;
		Profile.StampSizeJitter = 0.12f;
		break;

	case ESlimeElement::Wind:
		Profile.StampKind = ESlimeTrailStampKind::Decal;
		Profile.DecalMaterial = TSoftObjectPtr<UMaterialInterface>(SlimeTrailDefaults::WindDecal);
		Profile.SpawnDistance = 16.f;
		Profile.StampSize = 44.f;
		Profile.StampLifetime = 1.6f;
		Profile.MaxStamps = 14;
		Profile.StampSizeJitter = 0.15f;
		break;

	case ESlimeElement::Fire:
		Profile.StampKind = ESlimeTrailStampKind::Niagara;
		Profile.GroundNiagara = TSoftObjectPtr<UNiagaraSystem>(SlimeTrailDefaults::FireNiagara);
		Profile.AttachedOverlayMaterial = TSoftObjectPtr<UMaterialInterface>(SlimeTrailDefaults::FireOverlay);
		Profile.SpawnDistance = 16.f;
		Profile.StampSize = 0.9f;
		Profile.StampLifetime = 3.f;
		Profile.MaxStamps = 14;
		break;

	case ESlimeElement::Lightning:
		Profile.StampKind = ESlimeTrailStampKind::Niagara;
		Profile.GroundNiagara = TSoftObjectPtr<UNiagaraSystem>(SlimeTrailDefaults::TeslaArc);
		Profile.AttachedNiagara = TSoftObjectPtr<UNiagaraSystem>(SlimeTrailDefaults::ElectricityWrap);
		Profile.AttachedOverlayMaterial = TSoftObjectPtr<UMaterialInterface>(SlimeTrailDefaults::Overlay);
		Profile.bGroundNiagaraIsArc = true;
		Profile.SpawnDistance = 55.f;
		Profile.StampSize = 1.f;
		Profile.StampLifetime = 0.6f;
		Profile.MaxStamps = 3;
		Profile.ArcRadius = 180.f;
		Profile.AttachedScale = FVector(0.85f);
		Profile.AttachedOffset = FVector(0.f, 0.f, 8.f);
		break;

	case ESlimeElement::Dark:
		Profile.StampKind = ESlimeTrailStampKind::Decal;
		Profile.DecalMaterial = TSoftObjectPtr<UMaterialInterface>(SlimeTrailDefaults::DarkDecal);
		Profile.SpawnDistance = 16.f;
		Profile.StampSize = 48.f;
		Profile.StampLifetime = 3.5f;
		Profile.MaxStamps = 16;
		Profile.StampSizeJitter = 0.12f;
		break;

	case ESlimeElement::Physical:
	default:
		Profile.StampKind = ESlimeTrailStampKind::Niagara;
		Profile.GroundNiagara = TSoftObjectPtr<UNiagaraSystem>(SlimeTrailDefaults::PhysicalNiagara);
		Profile.SpawnDistance = 42.f;
		Profile.StampSize = 0.22f;
		Profile.StampLifetime = 0.9f;
		Profile.MaxStamps = 4;
		break;
	}

	return Profile;
}

const FSlimeTrailProfile& USlimeTrailComponent::GetProfile(ESlimeElement Element) const
{
	for (const FSlimeTrailProfile& Profile : Profiles)
	{
		if (Profile.Element == Element)
		{
			return Profile;
		}
	}

	static const FSlimeTrailProfile WaterFallback = []()
	{
		FSlimeTrailProfile Profile;
		Profile.Element = ESlimeElement::Water;
		Profile.StampKind = ESlimeTrailStampKind::None;
		return Profile;
	}();
	return WaterFallback;
}

void USlimeTrailComponent::BeginPlay()
{
	Super::BeginPlay();

	EnsureProfileDefaults();

	OwnerCharacter = Cast<ACharacter>(GetOwner());
	ElementComponent = GetOwner() ? GetOwner()->FindComponentByClass<USlimeElementComponent>() : nullptr;
	BodyComponent = GetOwner() ? GetOwner()->FindComponentByClass<USlimeBodyComponent>() : nullptr;

	if (const ASlimeCharacter* Slime = Cast<ASlimeCharacter>(GetOwner()))
	{
		SurfaceMesh = Slime->GetSurfaceMesh();
	}
	else if (GetOwner())
	{
		SurfaceMesh = GetOwner()->FindComponentByClass<UProceduralMeshComponent>();
	}

	if (ElementComponent)
	{
		ElementComponent->OnElementChanged.AddDynamic(this, &USlimeTrailComponent::HandleElementChanged);
		RefreshAttachedEffects(ElementComponent->CurrentElement);
	}

	if (OwnerCharacter)
	{
		LastStampLocation = OwnerCharacter->GetActorLocation();
		bHasLastStampLocation = true;
	}
}

void USlimeTrailComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ElementComponent)
	{
		ElementComponent->OnElementChanged.RemoveDynamic(this, &USlimeTrailComponent::HandleElementChanged);
	}

	ClearAttachedEffects();
	ClearClingFireFx();
	ClearShotLinkArcs();

	for (FActiveStamp& Stamp : ActiveStamps)
	{
		if (UDecalComponent* Decal = Stamp.Decal.Get())
		{
			Decal->DestroyComponent();
		}
		if (UNiagaraComponent* Niagara = Stamp.Niagara.Get())
		{
			Niagara->DeactivateImmediate();
			Niagara->DestroyComponent();
		}
	}
	ActiveStamps.Reset();

	Super::EndPlay(EndPlayReason);
}

void USlimeTrailComponent::HandleElementChanged(ESlimeElement NewElement, ESlimeElement PreviousElement)
{
	RefreshAttachedEffects(NewElement);
	if (NewElement != ESlimeElement::Fire)
	{
		ClearClingFireFx();
	}
	if (NewElement != ESlimeElement::Lightning)
	{
		ClearShotLinkArcs();
	}
	DistanceAccumulator = 0.f;
	if (OwnerCharacter)
	{
		LastStampLocation = OwnerCharacter->GetActorLocation();
		bHasLastStampLocation = true;
	}
}

void USlimeTrailComponent::ClearAttachedEffects()
{
	if (AttachedNiagaraComp)
	{
		AttachedNiagaraComp->DeactivateImmediate();
		AttachedNiagaraComp->DestroyComponent();
		AttachedNiagaraComp = nullptr;
	}

	if (SurfaceMesh && ActiveOverlay)
	{
		SurfaceMesh->SetOverlayMaterial(nullptr);
	}
	ActiveOverlay = nullptr;

	if (LightningSampleComp)
	{
		LightningSampleComp->SetHiddenInGame(true);
		LightningSampleComp->SetVisibility(false);
		LightningSampleComp->SetComponentTickEnabled(false);
	}
}

void USlimeTrailComponent::EnsureLightningSampleMesh()
{
	if (LightningSampleComp)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	LightningSampleComp = NewObject<USkeletalMeshComponent>(Owner, TEXT("SlimeLightningSample"));
	if (!LightningSampleComp)
	{
		return;
	}

	LightningSampleComp->SetupAttachment(Owner->GetRootComponent());
	LightningSampleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LightningSampleComp->SetGenerateOverlapEvents(false);
	LightningSampleComp->SetCastShadow(false);
	LightningSampleComp->SetHiddenInGame(true);
	LightningSampleComp->SetVisibility(false);
	LightningSampleComp->bNoSkeletonUpdate = false;
	LightningSampleComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	LightningSampleComp->RegisterComponent();

	if (USkeletalMesh* Mesh = ResolveSampleMesh())
	{
		LightningSampleComp->SetSkeletalMesh(Mesh);
	}

	LightningSampleComp->SetRelativeLocation(LightningSampleOffset);
	LightningSampleComp->SetRelativeScale3D(LightningSampleScale);
}

void USlimeTrailComponent::RefreshAttachedEffects(ESlimeElement Element)
{
	ClearAttachedEffects();

	const FSlimeTrailProfile& Profile = GetProfile(Element);

	if (UMaterialInterface* Overlay = ResolveMaterial(Profile.AttachedOverlayMaterial))
	{
		if (SurfaceMesh)
		{
			SurfaceMesh->SetOverlayMaterial(Overlay);
			ActiveOverlay = Overlay;
		}
	}

	UNiagaraSystem* AttachedSystem = ResolveNiagara(Profile.AttachedNiagara);
	if (!AttachedSystem || !GetOwner())
	{
		return;
	}

	const bool bNeedsSkeletalSample = (Element == ESlimeElement::Lightning);
	USceneComponent* AttachParent = GetOwner()->GetRootComponent();

	if (bNeedsSkeletalSample)
	{
		EnsureLightningSampleMesh();
		if (LightningSampleComp)
		{
			LightningSampleComp->SetRelativeLocation(LightningSampleOffset);
			LightningSampleComp->SetRelativeScale3D(LightningSampleScale);
			LightningSampleComp->SetHiddenInGame(true);
			LightningSampleComp->SetVisibility(false);
			LightningSampleComp->SetComponentTickEnabled(true);
			AttachParent = LightningSampleComp;
		}
	}

	AttachedNiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
		AttachedSystem,
		AttachParent,
		NAME_None,
		Profile.AttachedOffset,
		FRotator::ZeroRotator,
		Profile.AttachedScale,
		EAttachLocation::KeepRelativeOffset,
		false,
		ENCPoolMethod::None,
		true,
		true);

	if (AttachedNiagaraComp && bNeedsSkeletalSample && LightningSampleComp)
	{
		AttachedNiagaraComp->SetVariableObject(SlimeTrailDefaults::SkeletalMeshUser, LightningSampleComp);
		AttachedNiagaraComp->SetVariableObject(FName(TEXT("System.Skeletal Mesh")), LightningSampleComp);
	}
}

void USlimeTrailComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TickActiveStamps(DeltaTime);
	TickShotLinkArcs();
	UpdateClingFireFx();

	if (!bEnabled || !OwnerCharacter || !ElementComponent)
	{
		return;
	}

	const FSlimeTrailProfile& Profile = GetProfile(ElementComponent->CurrentElement);
	if (Profile.StampKind == ESlimeTrailStampKind::None)
	{
		return;
	}

	const FVector Location = OwnerCharacter->GetActorLocation();
	if (!bHasLastStampLocation)
	{
		LastStampLocation = Location;
		bHasLastStampLocation = true;
		return;
	}

	const FVector Delta = Location - LastStampLocation;
	const bool bClinging = IsOwnerClinging();
	const float Travel = bClinging ? Delta.Size() : FVector(Delta.X, Delta.Y, 0.f).Size();
	LastStampLocation = Location;
	DistanceAccumulator += Travel;

	if (!CanStamp(Profile))
	{
		return;
	}

	const float SpawnDistance = FMath::Max(Profile.SpawnDistance, 5.f);
	while (DistanceAccumulator >= SpawnDistance)
	{
		DistanceAccumulator -= SpawnDistance;
		TryStamp(Profile);
	}
}

const USlimeClingComponent* USlimeTrailComponent::GetOwnerCling() const
{
	if (const ASlimeCharacter* Slime = Cast<ASlimeCharacter>(OwnerCharacter))
	{
		return Slime->GetSlimeCling();
	}
	return nullptr;
}

bool USlimeTrailComponent::IsOwnerClinging() const
{
	if (const USlimeClingComponent* Cling = GetOwnerCling())
	{
		return Cling->IsClinging();
	}
	return false;
}

void USlimeTrailComponent::ClearClingFireFx()
{
	if (ClingFireNiagaraComp)
	{
		ClingFireNiagaraComp->DeactivateImmediate();
		ClingFireNiagaraComp->DestroyComponent();
		ClingFireNiagaraComp = nullptr;
	}
}

void USlimeTrailComponent::UpdateClingFireFx()
{
	const bool bWantFire = bEnabled
		&& IsOwnerClinging()
		&& ElementComponent
		&& ElementComponent->CurrentElement == ESlimeElement::Fire
		&& GetOwner();

	if (!bWantFire)
	{
		ClearClingFireFx();
		return;
	}

	const FSlimeTrailProfile& Profile = GetProfile(ESlimeElement::Fire);
	UNiagaraSystem* System = ResolveNiagara(Profile.GroundNiagara);
	if (!System)
	{
		ClearClingFireFx();
		return;
	}

	const USlimeClingComponent* Cling = GetOwnerCling();
	const FVector Normal = Cling ? Cling->GetWallNormal() : FVector::UpVector;
	const FRotator Rotation = FRotationMatrix::MakeFromZ(Normal).Rotator();
	const FVector WorldLoc = GetOwner()->GetActorLocation() + Normal * 12.f;
	const FVector Scale(FMath::Max(Profile.StampSize, 0.2f));

	if (!ClingFireNiagaraComp)
	{
		USceneComponent* AttachParent = GetOwner()->GetRootComponent();
		const FTransform ParentXf = AttachParent ? AttachParent->GetComponentTransform() : GetOwner()->GetActorTransform();
		ClingFireNiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
			System,
			AttachParent,
			NAME_None,
			ParentXf.InverseTransformPosition(WorldLoc),
			ParentXf.InverseTransformRotation(Rotation.Quaternion()).Rotator(),
			Scale,
			EAttachLocation::KeepRelativeOffset,
			false,
			ENCPoolMethod::None,
			true,
			true);
	}

	if (!ClingFireNiagaraComp)
	{
		return;
	}

	ClingFireNiagaraComp->SetWorldLocationAndRotation(WorldLoc, Rotation);
	ClingFireNiagaraComp->SetWorldScale3D(Scale);
	ConfigureFireFootstep(ClingFireNiagaraComp);
	if (!ClingFireNiagaraComp->IsActive())
	{
		ClingFireNiagaraComp->Activate(true);
		ConfigureFireFootstep(ClingFireNiagaraComp);
	}
}

bool USlimeTrailComponent::CanStamp(const FSlimeTrailProfile& Profile) const
{
	if (!OwnerCharacter)
	{
		return false;
	}

	const UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	if (!Movement)
	{
		return false;
	}

	if (bOnlyOnGround && !IsOwnerClinging() && !Movement->IsMovingOnGround())
	{
		return false;
	}

	if (IsOwnerClinging())
	{
		return true;
	}

	const FVector Velocity = Movement->Velocity;
	const float HorizontalSpeed = FVector(Velocity.X, Velocity.Y, 0.f).Size();
	return HorizontalSpeed >= Profile.MinSpeed;
}

bool USlimeTrailComponent::ResolveStampLocation(FVector& OutLocation, FVector& OutNormal) const
{
	if (!OwnerCharacter || !GetWorld())
	{
		return false;
	}

	if (const USlimeClingComponent* Cling = GetOwnerCling())
	{
		if (Cling->IsClinging())
		{
			const FVector WallNormal = Cling->GetWallNormal();
			const FVector Start = OwnerCharacter->GetActorLocation();
			const FVector End = Start - WallNormal * GroundTraceDistance;
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeTrailWall), false, OwnerCharacter);
			if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, GroundTraceChannel, Params))
			{
				OutLocation = Hit.ImpactPoint;
				OutNormal = Hit.ImpactNormal;
				return true;
			}

			OutLocation = Cling->GetWallPoint();
			OutNormal = WallNormal;
			return !OutNormal.IsNearlyZero();
		}
	}

	if (const UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
	{
		if (Movement->CurrentFloor.bBlockingHit)
		{
			OutLocation = Movement->CurrentFloor.HitResult.ImpactPoint;
			OutNormal = Movement->CurrentFloor.HitResult.ImpactNormal;
			return true;
		}
	}

	const FVector Start = OwnerCharacter->GetActorLocation();
	const FVector End = Start - FVector(0.f, 0.f, GroundTraceDistance);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeTrailGround), false, OwnerCharacter);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, GroundTraceChannel, Params))
	{
		OutLocation = Hit.ImpactPoint;
		OutNormal = Hit.ImpactNormal;
		return true;
	}

	return false;
}

void USlimeTrailComponent::TryStamp(const FSlimeTrailProfile& Profile)
{
	while (ActiveStamps.Num() >= FMath::Max(Profile.MaxStamps, 1))
	{
		RecycleOldestStamp();
	}

	if (Profile.bGroundNiagaraIsArc)
	{
		StampLightningArc(Profile);
		return;
	}

	FVector Location;
	FVector Normal;
	if (!ResolveStampLocation(Location, Normal))
	{
		return;
	}

	const float Jitter = 1.f + FMath::FRandRange(-Profile.StampSizeJitter, Profile.StampSizeJitter);
	const float Size = FMath::Max(Profile.StampSize * Jitter, 0.01f);

	if (Profile.StampKind == ESlimeTrailStampKind::Decal)
	{
		StampDecal(Profile, Location, Normal, Size);
	}
	else if (Profile.StampKind == ESlimeTrailStampKind::Niagara)
	{
		StampGroundNiagara(Profile, Location, Normal, Size);
	}
}

void USlimeTrailComponent::StampDecal(
	const FSlimeTrailProfile& Profile,
	const FVector& Location,
	const FVector& Normal,
	float Size)
{
	UMaterialInterface* Material = ResolveMaterial(Profile.DecalMaterial);
	if (!Material || !GetOwner())
	{
		return;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Material, this);
	if (!MID)
	{
		return;
	}
	MID->SetScalarParameterValue(SlimeTrailDefaults::FadeParam, 1.f);

	const FQuat BaseQuat = FRotationMatrix::MakeFromX(-Normal).ToQuat();
	const FQuat Spin = FQuat(Normal, FMath::FRandRange(0.f, 2.f * PI));
	const FRotator Rotation = (Spin * BaseQuat).Rotator();

	UDecalComponent* Decal = NewObject<UDecalComponent>(GetOwner());
	if (!Decal)
	{
		return;
	}

	Decal->SetDecalMaterial(MID);
	// UE decal: X = projection depth, Y/Z = footprint. StampSize ≈ footprint diameter in cm.
	Decal->DecalSize = FVector(Size * 0.4f, Size, Size);
	Decal->SetWorldLocationAndRotation(Location + Normal * 1.5f, Rotation);
	Decal->SetFadeScreenSize(0.001f);
	Decal->RegisterComponent();

	FActiveStamp Stamp;
	Stamp.Decal = Decal;
	Stamp.DecalMID = MID;
	Stamp.Lifetime = Profile.StampLifetime;
	ActiveStamps.Add(Stamp);
}

void USlimeTrailComponent::StampGroundNiagara(
	const FSlimeTrailProfile& Profile,
	const FVector& Location,
	const FVector& Normal,
	float Size)
{
	UNiagaraSystem* System = ResolveNiagara(Profile.GroundNiagara);
	if (!System)
	{
		return;
	}

	const FRotator Rotation = FRotationMatrix::MakeFromZ(Normal).Rotator();
	const float NormalOffset = IsOwnerClinging() ? 12.f : 2.f;
	UNiagaraComponent* Niagara = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		System,
		Location + Normal * NormalOffset,
		Rotation,
		FVector(Size),
		false,
		true,
		ENCPoolMethod::None,
		true);

	if (!Niagara)
	{
		return;
	}

	// Some trail systems ignore spawn rotation; force local +Z along the surface normal
	// so fire / gravel spray out of a wall instead of world up.
	Niagara->SetWorldRotation(Rotation);

	if (Profile.Element == ESlimeElement::Fire)
	{
		ConfigureFireFootstep(Niagara);
	}

	FActiveStamp Stamp;
	Stamp.Niagara = Niagara;
	Stamp.Lifetime = Profile.StampLifetime;
	ActiveStamps.Add(Stamp);
}

bool USlimeTrailComponent::PickArcTarget(const FSlimeTrailProfile& Profile, FVector& OutTarget) const
{
	if (!OwnerCharacter || !GetWorld())
	{
		return false;
	}

	const FVector Origin = BodyComponent ? BodyComponent->GetBlobCenter() : OwnerCharacter->GetActorLocation();
	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	const float Radius = FMath::FRandRange(Profile.ArcRadius * 0.35f, Profile.ArcRadius);
	const FVector Horizontal = FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
	const FVector Probe = Origin + Horizontal + FVector(0.f, 0.f, 40.f);
	const FVector End = Probe - FVector(0.f, 0.f, GroundTraceDistance + 80.f);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeTrailArc), false, OwnerCharacter);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Probe, End, GroundTraceChannel, Params))
	{
		OutTarget = Hit.ImpactPoint;
		return true;
	}

	OutTarget = Origin + Horizontal;
	OutTarget.Z = Origin.Z - 40.f;
	return true;
}

void USlimeTrailComponent::StampLightningArc(const FSlimeTrailProfile& Profile)
{
	UNiagaraSystem* System = ResolveNiagara(Profile.GroundNiagara);
	if (!System || !OwnerCharacter)
	{
		return;
	}

	FVector Target;
	if (!PickArcTarget(Profile, Target))
	{
		return;
	}

	const FVector Origin = BodyComponent ? BodyComponent->GetBlobCenter() : OwnerCharacter->GetActorLocation();

	UNiagaraComponent* Niagara = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		System,
		Origin,
		FRotator::ZeroRotator,
		FVector(Profile.StampSize),
		false,
		false,
		ENCPoolMethod::None,
		true);

	if (!Niagara)
	{
		return;
	}

	ConfigureTeslaArc(Niagara);
	Niagara->SetVariablePosition(SlimeTrailDefaults::PositionTarget, Target);
	Niagara->Activate(true);
	ConfigureTeslaArc(Niagara);

	FActiveStamp Stamp;
	Stamp.Niagara = Niagara;
	Stamp.Lifetime = Profile.StampLifetime;
	Stamp.bIsArc = true;
	ActiveStamps.Add(Stamp);
}

void USlimeTrailComponent::ConfigureTeslaArc(UNiagaraComponent* Niagara)
{
	if (!Niagara)
	{
		return;
	}

	Niagara->SetEmitterEnable(SlimeTrailDefaults::TeslaSmokeEmitter, false);

	if (UNiagaraSystem* System = Niagara->GetAsset())
	{
		for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		{
			const FName EmitterName = Handle.GetName();
			if (EmitterName.ToString().Contains(TEXT("Smoke"), ESearchCase::IgnoreCase))
			{
				Niagara->SetEmitterEnable(EmitterName, false);
			}
		}
	}

	Niagara->SetVariableLinearColor(SlimeTrailDefaults::TeslaSmokeColor, FLinearColor(0.f, 0.f, 0.f, 0.f));
}

void USlimeTrailComponent::ConfigureFireFootstep(UNiagaraComponent* Niagara)
{
	if (!Niagara)
	{
		return;
	}

	auto ShouldDisable = [](const FString& Name) -> bool
	{
		return Name.Contains(TEXT("Foot"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("Print"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("Decal"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("Mesh"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("Shoe"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("Step"), ESearchCase::IgnoreCase);
	};

	// Common authoring names on NS_Footstep_Fire.
	static const FName Candidates[] = {
		FName(TEXT("Footprint")),
		FName(TEXT("Footprints")),
		FName(TEXT("Foot Print")),
		FName(TEXT("FootPrint")),
		FName(TEXT("Footstep")),
		FName(TEXT("Footsteps")),
		FName(TEXT("Print")),
		FName(TEXT("Decal")),
		FName(TEXT("Mesh")),
	};
	for (const FName& Name : Candidates)
	{
		Niagara->SetEmitterEnable(Name, false);
	}

	if (UNiagaraSystem* System = Niagara->GetAsset())
	{
		for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		{
			const FString EmitterName = Handle.GetName().ToString();
			if (ShouldDisable(EmitterName))
			{
				Niagara->SetEmitterEnable(Handle.GetName(), false);
				UE_LOG(LogSlimeFable, Verbose, TEXT("FireTrail: disabled emitter '%s'"), *EmitterName);
			}
		}
	}
}

void USlimeTrailComponent::TickShotLinkArcs()
{
	const bool bLightningActive =
		bEnabled && bLinkArcsToShots && ElementComponent
		&& ElementComponent->CurrentElement == ESlimeElement::Lightning
		&& BodyComponent && GetWorld();

	if (!bLightningActive)
	{
		ClearShotLinkArcs();
		return;
	}

	const FSlimeTrailProfile& Profile = GetProfile(ESlimeElement::Lightning);
	UNiagaraSystem* System = ResolveNiagara(Profile.GroundNiagara);
	if (!System)
	{
		ClearShotLinkArcs();
		return;
	}

	TArray<FVector> ShotCenters;
	BodyComponent->GetActiveShotCenters(ShotCenters);

	const int32 Desired = FMath::Min(ShotCenters.Num(), FMath::Max(MaxShotArcs, 0));
	const FVector Origin = BodyComponent->GetBlobCenter();

	// Shrink pool.
	while (ShotLinkArcs.Num() > Desired)
	{
		if (UNiagaraComponent* Extra = ShotLinkArcs.Last())
		{
			Extra->DeactivateImmediate();
			Extra->DestroyComponent();
		}
		ShotLinkArcs.RemoveAt(ShotLinkArcs.Num() - 1);
	}

	// Grow pool.
	while (ShotLinkArcs.Num() < Desired)
	{
		UNiagaraComponent* Niagara = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			System,
			Origin,
			FRotator::ZeroRotator,
			FVector(Profile.StampSize),
			false,
			false,
			ENCPoolMethod::None,
			true);
		if (!Niagara)
		{
			break;
		}
		ConfigureTeslaArc(Niagara);
		Niagara->Activate(true);
		ConfigureTeslaArc(Niagara);
		ShotLinkArcs.Add(Niagara);
	}

	for (int32 Index = 0; Index < ShotLinkArcs.Num(); ++Index)
	{
		UNiagaraComponent* Niagara = ShotLinkArcs[Index];
		if (!Niagara || !ShotCenters.IsValidIndex(Index))
		{
			continue;
		}
		ConfigureTeslaArc(Niagara);
		Niagara->SetWorldLocation(Origin);
		Niagara->SetVariablePosition(SlimeTrailDefaults::PositionTarget, ShotCenters[Index]);
		if (!Niagara->IsActive())
		{
			Niagara->Activate(true);
		}
	}
}

void USlimeTrailComponent::ClearShotLinkArcs()
{
	for (UNiagaraComponent* Niagara : ShotLinkArcs)
	{
		if (Niagara)
		{
			Niagara->DeactivateImmediate();
			Niagara->DestroyComponent();
		}
	}
	ShotLinkArcs.Reset();
}

void USlimeTrailComponent::TickActiveStamps(float DeltaTime)
{
	for (int32 Index = ActiveStamps.Num() - 1; Index >= 0; --Index)
	{
		FActiveStamp& Stamp = ActiveStamps[Index];
		Stamp.Age += DeltaTime;

		if (UMaterialInstanceDynamic* MID = Stamp.DecalMID)
		{
			const float FadeStart = FMath::Max(Stamp.Lifetime - DecalFadeOutDuration, 0.f);
			float Fade = 1.f;
			if (Stamp.Age >= FadeStart && DecalFadeOutDuration > KINDA_SMALL_NUMBER)
			{
				Fade = 1.f - ((Stamp.Age - FadeStart) / DecalFadeOutDuration);
			}
			MID->SetScalarParameterValue(SlimeTrailDefaults::FadeParam, FMath::Clamp(Fade, 0.f, 1.f));
		}

		const bool bExpired = Stamp.Age >= Stamp.Lifetime;
		const bool bMissing = (!Stamp.Decal.IsValid() && !Stamp.Niagara.IsValid());

		if (bExpired || bMissing)
		{
			if (UDecalComponent* Decal = Stamp.Decal.Get())
			{
				Decal->DestroyComponent();
			}
			if (UNiagaraComponent* Niagara = Stamp.Niagara.Get())
			{
				Niagara->Deactivate();
				Niagara->DestroyComponent();
			}
			ActiveStamps.RemoveAtSwap(Index);
		}
	}
}

void USlimeTrailComponent::RecycleOldestStamp()
{
	if (ActiveStamps.Num() == 0)
	{
		return;
	}

	int32 OldestIndex = 0;
	float OldestAge = -1.f;
	for (int32 Index = 0; Index < ActiveStamps.Num(); ++Index)
	{
		if (ActiveStamps[Index].Age > OldestAge)
		{
			OldestAge = ActiveStamps[Index].Age;
			OldestIndex = Index;
		}
	}

	FActiveStamp& Stamp = ActiveStamps[OldestIndex];
	if (UDecalComponent* Decal = Stamp.Decal.Get())
	{
		Decal->DestroyComponent();
	}
	if (UNiagaraComponent* Niagara = Stamp.Niagara.Get())
	{
		Niagara->DeactivateImmediate();
		Niagara->DestroyComponent();
	}
	ActiveStamps.RemoveAtSwap(OldestIndex);
}

UMaterialInterface* USlimeTrailComponent::ResolveMaterial(const TSoftObjectPtr<UMaterialInterface>& Soft) const
{
	if (Soft.IsNull())
	{
		return nullptr;
	}
	return Soft.LoadSynchronous();
}

UNiagaraSystem* USlimeTrailComponent::ResolveNiagara(const TSoftObjectPtr<UNiagaraSystem>& Soft) const
{
	if (Soft.IsNull())
	{
		return nullptr;
	}
	return Soft.LoadSynchronous();
}

USkeletalMesh* USlimeTrailComponent::ResolveSampleMesh() const
{
	if (LightningSampleMesh.IsNull())
	{
		return nullptr;
	}
	return LightningSampleMesh.LoadSynchronous();
}
