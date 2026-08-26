// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "ProceduralMeshComponent.h"
#include "SlimeAbilityComponent.h"
#include "SlimeBodyComponent.h"
#include "SlimeClingComponent.h"
#include "SlimeCharacterMovementComponent.h"
#include "SlimeCombatComponent.h"
#include "SlimeElementComponent.h"
#include "Slime/SlimeElementProgressSubsystem.h"
#include "SlimeHealthComponent.h"
#include "SlimeCombatHUDWidget.h"
#include "Quest/QuestSubsystem.h"
#include "SlimeDodgeComponent.h"
#include "SlimeDevourComponent.h"
#include "SlimePathSwordComponent.h"
#include "SlimeFluidNinjaContactComponent.h"
#include "SlimeLockOnComponent.h"
#include "SlimeMorphComponent.h"
#include "SlimeSpringArmComponent.h"
#include "SlimeStatusComponent.h"
#include "SlimeTrailComponent.h"
#include "Inventory/SlimePlacementComponent.h"
#include "Inventory/SlimeInteractComponent.h"
#include "Settings/SlimeCheatComponent.h"
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
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Settings/SlimeAudioPlay.h"

namespace SlimeMoveAudio
{
	static const TCHAR* DefaultFootstep = TEXT("/Game/Audio/SFX/Movement/sfx_crawl_01.sfx_crawl_01");
	static const TCHAR* DefaultJump = TEXT("/Game/Audio/SFX/Movement/sfx_jump_01.sfx_jump_01");
	static const TCHAR* DefaultHitTaken = TEXT("/Game/Audio/SFX/Combat/sfx_hit_01.sfx_hit_01");

	USoundBase* Resolve(const TSoftObjectPtr<USoundBase>& Soft, const TCHAR* Fallback)
	{
		if (!Soft.IsNull())
		{
			if (USoundBase* Loaded = Soft.LoadSynchronous())
			{
				return Loaded;
			}
		}
		return LoadObject<USoundBase>(nullptr, Fallback);
	}
}

ASlimeCharacter::ASlimeCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer
		.SetDefaultSubobjectClass<USlimeCharacterMovementComponent>(ACharacter::CharacterMovementComponentName)
		.SetDefaultSubobjectClass<USlimeSpringArmComponent>(TEXT("CameraBoom")))
{
	PrimaryActorTick.bCanEverTick = true;
	FootstepSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(SlimeMoveAudio::DefaultFootstep));
	JumpSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(SlimeMoveAudio::DefaultJump));
	HitTakenSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(SlimeMoveAudio::DefaultHitTaken));

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
		Boom->SocketOffset = FVector(0.f, 0.f, 12.f);
		Boom->bEnableCameraLag = true;
		Boom->CameraLagSpeed = 12.f;
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		// Needed for FluidNinja Live Activation (PawnInsideActivationBounds).
		Capsule->SetGenerateOverlapEvents(true);
		// Enemies must not treat the short slime capsule as a step-up ledge.
		Capsule->CanCharacterStepUpOn = ECB_No;
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
	// Visible jelly does not cast ï¿½?the opaque ShadowMesh proxy owns ground shadows.
	SurfaceMesh->SetCastShadow(false);
	SurfaceMesh->bCastDynamicShadow = false;
	SurfaceMesh->bCastVolumetricTranslucentShadow = false;
	SurfaceMesh->bCastContactShadow = false;
	// Avoid CSM speckles on the translucent shell (proxy still shades the ground).
	// UE 5.8 dropped SetReceiveShadows; bReceiveMobileCSMShadows is the remaining receive toggle.
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
	// Occlusion-edge silhouette: Unlit + DisableDepthTest, opacity gated by SceneDepth.
	// Stays visible so walls can reveal the Fresnel rim; transparent when unoccluded.
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
	SlimeCheat = CreateDefaultSubobject<USlimeCheatComponent>(TEXT("SlimeCheat"));
	SlimeDodge = CreateDefaultSubobject<USlimeDodgeComponent>(TEXT("SlimeDodge"));
	SlimeDevour = CreateDefaultSubobject<USlimeDevourComponent>(TEXT("SlimeDevour"));
	SlimeVehicle = CreateDefaultSubobject<USlimeVehicleComponent>(TEXT("SlimeVehicle"));
	SlimeMorph = CreateDefaultSubobject<USlimeMorphComponent>(TEXT("SlimeMorph"));
	PathSword = CreateDefaultSubobject<USlimePathSwordComponent>(TEXT("PathSword"));
	SlimeFluidNinjaContact = CreateDefaultSubobject<USlimeFluidNinjaContactComponent>(TEXT("SlimeFluidNinjaContact"));

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
	ApplyCameraViewLimits();
	if (SlimeHealth)
	{
		SlimeHealth->OnDied.AddDynamic(this, &ASlimeCharacter::HandleDeath);
	}

	if (IsPlayerControlled() && SlimeElement)
	{
		if (USlimeElementProgressSubsystem* Progress = USlimeElementProgressSubsystem::Get(this))
		{
			SlimeElement->SetElement(Progress->GetSavedElement(), true);
		}
	}
}

void ASlimeCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	ApplyCameraViewLimits();
	if (Cast<APlayerController>(NewController) && SlimeElement)
	{
		if (USlimeElementProgressSubsystem* Progress = USlimeElementProgressSubsystem::Get(this))
		{
			SlimeElement->SetElement(Progress->GetSavedElement(), true);
		}
	}
}

void ASlimeCharacter::ApplyCameraViewLimits()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	if (APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
	{
		CamMgr->ViewPitchMin = ViewPitchMin;
		CamMgr->ViewPitchMax = ViewPitchMax;
	}
}

void ASlimeCharacter::Tick(float DeltaSeconds)
{
	LastVelocity = GetVelocity();
	if (IsPlayerControlled())
	{
		PollCustomMoveKeys(DeltaSeconds);
		UpdateSprintSpeed();
	}
	if (SlimeCling)
	{
		SlimeCling->UpdateCling(DeltaSeconds);
	}
	TickFootsteps(DeltaSeconds);
	Super::Tick(DeltaSeconds);
	if (IsPlayerControlled())
	{
		UpdateCameraZoom(DeltaSeconds);
	}
}

void ASlimeCharacter::TickFootsteps(float DeltaSeconds)
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const FVector Horiz = FVector(GetVelocity().X, GetVelocity().Y, 0.f);
	const bool bMovingOnGround =
		Movement && Movement->IsMovingOnGround() && Horiz.SizeSquared() >= 400.f;

	if (!bMovingOnGround)
	{
		FootstepTimer = 0.f;
		bWasMovingForFootstep = false;
		StopFootstepAudio(false);
		return;
	}

	// First step plays immediately so leading silence in the wave is less noticeable.
	if (!bWasMovingForFootstep)
	{
		bWasMovingForFootstep = true;
		FootstepTimer = 0.f;
		PlayFootstepAudio();
		return;
	}

	FootstepTimer += DeltaSeconds;
	if (FootstepTimer < FootstepInterval)
	{
		return;
	}
	FootstepTimer = 0.f;
	PlayFootstepAudio();
}

void ASlimeCharacter::StopFootstepAudio(bool bImmediate)
{
	if (!FootstepAudio)
	{
		return;
	}
	if (bImmediate)
	{
		FootstepAudio->Stop();
	}
	else if (FootstepAudio->IsPlaying())
	{
		FootstepAudio->FadeOut(0.06f, 0.f);
	}
}

void ASlimeCharacter::PlayFootstepAudio()
{
	USoundBase* Sfx = SlimeMoveAudio::Resolve(FootstepSound, SlimeMoveAudio::DefaultFootstep);
	if (!Sfx)
	{
		return;
	}
	StopFootstepAudio(true);
	FootstepAudio = UGameplayStatics::SpawnSoundAttached(
		Sfx,
		GetRootComponent(),
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

void ASlimeCharacter::PlayJumpSound()
{
	if (USoundBase* Sfx = SlimeMoveAudio::Resolve(JumpSound, SlimeMoveAudio::DefaultJump))
	{
		SlimeAudioPlay::PlaySfxAt(this, Sfx, GetActorLocation());
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
					AdjustCameraZoom(-1);
				}
				else if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown))
				{
					AdjustCameraZoom(1);
				}
			}
		}
	}

	Boom->TargetArmLength = FMath::FInterpTo(
		Boom->TargetArmLength,
		DesiredCameraArmLength,
		DeltaSeconds,
		CameraZoomInterpSpeed);

	if (bLockOnFramingActive)
	{
		return;
	}

	// Lower socket when zoomed in so the short slime stays framed (capsule ~40cm tall).
	const float ArmAlpha = FMath::GetMappedRangeValueClamped(
		FVector2D(CameraArmLengthMin, CameraArmLengthMax),
		FVector2D(0.f, 1.f),
		Boom->TargetArmLength);
	const float DesiredSocketZ = FMath::Lerp(8.f, 20.f, ArmAlpha);
	Boom->SocketOffset = FMath::VInterpTo(
		Boom->SocketOffset,
		FVector(0.f, 0.f, DesiredSocketZ),
		DeltaSeconds,
		CameraZoomInterpSpeed);
}

