// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeMorphComponent.h"

#include "EnemyCharacter.h"
#include "EnemyCombatComponent.h"
#include "EnemyFighter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "SlimeBodyComponent.h"
#include "SlimeCharacter.h"
#include "SlimeDevourComponent.h"
#include "SlimeElementComponent.h"
#include "SlimeElementTypes.h"
#include "SlimeFable.h"
#include "Settings/SlimeInputSettings.h"
#include "UI/SlimePhantomWheelWidget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

// Material parameter names 鈥?mirror SlimeElementParams in SlimeElementComponent.cpp.
namespace SlimeMorphParams
{
	static const FName GrowProgress(TEXT("GrowProgress"));
	static const FName EdgeSoftness(TEXT("EdgeSoftness"));
	static const FName ShellOpacity(TEXT("ShellOpacity"));
	static const FName BaseColor(TEXT("BaseColor"));
	static const FName SubsurfaceColor(TEXT("SubsurfaceColor"));
	static const FName EmissiveColor(TEXT("EmissiveColor"));
	static const FName RimColor(TEXT("RimColor"));
	static const FName EmissiveIntensity(TEXT("EmissiveIntensity"));
	static const FName Opacity(TEXT("Opacity"));
	static const FName Roughness(TEXT("Roughness"));
	static const FName Refraction(TEXT("Refraction"));
	static const FName FlowSpeed(TEXT("FlowSpeed"));
	static const FName NoiseScale(TEXT("NoiseScale"));
	static const FName RimPower(TEXT("RimPower"));
}

namespace
{
	void CopyCameraRig(const USpringArmComponent* SourceBoom, USpringArmComponent* TargetBoom,
		const UCameraComponent* SourceCamera, UCameraComponent* TargetCamera)
	{
		if (SourceBoom && TargetBoom)
		{
			TargetBoom->TargetArmLength = SourceBoom->TargetArmLength;
			TargetBoom->SocketOffset = SourceBoom->SocketOffset;
			TargetBoom->TargetOffset = SourceBoom->TargetOffset
				+ SourceBoom->GetComponentLocation() - TargetBoom->GetComponentLocation();
			TargetBoom->ProbeSize = SourceBoom->ProbeSize;
			TargetBoom->ProbeChannel = SourceBoom->ProbeChannel;
			TargetBoom->bDoCollisionTest = SourceBoom->bDoCollisionTest;
			TargetBoom->bUsePawnControlRotation = SourceBoom->bUsePawnControlRotation;
			TargetBoom->bInheritPitch = SourceBoom->bInheritPitch;
			TargetBoom->bInheritYaw = SourceBoom->bInheritYaw;
			TargetBoom->bInheritRoll = SourceBoom->bInheritRoll;
			TargetBoom->bEnableCameraLag = SourceBoom->bEnableCameraLag;
			TargetBoom->CameraLagSpeed = SourceBoom->CameraLagSpeed;
			TargetBoom->CameraLagMaxDistance = SourceBoom->CameraLagMaxDistance;
			TargetBoom->bUseCameraLagSubstepping = SourceBoom->bUseCameraLagSubstepping;
			TargetBoom->CameraLagMaxTimeStep = SourceBoom->CameraLagMaxTimeStep;
			TargetBoom->bEnableCameraRotationLag = SourceBoom->bEnableCameraRotationLag;
			TargetBoom->CameraRotationLagSpeed = SourceBoom->CameraRotationLagSpeed;
		}

		if (SourceCamera && TargetCamera)
		{
			TargetCamera->SetFieldOfView(SourceCamera->FieldOfView);
		}
	}

	void RefreshCameraRig(USpringArmComponent* CameraBoom)
	{
		if (!CameraBoom)
		{
			return;
		}

		const bool bLocationLagEnabled = CameraBoom->bEnableCameraLag;
		const bool bRotationLagEnabled = CameraBoom->bEnableCameraRotationLag;
		CameraBoom->bEnableCameraLag = false;
		CameraBoom->bEnableCameraRotationLag = false;
		CameraBoom->TickComponent(0.f, LEVELTICK_All, nullptr);
		CameraBoom->bEnableCameraLag = bLocationLagEnabled;
		CameraBoom->bEnableCameraRotationLag = bRotationLagEnabled;
	}
}

