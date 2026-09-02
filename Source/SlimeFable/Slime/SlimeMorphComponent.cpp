// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeMorphComponent.h"

#include "EnemyCharacter.h"
#include "EnemyCombatComponent.h"
#include "EnemyFighter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "SceneTypes.h"
#include "SlimeBodyComponent.h"
#include "SlimeCharacter.h"
#include "SlimeDevourComponent.h"
#include "SlimeDodgeComponent.h"
#include "SlimeElementComponent.h"
#include "SlimeElementTypes.h"
#include "SlimeFable.h"
#include "SlimeLockOnComponent.h"
#include "SlimeSpringArmComponent.h"
#include "Settings/SlimeInputSettings.h"
#include "Settings/SlimeInputTypes.h"
#include "InputCoreTypes.h"
#include "UI/SlimePhantomWheelWidget.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
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

namespace SlimeMorphPolicies
{
	EMovementMode ResolveActivationMovementMode(EMovementMode CachedMode)
	{
		return CachedMode == MOVE_None ? MOVE_Walking : CachedMode;
	}

	FVector AlignCapsuleCenterToFoot(const FVector& CurrentLocation, float FootZ, float CapsuleHalfHeight)
	{
		FVector Aligned = CurrentLocation;
		Aligned.Z = FootZ + FMath::Max(CapsuleHalfHeight, 0.f);
		return Aligned;
	}

	bool IsSlotUsable(const TArray<FSlimeDevourCapture>& Slots, int32 SelectedSlot)
	{
		return Slots.IsValidIndex(SelectedSlot) && Slots[SelectedSlot].IsValidCapture();
	}

	bool IsMissingEngineMaterial(UMaterialInterface* Mat)
	{
		if (!Mat)
		{
			return true;
		}
		const FString Name = Mat->GetName();
		const FString Path = Mat->GetPathName();
		return Name.Contains(TEXT("WorldGrid"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("DefaultMaterial"), ESearchCase::IgnoreCase)
			|| Path.Contains(TEXT("Engine/EngineMaterials"), ESearchCase::IgnoreCase);
	}

	int32 CountVisualMaterialSlots(const UMeshComponent* MeshComp)
	{
		if (!MeshComp)
		{
			return 0;
		}
		int32 Num = MeshComp->GetNumMaterials();
		if (const USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(MeshComp))
		{
			if (const USkeletalMesh* SK = Skel->GetSkeletalMeshAsset())
			{
				Num = FMath::Max(Num, SK->GetMaterials().Num());
			}
		}
		return Num;
	}

	bool IsSlimeMorphMaterial(UMaterialInterface* Mat)
	{
		if (!Mat)
		{
			return false;
		}
		return Mat->GetPathName().Contains(TEXT("M_SlimeMorph"), ESearchCase::IgnoreCase);
	}

	bool SlotNeedsHairMorphSkin(UMaterialInterface* Saved, FName SlotName)
	{
		const FString Slot = SlotName.ToString();
		if (Slot.Contains(TEXT("Hair"), ESearchCase::IgnoreCase))
		{
			return true;
		}
		if (!Saved)
		{
			return false;
		}
		return Saved->GetShadingModels().HasShadingModel(MSM_Hair);
	}

	bool IsTargetGameplayEnabled(ESlimeMorphPhase Phase)
	{
		return Phase == ESlimeMorphPhase::Morphed;
	}

	bool KeepsTargetGroundCollision(ESlimeMorphPhase Phase)
	{
		return Phase != ESlimeMorphPhase::Idle;
	}
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
			if (const USlimeSpringArmComponent* SourceSlimeBoom = Cast<USlimeSpringArmComponent>(SourceBoom))
			{
				if (USlimeSpringArmComponent* TargetSlimeBoom = Cast<USlimeSpringArmComponent>(TargetBoom))
				{
					TargetSlimeBoom->MinCameraClearance = SourceSlimeBoom->MinCameraClearance;
					TargetSlimeBoom->FootClampRadius = SourceSlimeBoom->FootClampRadius;
					TargetSlimeBoom->MaxFootLift = SourceSlimeBoom->MaxFootLift;
				}
			}
		}

		if (SourceCamera && TargetCamera)
		{
			TargetCamera->SetFieldOfView(SourceCamera->FieldOfView);
		}
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
	TickCameraBlend(DeltaTime);
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

