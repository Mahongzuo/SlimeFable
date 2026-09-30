// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeFluidNinjaContactComponent.h"

#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Hub/HomeBuild/SlimeHomeFluidPad.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/SlimeAudioPlay.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"

namespace
{
	bool InheritsToken(const UClass* Class, const TCHAR* Token)
	{
		for (const UClass* Cursor = Class; Cursor; Cursor = Cursor->GetSuperClass())
		{
			if (Cursor->GetName().Contains(Token, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	bool IsHomeFluidActor(const AActor* Actor)
	{
		if (!Actor)
		{
			return false;
		}
		if (Actor->IsA(ASlimeHomeFluidPad::StaticClass()))
		{
			return true;
		}
		const AActor* Parent = Actor->GetAttachParentActor();
		return Parent && Parent->IsA(ASlimeHomeFluidPad::StaticClass());
	}
}

USlimeFluidNinjaContactComponent::USlimeFluidNinjaContactComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USlimeFluidNinjaContactComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (UCapsuleComponent* Capsule = OwnerCharacter ? OwnerCharacter->GetCapsuleComponent() : nullptr)
	{
		// FluidNinja Live Activation requires a Pawn with Generate Overlap Events inside
		// ActivationVolume (PawnInsideActivationBounds). Without this, TraceMesh stays on
		// MI_FluidNinjaLive_TraceMesh_Inactive (opaque white/gray box).
		// Interaction paint still prefers the small WorldDynamic contact spheres.
		Capsule->SetGenerateOverlapEvents(true);
	}

	if (bEnableFluidContacts)
	{
		RebuildContacts();
	}

	PassthroughRescanTimer = 0.f;
	ApplyFluidNinjaPawnPassthrough();
}

void USlimeFluidNinjaContactComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCapsuleComponent* Capsule = OwnerCharacter ? OwnerCharacter->GetCapsuleComponent() : nullptr)
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& WeakComp : PassthroughIgnoredComponents)
		{
			if (UPrimitiveComponent* Comp = WeakComp.Get())
			{
				Capsule->IgnoreComponentWhenMoving(Comp, false);
			}
		}
	}
	PassthroughIgnoredComponents.Reset();

	StopPoolLoop();
	DestroyContacts();
	Super::EndPlay(EndPlayReason);
}

void USlimeFluidNinjaContactComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	PassthroughRescanTimer -= DeltaTime;
	if (PassthroughRescanTimer <= 0.f)
	{
		PassthroughRescanTimer = FMath::Max(0.1f, NinjaPassthroughRescanInterval);
		ApplyFluidNinjaPawnPassthrough();
	}

	if (!bEnableFluidContacts)
	{
		if (ContactSpheres.Num() > 0)
		{
			DestroyContacts();
		}
		UpdatePoolAudio();
		return;
	}

	if (ContactSpheres.Num() == 0)
	{
		RebuildContacts();
	}

	SyncContactTransforms();
	UpdatePoolAudio();
}

bool USlimeFluidNinjaContactComponent::IsFluidNinjaBlockingSimGeom(const UPrimitiveComponent* Component)
{
	if (!Component)
	{
		return false;
	}

	const AActor* Owner = Component->GetOwner();
	if (!Owner)
	{
		return false;
	}

	const FString OwnerName = Owner->GetName();
	const FString OwnerClass = Owner->GetClass() ? Owner->GetClass()->GetName() : FString();
	const bool bNinjaOwner = OwnerName.Contains(TEXT("NinjaLive"), ESearchCase::IgnoreCase)
		|| OwnerClass.Contains(TEXT("NinjaLive"), ESearchCase::IgnoreCase)
		|| InheritsToken(Owner->GetClass(), TEXT("NinjaLive"))
		|| IsHomeFluidActor(Owner);
	if (!bNinjaOwner)
	{
		return false;
	}

	const FString CompName = Component->GetName();
	// ActivationVolume must NOT be move-ignored: Live Activation tracks Pawn overlap there.
	return CompName.Contains(TEXT("TraceMesh"), ESearchCase::IgnoreCase)
		|| CompName.Contains(TEXT("InteractionVolume"), ESearchCase::IgnoreCase)
		|| CompName.Contains(TEXT("InteractionVol"), ESearchCase::IgnoreCase);
}