USlimeMorphComponent::USlimeMorphComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USlimeMorphComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		Body = Owner->FindComponentByClass<USlimeBodyComponent>();
		Element = Owner->FindComponentByClass<USlimeElementComponent>();
		Devour = Owner->FindComponentByClass<USlimeDevourComponent>();
	}
}

void USlimeMorphComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (Phase != ESlimeMorphPhase::Idle)
	{
		TickPhase(DeltaTime);
	}
}

APlayerController* USlimeMorphComponent::GetActivePlayerController() const
{
	// While morphed the player controller possesses the morph body, so the slime's own
	// GetController() is null 鈥?ask the morph target first.
	if (MorphTarget)
	{
		if (APlayerController* PC = Cast<APlayerController>(MorphTarget->GetController()))
		{
			return PC;
		}
	}
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		return Cast<APlayerController>(Pawn->GetController());
	}
	return nullptr;
}

void USlimeMorphComponent::TickMorphedKeyInput(float DeltaTime)
{
	APlayerController* PC = GetActivePlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	const USlimeInputSettings* Settings = nullptr;
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			Settings = GI->GetSubsystem<USlimeInputSettings>();
		}
	}

	const bool bDown = Settings
		? Settings->IsKeyDown(PC, ESlimeInputAction::Morph)
		: PC->IsInputKeyDown(EKeys::Z);

	if (bDown && !bMorphedKeyDown)
	{
		bMorphedKeyDown = true;
		BeginUnmorph();
	}
	else if (!bDown)
	{
		bMorphedKeyDown = false;
	}

	ASlimeCharacter* Slime = Cast<ASlimeCharacter>(GetOwner());
	USpringArmComponent* MorphCameraBoom = MorphTarget
		? MorphTarget->FindComponentByClass<USpringArmComponent>()
		: nullptr;
	if (!Slime || !MorphCameraBoom)
	{
		return;
	}

	if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp))
	{
		Slime->AdjustCameraZoom(-1);
	}
	else if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown))
	{
		Slime->AdjustCameraZoom(1);
	}

	const float ArmLength = FMath::FInterpTo(
		MorphCameraBoom->TargetArmLength,
		Slime->GetDesiredCameraArmLength(),
		DeltaTime,
		Slime->CameraZoomInterpSpeed);
	MorphCameraBoom->TargetArmLength = ArmLength;
	if (USpringArmComponent* SlimeCameraBoom = Slime->GetCameraBoom())
	{
		SlimeCameraBoom->TargetArmLength = ArmLength;
	}
}

void USlimeMorphComponent::ToggleMorph()
{
	if (Phase == ESlimeMorphPhase::Idle)
	{
		BeginMorph();
	}
	else if (Phase == ESlimeMorphPhase::Morphed)
	{
		BeginUnmorph();
	}
}

void USlimeMorphComponent::ForceUnmorph(bool bConsumeSlot)
{
	if (Phase == ESlimeMorphPhase::Idle || Phase == ESlimeMorphPhase::Spreading)
	{
		return;
	}

	bConsumeMorphedSlotOnExit |= bConsumeSlot;

	// If the morph body dies before possession, abort the visual sequence and consume only
	// when the caller marked this as a death. The active slot index was captured at start.
	if (Phase == ESlimeMorphPhase::Growing
		|| (Phase == ESlimeMorphPhase::Blending && !bPossessDone))
	{
		if (Body)
		{
			Body->SetSpread(false);
		}
		DestroyMorphTarget();
		UpdateSlimeOpacity(1.f);
		ConsumeMorphedSlotIfRequested();
		EnterPhase(ESlimeMorphPhase::Idle);
		return;
	}

	if (Phase == ESlimeMorphPhase::Morphed || Phase == ESlimeMorphPhase::Blending)
	{
		BeginUnmorph();
	}
	// Unblending/Shrinking/Reforming already own cleanup. The consumption flag remains set
	// until their common completion path processes it.
}

