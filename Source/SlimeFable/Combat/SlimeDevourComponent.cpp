// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeDevourComponent.h"

#include "SlimePhantomWheelWidget.h"
#include "SlimeAbilityComponent.h"
#include "SlimeBodyComponent.h"
#include "SlimeClingComponent.h"
#include "SlimeCombatComponent.h"
#include "SlimeLockOnComponent.h"
#include "SlimeFable.h"
#include "SlimeHealthComponent.h"
#include "SlimePlacementComponent.h"
#include "SlimeStatusComponent.h"
#include "SlimeVehicleComponent.h"
#include "SlimeFableCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "BrainComponent.h"
#include "EnemyAllyAIController.h"
#include "EnemyCharacter.h"
#include "EnemyCombatComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Settings/SlimeInputSettings.h"
#include "Settings/SlimeInputTypes.h"
#include "Settings/SlimeAudioPlay.h"
#include "Sound/SoundBase.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "Engine/GameInstance.h"

USlimeDevourComponent::USlimeDevourComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;
	SwallowSound = TSoftObjectPtr<USoundBase>(
		FSoftObjectPath(TEXT("/Game/Audio/SFX/Combat/sfx_swallow_01.sfx_swallow_01")));
}

void USlimeDevourComponent::BeginPlay()
{
	Super::BeginPlay();
	if (FMath::IsNearlyEqual(DevourRadius, 350.f, 1.f)
		|| FMath::IsNearlyEqual(DevourRadius, 220.f, 1.f)
		|| FMath::IsNearlyEqual(DevourRadius, 1000.f, 1.f))
	{
		DevourRadius = 800.f;
	}
	if (FMath::IsNearlyEqual(CloseRangeRadius, 200.f, 1.f))
	{
		CloseRangeRadius = 300.f;
	}
	if (FMath::IsNearlyEqual(CloseRangeShrinkSeconds, 1.f, 0.01f))
	{
		CloseRangeShrinkSeconds = 0.75f;
	}
	if (FMath::IsNearlyEqual(CloseRangeDashSeconds, 0.35f, 0.01f))
	{
		CloseRangeDashSeconds = 0.5f;
	}
	if (FMath::IsNearlyEqual(LatchShotFraction, 0.06f, 0.005f))
	{
		LatchShotFraction = 0.5f;
	}
	if (FMath::IsNearlyEqual(HoldSeconds, 1.5f, 0.01f))
	{
		HoldSeconds = 1.2f;
	}
	if (FMath::IsNearlyEqual(DevourHealthThreshold, 0.1f, 0.001f))
	{
		DevourHealthThreshold = 0.2f;
	}
	if (FMath::IsNearlyEqual(LatchSeconds, 0.4f, 0.01f)
		|| FMath::IsNearlyEqual(LatchSeconds, 0.9f, 0.01f))
	{
		LatchSeconds = 0.65f;
	}
	if (LatchShotCount >= 3)
	{
		LatchShotCount = 1;
	}
	if (FMath::IsNearlyEqual(ShrinkSeconds, 2.f, 0.01f))
	{
		ShrinkSeconds = 1.5f;
	}
	if (FMath::IsNearlyEqual(RetractSeconds, 1.2f, 0.01f))
	{
		RetractSeconds = 0.9f;
	}
	if (FMath::IsNearlyEqual(LatchPullSpeed, 2800.f, 1.f))
	{
		LatchPullSpeed = 900.f;
	}
	if (AActor* Owner = GetOwner())
	{
		Body = Owner->FindComponentByClass<USlimeBodyComponent>();
		Combat = Owner->FindComponentByClass<USlimeCombatComponent>();
	}
}

void USlimeDevourComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bPhantomWheelOpen)
	{
		ClosePhantomWheel(false);
	}
	AbortDevour(true);
	Super::EndPlay(EndPlayReason);
}

void USlimeDevourComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	CycleCooldownRemaining = FMath::Max(CycleCooldownRemaining - FApp::GetDeltaTime(), 0.f);
	if (Phase != ESlimeDevourPhase::Idle)
	{
		TickPhase(DeltaTime);
	}
}

bool USlimeDevourComponent::IsCombatLocked() const
{
	return Phase == ESlimeDevourPhase::CloseRangeShrink
		|| Phase == ESlimeDevourPhase::CloseRangeDash
		|| Phase == ESlimeDevourPhase::Latch
		|| Phase == ESlimeDevourPhase::Shrink
		|| Phase == ESlimeDevourPhase::Retract;
}

float USlimeDevourComponent::GetDigestAlpha() const
{
	if (Phase != ESlimeDevourPhase::Digest || DigestSeconds <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	return FMath::Clamp(PhaseElapsed / DigestSeconds, 0.f, 1.f);
}

float USlimeDevourComponent::GetHoldProgress() const
{
	if (Phase != ESlimeDevourPhase::Charging || HoldSeconds <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	return FMath::Clamp(PhaseElapsed / HoldSeconds, 0.f, 1.f);
}

APlayerController* USlimeDevourComponent::GetPlayerController() const
{
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		return Cast<APlayerController>(Pawn->GetController());
	}
	return nullptr;
}

FVector USlimeDevourComponent::GetBlobCenter() const
{
	if (Body)
	{
		return Body->GetVisualBlobCenter();
	}
	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

float USlimeDevourComponent::GetBlobRadius() const
{
	if (!Body)
	{
		return 27.f;
	}
	return Body->SolverParams.RestRadius * FMath::Max(Body->GetAppliedBodyScale(), Body->GetBodyScale());
}

bool USlimeDevourComponent::CanDevourTarget(const AEnemyCharacter* Enemy) const
{
	if (!Enemy)
	{
		return false;
	}
	if (!Enemy->IsDevourableNow())
	{
		return false;
	}
	const USlimeHealthComponent* Health = Enemy->GetEnemyHealth();
	const float Threshold = Enemy->DevourHealthThreshold > KINDA_SMALL_NUMBER
		? Enemy->DevourHealthThreshold
		: DevourHealthThreshold;
	if (!Health || !Health->IsAlive() || Health->GetHealthPercent() > Threshold)
	{
		return false;
	}
	return true;
}

bool USlimeDevourComponent::IsTargetStillValid(const AEnemyCharacter* Enemy) const
{
	if (!IsValid(Enemy) || Enemy->IsInDeathSequence())
	{
		return false;
	}
	return CanDevourTarget(Enemy);
}

AEnemyCharacter* USlimeDevourComponent::FindBestDevourTarget() const
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		return nullptr;
	}
	if (Phase == ESlimeDevourPhase::Latch
		|| Phase == ESlimeDevourPhase::CloseRangeShrink
		|| Phase == ESlimeDevourPhase::CloseRangeDash
		|| Phase == ESlimeDevourPhase::Shrink
		|| Phase == ESlimeDevourPhase::Retract
		|| Phase == ESlimeDevourPhase::Digest)
	{
		return nullptr;
	}
	if (const USlimeVehicleComponent* Vehicle = Pawn->FindComponentByClass<USlimeVehicleComponent>())
	{
		if (Vehicle->IsUsingVehicle())
		{
			return nullptr;
		}
	}

	if (Phase == ESlimeDevourPhase::Charging)
	{
		if (AEnemyCharacter* Current = DevourTarget.Get())
		{
			if (IsTargetStillValid(Current))
			{
				return Current;
			}
		}
	}

	const FVector Loc = Pawn->GetActorLocation();
	const float RadiusSq = FMath::Square(DevourRadius);
	AEnemyCharacter* Best = nullptr;
	float BestDistSq = RadiusSq;
	for (TActorIterator<AEnemyCharacter> It(World); It; ++It)
	{
		AEnemyCharacter* Enemy = *It;
		if (!CanDevourTarget(Enemy))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Loc, Enemy->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Enemy;
		}
	}
	return Best;
}

