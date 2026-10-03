#include "MiniGame/SlimeRunnerActors.h"
#include "MiniGame/SlimeRunnerDirector.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

namespace SlimeRunnerActorsPrivate
{
	UBoxComponent* MakeTrigger(AActor* Owner, FName Name, const FVector& Extent)
	{
		UBoxComponent* Box = Owner->CreateDefaultSubobject<UBoxComponent>(Name);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionProfileName(TEXT("Trigger"));
		Box->SetGenerateOverlapEvents(true);
		Box->SetMobility(EComponentMobility::Movable);
		return Box;
	}

	UStaticMeshComponent* MakeVisual(AActor* Owner, USceneComponent* Parent)
	{
		UStaticMeshComponent* Mesh = Owner->CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
		Mesh->SetupAttachment(Parent);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetCanEverAffectNavigation(false);
		return Mesh;
	}
}

ASlimeRunnerDirector* ASlimeRunnerPiece::GetDirector()
{
	if (!CachedDirector.IsValid()) CachedDirector = ASlimeRunnerDirector::Find(this);
	return CachedDirector.Get();
}

bool ASlimeRunnerPiece::IsRunnerPlayer(AActor* Other)
{
	ASlimeRunnerDirector* Director = GetDirector();
	return Director && Director->IsModeActive() && Other && Other == Director->GetPlayerCharacter();
}

// ---------------------------------------------------------------- Pickup

ASlimeRunnerPickup::ASlimeRunnerPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->SetSphereRadius(70.f);
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	Trigger->SetMobility(EComponentMobility::Movable);
	RootComponent = Trigger;
	Mesh = SlimeRunnerActorsPrivate::MakeVisual(this, Trigger);
}

void ASlimeRunnerPickup::BeginPlay()
{
	Super::BeginPlay();
	MeshHome = Mesh->GetRelativeLocation();
	Clock = FMath::FRand() * 6.f;
	if (ASlimeRunnerDirector* Director = GetDirector()) Director->RegisterPickup();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ASlimeRunnerPickup::HandleOverlap);
}

void ASlimeRunnerPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	Mesh->SetRelativeLocation(MeshHome + FVector(0.f, 0.f, FMath::Sin(Clock * 2.4f) * BobHeight));
	if (SpinDegreesPerSecond != 0.f)
	{
		Mesh->AddRelativeRotation(FRotator(0.f, SpinDegreesPerSecond * DeltaSeconds, 0.f));
	}
}

void ASlimeRunnerPickup::HandleOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (bTaken || !IsRunnerPlayer(Other)) return;
	bTaken = true;
	GetDirector()->CollectFlag(Value);
	if (CollectFX) UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, CollectFX, GetActorLocation());
	if (CollectSound) UGameplayStatics::PlaySoundAtLocation(this, CollectSound, GetActorLocation());
	Destroy();
}

// ---------------------------------------------------------------- Hazard

ASlimeRunnerHazard::ASlimeRunnerHazard()
{
	PrimaryActorTick.bCanEverTick = true;
	Box = SlimeRunnerActorsPrivate::MakeTrigger(this, TEXT("Box"), FVector(45.f, 45.f, 40.f));
	RootComponent = Box;
	Mesh = SlimeRunnerActorsPrivate::MakeVisual(this, Box);
}

void ASlimeRunnerHazard::BeginPlay()
{
	Super::BeginPlay();
	Home = GetActorLocation();
	MeshHome = Mesh->GetRelativeLocation();
	Clock = FMath::FRand() * 6.f;
	ASlimeRunnerDirector* Director = GetDirector();
	if (bHardWeeksOnly && (!Director || Director->GetWeekIndex() < 2))
	{
		Destroy();
		return;
	}
	Box->OnComponentBeginOverlap.AddDynamic(this, &ASlimeRunnerHazard::HandleOverlap);
}

void ASlimeRunnerHazard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDead) return;
	ASlimeRunnerDirector* Director = GetDirector();
	Clock += DeltaSeconds;

	if (bFallWhenNear)
	{
		const ACharacter* Player = Director ? Director->GetPlayerCharacter() : nullptr;
		if (FallTimer < 0.f && Player && FMath::Abs(Player->GetActorLocation().X - Home.X) < FallTriggerDistance)
		{
			FallTimer = 0.f;
		}
		if (FallTimer >= 0.f)
		{
			FallTimer += DeltaSeconds;
			if (FallTimer < FallDelay)
			{
				Mesh->SetRelativeLocation(MeshHome + FVector(FMath::Sin(Clock * 60.f) * 4.f, 0.f, 0.f));
			}
			else
			{
				FallSpeed += 2200.f * DeltaSeconds;
				AddActorWorldOffset(FVector(0.f, 0.f, -FallSpeed * DeltaSeconds));
				if (GetActorLocation().Z < Home.Z - 3000.f) Destroy();
			}
		}
		return;
	}

	if (!PatrolOffset.IsNearlyZero())
	{
		const float Scale = Director ? Director->GetPatrolSpeedScale() : 1.f;
		Phase += DeltaSeconds * Scale / PatrolSeconds;
		const float Alpha = 0.5f - 0.5f * FMath::Cos(PI * Phase);
		SetActorLocation(Home + GetActorQuat().RotateVector(PatrolOffset) * Alpha);
	}
	if (bStompable)
	{
		const float Squash = FMath::Sin(Clock * 5.f);
		Mesh->SetRelativeScale3D(FVector(1.f + 0.06f * Squash, 1.f + 0.06f * Squash, 1.f - 0.08f * Squash));
	}
}