float ASlimeCharacter::AdjustCameraZoom(int32 WheelSteps)
{
	const float MinArm = bLockOnFramingActive
		? FMath::Max(CameraArmLengthMin, LockOnFramingFloorArm)
		: CameraArmLengthMin;
	const float MaxArm = bLockOnFramingActive ? LockOnFramingMaxArm : CameraArmLengthMax;
	DesiredCameraArmLength = FMath::Clamp(
		DesiredCameraArmLength + CameraZoomStep * WheelSteps,
		MinArm,
		MaxArm);
	return DesiredCameraArmLength;
}

void ASlimeCharacter::SetLockOnFramingActive(bool bActive)
{
	bLockOnFramingActive = bActive;
}

void ASlimeCharacter::SetLockOnFramingArm(float FramingFloorArm, float FramingMaxArm)
{
	LockOnFramingFloorArm = FMath::Max(FramingFloorArm, CameraArmLengthMin);
	LockOnFramingMaxArm = FMath::Max(FramingMaxArm, LockOnFramingFloorArm);
}

void ASlimeCharacter::SetDesiredCameraArmLengthClamped(float Length)
{
	const float MinArm = bLockOnFramingActive
		? FMath::Max(CameraArmLengthMin, LockOnFramingFloorArm)
		: CameraArmLengthMin;
	const float MaxArm = bLockOnFramingActive ? LockOnFramingMaxArm : CameraArmLengthMax;
	DesiredCameraArmLength = FMath::Clamp(Length, MinArm, MaxArm);
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

void ASlimeCharacter::UpdateSprintSpeed()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!Movement || !PC)
	{
		return;
	}

	bool bSprint = false;
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (const USlimeInputSettings* InputSettings = GI->GetSubsystem<USlimeInputSettings>())
			{
				bSprint = InputSettings->IsKeyDown(PC, ESlimeInputAction::Sprint);
			}
		}
	}
	if (!bSprint)
	{
		bSprint = PC->IsInputKeyDown(EKeys::LeftShift);
	}

	float StatusMul = 1.f;
	if (SlimeStatus)
	{
		StatusMul = SlimeStatus->GetMoveSpeedMul();
	}
	const float SprintMul = bSprint ? SprintSpeedMul : 1.f;
	Movement->MaxWalkSpeed = BaseWalkSpeed * StatusMul * SprintMul;
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

void ASlimeCharacter::SetMorphParked(bool bParked)
{
	SetActorHiddenInGame(bParked);
	SetActorEnableCollision(!bParked);

	// The shadow proxy is already hidden-in-game and casts anyway (bCastHiddenShadow), so the
	// actor-level hide does not stop it. This has to go through the body component: its surface
	// rebuild re-asserts the cast flags every frame and would stomp a direct write here.
	if (SlimeBody)
	{
		SlimeBody->SetShadowCastSuppressed(bParked);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		if (bParked)
		{
			// Otherwise the hidden capsule keeps coasting on whatever velocity it had.
			Movement->DisableMovement();
		}
		else
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}
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
	PlayJumpSound();

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
	if (!SlimeHealth)
	{
		return;
	}
	const float Applied = SlimeHealth->ApplyDamage(Damage, DamageCauser, DamageLocation, DamageImpulse);
	if (Applied > 0.f)
	{
		if (USoundBase* Sfx = SlimeMoveAudio::Resolve(HitTakenSound, SlimeMoveAudio::DefaultHitTaken))
		{
			SlimeAudioPlay::PlaySfxAt(this, Sfx, GetActorLocation());
		}
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