bool USlimeDevourComponent::TryDevourFocused()
{
	return BeginHold(FindBestDevourTarget());
}

bool USlimeDevourComponent::BeginHold(AEnemyCharacter* Enemy)
{
	if (!CanStartDevour() || !CanDevourTarget(Enemy) || !Body)
	{
		return false;
	}

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return false;
	}

	if (USlimeClingComponent* Cling = Character->FindComponentByClass<USlimeClingComponent>())
	{
		if (Cling->IsClinging())
		{
			Cling->TryDetach();
		}
	}
	if (Body->IsSpreading())
	{
		Body->SetSpread(false);
	}
	if (Combat)
	{
		Combat->InterruptCombat();
	}

	DevourTarget = Enemy;
	PendingDestroyEnemy.Reset();
	ActiveCapture = FSlimeDevourCapture();
	LatchShotIds.Reset();
	CloseRangeShotId = 0;
	FaceTarget(Enemy);
	EnterPhase(ESlimeDevourPhase::Charging);
	return true;
}

void USlimeDevourComponent::CancelHold()
{
	if (Phase != ESlimeDevourPhase::Charging)
	{
		return;
	}
	DevourTarget.Reset();
	Phase = ESlimeDevourPhase::Idle;
	PhaseElapsed = 0.f;
}

bool USlimeDevourComponent::ReleaseHold()
{
	if (Phase != ESlimeDevourPhase::Charging)
	{
		return false;
	}

	AEnemyCharacter* Target = DevourTarget.Get();
	const bool bCloseRange = PhaseElapsed < HoldSeconds
		&& IsTargetStillValid(Target)
		&& GetOwner()
		&& FVector::Dist(GetOwner()->GetActorLocation(), Target->GetActorLocation()) < CloseRangeRadius;
	if (bCloseRange)
	{
		BeginCloseRange(Target);
		return Phase != ESlimeDevourPhase::Idle;
	}

	CancelHold();
	return false;
}

bool USlimeDevourComponent::TryStartDevour(AEnemyCharacter* Enemy)
{
	if (!CanStartDevour() || !CanDevourTarget(Enemy) || !Body)
	{
		return false;
	}
	BeginLatch(Enemy);
	return DevourTarget.IsValid();
}

bool USlimeDevourComponent::PrepareDevourTarget(AEnemyCharacter* Enemy)
{
	if (!Enemy || !Body || !CanDevourTarget(Enemy))
	{
		return false;
	}

	if (USlimeClingComponent* Cling = GetOwner() ? GetOwner()->FindComponentByClass<USlimeClingComponent>() : nullptr)
	{
		if (Cling->IsClinging())
		{
			Cling->TryDetach();
		}
	}
	if (Body->IsSpreading())
	{
		Body->SetSpread(false);
	}
	if (Combat)
	{
		Combat->InterruptCombat();
	}
	ClearOwnerLockOn();

	DevourTarget = Enemy;
	PendingDestroyEnemy = Enemy;
	FreezeDevourTarget(Enemy);
	FaceTarget(Enemy);

	FBox MeshBox;
	if (!GetEnemyMeshBox(Enemy, MeshBox))
	{
		MeshBox = FBox::BuildAABB(Enemy->GetActorLocation(), FVector(40.f));
	}
	const float Radius = FMath::Max(float(MeshBox.GetExtent().Size()), 10.f);
	const float Want = GetBlobRadius() * FMath::Clamp(ShrinkFitFraction, 0.2f, 1.f);
	const float ScaleMul = FMath::Min(Want / Radius, 1.f);
	EnemyStartScale = Enemy->GetActorScale3D();
	EnemyTargetScale = EnemyStartScale * ScaleMul;

	LatchShotIds.Reset();
	LatchPinned.Reset();
	LatchLaunchIndex = 0;
	CloseRangeShotId = 0;
	if (Body->HasFragments())
	{
		Body->ClearFragments();
	}
	return true;
}

void USlimeDevourComponent::BeginCloseRange(AEnemyCharacter* Enemy)
{
	if (!PrepareDevourTarget(Enemy))
	{
		AbortDevour(true);
		return;
	}
	ClearOwnerLockOn();
	EnterPhase(ESlimeDevourPhase::CloseRangeShrink);
}

void USlimeDevourComponent::SetCloseRangeCameraLag(bool bBoost)
{
	ASlimeFableCharacter* Slime = Cast<ASlimeFableCharacter>(GetOwner());
	USpringArmComponent* Boom = Slime ? Slime->GetCameraBoom() : nullptr;
	if (!Boom)
	{
		return;
	}
	if (bBoost)
	{
		if (!bCameraLagBoosted)
		{
			SavedCameraLagSpeed = Boom->CameraLagSpeed;
			bCameraLagBoosted = true;
		}
		Boom->bEnableCameraLag = true;
		Boom->CameraLagSpeed = CloseRangeDashCameraLag;
	}
	else if (bCameraLagBoosted)
	{
		Boom->CameraLagSpeed = SavedCameraLagSpeed;
		bCameraLagBoosted = false;
	}
}

void USlimeDevourComponent::SetOwnerMovementFrozen(bool bFrozen)
{
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Move = OwnerChar ? OwnerChar->GetCharacterMovement() : nullptr;
	if (!Move)
	{
		bOwnerMovementFrozen = false;
		return;
	}

	if (bFrozen)
	{
		if (!bOwnerMovementFrozen)
		{
			Move->StopMovementImmediately();
			Move->Velocity = FVector::ZeroVector;
			Move->SetMovementMode(MOVE_Flying);
			bOwnerMovementFrozen = true;
		}
		else
		{
			Move->Velocity = FVector::ZeroVector;
		}
	}
	else if (bOwnerMovementFrozen)
	{
		Move->Velocity = FVector::ZeroVector;
		Move->SetMovementMode(MOVE_Falling);
		bOwnerMovementFrozen = false;
	}
}

void USlimeDevourComponent::BeginCloseRangeDash(AEnemyCharacter* Enemy)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Enemy)
	{
		AbortDevour(true);
		return;
	}
	SetOwnerMovementFrozen(true);
	CloseRangeDashStart = Owner->GetActorLocation();
	const FVector BlobOffset = Owner->GetActorLocation() - GetBlobCenter();
	CloseRangeDashEnd = GetWrapCenter(Enemy) + BlobOffset;
	SetCloseRangeCameraLag(true);
	EnterPhase(ESlimeDevourPhase::CloseRangeDash);
}

void USlimeDevourComponent::TickCloseRangeDash(float DeltaTime)
{
	(void)DeltaTime;
	AActor* Owner = GetOwner();
	AEnemyCharacter* Target = DevourTarget.Get();
	if (!Owner || !Target)
	{
		AbortDevour(true);
		return;
	}

	SetOwnerMovementFrozen(true);
	// Align visual blob center onto enemy mesh wrap center (not ActorLocation).
	const FVector BlobOffset = Owner->GetActorLocation() - GetBlobCenter();
	CloseRangeDashEnd = GetWrapCenter(Target) + BlobOffset;
	const float Duration = FMath::Max(CloseRangeDashSeconds, 0.05f);
	const float Alpha = FMath::Clamp(PhaseElapsed / Duration, 0.f, 1.f);
	const float Smooth = Alpha * Alpha * (3.f - 2.f * Alpha);
	const FVector Loc = FMath::Lerp(CloseRangeDashStart, CloseRangeDashEnd, Smooth);
	Owner->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.f)
	{
		Owner->SetActorLocation(CloseRangeDashEnd, false, nullptr, ETeleportType::TeleportPhysics);
		SetCloseRangeCameraLag(false);
		// Hover-wrap at enemy center (owner still frozen), inhale, then swallow in air.
		EnterPhase(ESlimeDevourPhase::Retract);
	}
}