void USlimeMorphComponent::BeginMorph()
{
	if (!GetOwner())
	{
		return;
	}

	Body = GetOwner()->FindComponentByClass<USlimeBodyComponent>();
	Element = GetOwner()->FindComponentByClass<USlimeElementComponent>();
	Devour = GetOwner()->FindComponentByClass<USlimeDevourComponent>();

	if (!Body || !Devour)
	{
		return;
	}

	// Need at least one captured enemy to morph into.
	const TArray<FSlimeDevourCapture>& Slots = Devour->GetPhantomSlots();
	if (Slots.Num() == 0 || !Slots[Devour->GetSelectedPhantomSlot()].IsValidCapture())
	{
		return;
	}

	MorphedSlotIndex = Devour->GetSelectedPhantomSlot();
	bConsumeMorphedSlotOnExit = false;
	bHasCachedSlimeReturnTransform = false;
	CachedSlimeReturnTransform = FTransform::Identity;
	bPossessDone = false;

	if (Body)
	{
		Body->SetSpread(true);
	}

	EnterPhase(ESlimeMorphPhase::Spreading);
}

void USlimeMorphComponent::BeginUnmorph()
{
	CacheUnmorphPoseAndFreezeTarget();
	bPossessDone = false;
	EnterPhase(ESlimeMorphPhase::Unblending);
}

void USlimeMorphComponent::EnterPhase(ESlimeMorphPhase Next)
{
	Phase = Next;
	PhaseElapsed = 0.f;

	if (Next == ESlimeMorphPhase::Growing)
	{
		SpawnMorphTarget();
	}
	else if (Next == ESlimeMorphPhase::Blending)
	{
		// Hand the mesh back to the enemy's own materials and move the slime look into an
		// overlay shell, so the final appearance can never depend on the morph material.
		ApplyOriginalMaterials();
		SetShellActive(true);
	}
	else if (Next == ESlimeMorphPhase::Morphed)
	{
		// No shell while morphed: the model renders purely with its own materials.
		SetShellActive(false);
		// Seed the key state from reality 鈥?if Z is still held from the wheel commit, don't
		// let that same press immediately unmorph.
		bMorphedKeyDown = true;
		if (const APlayerController* PC = GetActivePlayerController())
		{
			const USlimeInputSettings* Settings = nullptr;
			if (const UWorld* World = GetWorld())
			{
				if (const UGameInstance* GI = World->GetGameInstance())
				{
					Settings = GI->GetSubsystem<USlimeInputSettings>();
				}
			}
			bMorphedKeyDown = Settings
				? Settings->IsKeyDown(PC, ESlimeInputAction::Morph)
				: PC->IsInputKeyDown(EKeys::Z);
		}
	}
	else if (Next == ESlimeMorphPhase::Unblending)
	{
		SetShellActive(true);
	}
	else if (Next == ESlimeMorphPhase::Shrinking)
	{
		// Back to a full slime body: drop the shell and put the slime skin on the mesh itself.
		SetShellActive(false);
		ApplySlimeSkin();
	}
	else if (Next == ESlimeMorphPhase::Reforming)
	{
		DestroyMorphTarget();
		if (Body)
		{
			Body->SetSpread(false);
		}
	}
	else if (Next == ESlimeMorphPhase::Idle)
	{
		MorphTarget = nullptr;
		MorphMIDs.Reset();
		ShellMID = nullptr;
		SavedEnemyMaterials.Reset();
		MorphedSlotIndex = INDEX_NONE;
		bConsumeMorphedSlotOnExit = false;
		bHasCachedSlimeReturnTransform = false;
		CachedSlimeReturnTransform = FTransform::Identity;
		bOriginalMaterialsActive = false;
		bShellActive = false;
		bMorphedKeyDown = false;
	}
}