namespace
{
	bool IsPoolActor(const AActor* Actor)
	{
		if (!Actor)
		{
			return false;
		}
		const FString ActorName = Actor->GetName();
		const FString ActorClass = Actor->GetClass() ? Actor->GetClass()->GetName() : FString();
		return ActorName.Contains(TEXT("Pool"), ESearchCase::IgnoreCase)
			|| ActorClass.Contains(TEXT("Pool"), ESearchCase::IgnoreCase)
			|| InheritsToken(Actor->GetClass(), TEXT("Pool"));
	}

	/** Upright capsule against the water cube. The cube is not expanded. */
	bool CapsuleOverlapsBox(const UCapsuleComponent* Capsule, const FBox& Box)
	{
		if (!Capsule || !Box.IsValid)
		{
			return false;
		}
		const FVector Center = Capsule->GetComponentLocation();
		const float Radius = Capsule->GetScaledCapsuleRadius();
		const float InnerHalf = FMath::Max(Capsule->GetScaledCapsuleHalfHeight() - Radius, 0.f);
		const float UseZ = FMath::Clamp(FMath::Clamp(Center.Z, Box.Min.Z, Box.Max.Z), Center.Z - InnerHalf, Center.Z + InnerHalf);
		const FVector OnCapsule(Center.X, Center.Y, UseZ);
		const FVector OnBox(
			FMath::Clamp(OnCapsule.X, Box.Min.X, Box.Max.X),
			FMath::Clamp(OnCapsule.Y, Box.Min.Y, Box.Max.Y),
			FMath::Clamp(OnCapsule.Z, Box.Min.Z, Box.Max.Z));
		return FVector::DistSquared(OnCapsule, OnBox) <= FMath::Square(Radius);
	}
}

bool USlimeFluidNinjaContactComponent::IsOverlappingPool() const
{
	UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = OwnerCharacter ? OwnerCharacter->GetCapsuleComponent() : nullptr;
	if (!World || !Capsule)
	{
		return false;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const AActor* Actor = *It;
		if (!IsPoolActor(Actor))
		{
			continue;
		}
		TArray<UPrimitiveComponent*> Prims;
		Actor->GetComponents<UPrimitiveComponent>(Prims);
		for (const UPrimitiveComponent* Prim : Prims)
		{
			if (!Prim || !Prim->GetName().Contains(TEXT("TraceMesh"), ESearchCase::IgnoreCase))
			{
				continue;
			}
			const FBox Water = Prim->CalcBounds(Prim->GetComponentTransform()).GetBox();
			if (CapsuleOverlapsBox(Capsule, Water))
			{
				return true;
			}
		}
	}
	return false;
}

void USlimeFluidNinjaContactComponent::ResolvePoolSounds()
{
	if (bPoolSoundsResolved)
	{
		return;
	}
	bPoolSoundsResolved = true;
	PoolSplashSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/Movement/sfx_water_in.sfx_water_in"));
	PoolLoopSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/Movement/sfx_water_loop.sfx_water_loop"));
}

void USlimeFluidNinjaContactComponent::StartPoolLoop()
{
	if (PoolLoopAudio && PoolLoopAudio->IsPlaying())
	{
		return;
	}
	ResolvePoolSounds();
	if (!PoolLoopSound || !OwnerCharacter)
	{
		return;
	}
	if (USoundWave* Wave = Cast<USoundWave>(PoolLoopSound))
	{
		Wave->bLooping = true;
	}
	PoolLoopAudio = UGameplayStatics::SpawnSoundAttached(
		PoolLoopSound,
		OwnerCharacter->GetRootComponent(),
		NAME_None,
		FVector::ZeroVector,
		EAttachLocation::KeepRelativeOffset,
		false,
		SlimeAudioPlay::SfxMul(this),
		1.f,
		0.f,
		nullptr,
		nullptr,
		true);
}