void USlimeDevourComponent::SetEnemyWrapCenter(AEnemyCharacter* Enemy, const FVector& DesiredWrapCenter) const
{
	if (!Enemy)
	{
		return;
	}
	const FVector CurrentWrap = GetWrapCenter(Enemy);
	const FVector ActorLoc = Enemy->GetActorLocation();
	const FVector Offset = CurrentWrap - ActorLoc;
	Enemy->SetActorLocation(DesiredWrapCenter - Offset, false, nullptr, ETeleportType::TeleportPhysics);
	if (UCharacterMovementComponent* Move = Enemy->GetCharacterMovement())
	{
		Move->Velocity = FVector::ZeroVector;
		Move->GravityScale = 0.f;
		Move->SetMovementMode(MOVE_None);
	}
}

FVector USlimeDevourComponent::GetWrapCenter(const AEnemyCharacter* Enemy) const
{
	if (!Enemy)
	{
		return FVector::ZeroVector;
	}
	FBox MeshBox;
	if (GetEnemyMeshBox(Enemy, MeshBox))
	{
		return MeshBox.GetCenter();
	}
	return Enemy->GetVisualBoundsCenter();
}

bool USlimeDevourComponent::IsCloseRangeWrapped(const AEnemyCharacter* Enemy) const
{
	if (!Body || !Enemy || CloseRangeShotId == 0)
	{
		return false;
	}
	const FVector ShotCenter = Body->GetShotCenter(CloseRangeShotId);
	const float Arrive = FMath::Max(GetLatchMiniRadius() * 0.4f, 8.f);
	return FVector::DistSquared(ShotCenter, GetWrapCenter(Enemy)) <= FMath::Square(Arrive);
}

bool USlimeDevourComponent::LaunchCloseRangeWrapper(AEnemyCharacter* Enemy)
{
	if (!Body || !Enemy)
	{
		return false;
	}

	const FVector From = GetBlobCenter();
	const FVector WrapCenter = GetWrapCenter(Enemy);
	const float TravelTime = FMath::Max(LatchSeconds, 0.1f);
	FVector Velocity = (WrapCenter - From) / TravelTime;
	Velocity.Z += 0.5f * FMath::Abs(Body->SolverParams.Gravity) * TravelTime;

	Body->SetLaunchFractionOverride(LatchShotFraction);
	uint8 ShotId = 0;
	if (Body->LaunchDevourShot(Velocity, LatchShotFraction, LatchShotLife, ShotId) <= 0 || ShotId == 0)
	{
		return false;
	}

	CloseRangeShotId = ShotId;
	LatchShotIds.Add(ShotId);
	LatchPinned.Add(1);
	Body->AddIgnoreWorldShot(ShotId);
	Body->SetShotTarget(ShotId, WrapCenter, LatchPullSpeed);
	return true;
}

void USlimeDevourComponent::EnterPhase(ESlimeDevourPhase NewPhase)
{
	Phase = NewPhase;
	PhaseElapsed = 0.f;
	if (NewPhase == ESlimeDevourPhase::Retract)
	{
		BeginRetract(DevourTarget.Get());
	}
}

void USlimeDevourComponent::FaceTarget(AEnemyCharacter* Enemy)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Enemy)
	{
		return;
	}
	FVector To = Enemy->GetActorLocation() - Owner->GetActorLocation();
	To.Z = 0.f;
	if (To.IsNearlyZero())
	{
		return;
	}
	FRotator Rot = Owner->GetActorRotation();
	Rot.Yaw = To.Rotation().Yaw;
	Owner->SetActorRotation(Rot);
}

void USlimeDevourComponent::TickPhase(float DeltaTime)
{
	AEnemyCharacter* Target = DevourTarget.Get();

	if (Phase == ESlimeDevourPhase::Charging
		|| Phase == ESlimeDevourPhase::CloseRangeShrink
		|| Phase == ESlimeDevourPhase::CloseRangeDash
		|| Phase == ESlimeDevourPhase::Latch
		|| Phase == ESlimeDevourPhase::Shrink
		|| Phase == ESlimeDevourPhase::Retract)
	{
		if (!IsTargetStillValid(Target))
		{
			AbortDevour(true);
			return;
		}
	}

	PhaseElapsed += DeltaTime;
	UpdateInnerMesh(DeltaTime);

	if (LatchShotIds.Num() > 0)
	{
		UpdateLatchTargets(Target);
	}

	switch (Phase)
	{
	case ESlimeDevourPhase::Charging:
		{
			FaceTarget(Target);
			// Cancel if Interact/F is no longer held (don't rely solely on InteractComponent poll).
			bool bHoldDown = false;
			if (APlayerController* PC = GetPlayerController())
			{
				const USlimeInputSettings* InputSettings = nullptr;
				if (const UWorld* World = GetWorld())
				{
					if (const UGameInstance* GI = World->GetGameInstance())
					{
						InputSettings = GI->GetSubsystem<USlimeInputSettings>();
					}
				}
				if (InputSettings)
				{
					bHoldDown = InputSettings->IsKeyDown(PC, ESlimeInputAction::Interact);
				}
				else
				{
					bHoldDown = PC->IsInputKeyDown(EKeys::F);
				}
			}
			if (!bHoldDown)
			{
				ReleaseHold();
				return;
			}
			const float Dist = FVector::Dist(GetOwner()->GetActorLocation(), Target->GetActorLocation());
			if (Dist > DevourRadius)
			{
				CancelHold();
				return;
			}
			if (PhaseElapsed >= HoldSeconds)
			{
				BeginLatch(Target);
			}
		}
		break;
	case ESlimeDevourPhase::CloseRangeShrink:
		{
			const float Alpha = FMath::Clamp(PhaseElapsed / FMath::Max(CloseRangeShrinkSeconds, 0.01f), 0.f, 1.f);
			ApplyEnemyShrink(Target, Alpha);
			if (PhaseElapsed >= CloseRangeShrinkSeconds)
			{
				ApplyEnemyShrink(Target, 1.f);
				BeginCloseRangeDash(Target);
			}
		}
		break;
	case ESlimeDevourPhase::CloseRangeDash:
		TickCloseRangeDash(DeltaTime);
		break;
	case ESlimeDevourPhase::Latch:
		TickLatch(Target, DeltaTime);
		break;
	case ESlimeDevourPhase::Shrink:
		{
			const float HoverZ = GetBlobRadius() * 0.35f;
			const FVector Hover = EnemyStartXform.GetLocation() + FVector(0.f, 0.f, HoverZ);
			Target->SetActorLocation(Hover, false, nullptr, ETeleportType::TeleportPhysics);
			const float Alpha = FMath::Clamp(PhaseElapsed / FMath::Max(ShrinkSeconds, 0.01f), 0.f, 1.f);
			ApplyEnemyShrink(Target, Alpha);
			if (PhaseElapsed >= ShrinkSeconds)
			{
				ApplyEnemyShrink(Target, 1.f);
				if (CloseRangeShotId == 0)
				{
					if (!LaunchCloseRangeWrapper(Target))
					{
						AbortDevour(true);
						return;
					}
				}
				EnterPhase(ESlimeDevourPhase::Latch);
			}
		}
		break;
	case ESlimeDevourPhase::Retract:
		TickRetract(DeltaTime);
		break;
	case ESlimeDevourPhase::Digest:
		{
			const float Remaining = DigestSeconds - PhaseElapsed;
			if (Remaining <= DigestDissolveSeconds)
			{
				const float Alpha = DigestDissolveSeconds > KINDA_SMALL_NUMBER
					? FMath::Clamp(Remaining / DigestDissolveSeconds, 0.f, 1.f)
					: 0.f;
				ApplyInnerDissolve(Alpha);
			}
			if (PhaseElapsed >= DigestSeconds)
			{
				FinishDevour();
			}
		}
		break;
	default:
		break;
	}
}