void USlimeMorphComponent::TickPhase(float Dt)
{
	PhaseElapsed += Dt;

	auto PhaseAlpha = [this](float Duration) -> float
	{
		return Duration > 0.f ? FMath::Clamp(PhaseElapsed / Duration, 0.f, 1.f) : 1.f;
	};

	switch (Phase)
	{
	case ESlimeMorphPhase::Spreading:
		if (PhaseElapsed >= SpreadDuration)
		{
			EnterPhase(ESlimeMorphPhase::Growing);
		}
		break;

	case ESlimeMorphPhase::Growing:
	{
		// Mesh wears the slime skin; the reveal mask walks up the model.
		const float Alpha = PhaseAlpha(GrowDuration);
		UpdateMorphMaterial(Alpha, 1.f);
		UpdateSlimeOpacity(1.f - Alpha);
		if (Alpha >= 1.f)
		{
			EnterPhase(ESlimeMorphPhase::Blending);
		}
		break;
	}

	case ESlimeMorphPhase::Blending:
	{
		// Mesh already shows the enemy's real materials; fade the slime shell off of it.
		const float Alpha = PhaseAlpha(BlendDuration);
		UpdateMorphMaterial(1.f, 1.f - Alpha);
		if (!bPossessDone && Alpha >= BlendPossessAlpha)
		{
			PossessEnemy();
		}
		if (Alpha >= 1.f)
		{
			EnterPhase(ESlimeMorphPhase::Morphed);
		}
		break;
	}

	case ESlimeMorphPhase::Morphed:
		// Hold: player controls the enemy. Poll the unmorph key + drive locomotion each tick.
		TickMorphedKeyInput(Dt);
		TickMorphLocomotion(Dt);
		break;

	case ESlimeMorphPhase::Unblending:
	{
		// Fade the slime shell back over the real materials.
		const float Alpha = PhaseAlpha(UnblendDuration);
		UpdateMorphMaterial(1.f, Alpha);
		if (!bPossessDone && Alpha >= UnblendUnpossessAlpha)
		{
			PossessSlime();
		}
		if (Alpha >= 1.f)
		{
			EnterPhase(ESlimeMorphPhase::Shrinking);
		}
		break;
	}

	case ESlimeMorphPhase::Shrinking:
	{
		// Mesh is back on the slime skin; pull the reveal mask down into the puddle.
		const float Alpha = PhaseAlpha(ShrinkDuration);
		UpdateMorphMaterial(1.f - Alpha, 1.f);
		UpdateSlimeOpacity(Alpha);
		if (Alpha >= 1.f)
		{
			EnterPhase(ESlimeMorphPhase::Reforming);
		}
		break;
	}

	case ESlimeMorphPhase::Reforming:
		if (PhaseElapsed >= ReformDuration)
		{
			ConsumeMorphedSlotIfRequested();
			EnterPhase(ESlimeMorphPhase::Idle);
		}
		break;

	default:
		break;
	}
}

void USlimeMorphComponent::SpawnMorphTarget()
{
	if (!Devour || !GetOwner())
	{
		return;
	}

	const TArray<FSlimeDevourCapture>& Slots = Devour->GetPhantomSlots();
	if (!Slots.IsValidIndex(MorphedSlotIndex) || !Slots[MorphedSlotIndex].IsValidCapture())
	{
		return;
	}

	const FSlimeDevourCapture& Capture = Slots[MorphedSlotIndex];
	UClass* SpawnClass = Capture.EnemyClass.Get();
	if (!SpawnClass)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Spawn at the slime's feet.
	ASlimeCharacter* Slime = Cast<ASlimeCharacter>(GetOwner());
	if (!Slime)
	{
		return;
	}

	// Stand the enemy's capsule on the puddle: same XY as the slime, feet on the slime's feet.
	// Its capsule is a different size, so reusing the slime's actor Z would sink or float it.
	const FVector SlimeLocation = Slime->GetActorLocation();
	const float FootZ = SlimeLocation.Z - Slime->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	float EnemyHalfHeight = 0.f;
	if (const ACharacter* EnemyCDO = SpawnClass->GetDefaultObject<ACharacter>())
	{
		if (const UCapsuleComponent* EnemyCapsule = EnemyCDO->GetCapsuleComponent())
		{
			EnemyHalfHeight = EnemyCapsule->GetScaledCapsuleHalfHeight();
		}
	}

	const FVector SpawnLocation(SlimeLocation.X, SlimeLocation.Y, FootZ + EnemyHalfHeight);
	const FTransform SpawnXform(Slime->GetActorRotation(), SpawnLocation, FVector::OneVector);

	// AlwaysSpawn, never Adjust: the slime capsule is still collidable at this point, so an
	// adjusting spawn shoves the enemy off to one side instead of growing out of the puddle.
	AEnemyCharacter* Enemy = World->SpawnActorDeferred<AEnemyCharacter>(SpawnClass, SpawnXform, Slime, Slime,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Enemy)
	{
		return;
	}

	Enemy->InitAsMorphTarget(Slime);
	Enemy->FinishSpawning(SpawnXform);

	MorphTarget = Enemy;

	// Save original materials so Blending can hand the mesh straight back to them.
	SavedEnemyMaterials.Reset();
	MorphMIDs.Reset();
	ShellMID = nullptr;
	bOriginalMaterialsActive = false;
	bShellActive = false;

	USkeletalMeshComponent* SkelMesh = Enemy->GetMesh();
	if (!SkelMesh)
	{
		return;
	}

	const int32 NumMats = SkelMesh->GetNumMaterials();
	for (int32 Idx = 0; Idx < NumMats; ++Idx)
	{
		SavedEnemyMaterials.Add(SkelMesh->GetMaterial(Idx));
	}

	UMaterialInterface* MorphMat = LoadMorphMaterial();
	if (MorphMat)
	{
		// One instance per slot for the Growing/Shrinking skin...
		for (int32 Idx = 0; Idx < NumMats; ++Idx)
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(MorphMat, this))
			{
				MorphMIDs.Add(MID);
			}
		}
		// ...and one more for the Blending/Unblending overlay shell.
		ShellMID = UMaterialInstanceDynamic::Create(MorphMat, this);

		ApplySlimeSkin();
		SyncElementProfileToMorphMaterial();
	}

	// The reveal mask is object-space, so the model can be visible from the start.
	Enemy->SetActorHiddenInGame(false);
	SkelMesh->SetVisibility(true);

	// GrowProgress = 0 鈫?nothing revealed yet.
	UpdateMorphMaterial(0.f, 1.f);
}

