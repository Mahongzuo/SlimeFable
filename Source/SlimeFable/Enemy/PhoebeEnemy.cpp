// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhoebeEnemy.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Materials/MaterialInterface.h"
#include "CharacterTrajectoryComponent.h"
#include "Combat/SlimeDodgeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "PhoebeClimbComponent.h"
#include "EnemyCombatComponent.h"
#include "SlimeLockOnComponent.h"
#include "Settings/SlimeInputSettings.h"
#include "Settings/SlimeInputTypes.h"
#include "SlimeStatusComponent.h"

APhoebeEnemy::APhoebeEnemy()
{
	Climb = CreateDefaultSubobject<UPhoebeClimbComponent>(TEXT("Climb"));
	CharacterTrajectory = CreateDefaultSubobject<UCharacterTrajectoryComponent>(TEXT("CharacterTrajectory"));

	bUseSingleNodeAnims = false;
	bABPDrivenLocomotion = true;
	WalkSpeed = 280.f;
	ChaseSpeed = 500.f;
	DetectRange = 1100.f;
	LeashRange = 1600.f;
	PreferredDistance = 180.f;
	bDevourable = true;
	bAutoFitCapsuleToMesh = true;
	DisplayName = FText::FromString(TEXT("菲比"));

	// AnimClass is bound on BP_PhoebeEnemy CDO to ABP_Phoebe (Python). Do not pin C++ pose proxy.
	PrimaryAnimClass.Reset();

	HitReactMontage = TSoftObjectPtr<UAnimMontage>(
		FSoftObjectPath(TEXT("/Game/Models/Phoebe/Animations/Montages/AM_Phoebe_Behit_S_L.AM_Phoebe_Behit_S_L")));

	ApplyThirdPersonLocomotionDefaults();

	DefaultOverlayMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/Models/Phoebe/Materials/MI_PhoebeOutline_Overlay.MI_PhoebeOutline_Overlay")));

	MorphCameraArmLengthMin = 90.f;
}

void APhoebeEnemy::Tick(float DeltaSeconds)
{
	UpdateFaceHeadForward();
	Super::Tick(DeltaSeconds);
}

void APhoebeEnemy::UpdateFaceHeadForward()
{
	USkeletalMeshComponent* Skel = GetMesh();
	if (!Skel)
	{
		return;
	}

	static const FName SocketCandidates[] = {
		FName(TEXT("Head")),
		FName(TEXT("head")),
		FName(TEXT("C_Head")),
		FName(TEXT("Bip001-Head")),
		FName(TEXT("Bip001_Head")),
		FName(TEXT("Neck")),
		FName(TEXT("neck")),
		FName(TEXT("neck_01")),
	};

	FVector Fwd = GetActorForwardVector();
	for (const FName& SocketName : SocketCandidates)
	{
		if (Skel->DoesSocketExist(SocketName))
		{
			Fwd = Skel->GetSocketQuaternion(SocketName).GetForwardVector();
			break;
		}
	}

	if (!Fwd.Normalize())
	{
		Fwd = GetActorForwardVector();
	}
	Skel->SetCustomPrimitiveDataVector3(0, Fwd);
}

void APhoebeEnemy::ApplyThirdPersonLocomotionDefaults()
{
	// 8-way BlendSpace is authored vs actor facing. Face the camera while moving
	// (first-round strafe). Climb tick forces yaw off so the capsule can face the wall.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->JumpZVelocity = 500.f;
		Move->AirControl = 0.35f;
		Move->MaxWalkSpeed = ChaseSpeed > 1.f ? ChaseSpeed : WalkSpeed;
		Move->MinAnalogWalkSpeed = 20.f;
		Move->BrakingDecelerationWalking = 2000.f;
		Move->BrakingDecelerationFalling = 1500.f;
		Move->GravityScale = 1.f;
		Move->bOrientRotationToMovement = false;
		Move->RotationRate = FRotator(0.f, 500.f, 0.f);
	}
}

void APhoebeEnemy::InitAsMorphTarget(AActor* Master)
{
	Super::InitAsMorphTarget(Master);
	// Super already set third-person orbit (yaw follows movement, camera orbits).
	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		Skel->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		if (UClass* AnimClass = PrimaryAnimClass.LoadSynchronous())
		{
			Skel->SetAnimInstanceClass(AnimClass);
		}
	}
}

bool APhoebeEnemy::TryStartAirAttack()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Combat || !Move)
	{
		return false;
	}

	const bool bFromClimb = Climb && Climb->IsClimbing();
	if (!bFromClimb && !Move->IsFalling())
	{
		return false;
	}

	if (bFromClimb)
	{
		Climb->BeginAirAttackDrop();
	}

	FEnemySkillDef Def;
	Def.DisplayName = NSLOCTEXT("SlimeFable", "PhoebeAirAttack", "下落攻击");
	Def.Exec = EEnemySkillExec::AoE;
	Def.Windup = 0.f;
	Def.HitStart = 0.f;
	Def.HitEnd = 0.f;
	Def.Recovery = 0.f;
	Def.Damage = AirAttackDamage;
	Def.Knockback = 480.f;
	Def.LaunchZ = 120.f;
	Def.Hit.Shape = ESlimeHitShape::Sphere;
	Def.Hit.Range = 0.f;
	Def.Hit.Radius = 145.f;
	Def.Hit.OriginForwardOffset = 0.f;
	Def.Hit.OriginZOffset = 0.f;

	return Combat->TryStartAirAttack(
		Def,
		AirAttackStartMontage.LoadSynchronous(),
		AirAttackLoopMontage.LoadSynchronous(),
		AirAttackEndMontage.LoadSynchronous(),
		AirAttackGravityMultiplier,
		AirAttackInitialDownSpeed);
}