	const float DesiredArm = Slime->GetDesiredCameraArmLength()
		* FMath::Clamp(FMath::Sqrt(FMath::Max(MorphCameraHeightScale, 1.f)), 1.f, 2.2f);
	if (bCameraBlendActive)
	{
		// Keep the blend target in sync with wheel zoom; TickCameraBlend owns the boom values.
		CameraBlendTargetArm = DesiredArm;
		CameraBlendTargetSocketZ = MorphCameraSocketZ;
	}
	else
	{
		const float ArmLength = FMath::FInterpTo(
			MorphCameraBoom->TargetArmLength,
			DesiredArm,
			DeltaTime,
			Slime->CameraZoomInterpSpeed);
		MorphCameraBoom->TargetArmLength = ArmLength;
		MorphCameraBoom->SocketOffset = FVector(
			MorphCameraBoom->SocketOffset.X,
			MorphCameraBoom->SocketOffset.Y,
			MorphCameraSocketZ);
	}
	if (USpringArmComponent* SlimeCameraBoom = Slime->GetCameraBoom())
	{
		SlimeCameraBoom->TargetArmLength = Slime->GetDesiredCameraArmLength();
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
	const int32 SelectedSlot = Devour->GetSelectedPhantomSlot();
	if (!SlimeMorphPolicies::IsSlotUsable(Slots, SelectedSlot))
	{
		return;
	}

	MorphedSlotIndex = SelectedSlot;
	bConsumeMorphedSlotOnExit = false;
	bHasCachedSlimeReturnTransform = false;
	CachedSlimeReturnTransform = FTransform::Identity;
	bHasCachedMorphTargetGameplayState = false;
	bHasCachedMorphTargetMeshCollisionState = false;
	bPossessDone = false;
	SetSlimeMovementEnabled(false);

	if (Body)
	{
		Body->SetSpread(true);
	}
	if (ASlimeCharacter* Slime = Cast<ASlimeCharacter>(GetOwner()))
	{
		// Prevent the two pawn capsules from pushing each other upward during the transition.
		Slime->SetActorEnableCollision(false);
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
		// Extra parts (hair) become visible already wearing slime skin from spawn.
		SetExtraPartsHidden(false);
		ApplySlimeSkin();
		UpdateMorphMaterial(1.f, 1.f);
		SetShellActive(false);
	}
	else if (Next == ESlimeMorphPhase::Morphed)
	{
		// No shell while morphed: the model renders purely with its own materials.
		SetExtraPartsHidden(false);
		SetShellActive(false);
		if (!bOriginalMaterialsActive)
		{
			ApplyOriginalMaterials();
		}
		RestoreAllHiddenMorphSlots();
		// Seed the key state from reality — if Z is still held from the wheel commit, don't
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
		// Put slime skin back on every slot (including hair extra parts). Keep ShellOpacity=1
		// so Masked Toon does not clip to grey clay.
		ApplySlimeSkin();
		UpdateMorphMaterial(1.f, 1.f);
		SetShellActive(false);
	}
	else if (Next == ESlimeMorphPhase::Shrinking)
	{
		// Back to a full slime body: drop the shell and put the slime skin on the mesh itself.
		// Hide extra parts so hair doesn't float while the body shrinks to slime.
		SetExtraPartsHidden(true);
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
		if (ASlimeCharacter* Slime = Cast<ASlimeCharacter>(GetOwner()))
		{
			Slime->SetActorEnableCollision(true);
		}
		SetSlimeMovementEnabled(true);
		MorphTarget = nullptr;
		MorphVisuals.Reset();
		MorphedSlotIndex = INDEX_NONE;
		bConsumeMorphedSlotOnExit = false;
		bHasCachedSlimeReturnTransform = false;
		CachedSlimeReturnTransform = FTransform::Identity;
		bHasCachedMorphTargetGameplayState = false;
		bHasCachedMorphTargetMeshCollisionState = false;
		bOriginalMaterialsActive = false;
		bShellActive = false;
		bMorphedKeyDown = false;
	}

	if (MorphTarget)
	{
		SetMorphTargetGameplayEnabled(SlimeMorphPolicies::IsTargetGameplayEnabled(Next));
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
		// Mesh wears the slime skin; GrowProgress walks the reveal mask up the model.
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
		const float Alpha = PhaseAlpha(BlendDuration);
		// Keep full slime skin until Morphed. Fading ShellOpacity on Masked Toon clips to grey clay.
		UpdateMorphMaterial(1.f, 1.f);
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
		const float Alpha = PhaseAlpha(UnblendDuration);
		UpdateMorphMaterial(1.f, 1.f);
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

	// FinishSpawning may resize the capsule from the actual mesh. Align using that final
	// half-height so the target's feet stay on the slime's ground plane instead of floating.
	if (UCapsuleComponent* EnemyCapsule = Enemy->GetCapsuleComponent())
	{
		const FVector AlignedLocation = SlimeMorphPolicies::AlignCapsuleCenterToFoot(
			Enemy->GetActorLocation(), FootZ, EnemyCapsule->GetScaledCapsuleHalfHeight());
		Enemy->TeleportTo(AlignedLocation, Enemy->GetActorRotation(), false, true);
	}

	MorphTarget = Enemy;

	// Save original materials on every visual mesh (primary, BP extras, Groom — skip placeholder).
	MorphVisuals.Reset();
	bOriginalMaterialsActive = false;
	bShellActive = false;

	USkeletalMeshComponent* PrimaryMesh = Enemy->GetMesh();
	UMaterialInterface* OverlayMat = LoadMorphMaterial();
	UMaterialInterface* SubstrateMat = LoadMorphSubstrateMaterial();
	UMaterialInterface* HairMat = LoadMorphHairMaterial();
	UMaterialInterface* SkinParent = SubstrateMat ? SubstrateMat : OverlayMat;
	if (!SubstrateMat)
	{
		UE_LOG(LogSlimeFable, Warning,
			TEXT("SlimeMorphComponent: M_SlimeMorph_Substrate missing — falling back to M_SlimeMorph"));
	}
	if (!HairMat)
	{
		UE_LOG(LogSlimeFable, Warning,
			TEXT("SlimeMorphComponent: M_SlimeMorph_Hair missing — Hair VF slots may keep original hair"));
	}

	// Snapshot every visual mesh. Transition wears Substrate slime on non-hair slots.
	// MSM_HAIR sheets keep a dedicated Masked hair slime skin (Substrate cannot cover Hair VF).
	Enemy->ForEachVisualMesh([this, PrimaryMesh](UMeshComponent* MeshComp)
	{
		if (!MeshComp)
		{
			return;
		}
		FSlimeMorphMeshVisual Entry;
		Entry.Mesh = MeshComp;
		Entry.bExtraPart = (MeshComp != PrimaryMesh);
		Entry.bBaseSkinMorphPath = true;

		const int32 NumMats = SlimeMorphPolicies::CountVisualMaterialSlots(MeshComp);
		for (int32 Idx = 0; Idx < NumMats; ++Idx)
		{
			Entry.SavedMaterials.Add(MeshComp->GetMaterial(Idx));
		}

		UE_LOG(LogSlimeFable, Log,
			TEXT("SlimeMorphComponent: capture %s (%s) extra=%d slots=%d"),
			*MeshComp->GetName(), *MeshComp->GetClass()->GetName(),
			Entry.bExtraPart ? 1 : 0, NumMats);

		MorphVisuals.Add(MoveTemp(Entry));
	});

	bool bAnySkin = false;
	for (FSlimeMorphMeshVisual& Entry : MorphVisuals)
	{
		UMeshComponent* MeshComp = Entry.Mesh.Get();
		if (!MeshComp || !SkinParent)
		{
			continue;
		}
		bAnySkin = true;
		const int32 NumMats = SlimeMorphPolicies::CountVisualMaterialSlots(MeshComp);
		const TArray<FName> SlotNames = MeshComp->GetMaterialSlotNames();
		for (int32 Idx = 0; Idx < NumMats; ++Idx)
		{
			UMaterialInterface* Parent = SkinParent;
			UMaterialInterface* Saved = Entry.SavedMaterials.IsValidIndex(Idx)
				? Entry.SavedMaterials[Idx].Get()
				: nullptr;
			const FName SlotName = SlotNames.IsValidIndex(Idx) ? SlotNames[Idx] : NAME_None;
			if (HairMat && SlimeMorphPolicies::SlotNeedsHairMorphSkin(Saved, SlotName))
			{
				Parent = HairMat;
			}
			if (Parent)
			{
				if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this))
				{
					Entry.MorphMIDs.Add(MID);
				}
			}
		}
	}