void ASlimeRunnerHazard::HandleOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (bDead || !IsRunnerPlayer(Other)) return;
	ACharacter* Character = Cast<ACharacter>(Other);
	const float Top = Box->GetComponentLocation().Z + Box->GetScaledBoxExtent().Z * 0.3f;
	const float Feet = Character->GetActorLocation().Z - Character->GetSimpleCollisionHalfHeight();
	if (bStompable && Character->GetVelocity().Z < -50.f && Feet > Top)
	{
		Stomp(Character);
		return;
	}
	GetDirector()->HurtPlayer(this);
}

void ASlimeRunnerHazard::Stomp(ACharacter* Character)
{
	bDead = true;
	Character->LaunchCharacter(FVector(0.f, 0.f, 760.f), false, true);
	if (StompFX) UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, StompFX, GetActorLocation());
	SetActorEnableCollision(false);
	Mesh->SetRelativeScale3D(FVector(1.4f, 1.4f, 0.2f));
	SetLifeSpan(0.25f);
}

// ---------------------------------------------------------------- Checkpoint / KillZone / Finish

ASlimeRunnerCheckpoint::ASlimeRunnerCheckpoint()
{
	Box = SlimeRunnerActorsPrivate::MakeTrigger(this, TEXT("Box"), FVector(60.f, 200.f, 200.f));
	RootComponent = Box;
	Mesh = SlimeRunnerActorsPrivate::MakeVisual(this, Box);
}

void ASlimeRunnerCheckpoint::BeginPlay()
{
	Super::BeginPlay();
	Box->OnComponentBeginOverlap.AddDynamic(this, &ASlimeRunnerCheckpoint::HandleOverlap);
}

void ASlimeRunnerCheckpoint::HandleOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!IsRunnerPlayer(Other)) return;
	GetDirector()->SetCheckpoint(GetActorLocation());
}

ASlimeRunnerKillZone::ASlimeRunnerKillZone()
{
	Box = SlimeRunnerActorsPrivate::MakeTrigger(this, TEXT("Box"), FVector(200.f, 400.f, 100.f));
	RootComponent = Box;
}

void ASlimeRunnerKillZone::BeginPlay()
{
	Super::BeginPlay();
	Box->OnComponentBeginOverlap.AddDynamic(this, &ASlimeRunnerKillZone::HandleOverlap);
}

void ASlimeRunnerKillZone::HandleOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!IsRunnerPlayer(Other)) return;
	GetDirector()->RespawnPlayer();
}

ASlimeRunnerFinishZone::ASlimeRunnerFinishZone()
{
	Box = SlimeRunnerActorsPrivate::MakeTrigger(this, TEXT("Box"), FVector(100.f, 400.f, 400.f));
	RootComponent = Box;
}

void ASlimeRunnerFinishZone::BeginPlay()
{
	Super::BeginPlay();
	Box->OnComponentBeginOverlap.AddDynamic(this, &ASlimeRunnerFinishZone::HandleOverlap);
}

void ASlimeRunnerFinishZone::HandleOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!IsRunnerPlayer(Other)) return;
	GetDirector()->BeginFinale();
}

// ---------------------------------------------------------------- JumpPad / Mover

ASlimeRunnerJumpPad::ASlimeRunnerJumpPad()
{
	Box = SlimeRunnerActorsPrivate::MakeTrigger(this, TEXT("Box"), FVector(70.f, 120.f, 30.f));
	RootComponent = Box;
	Mesh = SlimeRunnerActorsPrivate::MakeVisual(this, Box);
}

void ASlimeRunnerJumpPad::BeginPlay()
{
	Super::BeginPlay();
	Box->OnComponentBeginOverlap.AddDynamic(this, &ASlimeRunnerJumpPad::HandleOverlap);
}

void ASlimeRunnerJumpPad::HandleOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!IsRunnerPlayer(Other)) return;
	Cast<ACharacter>(Other)->LaunchCharacter(FVector(0.f, 0.f, LaunchZ), false, true);
	Mesh->SetRelativeScale3D(FVector(1.f, 1.f, 0.6f));
	FTimerHandle Restore;
	GetWorldTimerManager().SetTimer(Restore, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		Mesh->SetRelativeScale3D(FVector::OneVector);
	}), 0.15f, false);
}

ASlimeRunnerMover::ASlimeRunnerMover()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	RootComponent = Mesh;
}

void ASlimeRunnerMover::BeginPlay()
{
	Super::BeginPlay();
	Home = GetActorLocation();
	Phase = StartPhase * 2.f;
}

void ASlimeRunnerMover::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Phase += DeltaSeconds / MoveSeconds;
	const float Alpha = 0.5f - 0.5f * FMath::Cos(PI * Phase);
	SetActorLocation(Home + GetActorQuat().RotateVector(MoveOffset) * Alpha);
}
