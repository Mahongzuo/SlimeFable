// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "ProceduralMeshComponent.h"
#include "SlimeAbilityComponent.h"
#include "SlimeBodyComponent.h"
#include "SlimeElementComponent.h"

ASlimeCharacter::ASlimeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// A 40 cm tall dome, not a humanoid.
	GetCapsuleComponent()->InitCapsuleSize(32.f, 26.f);

	GetCharacterMovement()->JumpZVelocity = 460.f;
	GetCharacterMovement()->AirControl = 0.4f;
	GetCharacterMovement()->MaxWalkSpeed = 420.f;
	GetCharacterMovement()->GravityScale = 1.6f;
	GetCharacterMovement()->BrakingDecelerationWalking = 1600.f;
	// Scaled to the capsule: a knee high step is a wall to something this small.
	GetCharacterMovement()->MaxStepHeight = 22.f;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 420.f, 0.f);

	// The camera sits low and close so the deformation stays readable.
	if (USpringArmComponent* Boom = GetCameraBoom())
	{
		Boom->TargetArmLength = 260.f;
		Boom->SocketOffset = FVector(0.f, 0.f, 40.f);
		Boom->bEnableCameraLag = true;
		Boom->CameraLagSpeed = 12.f;
	}

	// The skeletal mesh stays for future accessories but the body itself is the procedural
	// surface, so nothing on it should render or tick.
	if (USkeletalMeshComponent* SkeletalBody = GetMesh())
	{
		SkeletalBody->SetHiddenInGame(true);
		SkeletalBody->SetVisibility(false);
		SkeletalBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SkeletalBody->SetComponentTickEnabled(false);
	}

	SurfaceMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SlimeSurface"));
	SurfaceMesh->SetupAttachment(RootComponent);
	// Marching cubes output is world space. Keeping the component at the origin with an
	// absolute transform means the mesh never has to be re-based as the character moves,
	// which also removes the one frame of swim between a mesh rebuild and the next move.
	SurfaceMesh->SetUsingAbsoluteLocation(true);
	SurfaceMesh->SetUsingAbsoluteRotation(true);
	SurfaceMesh->SetUsingAbsoluteScale(true);
	// Topology changes every rebuild, so cooking collision for it would cost more than the
	// whole simulation. The capsule blocks movement and particles answer gameplay queries.
	SurfaceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SurfaceMesh->bUseComplexAsSimpleCollision = false;
	SurfaceMesh->SetGenerateOverlapEvents(false);
	SurfaceMesh->bUseAsyncCooking = false;
	SurfaceMesh->SetCastShadow(true);

	SlimeBody = CreateDefaultSubobject<USlimeBodyComponent>(TEXT("SlimeBody"));
	SlimeBody->SetSurfaceMesh(SurfaceMesh);

	SlimeElement = CreateDefaultSubobject<USlimeElementComponent>(TEXT("SlimeElement"));
	SlimeAbilities = CreateDefaultSubobject<USlimeAbilityComponent>(TEXT("SlimeAbilities"));
}

void ASlimeCharacter::BeginPlay()
{
	if (SurfaceMesh)
	{
		SurfaceMesh->SetWorldTransform(FTransform::Identity);
	}

	// Runs before the body component's BeginPlay creates its section, which needs the mesh
	// transform to already be identity.
	Super::BeginPlay();
	PrimaryActorTick.bCanEverTick = true;
}

void ASlimeCharacter::Tick(float DeltaSeconds)
{
	LastVelocity = GetVelocity();
	Super::Tick(DeltaSeconds);
}

void ASlimeCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (SlimeAbilities)
	{
		SlimeAbilities->RegisterMappingContext();
	}
}

void ASlimeCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (SlimeBody)
	{
		SlimeBody->ApplyLandingSquash(FMath::Abs(LastVelocity.Z));
	}
}

void ASlimeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (SlimeAbilities)
		{
			SlimeAbilities->BindInput(EnhancedInput);
		}
	}
}