	if (bAnySkin)
	{
		ApplySlimeSkin();
		SyncElementProfileToMorphMaterial();
	}

	// Extra parts (e.g. samurai hair) stay hidden until grow finishes.
	SetExtraPartsHidden(true);

	Enemy->SetActorHiddenInGame(false);
	if (PrimaryMesh)
	{
		PrimaryMesh->SetVisibility(true);
	}
	UpdateMorphMaterial(0.f, 1.f);
}

void USlimeMorphComponent::ApplySlimeSkin()
{
	if (!MorphTarget)
	{
		return;
	}
	for (FSlimeMorphMeshVisual& Entry : MorphVisuals)
	{
		UMeshComponent* MeshComp = Entry.Mesh.Get();
		if (!MeshComp)
		{
			continue;
		}
		RestoreHiddenMorphSlots(Entry);
		MeshComp->SetOverlayMaterial(nullptr);
		const TArray<FName> SlotNames = MeshComp->GetMaterialSlotNames();
		const int32 NumSlots = SlimeMorphPolicies::CountVisualMaterialSlots(MeshComp);
		for (int32 Idx = 0; Idx < Entry.MorphMIDs.Num() && Idx < NumSlots; ++Idx)
		{
			UMaterialInstanceDynamic* MID = Entry.MorphMIDs[Idx];
			if (!MID)
			{
				continue;
			}
			MeshComp->SetMaterial(Idx, MID);
			UMaterialInterface* Applied = MeshComp->GetMaterial(Idx);
			if (SlimeMorphPolicies::IsMissingEngineMaterial(Applied)
				|| !SlimeMorphPolicies::IsSlimeMorphMaterial(Applied))
			{
				const FString SlotName = SlotNames.IsValidIndex(Idx)
					? SlotNames[Idx].ToString()
					: FString::FromInt(Idx);
				UE_LOG(LogSlimeFable, Warning,
					TEXT("SlimeMorphComponent: %s slot %d (%s) still %s after slime skin, retrying"),
					*MeshComp->GetName(), Idx, *SlotName,
					Applied ? *Applied->GetName() : TEXT("null"));
				MeshComp->SetMaterial(Idx, MID);
			}
		}
		HideFailedMorphSlots(Entry);
		// Hair VF slots that still refused slime skin are hidden; Face/Up are never
		// hidden via pointer compare (Substrate slots can report a different interface).
	}
	bOriginalMaterialsActive = false;
}