void USlimeDevourComponent::BeginLatch(AEnemyCharacter* Enemy)
{
	if (!PrepareDevourTarget(Enemy))
	{
		AbortDevour(true);
		return;
	}
	ClearOwnerLockOn();
	if (Body->HasFragments())
	{
		Body->ClearFragments();
	}
	// Far devour: shrink first, then one wrap shot (see TickPhase Shrink → Latch).
	EnterPhase(ESlimeDevourPhase::Shrink);
}

void USlimeDevourComponent::TryLaunchNextLatchShot(AEnemyCharacter* Enemy)
{
	const int32 ShotCount = GetActiveLatchShotCount();
	if (!Body || !Enemy || LatchLaunchIndex >= ShotCount)
	{
		return;
	}

	TArray<FVector> AttachPoints;
	GetLatchAttachPoints(Enemy, AttachPoints);
	if (!AttachPoints.IsValidIndex(LatchLaunchIndex))
	{
		return;
	}

	const FVector From = GetBlobCenter();
	const FVector Point = AttachPoints[LatchLaunchIndex];
	FVector Delta = Point - From;
	const float TravelTime = FMath::Max(LatchSeconds, 0.1f);
	FVector Velocity = Delta / TravelTime;
	const float GravityAbs = FMath::Abs(Body->SolverParams.Gravity);
	Velocity.Z += 0.5f * GravityAbs * TravelTime + LatchArcHeight / TravelTime;

	uint8 ShotId = 0;
	if (Body->LaunchDevourShot(Velocity, LatchShotFraction, LatchShotLife, ShotId) > 0 && ShotId != 0)
	{
		LatchShotIds.Add(ShotId);
		LatchPinned.Add(0);
		Body->AddIgnoreWorldShot(ShotId);
		++LatchLaunchIndex;
	}
}

void USlimeDevourComponent::TickLatch(AEnemyCharacter* Enemy, float DeltaTime)
{
	(void)DeltaTime;
	if (!Enemy)
	{
		return;
	}

	// Single wrap-ball path after Shrink: wait until wrapped or flight timeout, then retract.
	if (CloseRangeShotId != 0)
	{
		if (IsCloseRangeWrapped(Enemy) || PhaseElapsed >= LatchSeconds)
		{
			EnterPhase(ESlimeDevourPhase::Retract);
		}
		return;
	}

	const int32 ShotCount = GetActiveLatchShotCount();
	if (LatchLaunchIndex < ShotCount && PhaseElapsed + KINDA_SMALL_NUMBER >= float(LatchLaunchIndex) * LatchStaggerSeconds)
	{
		TryLaunchNextLatchShot(Enemy);
	}

	const bool bAllLaunched = LatchLaunchIndex >= ShotCount;
	const float MaxLatch = float(ShotCount) * LatchStaggerSeconds + LatchSeconds;
	const float LastLaunchTime = float(FMath::Max(LatchLaunchIndex - 1, 0)) * LatchStaggerSeconds;
	const bool bFlightExpired = bAllLaunched && PhaseElapsed >= LastLaunchTime + LatchSeconds;
	if (bFlightExpired || PhaseElapsed >= MaxLatch)
	{
		EnterPhase(ESlimeDevourPhase::Retract);
	}
}

float USlimeDevourComponent::GetLatchMiniRadius() const
{
	const float Fraction = FMath::Clamp(LatchShotFraction, 0.05f, 0.6f);
	return GetBlobRadius() * FMath::Pow(Fraction, 1.f / 3.f);
}

bool USlimeDevourComponent::GetEnemyMeshBox(const AEnemyCharacter* Enemy, FBox& OutBox) const
{
	OutBox = FBox(ForceInit);
	if (!Enemy)
	{
		return false;
	}

	bool bAny = false;
	auto Accumulate = [&](UPrimitiveComponent* Prim)
	{
		if (!Prim || Prim->bHiddenInGame || !Prim->IsVisible())
		{
			return;
		}
		OutBox += Prim->Bounds.GetBox();
		bAny = true;
	};

	if (USkeletalMeshComponent* Skel = Enemy->GetMesh())
	{
		if (Skel->GetSkeletalMeshAsset() != nullptr)
		{
			Accumulate(Skel);
		}
	}
	for (const TObjectPtr<USceneComponent>& Part : Enemy->GetGeneratedParts())
	{
		Accumulate(Cast<UPrimitiveComponent>(Part.Get()));
	}
	if (Enemy->GetPlaceholderMesh())
	{
		Accumulate(Enemy->GetPlaceholderMesh());
	}
	return bAny;
}

bool USlimeDevourComponent::TryGetLatchBoneLocation(const USkeletalMeshComponent* Skel, const TArray<FName>& Names, FVector& OutLocation) const
{
	if (!Skel || !Skel->GetSkeletalMeshAsset())
	{
		return false;
	}
	for (const FName Name : Names)
	{
		if (Name.IsNone())
		{
			continue;
		}
		if (Skel->GetBoneIndex(Name) != INDEX_NONE || Skel->DoesSocketExist(Name))
		{
			OutLocation = Skel->GetSocketLocation(Name);
			return true;
		}
	}
	return false;
}

FVector USlimeDevourComponent::MakeLatchPointFromAnchor(const AEnemyCharacter* Enemy, const FVector& Anchor, const FVector& Axis, float MiniRadius) const
{
	const FVector Dir = Axis.GetSafeNormal();
	const float StickOffset = MiniRadius * FMath::Clamp(LatchStickOffsetFraction, 0.05f, 1.f);
	constexpr float BoneSkin = 12.f;
	const FVector Fallback = Anchor + Dir * (StickOffset + BoneSkin);
	if (!Enemy || Dir.IsNearlyZero())
	{
		return Fallback;
	}

	const FVector Start = Anchor + Dir * 80.f;
	const FVector End = Anchor;
	FHitResult Best;
	bool bHit = false;
	float BestDistSq = TNumericLimits<float>::Max();

	auto TraceComp = [&](UPrimitiveComponent* Comp)
	{
		if (!Comp)
		{
			return;
		}
		const ECollisionEnabled::Type Saved = Comp->GetCollisionEnabled();
		if (Saved == ECollisionEnabled::NoCollision)
		{
			Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DevourLatchAttach), true);
		Params.bTraceComplex = true;
		const bool bGotHit = Comp->LineTraceComponent(Hit, Start, End, Params);
		if (Saved == ECollisionEnabled::NoCollision)
		{
			Comp->SetCollisionEnabled(Saved);
		}
		if (!bGotHit)
		{
			return;
		}
		const float DistSq = FVector::DistSquared(Start, Hit.ImpactPoint);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Hit;
			bHit = true;
		}
	};

	if (USkeletalMeshComponent* Skel = Enemy->GetMesh())
	{
		if (Skel->GetSkeletalMeshAsset())
		{
			TraceComp(Skel);
		}
	}
	for (const TObjectPtr<USceneComponent>& Part : Enemy->GetGeneratedParts())
	{
		TraceComp(Cast<UPrimitiveComponent>(Part.Get()));
	}
	TraceComp(Enemy->GetPlaceholderMesh());

	if (!bHit)
	{
		return Fallback;
	}

	FVector Normal = Best.ImpactNormal;
	if (Normal.IsNearlyZero() || (Normal | Dir) > 0.f)
	{
		Normal = Dir;
	}
	return Best.ImpactPoint + Normal.GetSafeNormal() * StickOffset;
}