void USlimeMorphComponent::ApplySlimeSkin()
{
	if (!MorphTarget)
	{
		return;
	}
	USkeletalMeshComponent* SkelMesh = MorphTarget->GetMesh();
	if (!SkelMesh)
	{
		return;
	}
	for (int32 Idx = 0; Idx < MorphMIDs.Num() && Idx < SkelMesh->GetNumMaterials(); ++Idx)
	{
		if (MorphMIDs[Idx])
		{
			SkelMesh->SetMaterial(Idx, MorphMIDs[Idx]);
		}
	}
	bOriginalMaterialsActive = false;
}

void USlimeMorphComponent::ApplyOriginalMaterials()
{
	if (!MorphTarget || bOriginalMaterialsActive)
	{
		return;
	}
	USkeletalMeshComponent* SkelMesh = MorphTarget->GetMesh();
	if (!SkelMesh)
	{
		return;
	}
	for (int32 Idx = 0; Idx < SavedEnemyMaterials.Num() && Idx < SkelMesh->GetNumMaterials(); ++Idx)
	{
		if (SavedEnemyMaterials[Idx])
		{
			SkelMesh->SetMaterial(Idx, SavedEnemyMaterials[Idx]);
		}
	}
	bOriginalMaterialsActive = true;
}

void USlimeMorphComponent::SetShellActive(bool bActive)
{
	if (!MorphTarget || bShellActive == bActive)
	{
		return;
	}
	USkeletalMeshComponent* SkelMesh = MorphTarget->GetMesh();
	if (!SkelMesh)
	{
		return;
	}
	SkelMesh->SetOverlayMaterial(bActive ? ShellMID.Get() : nullptr);
	bShellActive = bActive;
}

void USlimeMorphComponent::DestroyMorphTarget()
{
	if (MorphTarget)
	{
		// Drop the shell and restore the real materials so the enemy can dissolve properly.
		SetShellActive(false);
		ApplyOriginalMaterials();

		if (UEnemyCombatComponent* EnemyCombat = MorphTarget->GetEnemyCombat())
		{
			EnemyCombat->SetPlayerMorphed(false);
		}

		MorphTarget->Destroy();
		MorphTarget = nullptr;
	}
	MorphMIDs.Reset();
	ShellMID = nullptr;
	SavedEnemyMaterials.Reset();
	bOriginalMaterialsActive = false;
	bShellActive = false;
}

UMaterialInterface* USlimeMorphComponent::LoadMorphMaterial()
{
	if (MorphMaterial)
	{
		return MorphMaterial;
	}

	static const FSoftObjectPath MorphPath(TEXT("/Game/Characters/Slime/Materials/M_SlimeMorph.M_SlimeMorph"));
	MorphMaterial = Cast<UMaterialInterface>(MorphPath.TryLoad());
	if (!MorphMaterial)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeMorphComponent: M_SlimeMorph material not found at %s"), *MorphPath.ToString());
	}
	return MorphMaterial;
}