void APhoebeEnemy::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (Combat)
	{
		Combat->NotifyOwnerLanded();
	}
}

void APhoebeEnemy::EnterStagger(float Duration, AActor* StaggerInstigator)
{
	Super::EnterStagger(Duration, StaggerInstigator);

	if (IsInDeathSequence())
	{
		return;
	}

	if (UAnimMontage* React = HitReactMontage.LoadSynchronous())
	{
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			if (UAnimInstance* Anim = MeshComp->GetAnimInstance())
			{
				Anim->Montage_Play(React);
			}
		}
	}
}

void APhoebeEnemy::MorphMove(const FInputActionValue& Value)
{
	if (Combat && Combat->IsAirAttacking())
	{
		return;
	}

	const FVector2D MovementVector = Value.Get<FVector2D>();
	if (Combat && Combat->IsMovementLocked())
	{
		return;
	}

	if (Climb)
	{
		Climb->HandleMorphMove(MovementVector);
		if (Climb->IsClimbing())
		{
			return;
		}
	}

	Super::MorphMove(Value);
}

bool APhoebeEnemy::WantsCombatDodge() const
{
	if (Climb && Climb->IsClimbing())
	{
		return false;
	}
	if (const USlimeDodgeComponent* Dodge = FindComponentByClass<USlimeDodgeComponent>())
	{
		return Dodge->IsInPerfectWindow();
	}
	return false;
}

void APhoebeEnemy::UpdateMorphSprintSpeed()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!Move || !PC)
	{
		return;
	}
	bool bSprint = false;
	bool bRmbAccel = false;
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			if (const USlimeInputSettings* InputSettings = GI->GetSubsystem<USlimeInputSettings>())
			{
				bSprint = InputSettings->IsKeyDown(PC, ESlimeInputAction::Sprint);
				const bool bDodgeHeld = InputSettings->IsKeyDown(PC, ESlimeInputAction::Dodge);
				if (bDodgeHeld && !WantsCombatDodge())
				{
					bRmbAccel = true;
					if (!bSprint)
					{
						bSprint = true;
					}
				}
			}
		}
	}
	if (!bSprint)
	{
		bSprint = PC->IsInputKeyDown(EKeys::LeftShift);
	}
	if (!bRmbAccel && PC->IsInputKeyDown(EKeys::RightMouseButton) && !WantsCombatDodge())
	{
		bRmbAccel = true;
		bSprint = true;
	}

	if (bRmbAccel && Combat && (Combat->IsAttacking() || Combat->IsAirAttacking() || Combat->IsMovementLocked()))
	{
		Combat->InterruptCombat();
	}

	if (Combat && Combat->IsAirAttacking())
	{
		return;
	}

	const bool bClimbing = Climb && Climb->IsClimbing();
	bool bLockedOn = false;
	if (const USlimeLockOnComponent* Lock = FindComponentByClass<USlimeLockOnComponent>())
	{
		bLockedOn = Lock->IsLockedOn();
	}

	bUseControllerRotationYaw = false;
	Move->bOrientRotationToMovement = !bClimbing && !bLockedOn;

	const float Base = WalkSpeed > 1.f ? WalkSpeed : 280.f;
	const float Fast = FMath::Max(ChaseSpeed * FMath::Max(SprintSpeedMul, 1.5f), 650.f);
	float StatusMul = 1.f;
	if (USlimeStatusComponent* StatusComp = GetEnemyStatus())
	{
		StatusMul = StatusComp->GetMoveSpeedMul();
	}
	if (Combat && Combat->IsMovementLocked())
	{
		Move->MaxWalkSpeed = 0.f;
		return;
	}
	Move->MaxWalkSpeed = (bSprint ? Fast : Base) * StatusMul;
}

void APhoebeEnemy::MorphJump()
{
	if (Combat && Combat->IsAirAttacking())
	{
		return;
	}
	if (Climb && Climb->HandleMorphJump())
	{
		return;
	}

	Super::MorphJump();
}

float APhoebeEnemy::ResolveMoveLockSeconds(const FEnemySkillDef& Def) const
{
	const TArray<FEnemyMoveDef>& Kit = GetMoves();
	if (Kit.Num() > 0)
	{
		const FEnemySkillDef& Light = Kit[0].Skill;
		const bool bSameMontage = !Light.AttackMontage.IsNull() && Light.AttackMontage == Def.AttackMontage;
		const bool bSameName = !Light.DisplayName.IsEmpty() && Light.DisplayName.EqualTo(Def.DisplayName);
		if (bSameMontage || bSameName)
		{
			return LightAttackMoveLockSeconds;
		}
	}
	return SkillMoveLockSeconds;
}