FVector USlimeDevourComponent::TraceMeshAttachPoint(const AEnemyCharacter* Enemy, const FVector& Center, const FVector& Axis, float ExtentAlong, float MiniRadius) const
{
	const FVector Dir = Axis.GetSafeNormal();
	const float StickOffset = MiniRadius * FMath::Clamp(LatchStickOffsetFraction, 0.05f, 1.f);
	const FVector Fallback = Center + Dir * (ExtentAlong + StickOffset);
	if (!Enemy || Dir.IsNearlyZero())
	{
		return Fallback;
	}

	const FVector Start = Center + Dir * (ExtentAlong + 80.f);
	const FVector End = Center;
	FHitResult Best;
	bool bHit = false;
	float BestDistSq = TNumericLimits<float>::Max();

	auto TraceComp = [&](UPrimitiveComponent* Comp)
	{
		if (!Comp)
		{
			return;
		}
		const ECollisionEnabled::Type Saved = Comp->GetCollisionEnabled();
		if (Saved == ECollisionEnabled::NoCollision)
		{
			Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DevourLatchAttachMesh), true);
		Params.bTraceComplex = true;
		const bool bGotHit = Comp->LineTraceComponent(Hit, Start, End, Params);
		if (Saved == ECollisionEnabled::NoCollision)
		{
			Comp->SetCollisionEnabled(Saved);
		}
		if (!bGotHit)
		{
			return;
		}
		const float DistSq = FVector::DistSquared(Start, Hit.ImpactPoint);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Hit;
			bHit = true;
		}
	};

	if (USkeletalMeshComponent* Skel = Enemy->GetMesh())
	{
		if (Skel->GetSkeletalMeshAsset())
		{
			TraceComp(Skel);
		}
	}
	for (const TObjectPtr<USceneComponent>& Part : Enemy->GetGeneratedParts())
	{
		TraceComp(Cast<UPrimitiveComponent>(Part.Get()));
	}
	TraceComp(Enemy->GetPlaceholderMesh());

	if (!bHit)
	{
		return Fallback;
	}

	FVector Normal = Best.ImpactNormal;
	if (Normal.IsNearlyZero() || (Normal | Dir) > 0.f)
	{
		Normal = Dir;
	}
	return Best.ImpactPoint + Normal.GetSafeNormal() * StickOffset;
}

void USlimeDevourComponent::GetLatchAttachPoints(const AEnemyCharacter* Enemy, TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();
	if (!Enemy)
	{
		return;
	}

	FVector Forward = Enemy->GetActorForwardVector();
	Forward.Z = 0.f;
	Forward = Forward.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	const FVector Up = FVector::UpVector;
	const float MiniR = GetLatchMiniRadius();

	const TArray<FName> HeadNames = {
		TEXT("head"), TEXT("Head"), TEXT("neck_01"), TEXT("Neck"), TEXT("neck_02")
	};
	const TArray<FName> LeftNames = {
		TEXT("upperarm_l"), TEXT("UpperArm_L"), TEXT("clavicle_l"), TEXT("Clavicle_L"),
		TEXT("shoulder_l"), TEXT("LeftArm"), TEXT("upperarm_l_twist")
	};
	const TArray<FName> RightNames = {
		TEXT("upperarm_r"), TEXT("UpperArm_R"), TEXT("clavicle_r"), TEXT("Clavicle_R"),
		TEXT("shoulder_r"), TEXT("RightArm"), TEXT("upperarm_r_twist")
	};

	USkeletalMeshComponent* Skel = Enemy->GetMesh();
	FVector HeadAnchor = FVector::ZeroVector;
	FVector LeftAnchor = FVector::ZeroVector;
	FVector RightAnchor = FVector::ZeroVector;
	const bool bHead = TryGetLatchBoneLocation(Skel, HeadNames, HeadAnchor);
	const bool bLeft = TryGetLatchBoneLocation(Skel, LeftNames, LeftAnchor);
	const bool bRight = TryGetLatchBoneLocation(Skel, RightNames, RightAnchor);
	if (bHead && bLeft && bRight)
	{
		OutPoints.Add(MakeLatchPointFromAnchor(Enemy, HeadAnchor, Up, MiniR));
		OutPoints.Add(MakeLatchPointFromAnchor(Enemy, LeftAnchor, -Right, MiniR));
		OutPoints.Add(MakeLatchPointFromAnchor(Enemy, RightAnchor, Right, MiniR));
		return;
	}

	FBox MeshBox;
	FVector Center = Enemy->GetVisualBoundsCenter();
	FVector Extent(40.f, 40.f, 40.f);
	if (GetEnemyMeshBox(Enemy, MeshBox))
	{
		Center = MeshBox.GetCenter();
		Extent = MeshBox.GetExtent();
	}
	auto Along = [&Extent](const FVector& Axis) -> float
	{
		return FMath::Max(float(FMath::Abs(Extent | Axis)), 12.f);
	};
	OutPoints.Add(TraceMeshAttachPoint(Enemy, Center, Up, Along(Up), MiniR));
	OutPoints.Add(TraceMeshAttachPoint(Enemy, Center, -Right, Along(Right), MiniR));
	OutPoints.Add(TraceMeshAttachPoint(Enemy, Center, Right, Along(Right), MiniR));
}

void USlimeDevourComponent::UpdateLatchTargets(AEnemyCharacter* Enemy)
{
	if (!Body || !Enemy || LatchShotIds.Num() == 0)
	{
		return;
	}

	if (CloseRangeShotId != 0)
	{
		const FVector WrapCenter = GetWrapCenter(Enemy);
		for (int32 Index = 0; Index < LatchShotIds.Num(); ++Index)
		{
			if (LatchPinned.IsValidIndex(Index))
			{
				LatchPinned[Index] = 1;
			}
			Body->SetShotTarget(LatchShotIds[Index], WrapCenter, LatchPullSpeed);
		}
		return;
	}

	TArray<FVector> AttachPoints;
	GetLatchAttachPoints(Enemy, AttachPoints);
	const int32 Count = FMath::Min(LatchShotIds.Num(), AttachPoints.Num());
	constexpr float PinDistance = 24.f;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector Point = AttachPoints[Index];
		if (!LatchPinned.IsValidIndex(Index) || LatchPinned[Index] == 0)
		{
			const FVector ShotCenter = Body->GetShotCenter(LatchShotIds[Index]);
			const float LaunchTime = float(Index) * LatchStaggerSeconds;
			const bool bFlightDone = Phase != ESlimeDevourPhase::Latch
				|| PhaseElapsed >= LaunchTime + LatchSeconds;
			if (!bFlightDone && FVector::DistSquared(ShotCenter, Point) > FMath::Square(PinDistance))
			{
				continue;
			}
			if (LatchPinned.IsValidIndex(Index))
			{
				LatchPinned[Index] = 1;
			}
		}
		Body->SetShotTarget(LatchShotIds[Index], Point, LatchPullSpeed);
	}
}

int32 USlimeDevourComponent::GetActiveLatchShotCount() const
{
	return FMath::Clamp(LatchShotCount, 1, 3);
}

void USlimeDevourComponent::BeginRetract(AEnemyCharacter* Enemy)
{
	if (!Enemy)
	{
		return;
	}

	RetractStartWrapCenter = GetWrapCenter(Enemy);
	RetractStartLocation = Enemy->GetActorLocation();
	const FVector Home = GetBlobCenter();
	float MaxDist = FVector::Dist(RetractStartWrapCenter, Home);
	if (Body)
	{
		for (const uint8 ShotId : LatchShotIds)
		{
			MaxDist = FMath::Max(MaxDist, FVector::Dist(Body->GetShotCenter(ShotId), Home));
		}
		const float Duration = FMath::Max(RetractSeconds, 0.2f);
		Body->SetRecallPullSpeedOverride(FMath::Max(MaxDist / Duration, 80.f));
	}
}