void USlimeMorphComponent::UpdateMorphMaterial(float GrowProgress, float ShellOpacity)
{
	auto Push = [this, GrowProgress, ShellOpacity](UMaterialInstanceDynamic* MID)
	{
		if (!MID)
		{
			return;
		}
		MID->SetScalarParameterValue(SlimeMorphParams::GrowProgress, GrowProgress);
		MID->SetScalarParameterValue(SlimeMorphParams::EdgeSoftness, GrowEdgeSoftness);
		MID->SetScalarParameterValue(SlimeMorphParams::ShellOpacity, ShellOpacity);
	};

	for (UMaterialInstanceDynamic* MID : MorphMIDs)
	{
		Push(MID);
	}
	Push(ShellMID);

	SyncElementProfileToMorphMaterial();
}

void USlimeMorphComponent::UpdateSlimeOpacity(float Alpha)
{
	if (Element)
	{
		Element->SetOpacityScale(Alpha);
	}
}

void USlimeMorphComponent::PossessEnemy()
{
	if (!MorphTarget || !GetOwner())
	{
		return;
	}

	bPossessDone = true;

	ASlimeCharacter* Slime = Cast<ASlimeCharacter>(GetOwner());
	if (!Slime)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Slime->GetController());
	if (!PC)
	{
		return;
	}
	const FRotator PreservedViewRotation = PC->GetControlRotation();
	USpringArmComponent* MorphCameraBoom = MorphTarget->FindComponentByClass<USpringArmComponent>();
	UCameraComponent* MorphCamera = MorphTarget->FindComponentByClass<UCameraComponent>();
	CopyCameraRig(Slime->GetCameraBoom(), MorphCameraBoom, Slime->GetFollowCamera(), MorphCamera);

	// Park the slime: hidden, no collision, no movement, and no shadow-proxy cast. The actor
	// tick stays ON — this component lives on the slime and must keep running the phase
	// machine and polling the unmorph key while the player drives the morph body.
	Slime->SetMorphParked(true);

	PC->Possess(MorphTarget);
	PC->SetControlRotation(PreservedViewRotation);
	RefreshCameraRig(MorphCameraBoom);
	// Keys held across a possess swap never deliver their release to the new pawn, which
	// leaves the movement axis stuck and the character walking off on its own.
	PC->FlushPressedKeys();

	if (UEnemyCombatComponent* EnemyCombat = MorphTarget->GetEnemyCombat())
	{
		EnemyCombat->SetPlayerMorphed(true);
	}
}

void USlimeMorphComponent::PossessSlime()
{
	if (!MorphTarget || !GetOwner())
	{
		return;
	}

	bPossessDone = true;

	ASlimeCharacter* Slime = Cast<ASlimeCharacter>(GetOwner());
	if (!Slime)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(MorphTarget->GetController());
	if (!PC)
	{
		return;
	}
	const FRotator PreservedViewRotation = PC->GetControlRotation();

	if (UEnemyCombatComponent* EnemyCombat = MorphTarget->GetEnemyCombat())
	{
		EnemyCombat->SetPlayerMorphed(false);
	}

	if (!bHasCachedSlimeReturnTransform)
	{
		CacheUnmorphPoseAndFreezeTarget();
	}
	const FVector TargetLocation = bHasCachedSlimeReturnTransform
		? CachedSlimeReturnTransform.GetLocation()
		: MorphTarget->GetActorLocation();
	const FRotator TargetRotation = bHasCachedSlimeReturnTransform
		? CachedSlimeReturnTransform.Rotator()
		: MorphTarget->GetActorRotation();

	// The morph body stopped blocking this spot when unmorph began. Restore the slime's
	// collision, then resolve against world geometry without colliding with its old body.
	Slime->SetMorphParked(false);
	if (!Slime->TeleportTo(TargetLocation, TargetRotation, false, false))
	{
		UE_LOG(LogSlimeFable, Warning,
			TEXT("SlimeMorphComponent: collision-safe return failed at %s; forcing cached morph position."),
			*TargetLocation.ToCompactString());
		Slime->TeleportTo(TargetLocation, TargetRotation, false, true);
	}

	// The solver works in world space, so its particles are still pooled back at the morph
	// origin. Without a reseed the blob visibly flies across the level to catch up.
	if (Body)
	{
		Body->ResetBody();
		Body->SetSpread(true);
	}

	PC->Possess(Slime);
	PC->SetControlRotation(PreservedViewRotation);
	RefreshCameraRig(Slime->GetCameraBoom());
	PC->FlushPressedKeys();
}