void USlimeMorphComponent::ApplyOriginalMaterials()
{
	if (!MorphTarget || bOriginalMaterialsActive)
	{
		return;
	}
	for (FSlimeMorphMeshVisual& Entry : MorphVisuals)
	{
		UMeshComponent* MeshComp = Entry.Mesh.Get();
		if (!MeshComp)
		{
			continue;
		}
		RestoreHiddenMorphSlots(Entry);
		for (int32 Idx = 0; Idx < Entry.SavedMaterials.Num() && Idx < SlimeMorphPolicies::CountVisualMaterialSlots(MeshComp); ++Idx)
		{
			if (Entry.SavedMaterials[Idx])
			{
				MeshComp->SetMaterial(Idx, Entry.SavedMaterials[Idx]);
			}
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
	for (FSlimeMorphMeshVisual& Entry : MorphVisuals)
	{
		if (Entry.bBaseSkinMorphPath)
		{
			continue;
		}
		if (UMeshComponent* MeshComp = Entry.Mesh.Get())
		{
			MeshComp->SetOverlayMaterial(bActive ? Entry.ShellMID.Get() : nullptr);
		}
	}
	bShellActive = bActive;
}

void USlimeMorphComponent::SetExtraPartsHidden(bool bHidden)
{
	for (FSlimeMorphMeshVisual& Entry : MorphVisuals)
	{
		if (!Entry.bExtraPart)
		{
			continue;
		}
		if (UMeshComponent* MeshComp = Entry.Mesh.Get())
		{
			if (MeshComp->IsVisualizationComponent()
				|| Cast<UCameraComponent>(MeshComp->GetAttachParent()))
			{
				continue;
			}
			MeshComp->SetHiddenInGame(bHidden);
			MeshComp->SetVisibility(!bHidden);
		}
	}
}

void USlimeMorphComponent::DestroyMorphTarget()
{
	if (MorphTarget)
	{
		ClearMorphDodge();

		// Drop the shell and restore the real materials so the enemy can dissolve properly.
		SetShellActive(false);
		RestoreAllHiddenMorphSlots();
		ApplyOriginalMaterials();

		if (UEnemyCombatComponent* EnemyCombat = MorphTarget->GetEnemyCombat())
		{
			EnemyCombat->SetPlayerMorphed(false);
		}

		MorphTarget->Destroy();
		MorphTarget = nullptr;
	}
	MorphVisuals.Reset();
	bOriginalMaterialsActive = false;
	bShellActive = false;
	bMorphWalkPlaying = false;
	bMorphRunPlaying = false;
	bMorphJumpPlaying = false;
	MorphIdleTimer = 0.f;
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

UMaterialInterface* USlimeMorphComponent::LoadMorphGrowMaterial()
{
	if (MorphGrowMaterial)
	{
		return MorphGrowMaterial;
	}

	static const FSoftObjectPath GrowPath(TEXT("/Game/Characters/Slime/Materials/M_SlimeMorph_Grow.M_SlimeMorph_Grow"));
	MorphGrowMaterial = Cast<UMaterialInterface>(GrowPath.TryLoad());
	if (!MorphGrowMaterial)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeMorphComponent: M_SlimeMorph_Grow not found at %s — falling back to translucent morph"), *GrowPath.ToString());
	}
	return MorphGrowMaterial;
}

UMaterialInterface* USlimeMorphComponent::LoadMorphSubstrateMaterial()
{
	if (MorphSubstrateMaterial)
	{
		return MorphSubstrateMaterial;
	}

	static const FSoftObjectPath SubstratePath(
		TEXT("/Game/Characters/Slime/Materials/M_SlimeMorph_Substrate.M_SlimeMorph_Substrate"));
	MorphSubstrateMaterial = Cast<UMaterialInterface>(SubstratePath.TryLoad());
	if (!MorphSubstrateMaterial)
	{
		UE_LOG(LogSlimeFable, Warning,
			TEXT("SlimeMorphComponent: M_SlimeMorph_Substrate not found at %s"), *SubstratePath.ToString());
	}
	return MorphSubstrateMaterial;
}

UMaterialInterface* USlimeMorphComponent::LoadMorphHairMaterial()
{
	if (MorphHairMaterial)
	{
		return MorphHairMaterial;
	}

	static const FSoftObjectPath HairPath(
		TEXT("/Game/Characters/Slime/Materials/M_SlimeMorph_Hair.M_SlimeMorph_Hair"));
	MorphHairMaterial = Cast<UMaterialInterface>(HairPath.TryLoad());
	if (!MorphHairMaterial)
	{
		UE_LOG(LogSlimeFable, Warning,
			TEXT("SlimeMorphComponent: M_SlimeMorph_Hair not found at %s"), *HairPath.ToString());
	}
	return MorphHairMaterial;
}

bool USlimeMorphComponent::MaterialNeedsBaseSkinMorphPath(UMaterialInterface* Mat)
{
	if (!Mat)
	{
		return false;
	}

	const UMaterial* Base = Mat->GetMaterial();
	const FString Path = Base ? Base->GetPathName() : Mat->GetPathName();
	const FString Name = Base ? Base->GetName() : Mat->GetName();

	// Phoebe / other Substrate Toon masters in this project. OverlayMaterial cannot cover them.
	if (Path.Contains(TEXT("PhoebeToon"), ESearchCase::IgnoreCase)
		|| Name.Contains(TEXT("PhoebeToon"), ESearchCase::IgnoreCase)
		|| Name.Contains(TEXT("Substrate"), ESearchCase::IgnoreCase))
	{
		return true;
	}

	// MI_Phoebe* parents are Toon masters — catch by path even if parent rename fails.
	if (Path.Contains(TEXT("/Models/Phoebe/"), ESearchCase::IgnoreCase)
		|| Name.StartsWith(TEXT("MI_Phoebe"), ESearchCase::IgnoreCase)
		|| Name.StartsWith(TEXT("M_Phoebe"), ESearchCase::IgnoreCase))
	{
		return true;
	}

	return false;
}

bool USlimeMorphComponent::UsesBaseSkinMorphPath() const
{
	for (const FSlimeMorphMeshVisual& Entry : MorphVisuals)
	{
		if (Entry.bBaseSkinMorphPath)
		{
			return true;
		}
	}
	return false;
}

void USlimeMorphComponent::HideFailedMorphSlots(FSlimeMorphMeshVisual& Entry)
{
	Entry.HiddenMaterialSlots.Reset();
	USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(Entry.Mesh.Get());
	if (!Skel)
	{
		return;
	}

	const TArray<FName> SlotNames = Skel->GetMaterialSlotNames();
	const int32 NumSlots = SlimeMorphPolicies::CountVisualMaterialSlots(Skel);
	for (int32 Idx = 0; Idx < NumSlots; ++Idx)
	{
		UMaterialInterface* Saved = Entry.SavedMaterials.IsValidIndex(Idx)
			? Entry.SavedMaterials[Idx].Get()
			: nullptr;
		const FName SlotName = SlotNames.IsValidIndex(Idx) ? SlotNames[Idx] : NAME_None;
		if (!SlimeMorphPolicies::SlotNeedsHairMorphSkin(Saved, SlotName))
		{
			continue;
		}

		UMaterialInterface* Applied = Skel->GetMaterial(Idx);
		if (SlimeMorphPolicies::IsSlimeMorphMaterial(Applied)
			&& !SlimeMorphPolicies::IsMissingEngineMaterial(Applied))
		{
			continue;
		}

		Skel->ShowMaterialSection(Idx, 0, false, INDEX_NONE);
		Entry.HiddenMaterialSlots.Add(Idx);
		UE_LOG(LogSlimeFable, Warning,
			TEXT("SlimeMorphComponent: hiding hair slot %d (%s) still %s after slime skin"),
			Idx, *SlotName.ToString(),
			Applied ? *Applied->GetName() : TEXT("null"));
	}
}

void USlimeMorphComponent::RestoreHiddenMorphSlots(FSlimeMorphMeshVisual& Entry)
{
	USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(Entry.Mesh.Get());
	if (!Skel)
	{
		Entry.HiddenMaterialSlots.Reset();
		return;
	}

	for (const int32 Idx : Entry.HiddenMaterialSlots)
	{
		Skel->ShowMaterialSection(Idx, 0, true, INDEX_NONE);
	}
	Entry.HiddenMaterialSlots.Reset();
}

void USlimeMorphComponent::RestoreAllHiddenMorphSlots()
{
	for (FSlimeMorphMeshVisual& Entry : MorphVisuals)
	{
		RestoreHiddenMorphSlots(Entry);
	}
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

	for (FSlimeMorphMeshVisual& Entry : MorphVisuals)
	{
		for (UMaterialInstanceDynamic* MID : Entry.MorphMIDs)
		{
			Push(MID);
		}
		Push(Entry.ShellMID);
	}

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
	// Match slime world framing first (TargetOffset compensates boom origin). Height framing
	// eases in via TickCameraBlend — avoid RefreshCameraRig snap.
	CopyCameraRig(Slime->GetCameraBoom(), MorphCameraBoom, Slime->GetFollowCamera(), MorphCamera);

	float TargetArm = MorphCameraBoom ? MorphCameraBoom->TargetArmLength : 260.f;
	float TargetSocketZ = MorphCameraBoom ? MorphCameraBoom->SocketOffset.Z : 12.f;
	FVector TargetOffset = MorphCameraBoom ? MorphCameraBoom->TargetOffset : FVector::ZeroVector;
	if (MorphCameraBoom && MorphTarget)
	{
		float HalfH = 20.f;
		if (const UCapsuleComponent* Cap = MorphTarget->GetCapsuleComponent())
		{
			HalfH = Cap->GetScaledCapsuleHalfHeight();
		}
		constexpr float SlimeRefHalfH = 20.f;
		const float HeightScale = FMath::Max(HalfH / SlimeRefHalfH, 1.f);
		const float SocketZ = FMath::Clamp(HalfH * 0.60f, 12.f, 120.f);
		const float ArmMul = FMath::Clamp(FMath::Sqrt(HeightScale), 1.f, 2.2f);
		MorphCameraHeightScale = HeightScale;
		MorphCameraSocketZ = SocketZ;
		TargetArm = MorphCameraBoom->TargetArmLength * ArmMul;
		TargetSocketZ = SocketZ;
		TargetOffset = FVector::ZeroVector;
	}
	else
	{
		MorphCameraHeightScale = 1.f;
		MorphCameraSocketZ = 12.f;
	}

	// Park the slime: hidden, no collision, no movement, and no shadow-proxy cast. The actor
	// tick stays ON — this component lives on the slime and must keep running the phase
	// machine and polling the unmorph key while the player drives the morph body.
	Slime->SetMorphParked(true);

	PC->Possess(MorphTarget);
	PC->SetControlRotation(PreservedViewRotation);
	BeginCameraBlend(MorphCameraBoom, TargetArm, TargetSocketZ, TargetOffset);
	// Keys held across a possess swap never deliver their release to the new pawn, which
	// leaves the movement axis stuck and the character walking off on its own.
	PC->FlushPressedKeys();

	if (UEnemyCombatComponent* EnemyCombat = MorphTarget->GetEnemyCombat())
	{
		EnemyCombat->SetPlayerMorphed(true);
	}

	MorphTarget->RefreshHealthBarAnchor();

	EnsureMorphDodge();
}

void USlimeMorphComponent::EnsureMorphDodge()
{
	ClearMorphDodge();
	if (!MorphTarget)
	{
		return;
	}
	USlimeDodgeComponent* Created = NewObject<USlimeDodgeComponent>(MorphTarget, TEXT("MorphSlimeDodge"));
	Created->RegisterComponent();
	MorphDodge = Created;
}

void USlimeMorphComponent::ClearMorphDodge()
{
	if (USlimeDodgeComponent* Dodge = MorphDodge.Get())
	{
		Dodge->DestroyComponent();
	}
	MorphDodge = nullptr;
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

	if (USlimeLockOnComponent* Lock = MorphTarget->FindComponentByClass<USlimeLockOnComponent>())
	{
		Lock->ClearLockOn();
	}

	ClearMorphDodge();

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
	SetSlimeMovementEnabled(false);
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

	USpringArmComponent* SlimeBoom = Slime->GetCameraBoom();
	USpringArmComponent* MorphBoom = MorphTarget->FindComponentByClass<USpringArmComponent>();
	UCameraComponent* MorphCamera = MorphTarget->FindComponentByClass<UCameraComponent>();
	CopyCameraRig(MorphBoom, SlimeBoom, MorphCamera, Slime->GetFollowCamera());

	const float TargetArm = Slime->GetDesiredCameraArmLength();
	const float TargetSocketZ = 12.f;
	const FVector TargetOffset = FVector::ZeroVector;

	PC->Possess(Slime);
	PC->SetControlRotation(PreservedViewRotation);
	BeginCameraBlend(SlimeBoom, TargetArm, TargetSocketZ, TargetOffset);
	PC->FlushPressedKeys();
}

void USlimeMorphComponent::BeginCameraBlend(USpringArmComponent* Boom, float TargetArm, float TargetSocketZ, const FVector& TargetOffset)
{
	bCameraBlendActive = false;
	CameraBlendBoom = nullptr;
	if (!Boom)
	{
		return;
	}

	CameraBlendBoom = Boom;
	CameraBlendStartArm = Boom->TargetArmLength;
	CameraBlendTargetArm = TargetArm;
	CameraBlendStartSocketZ = Boom->SocketOffset.Z;
	CameraBlendTargetSocketZ = TargetSocketZ;
	CameraBlendStartOffset = Boom->TargetOffset;
	CameraBlendTargetOffset = TargetOffset;
	CameraBlendElapsed = 0.f;
	bCameraBlendActive = true;
}

void USlimeMorphComponent::TickCameraBlend(float DeltaTime)
{
	if (!bCameraBlendActive)
	{
		return;
	}

	USpringArmComponent* Boom = CameraBlendBoom.Get();
	if (!Boom)
	{
		bCameraBlendActive = false;
		return;
	}

	CameraBlendElapsed += DeltaTime;
	const float Duration = FMath::Max(CameraBlendDuration, 0.05f);
	const float Alpha = FMath::Clamp(CameraBlendElapsed / Duration, 0.f, 1.f);
	const float Ease = FMath::SmoothStep(0.f, 1.f, Alpha);

	Boom->TargetArmLength = FMath::Lerp(CameraBlendStartArm, CameraBlendTargetArm, Ease);
	Boom->TargetOffset = FMath::Lerp(CameraBlendStartOffset, CameraBlendTargetOffset, Ease);
	Boom->SocketOffset = FVector(
		Boom->SocketOffset.X,
		Boom->SocketOffset.Y,
		FMath::Lerp(CameraBlendStartSocketZ, CameraBlendTargetSocketZ, Ease));

	if (Alpha >= 1.f)
	{
		Boom->TargetArmLength = CameraBlendTargetArm;
		Boom->TargetOffset = CameraBlendTargetOffset;
		Boom->SocketOffset = FVector(Boom->SocketOffset.X, Boom->SocketOffset.Y, CameraBlendTargetSocketZ);
		bCameraBlendActive = false;
		CameraBlendBoom = nullptr;
	}
}

void USlimeMorphComponent::SetSlimeMovementEnabled(bool bEnabled)
{
	ASlimeCharacter* Slime = Cast<ASlimeCharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Slime ? Slime->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}

	Movement->StopMovementImmediately();
	Movement->Velocity = FVector::ZeroVector;
	if (bEnabled)
	{
		Movement->SetMovementMode(MOVE_Walking);
	}
	else
	{
		Movement->DisableMovement();
	}
}

void USlimeMorphComponent::SetMorphTargetGameplayEnabled(bool bEnabled)
{
	if (!MorphTarget)
	{
		return;
	}

	if (!bHasCachedMorphTargetGameplayState)
	{
		if (const UCharacterMovementComponent* Movement = MorphTarget->GetCharacterMovement())
		{
			CachedMorphTargetMovementMode = static_cast<uint8>(Movement->MovementMode.GetValue());
			CachedMorphTargetCustomMovementMode = Movement->CustomMovementMode;
		}
		bHasCachedMorphTargetGameplayState = true;
	}

	// Keep the target capsule and actor collision alive so the spawned body remains grounded.
	// Only the rendered mesh is removed from collision during the visual transition; otherwise
	// a giant mesh (for example Tianhuang) becomes a dynamic collider that pushes the slime up.
	if (USkeletalMeshComponent* Mesh = MorphTarget->GetMesh())
	{
		if (!bHasCachedMorphTargetMeshCollisionState)
		{
			CachedMorphTargetMeshCollisionEnabled = static_cast<uint8>(Mesh->GetCollisionEnabled());
			bHasCachedMorphTargetMeshCollisionState = true;
		}
		Mesh->SetCollisionEnabled(bEnabled
			? static_cast<ECollisionEnabled::Type>(CachedMorphTargetMeshCollisionEnabled)
			: ECollisionEnabled::NoCollision);
	}
	if (UCharacterMovementComponent* Movement = MorphTarget->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->Velocity = FVector::ZeroVector;
		if (bEnabled)
		{
			Movement->SetMovementMode(
				SlimeMorphPolicies::ResolveActivationMovementMode(
					static_cast<EMovementMode>(CachedMorphTargetMovementMode)),
				CachedMorphTargetCustomMovementMode);
		}
		else
		{
			Movement->DisableMovement();
		}
	}

	if (bEnabled)
	{
		if (APlayerController* PC = Cast<APlayerController>(MorphTarget->GetController()))
		{
			PC->FlushPressedKeys();
		}
	}
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

	// Morph-only lift: Masked Toon reads a bit heavier than the jelly body.
	// Fire/Lightning already have strong emissive — leave them alone.
	FSlimeElementProfile Profile = Element->GetCurrentProfile();
	float MorphBrightness = 1.55f;
	float MorphRimBoost = 1.45f;
	float MorphFillEmissive = 0.18f;

	switch (Profile.Element)
	{
	case ESlimeElement::Water:
	case ESlimeElement::Wind:
	case ESlimeElement::Physical:
		Profile.BaseColor *= 1.15f;
		Profile.SubsurfaceColor *= 1.25f;
		Profile.RimColor *= 1.35f;
		Profile.BaseColor.A = 1.f;
		Profile.SubsurfaceColor.A = 1.f;
		Profile.RimColor.A = 1.f;
		break;

	case ESlimeElement::Dark:
		Profile.BaseColor = FLinearColor(
			FMath::Max(Profile.BaseColor.R * 1.8f, 0.18f),
			FMath::Max(Profile.BaseColor.G * 1.8f, 0.18f),
			FMath::Max(Profile.BaseColor.B * 2.0f, 0.28f),
			1.f);
		Profile.SubsurfaceColor = FLinearColor(
			FMath::Max(Profile.SubsurfaceColor.R * 1.6f, 0.25f),
			FMath::Max(Profile.SubsurfaceColor.G * 1.6f, 0.25f),
			FMath::Max(Profile.SubsurfaceColor.B * 1.8f, 0.35f),
			1.f);
		Profile.RimColor = FLinearColor(
			FMath::Max(Profile.RimColor.R * 1.7f, 0.45f),
			FMath::Max(Profile.RimColor.G * 1.7f, 0.45f),
			FMath::Max(Profile.RimColor.B * 1.9f, 0.55f),
			1.f);
		if (Profile.EmissiveColor.GetLuminance() < 0.05f)
		{
			Profile.EmissiveColor = Profile.RimColor;
		}
		Profile.EmissiveIntensity = FMath::Max(Profile.EmissiveIntensity, 0.45f);
		Profile.RimPower = FMath::Clamp(Profile.RimPower, 1.f, 2.4f);
		MorphBrightness = 1.45f;
		MorphRimBoost = 1.55f;
		MorphFillEmissive = 0.22f;
		break;

	case ESlimeElement::Fire:
	case ESlimeElement::Lightning:
	default:
		MorphBrightness = 1.1f;
		MorphRimBoost = 1.1f;
		MorphFillEmissive = 0.05f;
		break;
	}

	auto Push = [&Profile, MorphBrightness, MorphRimBoost, MorphFillEmissive](UMaterialInstanceDynamic* MID)
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
		MID->SetScalarParameterValue(TEXT("IOR"), Profile.Refraction);
		MID->SetScalarParameterValue(SlimeMorphParams::FlowSpeed, Profile.FlowSpeed);
		MID->SetScalarParameterValue(SlimeMorphParams::NoiseScale, Profile.NoiseScale);
		MID->SetScalarParameterValue(SlimeMorphParams::RimPower, Profile.RimPower);
		MID->SetScalarParameterValue(TEXT("MorphBrightness"), MorphBrightness);
		MID->SetScalarParameterValue(TEXT("MorphRimBoost"), MorphRimBoost);
		MID->SetScalarParameterValue(TEXT("MorphFillEmissive"), MorphFillEmissive);
	};

	for (FSlimeMorphMeshVisual& Entry : MorphVisuals)
	{
		for (UMaterialInstanceDynamic* MID : Entry.MorphMIDs)
		{
			Push(MID);
		}
		Push(Entry.ShellMID);
	}
}