void USlimeDevourComponent::TickRetract(float DeltaTime)
{
	(void)DeltaTime;
	AEnemyCharacter* Target = DevourTarget.Get();
	if (!Target)
	{
		AbortDevour(true);
		return;
	}

	const FVector Home = GetBlobCenter();
	const float Duration = FMath::Max(RetractSeconds, 0.2f);
	const float Alpha = FMath::Clamp(PhaseElapsed / Duration, 0.f, 1.f);
	if (Alpha >= 1.f)
	{
		SetEnemyWrapCenter(Target, Home);
	}
	else
	{
		const float Smooth = Alpha * Alpha * (3.f - 2.f * Alpha);
		const FVector DesiredWrap = FMath::Lerp(RetractStartWrapCenter, Home, Smooth);
		SetEnemyWrapCenter(Target, DesiredWrap);
	}

	constexpr float InhaleAlignCm = 12.f;
	if (Alpha >= 1.f && FVector::Dist(GetWrapCenter(Target), GetBlobCenter()) <= InhaleAlignCm)
	{
		SwallowTarget();
	}
}

void USlimeDevourComponent::ApplyEnemyShrink(AEnemyCharacter* Enemy, float Alpha) const
{
	if (!Enemy)
	{
		return;
	}
	const float Ease = FMath::Clamp(Alpha, 0.f, 1.f);
	const float Smooth = Ease * Ease * (3.f - 2.f * Ease);
	Enemy->SetActorScale3D(FMath::Lerp(EnemyStartScale, EnemyTargetScale, Smooth));
}

void USlimeDevourComponent::SwallowTarget()
{
	AEnemyCharacter* Target = DevourTarget.Get();
	if (!IsValid(Target))
	{
		AbortDevour(true);
		return;
	}

	if (USoundBase* Sfx = SwallowSound.LoadSynchronous())
	{
		SlimeAudioPlay::PlaySfxAt(GetOwner(), Sfx, GetBlobCenter());
	}
	else if (USoundBase* Fallback = LoadObject<USoundBase>(
		nullptr, TEXT("/Game/Audio/SFX/Combat/sfx_swallow_01.sfx_swallow_01")))
	{
		SlimeAudioPlay::PlaySfxAt(GetOwner(), Fallback, GetBlobCenter());
	}

	CaptureEnemy(Target, ActiveCapture);
	PushPhantomSlot(ActiveCapture);
	SpawnInnerMesh(ActiveCapture);

	Target->ClearElementAuraFlash();
	if (USlimeStatusComponent* EnemyStatus = Target->GetEnemyStatus())
	{
		EnemyStatus->ClearAllAuras();
	}

	Target->BeginDevouredDeath(GetOwner());
	PendingDestroyEnemy.Reset();
	DevourTarget.Reset();
	bSavedEnemyGravity = false;
	Target->Destroy();

	SetOwnerMovementFrozen(false);

	if (Body)
	{
		Body->ClearShotTargets();
		Body->SetRecalling(true);
	}
	LatchShotIds.Reset();
	EnterPhase(ESlimeDevourPhase::Digest);
}

void USlimeDevourComponent::CleanupLatchShots()
{
	LatchShotIds.Reset();
	LatchPinned.Reset();
	CloseRangeShotId = 0;
	LatchLaunchIndex = 0;
	if (Body)
	{
		Body->ClearShotTargets();
		Body->ClearIgnoreWorldShots();
		Body->ClearRecallPullSpeedOverride();
		Body->ClearFragments();
		Body->ClearLaunchFractionOverride();
	}
}

void USlimeDevourComponent::FreezeDevourTarget(AEnemyCharacter* Enemy)
{
	if (!Enemy)
	{
		return;
	}
	EnemyStartXform = Enemy->GetActorTransform();
	EnemyStartScale = Enemy->GetActorScale3D();
	Enemy->SetDevourLocked(true);
	if (UEnemyCombatComponent* EnemyCombatComp = Enemy->GetEnemyCombat())
	{
		EnemyCombatComp->InterruptCombat();
	}
	Enemy->StopMeshAnimation();
	if (USkeletalMeshComponent* Skel = Enemy->GetMesh())
	{
		if (UAnimInstance* Anim = Skel->GetAnimInstance())
		{
			Anim->Montage_Stop(0.1f);
			Anim->StopAllMontages(0.1f);
		}
	}
	HideEnemyWidgets(Enemy, true);
	Enemy->SetActorEnableCollision(false);
	if (UCharacterMovementComponent* Move = Enemy->GetCharacterMovement())
	{
		if (!bSavedEnemyGravity)
		{
			SavedEnemyGravityScale = Move->GravityScale;
			bSavedEnemyGravity = true;
		}
		Move->StopMovementImmediately();
		Move->Velocity = FVector::ZeroVector;
		Move->GravityScale = 0.f;
		Move->SetMovementMode(MOVE_None);
		Move->DisableMovement();
	}
	if (AController* AI = Enemy->GetController())
	{
		AI->StopMovement();
		AI->SetActorTickEnabled(false);
		if (AAIController* AIC = Cast<AAIController>(AI))
		{
			if (UBrainComponent* Brain = AIC->GetBrainComponent())
			{
				Brain->StopLogic(TEXT("Devour"));
			}
		}
	}
}

void USlimeDevourComponent::RestoreDevourTarget(AEnemyCharacter* Enemy)
{
	if (!IsValid(Enemy) || Enemy->IsDevouredDeath() || Enemy->IsInDeathSequence())
	{
		return;
	}
	Enemy->SetActorTransform(EnemyStartXform, false, nullptr, ETeleportType::TeleportPhysics);
	Enemy->SetActorScale3D(EnemyStartScale);
	Enemy->SetActorHiddenInGame(false);
	Enemy->SetActorEnableCollision(true);
	Enemy->SetDevourLocked(false);
	HideEnemyWidgets(Enemy, false);
	if (UCharacterMovementComponent* Move = Enemy->GetCharacterMovement())
	{
		if (bSavedEnemyGravity)
		{
			Move->GravityScale = SavedEnemyGravityScale;
			bSavedEnemyGravity = false;
		}
		Move->SetDefaultMovementMode();
	}
	if (AController* AI = Enemy->GetController())
	{
		AI->SetActorTickEnabled(true);
	}
}

void USlimeDevourComponent::ClearOwnerLockOn() const
{
	if (AActor* Owner = GetOwner())
	{
		if (USlimeLockOnComponent* Lock = Owner->FindComponentByClass<USlimeLockOnComponent>())
		{
			Lock->ClearLockOn();
		}
	}
}

void USlimeDevourComponent::HideEnemyWidgets(AEnemyCharacter* Enemy, bool bHide) const
{
	if (!Enemy)
	{
		return;
	}
	TArray<UWidgetComponent*> Widgets;
	Enemy->GetComponents<UWidgetComponent>(Widgets);
	for (UWidgetComponent* Widget : Widgets)
	{
		if (Widget)
		{
			Widget->SetHiddenInGame(bHide);
			Widget->SetVisibility(!bHide);
		}
	}
}