void USlimeMorphComponent::CacheUnmorphPoseAndFreezeTarget()
{
	if (!MorphTarget)
	{
		return;
	}

	const FVector MorphLocation = MorphTarget->GetActorLocation();
	float MorphFootZ = MorphLocation.Z;
	if (const UCapsuleComponent* MorphCapsule = MorphTarget->GetCapsuleComponent())
	{
		MorphFootZ -= MorphCapsule->GetScaledCapsuleHalfHeight();
	}

	const ASlimeCharacter* Slime = Cast<ASlimeCharacter>(GetOwner());
	const float RestHalfHeight = Body
		? Body->DefaultCapsuleHalfHeight
		: (Slime && Slime->GetCapsuleComponent()
			? Slime->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
			: 0.f);
	const FVector ReturnLocation(MorphLocation.X, MorphLocation.Y, MorphFootZ + RestHalfHeight);
	CachedSlimeReturnTransform = FTransform(MorphTarget->GetActorRotation(), ReturnLocation, FVector::OneVector);
	bHasCachedSlimeReturnTransform = true;

	if (UCharacterMovementComponent* Movement = MorphTarget->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->Velocity = FVector::ZeroVector;
		Movement->DisableMovement();
	}
	MorphTarget->SetActorEnableCollision(false);
}

void USlimeMorphComponent::ConsumeMorphedSlotIfRequested()
{
	if (!bConsumeMorphedSlotOnExit || MorphedSlotIndex == INDEX_NONE || !Devour)
	{
		return;
	}

	Devour->ConsumePhantomSlot(MorphedSlotIndex);
	MorphedSlotIndex = INDEX_NONE;
	bConsumeMorphedSlotOnExit = false;
}

void USlimeMorphComponent::SyncElementProfileToMorphMaterial()
{
	if (!Element)
	{
		return;
	}

	const FSlimeElementProfile Profile = Element->GetCurrentProfile();
	auto Push = [&Profile](UMaterialInstanceDynamic* MID)
	{
		if (!MID)
		{
			return;
		}
		MID->SetVectorParameterValue(SlimeMorphParams::BaseColor, Profile.BaseColor);
		MID->SetVectorParameterValue(SlimeMorphParams::SubsurfaceColor, Profile.SubsurfaceColor);
		MID->SetVectorParameterValue(SlimeMorphParams::EmissiveColor, Profile.EmissiveColor);
		MID->SetVectorParameterValue(SlimeMorphParams::RimColor, Profile.RimColor);
		MID->SetScalarParameterValue(SlimeMorphParams::EmissiveIntensity, Profile.EmissiveIntensity);
		MID->SetScalarParameterValue(SlimeMorphParams::Opacity, Profile.Opacity);
		MID->SetScalarParameterValue(SlimeMorphParams::Roughness, Profile.Roughness);
		MID->SetScalarParameterValue(SlimeMorphParams::Refraction, Profile.Refraction);
		MID->SetScalarParameterValue(SlimeMorphParams::FlowSpeed, Profile.FlowSpeed);
		MID->SetScalarParameterValue(SlimeMorphParams::NoiseScale, Profile.NoiseScale);
		MID->SetScalarParameterValue(SlimeMorphParams::RimPower, Profile.RimPower);
	};

	for (UMaterialInstanceDynamic* MID : MorphMIDs)
	{
		Push(MID);
	}
	Push(ShellMID);
}

