// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimePoopActor.h"

#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Settings/SlimeAudioPlay.h"
#include "SlimeFaceComponent.h"
#include "SlimeFable.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

TArray<TWeakObjectPtr<ASlimePoopActor>> ASlimePoopActor::LivePiles;

ASlimePoopActor::ASlimePoopActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);
	SceneRoot->SetMobility(EComponentMobility::Movable);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PoopMesh"));
	BodyMesh->SetupAttachment(SceneRoot);
	BodyMesh->SetMobility(EComponentMobility::Movable);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BodyMesh->SetCollisionObjectType(ECC_WorldStatic);
	BodyMesh->SetCollisionResponseToAllChannels(ECR_Block);
	BodyMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	BodyMesh->SetGenerateOverlapEvents(false);
	BodyMesh->SetCastShadow(true);
	BodyMesh->CanCharacterStepUpOn = ECB_Yes;
	BodyMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PoopMesh(
		TEXT("/Game/Characters/Slime/Meshes/SM_SlimePoop/StaticMeshes/SM_SlimePoop.SM_SlimePoop"));
	if (PoopMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(PoopMesh.Object);
	}
}

void ASlimePoopActor::BeginPlay()
{
	Super::BeginPlay();
	RegisterLive();
	EnforceBudget();
	ApplyMeshAndScale();
}

void ASlimePoopActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterLive();
	Super::EndPlay(EndPlayReason);
}

void ASlimePoopActor::Configure(const FLinearColor& InColor, const FVector& InForward)
{
	(void)InColor;
	(void)InForward;
}

ASlimePoopActor* ASlimePoopActor::SpawnFromProducer(AActor* Producer)
{
	if (!Producer)
	{
		return nullptr;
	}
	UWorld* World = Producer->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FVector Face = ResolveBehindDirection(Producer);
	FVector Origin = ResolveSpawnOrigin(Producer, Face);

	FHitResult Hit;
	FCollisionQueryParams Params(TEXT("SlimePoopFloor"), false, Producer);
	const FVector TraceStart = Origin + FVector(0.f, 0.f, 80.f);
	const FVector TraceEnd = Origin - FVector(0.f, 0.f, 400.f);
	if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
	{
		Origin.Z = Hit.ImpactPoint.Z + 1.f;
	}

	const FTransform Xform(Face.Rotation(), Origin);
	ASlimePoopActor* Poop = World->SpawnActorDeferred<ASlimePoopActor>(
		ASlimePoopActor::StaticClass(), Xform, Producer, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Poop)
	{
		return nullptr;
	}
	Poop->Configure(FLinearColor::White, Face);
	Poop->FinishSpawning(Xform);
	return Poop;
}

void ASlimePoopActor::PlayProducerReaction(AActor* Producer)
{
	if (!Producer)
	{
		return;
	}

	if (USlimeFaceComponent* FaceComp = Producer->FindComponentByClass<USlimeFaceComponent>())
	{
		FaceComp->PulseWicked(1.2f);
	}

	USoundBase* Sfx = nullptr;
	if (const ASlimePoopActor* CDO = GetDefault<ASlimePoopActor>())
	{
		Sfx = CDO->PoopSound.LoadSynchronous();
	}
	if (!Sfx)
	{
		Sfx = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/Combat/sfx_poop_01.sfx_poop_01"));
	}
	if (Sfx)
	{
		SlimeAudioPlay::PlaySfxAt(Producer, Sfx, Producer->GetActorLocation());
	}
}

FVector ASlimePoopActor::ResolveBehindDirection(const AActor* Producer)
{
	FVector Face = FVector::ForwardVector;
	if (const USlimeFaceComponent* FaceComp = Producer->FindComponentByClass<USlimeFaceComponent>())
	{
		Face = FaceComp->GetFaceForward();
	}
	else if (Producer)
	{
		Face = Producer->GetActorForwardVector();
	}
	Face.Z = 0.0;
	if (!Face.Normalize())
	{
		Face = FVector::ForwardVector;
	}
	return Face;
}

FVector ASlimePoopActor::ResolveSpawnOrigin(const AActor* Producer, const FVector& Face)
{
	FVector Origin = Producer->GetActorLocation();
	float Radius = 32.f;
	if (const ACharacter* Character = Cast<ACharacter>(Producer))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			Origin = Capsule->GetComponentLocation();
			Radius = Capsule->GetScaledCapsuleRadius();
		}
	}
	constexpr float BehindGap = 20.f;
	Origin -= Face * (Radius + BehindGap);
	return Origin;
}

void ASlimePoopActor::ApplyMeshAndScale()
{
	if (!BodyMesh)
	{
		return;
	}

	UStaticMesh* Mesh = BodyMesh->GetStaticMesh();
	if (!Mesh)
	{
		Mesh = PoopMeshPath.LoadSynchronous();
		if (!Mesh)
		{
			Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Characters/Slime/Meshes/SM_SlimePoop/StaticMeshes/SM_SlimePoop.SM_SlimePoop"));
		}
		if (Mesh)
		{
			BodyMesh->SetStaticMesh(Mesh);
		}
	}
	if (!Mesh)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimePoop: missing SM_SlimePoop"));
		return;
	}

	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const float SourceHeight = FMath::Max(Bounds.BoxExtent.Z * 2.f, 1.f);
	const float Scale = TargetHeightCm / SourceHeight;
	BodyMesh->SetRelativeScale3D(FVector(Scale));

	const float Bottom = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale;
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -Bottom));
}

bool ASlimePoopActor::IsPoopActor(const AActor* Actor)
{
	return Actor && Actor->IsA(ASlimePoopActor::StaticClass());
}

void ASlimePoopActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (Age >= LifetimeSeconds)
	{
		Destroy();
	}
}

void ASlimePoopActor::RegisterLive()
{
	LivePiles.RemoveAll([](const TWeakObjectPtr<ASlimePoopActor>& Ptr) { return !Ptr.IsValid(); });
	LivePiles.AddUnique(this);
}

void ASlimePoopActor::UnregisterLive()
{
	LivePiles.RemoveAll([this](const TWeakObjectPtr<ASlimePoopActor>& Ptr)
	{
		return !Ptr.IsValid() || Ptr.Get() == this;
	});
}

void ASlimePoopActor::EnforceBudget()
{
	LivePiles.RemoveAll([](const TWeakObjectPtr<ASlimePoopActor>& Ptr) { return !Ptr.IsValid(); });
	const int32 Cap = FMath::Max(MaxConcurrent, 1);
	while (LivePiles.Num() > Cap)
	{
		if (ASlimePoopActor* Oldest = LivePiles[0].Get())
		{
			LivePiles.RemoveAt(0);
			Oldest->Destroy();
		}
		else
		{
			LivePiles.RemoveAt(0);
		}
	}
}
