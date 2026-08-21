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
#include "SlimeClingComponent.h"
#include "SlimeCombatComponent.h"
#include "SlimeElementComponent.h"
#include "SlimeHealthComponent.h"
#include "SlimeCombatHUDWidget.h"
#include "Quest/QuestSubsystem.h"
#include "SlimeDodgeComponent.h"
#include "SlimeDevourComponent.h"
#include "SlimeLockOnComponent.h"
#include "SlimeStatusComponent.h"
#include "SlimeTrailComponent.h"
#include "Inventory/SlimePlacementComponent.h"
#include "Inventory/SlimeInteractComponent.h"
#include "SlimeVehicleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Settings/SlimeInputSettings.h"
#include "Settings/SlimeInputTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

ASlimeCharacter::ASlimeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// A ~40 cm tall dome, not a humanoid.
	GetCapsuleComponent()->InitCapsuleSize(32.f, 20.f);

	// ACharacter::JumpMaxCount is already exposed; default to double jump.
	JumpMaxCount = 2;
	GetCharacterMovement()->JumpZVelocity = JumpZVelocity;
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
		Boom->TargetArmLength = CameraArmLengthDefault;
		Boom->SocketOffset = FVector(0.f, 0.f, 40.f);
		Boom->ProbeSize = 8.f;
		Boom->bEnableCameraLag = true;
		Boom->CameraLagSpeed = 12.f;
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	DesiredCameraArmLength = CameraArmLengthDefault;

	// The skeletal mesh stays for future accessories but the body itself is the procedural
	// surface, so nothing on it should render, tick, or cast shadows into the jelly.
	if (USkeletalMeshComponent* SkeletalBody = GetMesh())
	{
		SkeletalBody->SetHiddenInGame(true);
		SkeletalBody->SetVisibility(false);
		SkeletalBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SkeletalBody->SetComponentTickEnabled(false);
		SkeletalBody->SetCastShadow(false);
		SkeletalBody->bCastDynamicShadow = false;
		SkeletalBody->bCastContactShadow = false;
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
	// Visible jelly does not cast — the opaque ShadowMesh proxy owns ground shadows.
	SurfaceMesh->SetCastShadow(false);
	SurfaceMesh->bCastDynamicShadow = false;
	SurfaceMesh->bCastVolumetricTranslucentShadow = false;
	SurfaceMesh->bCastContactShadow = false;
	// Avoid CSM speckles on the translucent shell (proxy still shades the ground).
	SurfaceMesh->bReceiveMobileCSMShadows = false;

	ShadowMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SlimeShadow"));
	ShadowMesh->SetupAttachment(RootComponent);
	ShadowMesh->SetUsingAbsoluteLocation(true);
	ShadowMesh->SetUsingAbsoluteRotation(true);
	ShadowMesh->SetUsingAbsoluteScale(true);
	ShadowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShadowMesh->bUseComplexAsSimpleCollision = false;
	ShadowMesh->SetGenerateOverlapEvents(false);
	ShadowMesh->bUseAsyncCooking = false;
	// Hidden from view; only casts a ground shadow (avoids translucent self-shadow noise).
	ShadowMesh->SetHiddenInGame(true);
	ShadowMesh->SetVisibility(false);
	ShadowMesh->bCastHiddenShadow = true;
	ShadowMesh->SetCastShadow(true);
	ShadowMesh->bCastDynamicShadow = true;
	// Contact / volumetric casts paint black noise back onto the translucent SurfaceMesh.
	ShadowMesh->bCastVolumetricTranslucentShadow = false;
	ShadowMesh->bCastContactShadow = false;

	XRayMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SlimeXRay"));
	XRayMesh->SetupAttachment(RootComponent);
	XRayMesh->SetUsingAbsoluteLocation(true);
	XRayMesh->SetUsingAbsoluteRotation(true);
	XRayMesh->SetUsingAbsoluteScale(true);
	XRayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	XRayMesh->bUseComplexAsSimpleCollision = false;
	XRayMesh->SetGenerateOverlapEvents(false);
	XRayMesh->bUseAsyncCooking = false;
	XRayMesh->SetHiddenInGame(false);
	XRayMesh->SetVisibility(true);
	XRayMesh->SetCastShadow(false);
	XRayMesh->bCastDynamicShadow = false;
	XRayMesh->bCastVolumetricTranslucentShadow = false;
	XRayMesh->bCastContactShadow = false;

	SlimeBody = CreateDefaultSubobject<USlimeBodyComponent>(TEXT("SlimeBody"));
	SlimeBody->SetSurfaceMesh(SurfaceMesh);
	SlimeBody->SetShadowMesh(ShadowMesh);
	SlimeBody->SetXRayMesh(XRayMesh);

	SlimeElement = CreateDefaultSubobject<USlimeElementComponent>(TEXT("SlimeElement"));
	SlimeAbilities = CreateDefaultSubobject<USlimeAbilityComponent>(TEXT("SlimeAbilities"));
	SlimeTrail = CreateDefaultSubobject<USlimeTrailComponent>(TEXT("SlimeTrail"));
	SlimeHealth = CreateDefaultSubobject<USlimeHealthComponent>(TEXT("SlimeHealth"));
	SlimeStatus = CreateDefaultSubobject<USlimeStatusComponent>(TEXT("SlimeStatus"));
	SlimeCombat = CreateDefaultSubobject<USlimeCombatComponent>(TEXT("SlimeCombat"));
	SlimeLockOn = CreateDefaultSubobject<USlimeLockOnComponent>(TEXT("SlimeLockOn"));
	SlimeCling = CreateDefaultSubobject<USlimeClingComponent>(TEXT("SlimeCling"));
	SlimePlacement = CreateDefaultSubobject<USlimePlacementComponent>(TEXT("SlimePlacement"));
	SlimeInteract = CreateDefaultSubobject<USlimeInteractComponent>(TEXT("SlimeInteract"));
	SlimeDodge = CreateDefaultSubobject<USlimeDodgeComponent>(TEXT("SlimeDodge"));
	SlimeDevour = CreateDefaultSubobject<USlimeDevourComponent>(TEXT("SlimeDevour"));
	SlimeVehicle = CreateDefaultSubobject<USlimeVehicleComponent>(TEXT("SlimeVehicle"));

	VehicleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VehicleMesh"));
	VehicleMesh->SetupAttachment(RootComponent);
	VehicleMesh->SetRelativeLocation(FVector(0.f, 0.f, -24.f));
	VehicleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VehicleMesh->SetGenerateOverlapEvents(false);
	VehicleMesh->SetHiddenInGame(true);
	VehicleMesh->SetVisibility(false);
	VehicleMesh->SetCastShadow(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> VehicleMeshAsset(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (VehicleMeshAsset.Succeeded())
	{
		VehicleMesh->SetStaticMesh(VehicleMeshAsset.Object);
		VehicleMesh->SetRelativeScale3D(FVector(0.55f, 0.55f, 0.35f));
	}
	if (SlimeVehicle)
	{
		SlimeVehicle->SetVehicleMesh(VehicleMesh);
	}

	if (SlimeHealth)
	{
		SlimeHealth->Team = ESlimeTeam::Player;
		SlimeHealth->bDestroyOnDeath = false;
		SlimeHealth->bRegenOnDeath = false;
	}
}

void ASlimeCharacter::BeginPlay()
{
	if (SurfaceMesh)
	{
		SurfaceMesh->SetWorldTransform(FTransform::Identity);
	}
	if (ShadowMesh)
	{
		ShadowMesh->SetWorldTransform(FTransform::Identity);
		ShadowMesh->SetHiddenInGame(true);
		ShadowMesh->SetVisibility(false);
	}
	if (XRayMesh)
	{
		XRayMesh->SetWorldTransform(FTransform::Identity);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->JumpZVelocity = JumpZVelocity;
	}

	DesiredCameraArmLength = FMath::Clamp(CameraArmLengthDefault, CameraArmLengthMin, CameraArmLengthMax);
	if (USpringArmComponent* Boom = GetCameraBoom())
	{
		Boom->TargetArmLength = DesiredCameraArmLength;
	}

	// Runs before the body component's BeginPlay creates its section, which needs the mesh
	// transform to already be identity.
	Super::BeginPlay();
	PrimaryActorTick.bCanEverTick = true;
	SpawnTransform = GetActorTransform();
	bPlayerDead = false;
	if (SlimeHealth)
	{
		SlimeHealth->OnDied.AddDynamic(this, &ASlimeCharacter::HandleDeath);
	}
}

void ASlimeCharacter::Tick(float DeltaSeconds)
{
	LastVelocity = GetVelocity();
	if (IsPlayerControlled())
	{
		PollCustomMoveKeys(DeltaSeconds);
	}
	if (SlimeCling)
	{
		SlimeCling->UpdateCling(DeltaSeconds);
	}
	Super::Tick(DeltaSeconds);
	if (IsPlayerControlled())
	{
		UpdateCameraZoom(DeltaSeconds);
	}
}

void ASlimeCharacter::UpdateCameraZoom(float DeltaSeconds)
{
	USpringArmComponent* Boom = GetCameraBoom();
	if (!Boom)
	{
		return;
	}

	// Element wheel and G-charge own the scroll wheel.
	const bool bWheelOpen = SlimeAbilities && SlimeAbilities->IsWheelOpen();
	const bool bChargingLaunch = SlimeAbilities && SlimeAbilities->IsChargingLaunch();
	if (!bWheelOpen && !bChargingLaunch)
	{
		if (const APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (PC->IsLocalController())
			{
				if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp))
				{
					DesiredCameraArmLength = FMath::Clamp(
						DesiredCameraArmLength - CameraZoomStep,
						CameraArmLengthMin,
						CameraArmLengthMax);
				}
				else if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown))
				{
					DesiredCameraArmLength = FMath::Clamp(
						DesiredCameraArmLength + CameraZoomStep,
						CameraArmLengthMin,
						CameraArmLengthMax);
				}
			}
		}
	}

	Boom->TargetArmLength = FMath::FInterpTo(
		Boom->TargetArmLength,
		DesiredCameraArmLength,
		DeltaSeconds,
		CameraZoomInterpSpeed);
}

void ASlimeCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (SlimeAbilities)
	{
		SlimeAbilities->RegisterMappingContext();
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (USlimeInputSettings* InputSettings = GI->GetSubsystem<USlimeInputSettings>())
				{
					InputSettings->ApplyEnhancedInputRemaps(PC);
				}
			}
		}
	}
}

void ASlimeCharacter::PollCustomMoveKeys(float DeltaSeconds)
{
	(void)DeltaSeconds;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UGameInstance* GI = World->GetGameInstance();
	USlimeInputSettings* InputSettings = GI ? GI->GetSubsystem<USlimeInputSettings>() : nullptr;
	if (!InputSettings || !InputSettings->UsesCustomMovementKeys())
	{
		return;
	}

	const float Forward =
		(InputSettings->IsKeyDown(PC, ESlimeInputAction::MoveForward) ? 1.f : 0.f)
		- (InputSettings->IsKeyDown(PC, ESlimeInputAction::MoveBack) ? 1.f : 0.f);
	const float Right =
		(InputSettings->IsKeyDown(PC, ESlimeInputAction::MoveRight) ? 1.f : 0.f)
		- (InputSettings->IsKeyDown(PC, ESlimeInputAction::MoveLeft) ? 1.f : 0.f);

	DoMove(Right, Forward);

	if (InputSettings->WasKeyPressed(PC, ESlimeInputAction::Jump))
	{
		Jump();
	}
	if (!InputSettings->IsKeyDown(PC, ESlimeInputAction::Jump))
	{
		StopJumping();
	}
}