void USlimeFluidNinjaContactComponent::StopPoolLoop()
{
	if (!PoolLoopAudio)
	{
		return;
	}
	if (PoolLoopAudio->IsPlaying())
	{
		PoolLoopAudio->FadeOut(0.12f, 0.f);
	}
	PoolLoopAudio = nullptr;
}

void USlimeFluidNinjaContactComponent::UpdatePoolAudio()
{
	if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled())
	{
		StopPoolLoop();
		bWasInPool = false;
		return;
	}

	const bool bInPool = IsOverlappingPool();
	if (bInPool && !bWasInPool)
	{
		ResolvePoolSounds();
		SlimeAudioPlay::PlaySfxAt(this, PoolSplashSound, OwnerCharacter->GetActorLocation());
	}
	if (bInPool)
	{
		StartPoolLoop();
	}
	else
	{
		StopPoolLoop();
	}
	bWasInPool = bInPool;
}

void USlimeFluidNinjaContactComponent::ApplyFluidNinjaPawnPassthrough()
{
	if (!bPawnPassthroughNinjaSimGeom || !OwnerCharacter)
	{
		return;
	}

	UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	UWorld* World = GetWorld();
	if (!Capsule || !World)
	{
		return;
	}

	TSet<UPrimitiveComponent*> Desired;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		const FString ActorName = Actor->GetName();
		const FString ActorClass = Actor->GetClass() ? Actor->GetClass()->GetName() : FString();
		if (!ActorName.Contains(TEXT("NinjaLive"), ESearchCase::IgnoreCase)
			&& !ActorClass.Contains(TEXT("NinjaLive"), ESearchCase::IgnoreCase)
			&& !InheritsToken(Actor->GetClass(), TEXT("NinjaLive"))
			&& !IsHomeFluidActor(Actor))
		{
			continue;
		}

		TArray<UPrimitiveComponent*> Prims;
		Actor->GetComponents<UPrimitiveComponent>(Prims);
		for (UPrimitiveComponent* Comp : Prims)
		{
			if (!Comp)
			{
				continue;
			}

			const FString CompName = Comp->GetName();
			// Live Activation: ensure ActivationVolume can see the slime Pawn capsule.
			if (CompName.Contains(TEXT("ActivationVolume"), ESearchCase::IgnoreCase))
			{
				Comp->SetGenerateOverlapEvents(true);
				if (Comp->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
				{
					Comp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
				}
				continue;
			}

			if (IsFluidNinjaBlockingSimGeom(Comp))
			{
				Desired.Add(Comp);
			}
		}
	}

	// Drop stale ignores.
	for (int32 Index = PassthroughIgnoredComponents.Num() - 1; Index >= 0; --Index)
	{
		UPrimitiveComponent* Comp = PassthroughIgnoredComponents[Index].Get();
		if (!Comp || !Desired.Contains(Comp))
		{
			if (Comp)
			{
				Capsule->IgnoreComponentWhenMoving(Comp, false);
			}
			PassthroughIgnoredComponents.RemoveAtSwap(Index);
		}
	}

	for (UPrimitiveComponent* Comp : Desired)
	{
		bool bAlready = false;
		for (const TWeakObjectPtr<UPrimitiveComponent>& WeakComp : PassthroughIgnoredComponents)
		{
			if (WeakComp.Get() == Comp)
			{
				bAlready = true;
				break;
			}
		}
		if (bAlready)
		{
			continue;
		}

		// Slime-only: Mannequin can still stand on TraceMesh. Contact spheres keep Overlap.
		Capsule->IgnoreComponentWhenMoving(Comp, true);
		PassthroughIgnoredComponents.Add(Comp);
	}
}