void USlimeMorphComponent::TickMorphLocomotion(float Dt)
{
	if (!MorphTarget)
	{
		return;
	}

	// Only single-node-anim enemies need manual locomotion — AnimBP-driven enemies handle it themselves.
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

	// Jump anim is started in MorphJump on key press — never invent a jump just because spawn
	// briefly reported !IsMovingOnGround (that caused the "default pose jumps" bug).
	if (MorphTarget->IsMorphJumpAnimActive())
	{
		bMorphJumpPlaying = true;
		bMorphWalkPlaying = false;
		bMorphRunPlaying = false;
		MorphIdleTimer = 0.f;
	}

	const bool bAirborne = Move->IsFalling() || !Move->IsMovingOnGround();
	if (bAirborne)
	{
		// Hold jump clip if we started one; otherwise keep whatever idle/walk was already on.
		return;
	}

	if (bMorphJumpPlaying || MorphTarget->IsMorphJumpAnimActive())
	{
		MorphTarget->ClearMorphJumpAnim();
		bMorphJumpPlaying = false;
		bMorphWalkPlaying = false;
		bMorphRunPlaying = false;
	}

	bool bSprint = false;
	if (APlayerController* PC = Cast<APlayerController>(MorphTarget->GetController()))
	{
		if (const UWorld* World = GetWorld())
		{
			if (const UGameInstance* GI = World->GetGameInstance())
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
	}

	const float Speed = Move->Velocity.Size2D();
	const bool bMoving = Speed > 10.f;
	// Default morph locomotion is run (like WuWa); walk only in a low-speed band.
	constexpr float MorphWalkMaxSpeed = 180.f;
	const bool bShouldRun = bMoving && (Speed >= MorphWalkMaxSpeed || bSprint)
		&& !Fighter->RunMontage.IsNull();

	if (bShouldRun)
	{
		if (!bMorphRunPlaying)
		{
			if (UAnimMontage* Run = Fighter->RunMontage.LoadSynchronous())
			{
				Fighter->PlayMeshAnimation(Run, true);
				bMorphRunPlaying = true;
				bMorphWalkPlaying = false;
				MorphIdleTimer = 0.f;
			}
		}
		return;
	}

	if (bMoving)
	{
		if (!bMorphWalkPlaying || bMorphRunPlaying)
		{
			if (UAnimMontage* Walk = Fighter->WalkMontage.LoadSynchronous())
			{
				Fighter->PlayMeshAnimation(Walk, true);
				bMorphWalkPlaying = true;
				bMorphRunPlaying = false;
				MorphIdleTimer = 0.f;
			}
			else if (!Fighter->RunMontage.IsNull())
			{
				// No walk clip — fall back to run at low play rate feel.
				if (UAnimMontage* Run = Fighter->RunMontage.LoadSynchronous())
				{
					Fighter->PlayMeshAnimation(Run, true);
					bMorphRunPlaying = true;
					bMorphWalkPlaying = false;
				}
			}
		}
		return;
	}

	// Stopped on ground — standing idle (loop), never jump/lie.
	if (bMorphWalkPlaying || bMorphRunPlaying)
	{
		bMorphWalkPlaying = false;
		bMorphRunPlaying = false;
		MorphIdleTimer = 0.f;
		if (Fighter->IdleMontages.Num() > 0)
		{
			if (UAnimMontage* Idle = Fighter->IdleMontages[0].LoadSynchronous())
			{
				Fighter->PlayMeshAnimation(Idle, true);
			}
		}
		else
		{
			Fighter->StopMeshAnimation();
		}
	}
	else
	{
		MorphIdleTimer += Dt;
		if (MorphIdleTimer > 4.f && Fighter->IdleMontages.Num() > 1)
		{
			MorphIdleTimer = 0.f;
			const int32 Index = FMath::RandRange(0, Fighter->IdleMontages.Num() - 1);
			if (UAnimMontage* Idle = Fighter->IdleMontages[Index].LoadSynchronous())
			{
				Fighter->PlayMeshAnimation(Idle, true);
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

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeMorphSlotSelectionTest,
	"SlimeFable.Slime.Morph.EmptySlotSelectionIsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeMorphSlotSelectionTest::RunTest(const FString& Parameters)
{
	TArray<FSlimeDevourCapture> Slots;
	Slots.SetNum(3);

	TestFalse(TEXT("A capacity-only slot cannot index the capture array"), SlimeMorphPolicies::IsSlotUsable(Slots, 3));
	TestFalse(TEXT("An empty captured slot cannot start morphing"), SlimeMorphPolicies::IsSlotUsable(Slots, 1));

	Slots[1].EnemyClass = AEnemyCharacter::StaticClass();
	TestTrue(TEXT("A populated captured slot can start morphing"), SlimeMorphPolicies::IsSlotUsable(Slots, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeMorphTransitionGameplayTest,
	"SlimeFable.Slime.Morph.TransitionTargetIsNonInteractive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeMorphTransitionGameplayTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Growing target is visual-only"), SlimeMorphPolicies::IsTargetGameplayEnabled(ESlimeMorphPhase::Growing));
	TestFalse(TEXT("Blending target cannot move or collide"), SlimeMorphPolicies::IsTargetGameplayEnabled(ESlimeMorphPhase::Blending));
	TestTrue(TEXT("Completed morph target enables gameplay"), SlimeMorphPolicies::IsTargetGameplayEnabled(ESlimeMorphPhase::Morphed));
	TestFalse(TEXT("Unblending target is frozen again"), SlimeMorphPolicies::IsTargetGameplayEnabled(ESlimeMorphPhase::Unblending));
	TestTrue(TEXT("Transition keeps the target's ground collision"), SlimeMorphPolicies::KeepsTargetGroundCollision(ESlimeMorphPhase::Growing));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeMorphActivationMovementTest,
	"SlimeFable.Slime.Morph.ActivationRestoresWalkFromUninitializedMovement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeMorphActivationMovementTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("A deferred morph target cannot remain in MOVE_None when player control starts"),
		SlimeMorphPolicies::ResolveActivationMovementMode(MOVE_None), MOVE_Walking);
	TestEqual(TEXT("A valid cached movement mode remains unchanged"),
		SlimeMorphPolicies::ResolveActivationMovementMode(MOVE_Falling), MOVE_Falling);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeMorphFootAlignmentTest,
	"SlimeFable.Slime.Morph.PostSpawnCapsuleKeepsFeetOnSlimeGround",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeMorphFootAlignmentTest::RunTest(const FString& Parameters)
{
	const FVector CurrentLocation(120.0, -45.0, 96.0);
	const FVector AlignedLocation = SlimeMorphPolicies::AlignCapsuleCenterToFoot(CurrentLocation, 20.0, 140.0);
	TestEqual(TEXT("Alignment preserves X"), AlignedLocation.X, 120.0);
	TestEqual(TEXT("Alignment preserves Y"), AlignedLocation.Y, -45.0);
	TestEqual(TEXT("Alignment places the new capsule feet on the slime ground"), AlignedLocation.Z, 160.0);
	return true;
}

#endif