void ASlimeCharacter::DoMove(float Right, float Forward)
{
	if (SlimeCling)
	{
		SlimeCling->SetClingMoveInput(Right, Forward);
		if (SlimeCling->IsClinging())
		{
			return;
		}
	}
	Super::DoMove(Right, Forward);
}

void ASlimeCharacter::Jump()
{
	if (SlimeVehicle && SlimeVehicle->IsUsingVehicle())
	{
		return;
	}
	if (SlimeCling && SlimeCling->TryWallJump())
	{
		return;
	}
	Super::Jump();
}

void ASlimeCharacter::Unstuck()
{
	if (SlimeVehicle && SlimeVehicle->IsUsingVehicle())
	{
		SlimeVehicle->ExitVehicle(false);
	}

	if (SlimeCling)
	{
		SlimeCling->TryDetach();
	}
	if (SlimeDevour)
	{
		SlimeDevour->ClosePhantomWheel(false);
		SlimeDevour->AbortDevour(true);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->Velocity = FVector::ZeroVector;
		Movement->SetMovementMode(MOVE_Walking);
	}

	TeleportTo(SpawnTransform.GetLocation(), SpawnTransform.Rotator(), false, true);

	if (SlimeBody)
	{
		SlimeBody->ResetBody();
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

void ASlimeCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	// First jump uses CMC JumpZVelocity (kept in sync with JumpZVelocity). Air jump overrides.
	// JumpCurrentCount lives on ACharacter, not the movement component.
	if (JumpCurrentCount >= 2)
	{
		FVector Velocity = Movement->Velocity;
		Velocity.Z = AirJumpZVelocity;
		Movement->Velocity = Velocity;

		if (SlimeBody)
		{
			SlimeBody->ApplyAirBounce();
		}
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
		if (SlimeCombat)
		{
			SlimeCombat->BindInput(EnhancedInput);
		}
		if (SlimeLockOn)
		{
			SlimeLockOn->BindInput(EnhancedInput);
		}
	}
}

void ASlimeCharacter::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	if (SlimeHealth)
	{
		SlimeHealth->ApplyDamage(Damage, DamageCauser, DamageLocation, DamageImpulse);
	}
}

void ASlimeCharacter::HandleDeath()
{
	if (bPlayerDead)
	{
		return;
	}
	bPlayerDead = true;

	if (SlimeDevour)
	{
		SlimeDevour->ClosePhantomWheel(false);
		SlimeDevour->AbortDevour(true);
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	if (SlimeCombat)
	{
		if (USlimeCombatHUDWidget* HUD = SlimeCombat->GetCombatHUD())
		{
			HUD->SetDeathVisible(true);
		}
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PlayerDeathReloadTimer,
			this,
			&ASlimeCharacter::FinishPlayerDeathReload,
			1.5f,
			false);
	}
}

void ASlimeCharacter::FinishPlayerDeathReload()
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UQuestSubsystem* Quests = GI->GetSubsystem<UQuestSubsystem>())
			{
				Quests->ReloadActiveChapterAfterDeath();
			}
		}
	}
}

void ASlimeCharacter::ApplyHealing(float Healing, AActor* Healer)
{
	if (SlimeHealth)
	{
		SlimeHealth->ApplyHealing(Healing);
	}
}

void ASlimeCharacter::NotifyDanger(const FVector& DangerLocation, AActor* DangerSource)
{
}