void USlimeDevourComponent::CaptureEnemy(AEnemyCharacter* Enemy, FSlimeDevourCapture& OutCapture) const
{
	OutCapture = FSlimeDevourCapture();
	if (!Enemy)
	{
		return;
	}
	OutCapture.EnemyClass = Enemy->GetClass();
	OutCapture.DisplayName = Enemy->GetResolvedDisplayName();
	OutCapture.CaptureWorldXform = Enemy->GetActorTransform();
	OutCapture.WheelTint = Enemy->ResolveDevourWheelTint();

	auto FillBounds = [&OutCapture](UMeshComponent* Comp)
	{
		if (!Comp)
		{
			return;
		}
		const FBoxSphereBounds Local = Comp->GetLocalBounds();
		OutCapture.CaptureBoundsOrigin = Local.Origin;
		OutCapture.CaptureRadius = FMath::Max(Local.SphereRadius, 10.f);
		OutCapture.CaptureWorldXform = Comp->GetComponentTransform();
	};

	USkeletalMeshComponent* Skel = Enemy->GetMesh();
	if (Skel && Skel->GetSkeletalMeshAsset() && Skel->IsVisible())
	{
		BuildCaptureFromMesh(Skel, nullptr, OutCapture);
		FillBounds(Skel);
	}
	if (!OutCapture.SkeletalMesh && !OutCapture.StaticMesh)
	{
		if (Skel && Skel->GetSkeletalMeshAsset())
		{
			BuildCaptureFromMesh(Skel, nullptr, OutCapture);
			FillBounds(Skel);
		}
	}
	if (!OutCapture.SkeletalMesh && !OutCapture.StaticMesh)
	{
		for (USceneComponent* Part : Enemy->GetGeneratedParts())
		{
			if (USkeletalMeshComponent* PartSkel = Cast<USkeletalMeshComponent>(Part))
			{
				if (BuildCaptureFromMesh(PartSkel, nullptr, OutCapture))
				{
					FillBounds(PartSkel);
					break;
				}
			}
			if (UStaticMeshComponent* PartStatic = Cast<UStaticMeshComponent>(Part))
			{
				if (BuildCaptureFromMesh(nullptr, PartStatic, OutCapture))
				{
					FillBounds(PartStatic);
					break;
				}
			}
		}
	}
	if (!OutCapture.SkeletalMesh && !OutCapture.StaticMesh)
	{
		BuildCaptureFromMesh(nullptr, Enemy->GetPlaceholderMesh(), OutCapture);
		FillBounds(Enemy->GetPlaceholderMesh());
	}
}

bool USlimeDevourComponent::BuildCaptureFromMesh(USkeletalMeshComponent* Skel, UStaticMeshComponent* StaticMeshComp, FSlimeDevourCapture& OutCapture) const
{
	if (Skel && Skel->GetSkeletalMeshAsset())
	{
		OutCapture.SkeletalMesh = Skel->GetSkeletalMeshAsset();
		OutCapture.SkeletalMaterials.Reset();
		const int32 Mats = Skel->GetNumMaterials();
		for (int32 Index = 0; Index < Mats; ++Index)
		{
			OutCapture.SkeletalMaterials.Add(Skel->GetMaterial(Index));
		}
		OutCapture.CaptureWorldXform = Skel->GetComponentTransform();
		return true;
	}
	if (StaticMeshComp && StaticMeshComp->GetStaticMesh())
	{
		OutCapture.StaticMesh = StaticMeshComp->GetStaticMesh();
		OutCapture.StaticMaterials.Reset();
		const int32 Mats = StaticMeshComp->GetNumMaterials();
		for (int32 Index = 0; Index < Mats; ++Index)
		{
			OutCapture.StaticMaterials.Add(StaticMeshComp->GetMaterial(Index));
		}
		OutCapture.CaptureWorldXform = StaticMeshComp->GetComponentTransform();
		return true;
	}
	return false;
}

void USlimeDevourComponent::PushPhantomSlot(const FSlimeDevourCapture& Capture)
{
	if (!Capture.IsValidCapture())
	{
		return;
	}
	PhantomSlots.Insert(Capture, 0);
	while (PhantomSlots.Num() > PhantomSlotCapacity)
	{
		PhantomSlots.RemoveAt(PhantomSlots.Num() - 1);
	}
	SelectedPhantomSlot = 0;
}

void USlimeDevourComponent::ConfigureInnerMesh(UMeshComponent* Comp) const
{
	if (!Comp)
	{
		return;
	}
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetGenerateOverlapEvents(false);
	Comp->SetCastShadow(false);
	Comp->SetCanEverAffectNavigation(false);
	Comp->SetVisibility(true);
	Comp->SetHiddenInGame(false);
	Comp->SetBoundsScale(8.f);
	Comp->bUseAttachParentBound = false;
	Comp->SetUsingAbsoluteLocation(true);
	Comp->SetUsingAbsoluteRotation(true);
	Comp->SetUsingAbsoluteScale(true);
}

void USlimeDevourComponent::SpawnInnerMesh(const FSlimeDevourCapture& Capture)
{
	DestroyInnerMesh();
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (Capture.SkeletalMesh)
	{
		InnerPoseable = NewObject<UPoseableMeshComponent>(Owner, TEXT("DevourInnerPoseable"), RF_Transient);
		InnerPoseable->SetupAttachment(Owner->GetRootComponent());
		ConfigureInnerMesh(InnerPoseable);
		InnerPoseable->RegisterComponent();
		InnerPoseable->SetSkinnedAssetAndUpdate(Capture.SkeletalMesh);
		for (int32 Index = 0; Index < Capture.SkeletalMaterials.Num(); ++Index)
		{
			if (Capture.SkeletalMaterials[Index])
			{
				InnerPoseable->SetMaterial(Index, Capture.SkeletalMaterials[Index]);
			}
		}
		if (AEnemyCharacter* Target = DevourTarget.Get())
		{
			USkeletalMeshComponent* Source = Target->GetMesh();
			if (!Source || !Source->GetSkeletalMeshAsset())
			{
				for (USceneComponent* Part : Target->GetGeneratedParts())
				{
					if (USkeletalMeshComponent* PartSkel = Cast<USkeletalMeshComponent>(Part))
					{
						Source = PartSkel;
						break;
					}
				}
			}
			if (Source && Source->GetSkeletalMeshAsset())
			{
				InnerPoseable->CopyPoseFromSkeletalComponent(Source);
			}
		}
		InnerPoseable->SetWorldTransform(MakeFittedInnerTransform());
	}
	else if (Capture.StaticMesh)
	{
		InnerStatic = NewObject<UStaticMeshComponent>(Owner, TEXT("DevourInnerStatic"), RF_Transient);
		InnerStatic->SetupAttachment(Owner->GetRootComponent());
		ConfigureInnerMesh(InnerStatic);
		InnerStatic->RegisterComponent();
		InnerStatic->SetStaticMesh(Capture.StaticMesh);
		for (int32 Index = 0; Index < Capture.StaticMaterials.Num(); ++Index)
		{
			if (Capture.StaticMaterials[Index])
			{
				InnerStatic->SetMaterial(Index, Capture.StaticMaterials[Index]);
			}
		}
		InnerStatic->SetWorldTransform(MakeFittedInnerTransform());
	}
}

void USlimeDevourComponent::UpdateInnerMesh(float DeltaTime)
{
	USceneComponent* Inner = InnerPoseable ? static_cast<USceneComponent*>(InnerPoseable) : static_cast<USceneComponent*>(InnerStatic);
	if (!Inner)
	{
		return;
	}

	InnerWobble += DeltaTime;
	const float WobbleZ = FMath::Sin(InnerWobble * 3.f) * 2.f;
	Inner->SetWorldTransform(MakeFittedInnerTransform(WobbleZ));
}

FTransform USlimeDevourComponent::MakeFittedInnerTransform(float ExtraZ) const
{
	const float CaptureR = FMath::Max(ActiveCapture.CaptureRadius, 10.f);
	const float Fit = FMath::Max(GetBlobRadius() * FMath::Clamp(InnerFitFraction, 0.2f, 1.f) / CaptureR, 0.02f);
	const float Lift = GetBlobRadius() * FMath::Clamp(InnerMeshLiftFraction, 0.f, 1.f);
	const FVector Center = GetBlobCenter() + FVector(0.f, 0.f, ExtraZ + Lift);

	FTransform Xform = ActiveCapture.CaptureWorldXform;
	Xform.SetRotation(ActiveCapture.CaptureWorldXform.GetRotation() * FQuat(FVector::UpVector, InnerWobble * 0.6f));
	Xform.SetScale3D(FVector(Fit));
	const FVector BoundsWorld = Xform.TransformPosition(ActiveCapture.CaptureBoundsOrigin);
	Xform.AddToTranslation(Center - BoundsWorld);
	return Xform;
}