void USlimeMorphComponent::TickMorphLocomotion(float Dt)
{
	if (!MorphTarget)
	{
		return;
	}

	// Only single-node-anim enemies need manual locomotion 鈥?AnimBP-driven enemies handle it themselves.
	if (!MorphTarget->UsesSingleNodeAnims())
	{
		return;
	}

	AEnemyFighter* Fighter = Cast<AEnemyFighter>(MorphTarget);
	if (!Fighter)
	{
		return;
	}

	UCharacterMovementComponent* Move = MorphTarget->GetCharacterMovement();
	if (!Move)
	{
		return;
	}

	const float Speed = Move->Velocity.Size2D();
	const bool bShouldWalk = Speed > 10.f;

	if (bShouldWalk && !bMorphWalkPlaying)
	{
		if (UAnimMontage* Walk = Fighter->WalkMontage.LoadSynchronous())
		{
			Fighter->PlayMeshAnimation(Walk, true);
			bMorphWalkPlaying = true;
			MorphIdleTimer = 0.f;
		}
	}
	else if (!bShouldWalk && bMorphWalkPlaying)
	{
		Fighter->StopMeshAnimation();
		bMorphWalkPlaying = false;
		MorphIdleTimer = 0.f;

		// Play a random idle montage when stopping.
		if (Fighter->IdleMontages.Num() > 0)
		{
			const int32 Index = FMath::RandRange(0, Fighter->IdleMontages.Num() - 1);
			if (UAnimMontage* Idle = Fighter->IdleMontages[Index].LoadSynchronous())
			{
				Fighter->PlayMeshAnimation(Idle, false);
			}
		}
	}
	else if (!bShouldWalk && !bMorphWalkPlaying)
	{
		// After a while standing, play another random idle.
		MorphIdleTimer += Dt;
		if (MorphIdleTimer > 4.f && Fighter->IdleMontages.Num() > 0)
		{
			MorphIdleTimer = 0.f;
			const int32 Index = FMath::RandRange(0, Fighter->IdleMontages.Num() - 1);
			if (UAnimMontage* Idle = Fighter->IdleMontages[Index].LoadSynchronous())
			{
				Fighter->PlayMeshAnimation(Idle, false);
			}
		}
	}
}

bool USlimeMorphComponent::TryOpenMorphWheel()
{
	if (bMorphWheelOpen || !Devour || Devour->GetPhantomSlots().Num() == 0)
	{
		return false;
	}

	APlayerController* PC = nullptr;
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		PC = Cast<APlayerController>(Pawn->GetController());
	}
	if (!PC)
	{
		return false;
	}

	if (!MorphWheelWidget)
	{
		MorphWheelWidget = CreateWidget<USlimePhantomWheelWidget>(PC, USlimePhantomWheelWidget::StaticClass());
		if (!MorphWheelWidget)
		{
			return false;
		}
	}

	bMorphWheelOpen = true;
	MorphWheelWidget->SetSlots(Devour->GetPhantomSlots(), Devour->GetSelectedPhantomSlot(), Devour->GetPhantomSlotCapacity());
	if (!MorphWheelWidget->IsInViewport())
	{
		MorphWheelWidget->AddToViewport(50);
	}
	MorphWheelWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (bSlowTimeWhileWheelOpen)
	{
		if (UWorld* World = GetWorld())
		{
			SavedTimeDilation = UGameplayStatics::GetGlobalTimeDilation(World);
			UGameplayStatics::SetGlobalTimeDilation(World, WheelTimeDilation);
		}
	}
	return true;
}

void USlimeMorphComponent::CloseMorphWheel(bool bCommit)
{
	if (!bMorphWheelOpen)
	{
		return;
	}
	bMorphWheelOpen = false;
	if (MorphWheelWidget)
	{
		MorphWheelWidget->SetVisibility(ESlateVisibility::Collapsed);
		MorphWheelWidget->RemoveFromParent();
	}
	if (bSlowTimeWhileWheelOpen)
	{
		if (UWorld* World = GetWorld())
		{
			UGameplayStatics::SetGlobalTimeDilation(World, SavedTimeDilation);
		}
	}
	if (bCommit)
	{
		BeginMorph();
	}
}

void USlimeMorphComponent::TickMorphWheelInput()
{
	if (!bMorphWheelOpen || !Devour)
	{
		return;
	}
	APlayerController* PC = nullptr;
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		PC = Cast<APlayerController>(Pawn->GetController());
	}
	if (!PC)
	{
		return;
	}

	int32 Step = 0;
	if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp))
	{
		Step = 1;
	}
	else if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown))
	{
		Step = -1;
	}
	if (Step != 0)
	{
		Devour->CyclePhantomSelection(Step);
		if (MorphWheelWidget)
		{
			MorphWheelWidget->SetSlots(Devour->GetPhantomSlots(), Devour->GetSelectedPhantomSlot(), Devour->GetPhantomSlotCapacity());
		}
	}
}