USphereComponent* USlimeFluidNinjaContactComponent::CreateContactSphere(const FName& Name)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	USphereComponent* Sphere = NewObject<USphereComponent>(Owner, Name);
	if (!Sphere)
	{
		return nullptr;
	}

	Sphere->SetupAttachment(Owner->GetRootComponent());
	Sphere->SetMobility(EComponentMobility::Movable);
	Sphere->SetHiddenInGame(true);
	Sphere->SetVisibility(false);
	Sphere->SetCastShadow(false);
	Sphere->SetCanEverAffectNavigation(false);
	Sphere->SetAbsolute(false, false, false);

	// WorldDynamic: many NinjaLive pools include this object type for props/rotors.
	// Query-only overlap so we never block movement or soft-body gathering as geometry.
	Sphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionObjectType(ECC_WorldDynamic);
	Sphere->SetCollisionResponseToAllChannels(ECR_Overlap);
	Sphere->SetGenerateOverlapEvents(true);
	Sphere->ComponentTags.Add(TEXT("FluidNinjaContact"));

	Sphere->RegisterComponent();
	return Sphere;
}

void USlimeFluidNinjaContactComponent::DestroyContacts()
{
	for (USphereComponent* Sphere : ContactSpheres)
	{
		if (Sphere)
		{
			Sphere->DestroyComponent();
		}
	}
	ContactSpheres.Reset();
}

void USlimeFluidNinjaContactComponent::RebuildContacts()
{
	DestroyContacts();

	if (!bEnableFluidContacts || !GetOwner())
	{
		return;
	}

	const int32 VertCount = FMath::Clamp(VerticalSampleCount, 1, 12);
	const int32 RingCount = FMath::Clamp(BodyRingSampleCount, 0, 8);
	const int32 Total = VertCount + RingCount;
	ContactSpheres.Reserve(Total);

	for (int32 Index = 0; Index < Total; ++Index)
	{
		const FName SphereName(*FString::Printf(TEXT("FluidNinjaContact_%d"), Index));
		if (USphereComponent* Sphere = CreateContactSphere(SphereName))
		{
			ContactSpheres.Add(Sphere);
		}
	}

	SyncContactTransforms();
}

void USlimeFluidNinjaContactComponent::SyncContactTransforms()
{
	if (ContactSpheres.Num() == 0 || !OwnerCharacter)
	{
		return;
	}

	UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	if (!Capsule)
	{
		return;
	}

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
	const float Radius = FMath::Max(2.f, ContactRadius);

	const int32 VertCount = FMath::Clamp(VerticalSampleCount, 1, 12);
	const int32 RingCount = FMath::Clamp(BodyRingSampleCount, 0, 8);

	// Vertical column: sole → slightly above head (thin water sheets often sit near TraceMesh).
	const float ZMin = -HalfHeight + Radius;
	const float ZMax = HalfHeight + FMath::Max(0.f, ExtraHeightAboveCapsule);
	for (int32 Index = 0; Index < VertCount && Index < ContactSpheres.Num(); ++Index)
	{
		USphereComponent* Sphere = ContactSpheres[Index];
		if (!Sphere)
		{
			continue;
		}

		const float Alpha = (VertCount == 1) ? 0.f : static_cast<float>(Index) / static_cast<float>(VertCount - 1);
		const float Z = FMath::Lerp(ZMin, ZMax, Alpha);
		Sphere->SetSphereRadius(Radius);
		Sphere->SetRelativeLocation(FVector(0.f, 0.f, Z));
	}

	// Body ring near mid-capsule for splash / body displacement (Mannequin torso equivalent).
	const float RingZ = FMath::Lerp(-HalfHeight * 0.25f, HalfHeight * 0.35f, 0.5f);
	const float RingRadius = CapsuleRadius * FMath::Clamp(BodyRingRadiusScale, 0.1f, 1.f);
	for (int32 RingIndex = 0; RingIndex < RingCount; ++RingIndex)
	{
		const int32 SphereIndex = VertCount + RingIndex;
		if (!ContactSpheres.IsValidIndex(SphereIndex) || !ContactSpheres[SphereIndex])
		{
			continue;
		}

		const float Angle = (2.f * PI * static_cast<float>(RingIndex)) / static_cast<float>(RingCount);
		const FVector Loc(FMath::Cos(Angle) * RingRadius, FMath::Sin(Angle) * RingRadius, RingZ);
		ContactSpheres[SphereIndex]->SetSphereRadius(Radius);
		ContactSpheres[SphereIndex]->SetRelativeLocation(Loc);
	}
}