void USlimeDevourComponent::ApplyInnerDissolve(float Alpha)
{
	auto Fade = [Alpha, this](UMeshComponent* Comp)
	{
		if (!Comp)
		{
			return;
		}
		if (Alpha <= 0.02f)
		{
			Comp->SetVisibility(false);
			return;
		}
		Comp->SetVisibility(true);
		const int32 Mats = Comp->GetNumMaterials();
		for (int32 Index = 0; Index < Mats; ++Index)
		{
			UMaterialInterface* Base = DigestDissolveMaterial ? DigestDissolveMaterial.Get() : Comp->GetMaterial(Index);
			if (!Base)
			{
				continue;
			}
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(Index));
			if (!MID)
			{
				MID = Comp->CreateAndSetMaterialInstanceDynamic(Index);
			}
			if (MID)
			{
				MID->SetScalarParameterValue(TEXT("DissolveLine"), 1.f - Alpha);
				MID->SetScalarParameterValue(TEXT("Opacity"), Alpha);
			}
		}
	};
	Fade(InnerPoseable);
	Fade(InnerStatic);
}

void USlimeDevourComponent::DestroyInnerMesh()
{
	if (InnerPoseable)
	{
		InnerPoseable->DestroyComponent();
		InnerPoseable = nullptr;
	}
	if (InnerStatic)
	{
		InnerStatic->DestroyComponent();
		InnerStatic = nullptr;
	}
}

void USlimeDevourComponent::RestoreBody()
{
	if (Body)
	{
		Body->SetBodyScale(1.f, true);
		Body->ClearCombatPose();
		Body->SetExternalMoveSpeedScale(1.f);
		Body->SetExternalJumpScale(1.f);
	}
}

void USlimeDevourComponent::FinishDevour()
{
	DestroyInnerMesh();
	RestoreBody();
	CleanupLatchShots();
	DevourTarget.Reset();
	PendingDestroyEnemy.Reset();
	Phase = ESlimeDevourPhase::Idle;
	PhaseElapsed = 0.f;
}

void USlimeDevourComponent::AbortDevour(bool bRestoreBody)
{
	SetCloseRangeCameraLag(false);
	SetOwnerMovementFrozen(false);
	if (AEnemyCharacter* Live = DevourTarget.Get())
	{
		RestoreDevourTarget(Live);
	}
	bSavedEnemyGravity = false;
	if (AEnemyCharacter* Pending = Cast<AEnemyCharacter>(PendingDestroyEnemy.Get()))
	{
		if (IsValid(Pending) && Pending->IsDevouredDeath())
		{
			Pending->Destroy();
		}
	}
	PendingDestroyEnemy.Reset();
	DevourTarget.Reset();
	DestroyInnerMesh();
	CleanupLatchShots();
	if (bRestoreBody)
	{
		RestoreBody();
	}
	Phase = ESlimeDevourPhase::Idle;
	PhaseElapsed = 0.f;
}

void USlimeDevourComponent::CyclePhantomSelection(int32 Step)
{
	if (Step == 0 || PhantomSlotCapacity <= 0)
	{
		return;
	}
	SelectedPhantomSlot = (SelectedPhantomSlot + Step) % PhantomSlotCapacity;
	if (SelectedPhantomSlot < 0)
	{
		SelectedPhantomSlot += PhantomSlotCapacity;
	}
	if (PhantomWheelWidget)
	{
		PhantomWheelWidget->SetSlots(PhantomSlots, SelectedPhantomSlot, PhantomSlotCapacity);
	}
}

bool USlimeDevourComponent::TryOpenPhantomWheel()
{
	if (bPhantomWheelOpen || PhantomSlots.Num() == 0 || IsCombatLocked())
	{
		return false;
	}
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return false;
	}
	if (const USlimeVehicleComponent* Vehicle = Pawn->FindComponentByClass<USlimeVehicleComponent>())
	{
		if (Vehicle->IsUsingVehicle())
		{
			return false;
		}
	}
	if (const USlimeAbilityComponent* Abilities = Pawn->FindComponentByClass<USlimeAbilityComponent>())
	{
		if (Abilities->IsWheelOpen() || Abilities->IsChargingLaunch())
		{
			return false;
		}
	}
	if (const USlimePlacementComponent* Placement = Pawn->FindComponentByClass<USlimePlacementComponent>())
	{
		if (Placement->IsPlacing())
		{
			return false;
		}
	}

	APlayerController* PC = GetPlayerController();
	if (!PC)
	{
		return false;
	}
	if (!PhantomWheelWidget)
	{
		PhantomWheelWidget = CreateWidget<USlimePhantomWheelWidget>(PC, USlimePhantomWheelWidget::StaticClass());
		if (!PhantomWheelWidget)
		{
			return false;
		}
	}

	SelectedPhantomSlot = 0;
	bPhantomWheelOpen = true;
	PhantomWheelWidget->SetSlots(PhantomSlots, SelectedPhantomSlot, PhantomSlotCapacity);
	if (!PhantomWheelWidget->IsInViewport())
	{
		PhantomWheelWidget->AddToViewport(50);
	}
	PhantomWheelWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

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

void USlimeDevourComponent::ClosePhantomWheel(bool bCommit)
{
	if (!bPhantomWheelOpen)
	{
		return;
	}
	bPhantomWheelOpen = false;
	if (PhantomWheelWidget)
	{
		PhantomWheelWidget->SetVisibility(ESlateVisibility::Collapsed);
		PhantomWheelWidget->RemoveFromParent();
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
		TryPhantom(SelectedPhantomSlot);
	}
}

void USlimeDevourComponent::TickPhantomWheelInput()
{
	if (!bPhantomWheelOpen || CycleCooldownRemaining > 0.f)
	{
		return;
	}
	APlayerController* PC = GetPlayerController();
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
		CyclePhantomSelection(Step);
		CycleCooldownRemaining = CycleCooldown;
	}
}

bool USlimeDevourComponent::TryPhantom(int32 Slot)
{
	if (!PhantomSlots.IsValidIndex(Slot) || !PhantomSlots[Slot].IsValidCapture())
	{
		return false;
	}
	APawn* Pawn = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		return false;
	}

	const FSlimeDevourCapture Capture = PhantomSlots[Slot];
	UClass* SpawnClass = Capture.EnemyClass.Get();
	if (!SpawnClass)
	{
		return false;
	}

	FVector SpawnLoc = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 100.f;
	if (const UCapsuleComponent* Capsule = Pawn->FindComponentByClass<UCapsuleComponent>())
	{
		SpawnLoc.Z = Pawn->GetActorLocation().Z - Capsule->GetScaledCapsuleHalfHeight();
	}
	FHitResult Hit;
	const FVector TraceStart = SpawnLoc + FVector(0.f, 0.f, 80.f);
	const FVector TraceEnd = SpawnLoc - FVector(0.f, 0.f, 400.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PhantomSpawn), false, Pawn);
	if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		SpawnLoc = Hit.ImpactPoint + FVector(0.f, 0.f, 8.f);
	}
	const FTransform SpawnXform(Pawn->GetActorRotation(), SpawnLoc);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.Owner = Pawn;
	SpawnParams.Instigator = Pawn;

	AEnemyCharacter* Phantom = World->SpawnActorDeferred<AEnemyCharacter>(SpawnClass, SpawnXform, Pawn, Pawn, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Phantom)
	{
		return false;
	}
	Phantom->InitAsPhantom(PhantomLifeSeconds, Pawn);
	Phantom->FinishSpawning(SpawnXform);
	PhantomSlots.RemoveAt(Slot);
	if (SelectedPhantomSlot >= PhantomSlots.Num())
	{
		SelectedPhantomSlot = 0;
	}
	return true;
}

void USlimeDevourComponent::ConsumePhantomSlot(int32 Slot)
{
	if (!PhantomSlots.IsValidIndex(Slot))
	{
		return;
	}
	PhantomSlots.RemoveAt(Slot);
	if (SelectedPhantomSlot >= PhantomSlots.Num())
	{
		SelectedPhantomSlot = 0;
	}
}
