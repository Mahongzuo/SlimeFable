// Copyright Epic Games, Inc. All Rights Reserved.

#include "GaspMoverEnemy.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimationAsset.h"
#include "BrainComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Combat/SlimeHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Backends/MoverBackendLiaison.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "TimerManager.h"
#include "DefaultMovementSet/NavMoverComponent.h"
#include "MovementMode.h"
#include "EnemyAttributeSet.h"
#include "EnemyCombatComponent.h"
#include "EnemyCombatTypes.h"
#include "SlimeCombatTypes.h"
#include "GaspEnemyAIController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "MoverDataModelTypes.h"
#include "MotionWarpingComponent.h"
#include "UserDefinedStructSupport.h"
#include "SlimeFable.h"
#include "SlimeFableCharacter.h"
#include "Slime/SlimeMorphComponent.h"
#include "SlimeFoliageInteractComponent.h"
#include "SlimeLockOnComponent.h"
#include "SlimeStatusComponent.h"
#include "SlimeWorldHealthBar.h"
#include "UObject/UnrealType.h"
#include "UObject/Class.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/UserDefinedEnum.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"

static int32 GGaspMoveDebug = 0;
static FAutoConsoleVariableRef CVarGaspMoveDebug(
	TEXT("slime.GaspMoveDebug"),
	GGaspMoveDebug,
	TEXT("Log GASP mover controller/mode/velocity/intent every second when non-zero."),
	ECVF_Default);

static int64 MatchGaspMoverEnumByDisplayName(UEnum* Enum, const FString& Needle);

AGaspMoverEnemy::AGaspMoverEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AGaspEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	SetReplicatingMovement(false);

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(42.f, 96.f);
	Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	SetRootComponent(Capsule);

	SourceMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SourceMesh"));
	SourceMesh->SetupAttachment(Capsule);
	SourceMesh->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
	SourceMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	SourceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SourceMesh->SetHiddenInGame(true);
	SourceMesh->SetVisibility(false);
	SourceMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	Mover = CreateDefaultSubobject<UCharacterMoverComponent>(TEXT("Mover"));
	Mover->SetUpdatedComponent(Capsule);
	Mover->InputProducer = this;
	NavMover = CreateDefaultSubobject<UNavMoverComponent>(TEXT("NavMover"));

	Health = CreateDefaultSubobject<USlimeHealthComponent>(TEXT("Health"));
	Health->Team = ESlimeTeam::Enemy;
	Health->bDestroyOnDeath = false;
	Health->bRegenOnDeath = false;
	Health->MaxHP = 220.f;
	Status = CreateDefaultSubobject<USlimeStatusComponent>(TEXT("Status"));
	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("EnemyAbilitySystem"));
	EnemyAttributes = CreateDefaultSubobject<UEnemyAttributeSet>(TEXT("EnemyAttributes"));
	Combat = CreateDefaultSubobject<UEnemyCombatComponent>(TEXT("Combat"));
	Combat->GlobalHitDelay = 0.f;
	Combat->AttackSwingSound = TSoftObjectPtr<USoundBase>(
		FSoftObjectPath(EnemyCombat::DefaultGaspAttackSound));
	Combat->AttackImpactSound = TSoftObjectPtr<USoundBase>(
		FSoftObjectPath(EnemyCombat::DefaultGaspAttackImpactSound));
	FoliageInteract = CreateDefaultSubobject<USlimeFoliageInteractComponent>(TEXT("FoliageInteract"));

	HealthBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBar->SetupAttachment(RootComponent);
	HealthBar->SetRelativeLocation(FVector(0.f, 0.f, HealthBarZOffset));
	HealthBar->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBar->SetDrawAtDesiredSize(false);
	HealthBar->SetDrawSize(FVector2D(110.f, 14.f));
	HealthBar->SetPivot(FVector2D(0.5f, 1.f));
	HealthBar->SetWidgetClass(USlimeWorldHealthBar::StaticClass());
	HealthBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	DisplayName = FText::FromString(TEXT("动作试样"));
	HitFlashMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(EnemyCombat::DefaultHitFlashPath));
	LightningHitOverlay = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(EnemyCombat::DefaultLightningOverlayPath));
	WindHitOverlay = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(EnemyCombat::DefaultWindOverlayPath));
	AttackSound = TSoftObjectPtr<USoundBase>(
		FSoftObjectPath(EnemyCombat::DefaultGaspAttackSound));
	HitTakenSound = TSoftObjectPtr<USoundBase>(
		FSoftObjectPath(EnemyCombat::DefaultGaspHitTakenSound));
	HitReactFront = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(
		TEXT("/Game/_Slime/Enemies/GASP/Montages/AM_Gasp_Hit_F.AM_Gasp_Hit_F")));
	HitReactBack = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(
		TEXT("/Game/_Slime/Enemies/GASP/Montages/AM_Gasp_Hit_B.AM_Gasp_Hit_B")));
	HitReactLeft = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(
		TEXT("/Game/_Slime/Enemies/GASP/Montages/AM_Gasp_Hit_L.AM_Gasp_Hit_L")));
	HitReactRight = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(
		TEXT("/Game/_Slime/Enemies/GASP/Montages/AM_Gasp_Hit_R.AM_Gasp_Hit_R")));
	SandboxMapping = TSoftObjectPtr<UInputMappingContext>(
		FSoftObjectPath(TEXT("/Game/Input/IMC_GaspMorphLocomotion.IMC_GaspMorphLocomotion")));
	InteractAction = TSoftObjectPtr<UInputAction>(
		FSoftObjectPath(TEXT("/Game/Input/IA_Interact.IA_Interact")));
	TakedownAction = TSoftObjectPtr<UInputAction>(
		FSoftObjectPath(TEXT("/Game/Input/IA_Takedown.IA_Takedown")));
	TriggerRagdollAction = TSoftObjectPtr<UInputAction>(
		FSoftObjectPath(TEXT("/Game/Input/IA_TriggerRagdoll.IA_TriggerRagdoll")));
	CrouchAction = TSoftObjectPtr<UInputAction>(
		FSoftObjectPath(TEXT("/Game/Input/IA_Crouch.IA_Crouch")));
	SprintAction = TSoftObjectPtr<UInputAction>(
		FSoftObjectPath(TEXT("/Game/Input/IA_Sprint.IA_Sprint")));
	DeathDissolveMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/_Slime/FX/M_EnemyDeathDissolve.M_EnemyDeathDissolve")));
}

void AGaspMoverEnemy::EnsureMoverModes()
{
	if (!Mover)
	{
		return;
	}

	struct FModeSpec
	{
		FName Name;
		const TCHAR* Path;
	};
	static const FModeSpec Specs[] = {
		{ FName(TEXT("Walking")), TEXT("/Game/Blueprints/MovementModes/BP_MovementMode_Walking.BP_MovementMode_Walking_C") },
		{ FName(TEXT("Falling")), TEXT("/Game/Blueprints/MovementModes/BP_MovementMode_Falling.BP_MovementMode_Falling_C") },
		{ FName(TEXT("Flying")), TEXT("/Game/Blueprints/MovementModes/B_MovementMode_Flying.B_MovementMode_Flying_C") },
		{ FName(TEXT("Sliding")), TEXT("/Game/Blueprints/MovementModes/BP_MovementMode_Slide.BP_MovementMode_Slide_C") },
		{ FName(TEXT("Traversing")), TEXT("/Script/Mover.FlyingMode") },
		{ FName(TEXT("Ragdoll")), TEXT("/Game/Blueprints/MovementModes/BP_MovementMode_Ragdoll.BP_MovementMode_Ragdoll_C") },
	};

	for (const FModeSpec& Spec : Specs)
	{
		if (UBaseMovementMode* Existing = Mover->FindMovementModeByName(Spec.Name))
		{
			(void)Existing;
			continue;
		}
		UClass* ModeClass = LoadClass<UBaseMovementMode>(nullptr, Spec.Path);
		if (!ModeClass)
		{
			continue;
		}
		UBaseMovementMode* NewMode = NewObject<UBaseMovementMode>(Mover, ModeClass, Spec.Name);
		Mover->MovementModes.Add(Spec.Name, NewMode);
	}

	if (Mover->StartingMovementMode.IsNone() || !Mover->FindMovementModeByName(Mover->StartingMovementMode))
	{
		// Prefer Walking when spawned on the ground — Falling can stick at vel=0 if floor is already underfoot.
		Mover->StartingMovementMode = Mover->FindMovementModeByName(FName(TEXT("Walking")))
			? FName(TEXT("Walking"))
			: FName(TEXT("Falling"));
	}
}

void AGaspMoverEnemy::EnsureMoveKit()
{
	if (Moves.Num() == 0)
	{
		EnemyCombat::FillDefaultGaspMoves(Moves);
	}
}

void AGaspMoverEnemy::SnapSourceMeshToCapsule()
{
	if (!SourceMesh || !Capsule)
	{
		return;
	}
	if (SourceMesh->GetAttachParent() != Capsule)
	{
		SourceMesh->AttachToComponent(Capsule, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	SourceMesh->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
	SourceMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	SourceMesh->SetRelativeScale3D(FVector::OneVector);
	SourceMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void AGaspMoverEnemy::SnapVisualMeshToSource(USkeletalMeshComponent* Mesh)
{
	if (!Mesh || !SourceMesh || Mesh == SourceMesh)
	{
		return;
	}
	if (Mesh->GetAttachParent() != SourceMesh)
	{
		Mesh->AttachToComponent(SourceMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	Mesh->SetRelativeLocation(FVector::ZeroVector);
	Mesh->SetRelativeRotation(FRotator::ZeroRotator);
	Mesh->SetRelativeScale3D(FVector::OneVector);
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void AGaspMoverEnemy::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyVisualOverride();
}

void AGaspMoverEnemy::PostInitializeComponents()
{
	if (Mover)
	{
		UE_LOG(LogSlimeFable, Log, TEXT("GaspMoverEnemy %s: Mover InputProducer was %s; rebinding to self."),
			*GetName(), *GetNameSafe(Mover->InputProducer.Get()));
		Mover->InputProducer = this;
		Mover->SetUpdatedComponent(Capsule);
	}
	EnsureMoverModes();
	EnsureMoveKit();
	Super::PostInitializeComponents();
	if (Health)
	{
		Health->MaxHP = FMath::Max(MaxHP, 1.f);
	}
	if (Mover)
	{
		Mover->InputProducer = this;
	}
	EnsureMoverModes();
	EnsureMoveKit();
	ApplyVisualOverride();
}

void AGaspMoverEnemy::BeginPlay()
{
	EnsureMoverModes();
	EnsureMoveKit();
	if (Mover)
	{
		Mover->InputProducer = this;
	}
	WireSandboxAnimInterface();
	Super::BeginPlay();
	Health->MaxHP = MaxHP;
	Health->ResetHP();
	if (DebugStartHealthPercent > KINDA_SMALL_NUMBER)
	{
		Health->CurrentHP = MaxHP * DebugStartHealthPercent;
	}
	Health->OnHealthChanged.Broadcast(Health->CurrentHP, Health->MaxHP);
	Health->OnDied.AddDynamic(this, &AGaspMoverEnemy::HandleDied);
	if (AbilitySystem && EnemyAttributes)
	{
		AbilitySystem->InitAbilityActorInfo(this, this);
	}
	ApplyVisualOverride();
	WireSandboxAnimInterface();
	RefreshHealthBarAnchor();
	BindWorldHealthBar();
	if (Mover)
	{
		Mover->PrimaryComponentTick.bCanEverTick = true;
		Mover->SetComponentTickEnabled(true);
		if (Mover->FindMovementModeByName(FName(TEXT("Walking"))))
		{
			Mover->StartingMovementMode = FName(TEXT("Walking"));
			Mover->QueueNextMode(FName(TEXT("Walking")), true);
		}
	}
}

void AGaspMoverEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetMorphLocomotionTicksEnabled(false);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CombatGetUpTimer);
		World->GetTimerManager().ClearTimer(DeathRagdollTimer);
		World->GetTimerManager().ClearTimer(DeathDissolveTimer);
	}
	if (APlayerController* OwnedPC = Cast<APlayerController>(GetController()))
	{
		RemoveSandboxMapping(OwnedPC);
	}
	else if (AActor* Master = MorphMaster.Get())
	{
		if (APlayerController* MasterPC = Cast<APlayerController>(Master->GetInstigatorController()))
		{
			RemoveSandboxMapping(MasterPC);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AGaspMoverEnemy::BindWorldHealthBar()
{
	if (!HealthBar)
	{
		return;
	}
	HealthBar->InitWidget();
	if (USlimeWorldHealthBar* Bar = Cast<USlimeWorldHealthBar>(HealthBar->GetWidget()))
	{
		Bar->SetHealth(Health);
	}
}

void AGaspMoverEnemy::UpdateAnimMotionState(float DeltaSeconds)
{
	FVector Velocity = FVector::ZeroVector;
	if (Mover)
	{
		Velocity = Mover->GetVelocity();
		CachedMovementMode = Mover->GetMovementModeName();
	}
	if (DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		CachedAcceleration = (Velocity - LastVelocity) / DeltaSeconds;
	}
	LastVelocity = Velocity;

	CachedGaitName = ResolveDesiredGaitName();
	CachedStanceName = bWantsCrouch ? TEXT("Crouch") : TEXT("Stand");
	CachedRotationModeName = TEXT("OrientToMovement");
}

FString AGaspMoverEnemy::ResolveDesiredGaitName() const
{
	// Official Sandbox: Gait comes from input wants (Sprint/Run/Walk), not speed alone.
	const bool bPlayerControlled = Cast<APlayerController>(GetController()) != nullptr;
	if (bPlayerControlled && bWantsSprint)
	{
		return TEXT("Sprint");
	}
	if (bWantChaseGait)
	{
		return TEXT("Run");
	}

	const float Speed2D = LastVelocity.Size2D();
	if (bPlayerControlled)
	{
		if (!CachedMoveIntent.IsNearlyZero())
		{
			return TEXT("Run");
		}
		return TEXT("Walk");
	}

	if (Speed2D < 20.f)
	{
		return TEXT("Walk");
	}
	if (Speed2D > ChaseSpeed * 0.75f || Speed2D > 280.f)
	{
		return TEXT("Run");
	}
	return TEXT("Walk");
}

void AGaspMoverEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bMoverFrozen && !bDeathSequence && !bCombatKnockdown)
	{
		UpdateAnimMotionState(DeltaSeconds);
	}
	if (AGaspEnemyAIController* AIC = Cast<AGaspEnemyAIController>(GetController()))
	{
		// Controllers sometimes stay dormant under -game/nullrhi; drive AI from the pawn tick.
		AIC->DriveCombatAI(DeltaSeconds);
	}
	TickCombatKnockdown();
	ConfirmDeathRagdollThenStopAI();
	if (!bDeathSequence && !bMoverFrozen && !bCombatKnockdown
		&& Mover && Mover->FindMovementModeByName(FName(TEXT("Walking"))))
	{
		const FName ModeName = Mover->GetMovementModeName();
		if (ModeName == FName(TEXT("Falling"))
			|| ModeName.IsNone())
		{
			Mover->QueueNextMode(FName(TEXT("Walking")), true);
		}
	}
	KeepDeathRagdollPhysics();
	if (bEnableLookAt && SourceMesh)
	{
		AActor* POI = LookAtPOI.Get();
		if (!POI && MorphLockOn)
		{
			POI = MorphLockOn->GetLockedTarget();
		}
		if (POI)
		{
			SourceMesh->SetCustomPrimitiveDataVector3(0, POI->GetActorLocation());
		}
	}

	if (GGaspMoveDebug > 0)
	{
		MoveDebugAccum += DeltaSeconds;
		if (MoveDebugAccum >= 1.f)
		{
			MoveDebugAccum = 0.f;
			const AController* Ctrl = GetController();
			const FName ModeName = Mover ? Mover->GetMovementModeName() : NAME_None;
			const FVector Vel = Mover ? Mover->GetVelocity() : FVector::ZeroVector;
			const FVector Loc = GetActorLocation();
			APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
			const float DistToPlayer = Player
				? FVector::Dist(Loc, Player->GetActorLocation())
				: -1.f;
			UE_LOG(LogSlimeFable, Log,
				TEXT("GaspMoveDebug %s ctrl=%s mode=%s vel2d=%.1f velz=%.1f z=%.0f distP=%.0f detect=%.0f intent=(%.2f,%.2f) face=(%.2f,%.2f) frozen=%d morph=%d producer=%s"),
				*GetName(),
				Ctrl ? *Ctrl->GetClass()->GetName() : TEXT("None"),
				*ModeName.ToString(),
				Vel.Size2D(),
				Vel.Z,
				Loc.Z,
				DistToPlayer,
				DetectRange,
				AiMoveIntent.X, AiMoveIntent.Y,
				AiFaceIntent.X, AiFaceIntent.Y,
				bMoverFrozen ? 1 : 0,
				bMorphTarget ? 1 : 0,
				Mover ? *GetNameSafe(Mover->InputProducer.Get()) : TEXT("None"));
		}
	}
}

void AGaspMoverEnemy::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (bMorphTarget)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			AddSandboxMapping(PC);
		}
		if (UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(PlayerInputComponent))
		{
			BindMorphInput(Enhanced);
		}
	}
}

void AGaspMoverEnemy::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	(void)SimTimeMs;
	FCharacterDefaultInputs& Inputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	if (ApplyPendingRagdollInput(Inputs))
	{
		WriteMoverCustomInputs(InputCmdResult);
		return;
	}
	if (bMoverFrozen || bDeathSequence || bCombatKnockdown || !GetController())
	{
		Inputs.SetMoveInput(EMoveInputType::None, FVector::ZeroVector);
		bJumpJustPressed = false;
		WriteMoverCustomInputs(InputCmdResult);
		return;
	}

	FVector WorldIntent = FVector::ZeroVector;
	const bool bPlayerControlled = Cast<APlayerController>(GetController()) != nullptr;
	if (bPlayerControlled)
	{
		const FRotator ControlRot = GetControlRotation();
		const FRotator YawRot(0.f, ControlRot.Yaw, 0.f);
		const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
		const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
		WorldIntent = (Forward * CachedMoveIntent.Y + Right * CachedMoveIntent.X).GetClampedToMaxSize(1.f);
		Inputs.ControlRotation = ControlRot;
		Inputs.OrientationIntent = WorldIntent.IsNearlyZero() ? FVector::ZeroVector : WorldIntent.GetSafeNormal();
	}
	else
	{
		FVector NavIntent = FVector::ZeroVector;
		FVector NavVelocity = FVector::ZeroVector;
		if (NavMover && NavMover->ConsumeNavMovementData(NavIntent, NavVelocity))
		{
			if (!NavIntent.IsNearlyZero())
			{
				WorldIntent = NavIntent.GetSafeNormal2D();
			}
			else if (!NavVelocity.IsNearlyZero())
			{
				WorldIntent = NavVelocity.GetSafeNormal2D();
			}
		}
		if (WorldIntent.IsNearlyZero() && !AiMoveIntent.IsNearlyZero())
		{
			WorldIntent = AiMoveIntent.GetSafeNormal2D();
		}

		const FVector Face = !AiFaceIntent.IsNearlyZero()
			? AiFaceIntent.GetSafeNormal2D()
			: (!WorldIntent.IsNearlyZero() ? WorldIntent.GetSafeNormal2D() : FVector::ZeroVector);
		Inputs.OrientationIntent = Face;
		Inputs.ControlRotation = Face.IsNearlyZero() ? GetActorRotation() : Face.Rotation();
	}

	Inputs.SetMoveInput(EMoveInputType::DirectionalIntent, WorldIntent);
	Inputs.bIsJumpPressed = bJumpHeld;
	Inputs.bIsJumpJustPressed = bJumpJustPressed;
	if (Mover)
	{
		const FName ModeName = Mover->GetMovementModeName();
		const bool bStuckFalling = ModeName == FName(TEXT("Falling"))
			&& Mover->GetVelocity().SizeSquared() < 4.f;
		if (bStuckFalling && Mover->FindMovementModeByName(FName(TEXT("Walking"))))
		{
			Inputs.SuggestedMovementMode = FName(TEXT("Walking"));
		}
	}
	if (bPendingRagdoll && bEnableRagdollKit)
	{
		Inputs.SuggestedMovementMode = FName(TEXT("Ragdoll"));
		bPendingRagdoll = false;
	}
	bJumpJustPressed = false;
	WriteMoverCustomInputs(InputCmdResult);
}

void AGaspMoverEnemy::WriteMoverCustomInputs(FMoverInputCmdContext& InputCmdResult) const
{
	UScriptStruct* CustomStruct = LoadObject<UScriptStruct>(
		nullptr,
		TEXT("/Game/Blueprints/Data/S_MoverCustomInputs.S_MoverCustomInputs"));
	if (!CustomStruct)
	{
		return;
	}

	FMoverDataStructBase* Block = InputCmdResult.InputCollection.FindOrAddDataByType(CustomStruct);
	FMoverUserDefinedDataStruct* Wrapper = static_cast<FMoverUserDefinedDataStruct*>(Block);
	if (!Wrapper || !Wrapper->StructInstance.IsValid())
	{
		return;
	}

	void* StructPtr = Wrapper->StructInstance.GetMutableMemory();
	if (!StructPtr)
	{
		return;
	}

	const FString GaitName = ResolveDesiredGaitName();
	const bool bPlayerControlled = Cast<APlayerController>(GetController()) != nullptr;
	const bool bCrouch = bPlayerControlled && bWantsCrouch;

	auto WriteEnumByName = [&](const TCHAR* Prefix, const FString& DisplayNeedle)
	{
		for (TFieldIterator<FProperty> It(CustomStruct); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop || !Prop->GetName().StartsWith(Prefix))
			{
				continue;
			}
			if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
			{
				const int64 Value = MatchGaspMoverEnumByDisplayName(EnumProp->GetEnum(), DisplayNeedle);
				if (Value != INDEX_NONE)
				{
					void* ValuePtr = EnumProp->ContainerPtrToValuePtr<void>(StructPtr);
					EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, Value);
				}
				return;
			}
			if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
			{
				if (UEnum* Enum = ByteProp->GetIntPropertyEnum())
				{
					const int64 Value = MatchGaspMoverEnumByDisplayName(Enum, DisplayNeedle);
					if (Value != INDEX_NONE)
					{
						ByteProp->SetPropertyValue_InContainer(StructPtr, static_cast<uint8>(Value));
					}
				}
				return;
			}
		}
	};

	auto WriteBool = [&](const TCHAR* Prefix, bool bValue)
	{
		for (TFieldIterator<FProperty> It(CustomStruct); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop || !Prop->GetName().StartsWith(Prefix))
			{
				continue;
			}
			if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
			{
				BoolProp->SetPropertyValue_InContainer(StructPtr, bValue);
				return;
			}
		}
	};

	WriteEnumByName(TEXT("Gait"), GaitName);
	WriteEnumByName(TEXT("RotationMode"), TEXT("OrientToMovement"));
	WriteBool(TEXT("WantsToCrouch"), bCrouch);
}

void AGaspMoverEnemy::BindMorphInput(UEnhancedInputComponent* EnhancedInput)
{
	if (MorphJumpAction)
	{
		EnhancedInput->BindAction(MorphJumpAction, ETriggerEvent::Started, this, &AGaspMoverEnemy::MorphJumpStarted);
		EnhancedInput->BindAction(MorphJumpAction, ETriggerEvent::Completed, this, &AGaspMoverEnemy::MorphJumpReleased);
	}
	if (MorphMoveAction)
	{
		EnhancedInput->BindAction(MorphMoveAction, ETriggerEvent::Triggered, this, &AGaspMoverEnemy::MorphMove);
		EnhancedInput->BindAction(MorphMoveAction, ETriggerEvent::Completed, this, &AGaspMoverEnemy::MorphMoveStopped);
		EnhancedInput->BindAction(MorphMoveAction, ETriggerEvent::Canceled, this, &AGaspMoverEnemy::MorphMoveStopped);
	}
	if (MorphMouseLookAction)
	{
		EnhancedInput->BindAction(MorphMouseLookAction, ETriggerEvent::Triggered, this, &AGaspMoverEnemy::MorphLook);
	}
	if (MorphLookAction)
	{
		EnhancedInput->BindAction(MorphLookAction, ETriggerEvent::Triggered, this, &AGaspMoverEnemy::MorphLook);
	}
	if (UInputAction* Interact = LoadSoftAction(InteractAction))
	{
		EnhancedInput->BindAction(Interact, ETriggerEvent::Started, this, &AGaspMoverEnemy::MorphInteractStarted);
	}
	if (UInputAction* Takedown = LoadSoftAction(TakedownAction))
	{
		EnhancedInput->BindAction(Takedown, ETriggerEvent::Started, this, &AGaspMoverEnemy::MorphTakedownStarted);
	}
	if (UInputAction* Ragdoll = LoadSoftAction(TriggerRagdollAction))
	{
		EnhancedInput->BindAction(Ragdoll, ETriggerEvent::Started, this, &AGaspMoverEnemy::MorphTriggerRagdollStarted);
	}
	if (UInputAction* Crouch = LoadSoftAction(CrouchAction))
	{
		EnhancedInput->BindAction(Crouch, ETriggerEvent::Started, this, &AGaspMoverEnemy::MorphCrouchStarted);
	}
	if (UInputAction* Sprint = LoadSoftAction(SprintAction))
	{
		EnhancedInput->BindAction(Sprint, ETriggerEvent::Started, this, &AGaspMoverEnemy::MorphSprintStarted);
		EnhancedInput->BindAction(Sprint, ETriggerEvent::Completed, this, &AGaspMoverEnemy::MorphSprintStopped);
		EnhancedInput->BindAction(Sprint, ETriggerEvent::Canceled, this, &AGaspMoverEnemy::MorphSprintStopped);
	}
}

void AGaspMoverEnemy::MorphMove(const FInputActionValue& Value)
{
	if (Combat && Combat->IsMovementLocked())
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	CachedMoveIntent = FVector(Axis.X, Axis.Y, 0.f);
}

void AGaspMoverEnemy::MorphMoveStopped(const FInputActionValue& Value)
{
	(void)Value;
	CachedMoveIntent = FVector::ZeroVector;
}

void AGaspMoverEnemy::MorphLook(const FInputActionValue& Value)
{
	const FVector2D Look = Value.Get<FVector2D>();
	AddControllerYawInput(Look.X);
	AddControllerPitchInput(Look.Y);
}

void AGaspMoverEnemy::MorphJumpStarted()
{
	if (TryTraversalAction() || DispatchBoolEventOnComponents(TEXT("TryTraversalAction")))
	{
		return;
	}
	bJumpHeld = true;
	bJumpJustPressed = true;
	if (Mover)
	{
		Mover->Jump();
	}
}

void AGaspMoverEnemy::MorphInteractStarted()
{
	if (TrySmartObjectInteract())
	{
		return;
	}
	DispatchBoolEventOnComponents(TEXT("TryInteract"));
	DispatchBoolEventOnComponents(TEXT("Interact"));
}

void AGaspMoverEnemy::MorphTakedownStarted()
{
	if (TryTakedown())
	{
		return;
	}
	DispatchBoolEventOnComponents(TEXT("TryTakedown"));
}

void AGaspMoverEnemy::MorphTriggerRagdollStarted()
{
	TriggerSandboxRagdoll();
}

UInputAction* AGaspMoverEnemy::LoadSoftAction(const TSoftObjectPtr<UInputAction>& SoftAction) const
{
	return SoftAction.IsNull() ? nullptr : SoftAction.LoadSynchronous();
}

bool AGaspMoverEnemy::DispatchBoolEventOnComponents(FName FunctionName)
{
	TArray<UActorComponent*> Comps;
	GetComponents(Comps);
	for (UActorComponent* Comp : Comps)
	{
		if (!Comp)
		{
			continue;
		}
		UFunction* Fn = Comp->FindFunction(FunctionName);
		if (!Fn)
		{
			continue;
		}
		if (Fn->NumParms == 0)
		{
			Comp->ProcessEvent(Fn, nullptr);
			continue;
		}
		if (Fn->NumParms == 1 && Fn->GetReturnProperty() && Fn->GetReturnProperty()->IsA<FBoolProperty>())
		{
			bool bReturn = false;
			Comp->ProcessEvent(Fn, &bReturn);
			if (bReturn)
			{
				return true;
			}
		}
	}
	return false;
}

void AGaspMoverEnemy::MorphJumpReleased()
{
	bJumpHeld = false;
}

void AGaspMoverEnemy::MorphCrouchStarted()
{
	bWantsCrouch = !bWantsCrouch;
}

void AGaspMoverEnemy::MorphSprintStarted()
{
	bWantsSprint = true;
}

void AGaspMoverEnemy::MorphSprintStopped()
{
	bWantsSprint = false;
}

void AGaspMoverEnemy::AddSandboxMapping(APlayerController* PC)
{
	if (!PC || bSandboxMappingAdded)
	{
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		if (UInputMappingContext* IMC = SandboxMapping.LoadSynchronous())
		{
			Sub->AddMappingContext(IMC, 1);
			bSandboxMappingAdded = true;
		}
	}
}

void AGaspMoverEnemy::RemoveSandboxMapping(APlayerController* PC)
{
	if (!PC || !bSandboxMappingAdded)
	{
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		if (UInputMappingContext* IMC = SandboxMapping.LoadSynchronous())
		{
			Sub->RemoveMappingContext(IMC);
		}
	}
	bSandboxMappingAdded = false;
}

void AGaspMoverEnemy::InitAsMorphTarget(AActor* Master)
{
	bMorphTarget = true;
	bDevourable = false;
	MorphMaster = Master;
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;
	if (Health)
	{
		Health->Team = ESlimeTeam::Player;
		Health->bDestroyOnDeath = false;
	}
	if (HealthBar)
	{
		HealthBar->SetHiddenInGame(true);
		HealthBar->SetVisibility(false);
	}
	if (ASlimeFableCharacter* Slime = Cast<ASlimeFableCharacter>(Master))
	{
		MorphMoveAction = Slime->GetMoveAction();
		MorphLookAction = Slime->GetLookAction();
		MorphMouseLookAction = Slime->GetMouseLookAction();
		MorphJumpAction = Slime->GetJumpAction();
	}

	MorphCameraBoom = NewObject<USpringArmComponent>(this, TEXT("MorphCameraBoom"));
	MorphCameraBoom->SetupAttachment(RootComponent);
	MorphCameraBoom->TargetArmLength = 320.f;
	MorphCameraBoom->bUsePawnControlRotation = true;
	MorphCameraBoom->bEnableCameraLag = true;
	MorphCameraBoom->CameraLagSpeed = 12.f;
	MorphCameraBoom->RegisterComponent();

	MorphFollowCamera = NewObject<UCameraComponent>(this, TEXT("MorphFollowCamera"));
	MorphFollowCamera->SetupAttachment(MorphCameraBoom, USpringArmComponent::SocketName);
	MorphFollowCamera->bUsePawnControlRotation = false;
	MorphFollowCamera->RegisterComponent();

	if (!MorphLockOn)
	{
		MorphLockOn = NewObject<USlimeLockOnComponent>(this, TEXT("MorphLockOn"));
		MorphLockOn->bPollLockOnKey = true;
		MorphLockOn->RegisterComponent();
	}

	// Thin IMC (Crouch/Sprint only). Full IMC_Sandbox would consume WASD and lock slime after unmorph.
	if (APlayerController* PC = Cast<APlayerController>(Master ? Master->GetInstigatorController() : nullptr))
	{
		AddSandboxMapping(PC);
	}
	else if (APlayerController* OwnedPC = Cast<APlayerController>(GetController()))
	{
		AddSandboxMapping(OwnedPC);
	}
	ApplyVisualOverride();
}

void AGaspMoverEnemy::InitAsPhantom(float LifeSeconds, AActor* Master)
{
	bPhantomInstance = true;
	bDevourable = false;
	MorphMaster = Master;
	(void)LifeSeconds;
	ApplyVisualOverride();
}

void AGaspMoverEnemy::BeginDevouredDeath(AActor* Devourer)
{
	(void)Devourer;
	bDevouredDeath = true;
	bDeathSequence = true;
	bDevourable = false;
}

void AGaspMoverEnemy::FreezeForDevour()
{
	bMoverFrozen = true;
	CachedMoveIntent = FVector::ZeroVector;
	AiMoveIntent = FVector::ZeroVector;
	AiFaceIntent = FVector::ZeroVector;
	if (Mover)
	{
		Mover->SetComponentTickEnabled(false);
	}
}

void AGaspMoverEnemy::RestoreFromDevour()
{
	bMoverFrozen = false;
	if (Mover)
	{
		Mover->SetComponentTickEnabled(true);
	}
}

void AGaspMoverEnemy::SetMorphGameplayEnabled(bool bEnabled)
{
	if (bEnabled)
	{
		ResumeMoverSim();
	}
	else
	{
		CachedMoveIntent = FVector::ZeroVector;
		AiMoveIntent = FVector::ZeroVector;
		AiFaceIntent = FVector::ZeroVector;
		bWantsCrouch = false;
		bWantsSprint = false;
		bJumpHeld = false;
		SuspendMoverSim();
	}
}

void AGaspMoverEnemy::StopMeshAnimation()
{
	auto StopOn = [](USkeletalMeshComponent* Mesh)
	{
		if (UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr)
		{
			Anim->StopAllMontages(0.1f);
		}
	};
	StopOn(SourceMesh);
	USkeletalMeshComponent* Preview = GetDevourPreviewMesh();
	if (Preview != SourceMesh)
	{
		StopOn(Preview);
	}
}

void AGaspMoverEnemy::SetVisualOverride(EGaspVisualOverride Override)
{
	VisualOverride = Override;
	ApplyVisualOverride();
}

static FName GaspVisualMeshName(EGaspVisualOverride Override)
{
	static const FName Names[] = {
		TEXT("Visual_UEFN"), TEXT("Visual_Manny"), TEXT("Visual_Quinn"),
		TEXT("Visual_Echo"), TEXT("Visual_TwinBlast"), TEXT("Visual_UE4")
	};
	const int32 Index = static_cast<int32>(Override);
	return (Index >= 0 && Index < UE_ARRAY_COUNT(Names)) ? Names[Index] : NAME_None;
}

USkeletalMeshComponent* AGaspMoverEnemy::FindVisualMesh(EGaspVisualOverride Override) const
{
	const FName Desired = GaspVisualMeshName(Override);
	if (Desired.IsNone())
	{
		return nullptr;
	}
	const FString DesiredStr = Desired.ToString();
	TArray<USkeletalMeshComponent*> Meshes;
	GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!Mesh || Mesh == SourceMesh)
		{
			continue;
		}
		const FString MeshName = Mesh->GetName();
		if (Mesh->GetFName() == Desired || MeshName.StartsWith(DesiredStr))
		{
			return Mesh;
		}
	}
	return nullptr;
}

void AGaspMoverEnemy::ApplyVisualOverride()
{
	const bool bShowSource = bDebugShowSourceMesh || VisualOverride == EGaspVisualOverride::UEFN;
	SnapSourceMeshToCapsule();
	if (SourceMesh)
	{
		SourceMesh->SetHiddenInGame(!bShowSource, false);
		SourceMesh->SetVisibility(bShowSource, false);
	}

	USkeletalMeshComponent* Chosen = (VisualOverride == EGaspVisualOverride::UEFN)
		? nullptr
		: FindVisualMesh(VisualOverride);
	if (Chosen && !Chosen->GetSkeletalMeshAsset())
	{
		Chosen = nullptr;
	}
	TArray<USkeletalMeshComponent*> Meshes;
	GetComponents<USkeletalMeshComponent>(Meshes);
	bool bAnyVisualShown = false;
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!Mesh || Mesh == SourceMesh)
		{
			continue;
		}
		SnapVisualMeshToSource(Mesh);
		const bool bShow = (Mesh == Chosen);
		Mesh->SetHiddenInGame(!bShow, false);
		Mesh->SetVisibility(bShow, false);
		Mesh->SetCollisionEnabled(bShow ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		Mesh->SetCollisionObjectType(ECC_Pawn);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Mesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		bAnyVisualShown |= bShow;
	}

	if (SourceMesh && !bShowSource && !bAnyVisualShown)
	{
		SourceMesh->SetHiddenInGame(false, false);
		SourceMesh->SetVisibility(true, false);
	}
}

USkeletalMeshComponent* AGaspMoverEnemy::GetDevourPreviewMesh() const
{
	if (USkeletalMeshComponent* Vis = FindVisualMesh(VisualOverride))
	{
		if (Vis->GetSkeletalMeshAsset())
		{
			return Vis;
		}
	}
	return SourceMesh;
}

USkeletalMeshComponent* AGaspMoverEnemy::ResolveActiveVisualMesh() const
{
	if (VisualOverride == EGaspVisualOverride::UEFN)
	{
		return SourceMesh;
	}
	if (USkeletalMeshComponent* Vis = FindVisualMesh(VisualOverride))
	{
		if (Vis->GetSkeletalMeshAsset())
		{
			return Vis;
		}
	}
	return SourceMesh;
}

void AGaspMoverEnemy::ForEachVisualMesh(TFunctionRef<void(UMeshComponent*)> Fn) const
{
	USkeletalMeshComponent* Active = ResolveActiveVisualMesh();
	TArray<USkeletalMeshComponent*> Meshes;
	GetComponents<USkeletalMeshComponent>(Meshes);
	bool bAny = false;
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!Mesh || Mesh->IsVisualizationComponent() || !Mesh->GetSkeletalMeshAsset())
		{
			continue;
		}
		if (Mesh == SourceMesh && Active != SourceMesh)
		{
			continue;
		}
		if (Mesh->bHiddenInGame)
		{
			continue;
		}
		Fn(Mesh);
		bAny = true;
	}
	if (!bAny && Active && !Active->IsA<UWidgetComponent>() && !Active->IsVisualizationComponent())
	{
		Fn(Active);
	}
}

FText AGaspMoverEnemy::GetResolvedDisplayName() const
{
	return DisplayName.IsEmpty() ? FText::FromString(TEXT("动作试样")) : DisplayName;
}

FLinearColor AGaspMoverEnemy::ResolveDevourWheelTint() const
{
	return FLinearColor(0.35f, 0.55f, 0.75f);
}

bool AGaspMoverEnemy::IsDevourableNow() const
{
	// Do NOT gate on bDevourLocked — FreezeDevourTarget sets it true, and TickPhase
	// re-checks CanDevourTarget every frame (same rule as AEnemyCharacter).
	return bDevourable && !bMorphTarget && !bPhantomInstance && !bDeathSequence;
}

float AGaspMoverEnemy::GetHealthPercent() const
{
	return Health ? Health->GetHealthPercent() : 0.f;
}

bool AGaspMoverEnemy::CanBeLockedOn() const
{
	return Health && Health->IsAlive() && !bDeathSequence;
}

FVector AGaspMoverEnemy::GetLockOnLocation() const
{
	return GetHudAnchorLocation();
}

bool AGaspMoverEnemy::GetFoliageInteractVolume(FVector& OutLocation, float& OutRadius) const
{
	OutLocation = GetActorLocation();
	OutRadius = Capsule ? Capsule->GetScaledCapsuleRadius() * 1.4f : 50.f;
	return true;
}

bool AGaspMoverEnemy::ShouldSuppressFoliageInteract() const
{
	return bDeathSequence || bDevouredDeath || (Health && !Health->IsAlive());
}

FVector AGaspMoverEnemy::GetVisualBoundsCenter() const
{
	if (USkeletalMeshComponent* Mesh = GetDevourPreviewMesh())
	{
		return Mesh->Bounds.Origin;
	}
	return GetActorLocation();
}

FVector AGaspMoverEnemy::GetHudAnchorLocation() const
{
	const float Half = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 96.f;
	return GetActorLocation() + FVector(0.f, 0.f, Half + HealthBarZOffset);
}

void AGaspMoverEnemy::RefreshHealthBarAnchor()
{
	if (HealthBar)
	{
		const float Half = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 96.f;
		HealthBar->SetRelativeLocation(FVector(0.f, 0.f, Half + HealthBarZOffset));
	}
}

bool AGaspMoverEnemy::GetStableMeshBounds(FBox& OutBox) const
{
	if (USkeletalMeshComponent* Mesh = GetDevourPreviewMesh())
	{
		OutBox = Mesh->Bounds.GetBox();
		return true;
	}
	return false;
}

void AGaspMoverEnemy::PlayHitFlash()
{
	if (bDeathSequence)
	{
		return;
	}
	if (bAuraFlashActive && IsValid(HitFlashMID))
	{
		if (bAuraOverlayUsesHitFlashParams)
		{
			HitFlashMID->SetScalarParameterValue(TEXT("HitFlash"), 1.f);
		}
		return;
	}

	UMaterialInterface* FlashMat = HitFlashMaterial.LoadSynchronous();
	if (!IsValid(FlashMat))
	{
		FlashMat = EnemyCombat::LoadDefaultHitFlashMaterial();
	}
	if (!IsValid(FlashMat))
	{
		return;
	}
	if (!IsValid(HitFlashMID) || HitFlashMID->Parent != FlashMat)
	{
		HitFlashMID = UMaterialInstanceDynamic::Create(FlashMat, this);
	}
	if (!IsValid(HitFlashMID))
	{
		return;
	}
	bAuraOverlayUsesHitFlashParams = true;
	HitFlashStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	HitFlashMID->SetVectorParameterValue(TEXT("HitColor"), FLinearColor(1.f, 0.15f, 0.1f));
	EnemyCombat::DriveGaspOverlayIntensity(HitFlashMID, true, 1.f, AuraOverlayOpacityMul, HitFlashStartTime);
	EnemyCombat::ApplyGaspVisualOverlay(this, HitFlashMID);

	HitFlashRemaining = 0.35f;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitFlashTimer);
		World->GetTimerManager().SetTimer(
			HitFlashTimer, this, &AGaspMoverEnemy::TickHitFlash, 0.016f, true);
	}
}

void AGaspMoverEnemy::PlayElementAuraFlash(ESlimeElement Element, float Duration)
{
	if (bDeathSequence || Duration <= KINDA_SMALL_NUMBER)
	{
		if (Duration <= KINDA_SMALL_NUMBER)
		{
			ClearElementAuraFlash();
		}
		return;
	}

	UMaterialInterface* HitFlash = HitFlashMaterial.LoadSynchronous();
	if (!IsValid(HitFlash))
	{
		HitFlash = EnemyCombat::LoadDefaultHitFlashMaterial();
	}
	UMaterialInterface* Lightning = LightningHitOverlay.LoadSynchronous();
	if (!IsValid(Lightning))
	{
		Lightning = EnemyCombat::LoadDefaultLightningHitOverlay();
	}
	UMaterialInterface* Wind = WindHitOverlay.LoadSynchronous();
	if (!IsValid(Wind))
	{
		Wind = EnemyCombat::LoadDefaultWindHitOverlay();
	}
	UMaterialInterface* FlashMat = EnemyCombat::ResolveGaspElementHitOverlay(Element, HitFlash, Lightning, Wind);
	if (!IsValid(FlashMat))
	{
		return;
	}

	const bool bUsesHitParams = EnemyCombat::GaspElementOverlayUsesHitFlashParams(Element, FlashMat, HitFlash);
	if (!IsValid(HitFlashMID) || HitFlashMID->Parent != FlashMat)
	{
		HitFlashMID = UMaterialInstanceDynamic::Create(FlashMat, this);
	}
	if (!IsValid(HitFlashMID))
	{
		return;
	}

	bAuraFlashActive = true;
	bAuraOverlayUsesHitFlashParams = bUsesHitParams;
	if (!bUsesHitParams)
	{
		float Mul = 2.f;
		FlashMat->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Opacity Multiplier")), Mul);
		AuraOverlayOpacityMul = Mul > KINDA_SMALL_NUMBER ? Mul : 2.f;
	}
	HitFlashStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (bUsesHitParams)
	{
		HitFlashMID->SetVectorParameterValue(TEXT("HitColor"), SlimeCombat::GetElementVfxColor(Element));
	}
	EnemyCombat::DriveGaspOverlayIntensity(HitFlashMID, bUsesHitParams, 1.f, AuraOverlayOpacityMul, HitFlashStartTime);
	EnemyCombat::ApplyGaspVisualOverlay(this, HitFlashMID);
	HitFlashRemaining = Duration;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitFlashTimer);
		World->GetTimerManager().SetTimer(
			HitFlashTimer, this, &AGaspMoverEnemy::TickHitFlash, 0.016f, true);
	}
}

void AGaspMoverEnemy::PlayElementAuraFlashByColor(FLinearColor Color, float Duration)
{
	if (bDeathSequence || Duration <= KINDA_SMALL_NUMBER)
	{
		if (Duration <= KINDA_SMALL_NUMBER)
		{
			ClearElementAuraFlash();
		}
		return;
	}

	UMaterialInterface* FlashMat = HitFlashMaterial.LoadSynchronous();
	if (!IsValid(FlashMat))
	{
		FlashMat = EnemyCombat::LoadDefaultHitFlashMaterial();
	}
	if (!IsValid(FlashMat))
	{
		return;
	}
	if (!IsValid(HitFlashMID) || HitFlashMID->Parent != FlashMat)
	{
		HitFlashMID = UMaterialInstanceDynamic::Create(FlashMat, this);
	}
	if (!IsValid(HitFlashMID))
	{
		return;
	}
	bAuraFlashActive = true;
	bAuraOverlayUsesHitFlashParams = true;
	HitFlashStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	HitFlashMID->SetVectorParameterValue(TEXT("HitColor"), Color);
	EnemyCombat::DriveGaspOverlayIntensity(HitFlashMID, true, 1.f, AuraOverlayOpacityMul, HitFlashStartTime);
	EnemyCombat::ApplyGaspVisualOverlay(this, HitFlashMID);
	HitFlashRemaining = Duration;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitFlashTimer);
		World->GetTimerManager().SetTimer(
			HitFlashTimer, this, &AGaspMoverEnemy::TickHitFlash, 0.016f, true);
	}
}

void AGaspMoverEnemy::ClearElementAuraFlash()
{
	bAuraFlashActive = false;
	ClearHitFlash();
}

void AGaspMoverEnemy::TickHitFlash()
{
	HitFlashRemaining -= 0.016f;
	const float Pulse = bAuraFlashActive
		? (HitFlashRemaining > 0.f ? 1.f : 0.f)
		: FMath::Clamp(HitFlashRemaining / 0.35f, 0.f, 1.f);
	EnemyCombat::DriveGaspOverlayIntensity(
		HitFlashMID, bAuraOverlayUsesHitFlashParams, Pulse, AuraOverlayOpacityMul, HitFlashStartTime);
	if (HitFlashRemaining <= 0.f)
	{
		if (bAuraFlashActive)
		{
			ClearElementAuraFlash();
		}
		else
		{
			ClearHitFlash();
		}
	}
}

void AGaspMoverEnemy::ClearHitFlash()
{
	HitFlashRemaining = 0.f;
	bAuraFlashActive = false;
	bAuraOverlayUsesHitFlashParams = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitFlashTimer);
	}
	EnemyCombat::ClearGaspVisualOverlay(this);
}

UAnimMontage* AGaspMoverEnemy::ResolveHitReactMontage(const FVector& DamageLocation) const
{
	FVector ToHit = DamageLocation - GetActorLocation();
	ToHit.Z = 0.f;
	if (ToHit.IsNearlyZero())
	{
		return EnemyCombat::LoadGaspHitReactMontage(
			HitReactFront,
			TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01.MM_HitReact_Front_Lgt_01"));
	}
	ToHit.Normalize();
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = GetActorRightVector().GetSafeNormal2D();
	const float ForwardDot = FVector::DotProduct(Forward, ToHit);
	const float RightDot = FVector::DotProduct(Right, ToHit);
	if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
	{
		return ForwardDot >= 0.f
			? EnemyCombat::LoadGaspHitReactMontage(
				HitReactFront,
				TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01.MM_HitReact_Front_Lgt_01"))
			: EnemyCombat::LoadGaspHitReactMontage(
				HitReactBack,
				TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Back_Med_01.MM_HitReact_Back_Med_01"));
	}
	return RightDot >= 0.f
		? EnemyCombat::LoadGaspHitReactMontage(
			HitReactRight,
			TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_03.MM_HitReact_Front_Lgt_03"))
		: EnemyCombat::LoadGaspHitReactMontage(
			HitReactLeft,
			TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_02.MM_HitReact_Front_Lgt_02"));
}

void AGaspMoverEnemy::PlayHitReact(const FVector& DamageLocation)
{
	if (bDeathSequence || bDevourLocked || bMorphTarget)
	{
		return;
	}
	if (Combat && Combat->IsAttacking() && !bHitReactInterruptsAttack)
	{
		return;
	}
	UAnimMontage* Montage = ResolveHitReactMontage(DamageLocation);
	UAnimInstance* Anim = SourceMesh ? SourceMesh->GetAnimInstance() : nullptr;
	if (!Montage || !Anim)
	{
		return;
	}
	if (bHitReactInterruptsAttack && Combat)
	{
		Combat->InterruptCombat();
	}
	StopHitReact();
	if (Anim->Montage_Play(Montage) <= 0.f)
	{
		return;
	}
	ActiveHitReactMontage = Montage;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			HitReactTimer,
			this,
			&AGaspMoverEnemy::StopHitReact,
			FMath::Max(HitReactSeconds, 0.05f),
			false);
	}
}

void AGaspMoverEnemy::StopHitReact()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitReactTimer);
	}
	if (UAnimMontage* Montage = ActiveHitReactMontage.Get())
	{
		if (UAnimInstance* Anim = SourceMesh ? SourceMesh->GetAnimInstance() : nullptr)
		{
			Anim->Montage_Stop(0.25f, Montage);
		}
	}
	ActiveHitReactMontage.Reset();
}

void AGaspMoverEnemy::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	if (bDeathSequence)
	{
		return;
	}
	if (Damage > 0.f)
	{
		LastDamageLocation = DamageLocation;
		LastDamageImpulse = DamageImpulse;
		PlayHitFlash();
		EnemyCombat::PlayGaspSfxAt(this, HitTakenSound, EnemyCombat::DefaultGaspHitTakenSound, GetActorLocation());
		if (bEnableRagdollKit)
		{
			EnemyCombat::CallGaspRagdollOnHit(this, DamageLocation, DamageImpulse);
		}
	}
	if (Health)
	{
		Health->ApplyDamage(Damage, DamageCauser, DamageLocation, DamageImpulse);
	}
	if (Damage > 0.f && Health && Health->IsAlive() && !bCombatKnockdown)
	{
		AccrueCombatStun(Damage);
	}
}

void AGaspMoverEnemy::ApplyHealing(float Healing, AActor* Healer)
{
	(void)Healer;
	if (Health)
	{
		Health->ApplyHealing(Healing);
	}
}

void AGaspMoverEnemy::NotifyDanger(const FVector& DangerLocation, AActor* DangerSource)
{
	(void)DangerLocation;
	(void)DangerSource;
}

void AGaspMoverEnemy::HandleDeath()
{
	if (bDeathSequence)
	{
		return;
	}
	if (Health && Health->IsAlive())
	{
		ApplyDamage(FMath::Max(Health->CurrentHP, 1.f), this, GetActorLocation(), FVector::ZeroVector);
	}
	else
	{
		HandleDied();
	}
}

void AGaspMoverEnemy::HandleDied()
{
	if (bDeathSequence && !bMorphTarget)
	{
		// Allow re-entry only for morph path below when already marked.
	}
	if (bMorphTarget)
	{
		if (AActor* Master = MorphMaster.Get())
		{
			if (USlimeMorphComponent* Morph = Master->FindComponentByClass<USlimeMorphComponent>())
			{
				Morph->ForceUnmorph(true);
			}
		}
		return;
	}
	if (bDeathSequence)
	{
		return;
	}

	bDeathSequence = true;
	AiMoveIntent = FVector::ZeroVector;
	AiFaceIntent = FVector::ZeroVector;
	CachedMoveIntent = FVector::ZeroVector;
	bWantChaseGait = false;

	if (HealthBar)
	{
		HealthBar->SetVisibility(false);
		HealthBar->SetHiddenInGame(true);
	}
	if (Combat)
	{
		Combat->InterruptCombat();
	}

	if (bDevouredDeath)
	{
		SetActorEnableCollision(false);
		SetActorHiddenInGame(true);
		StopDeathController();
		return;
	}

	BeginKnockdownDeath();
}

void AGaspMoverEnemy::TriggerSandboxRagdoll()
{
	if (!bEnableRagdollKit || bDeathSequence)
	{
		return;
	}
	EnsureMoverModes();
	bPendingRagdoll = true;
	if (Mover && Mover->FindMovementModeByName(FName(TEXT("Ragdoll"))))
	{
		Mover->QueueNextMode(FName(TEXT("Ragdoll")), true);
	}
	DispatchBoolEventOnComponents(TEXT("TriggerRagdoll"));
	if (UFunction* Fn = FindFunction(TEXT("TriggerRagdoll")))
	{
		ProcessEvent(Fn, nullptr);
	}
}

void AGaspMoverEnemy::SuspendMoverSim()
{
	bMoverFrozen = true;
	if (bMoverSimSuspended)
	{
		return;
	}
	bMoverSimSuspended = true;
	if (Mover)
	{
		Mover->SetComponentTickEnabled(false);
	}
	if (UActorComponent* Liaison = FindComponentByInterface(UMoverBackendLiaisonInterface::StaticClass()))
	{
		Liaison->RegisterAllComponentTickFunctions(false);
	}
	SetMorphLocomotionTicksEnabled(false);
}

void AGaspMoverEnemy::ResumeMoverSim()
{
	bMoverFrozen = false;
	if (!bMoverSimSuspended)
	{
		return;
	}
	bMoverSimSuspended = false;
	if (UActorComponent* Liaison = FindComponentByInterface(UMoverBackendLiaisonInterface::StaticClass()))
	{
		Liaison->RegisterAllComponentTickFunctions(true);
	}
	if (Mover)
	{
		Mover->SetComponentTickEnabled(true);
	}
	SetMorphLocomotionTicksEnabled(true);
}

void AGaspMoverEnemy::SetMorphLocomotionTicksEnabled(bool bEnabled)
{
	auto SetMesh = [bEnabled](USkeletalMeshComponent* Mesh)
	{
		if (!Mesh)
		{
			return;
		}
		Mesh->bPauseAnims = !bEnabled;
		Mesh->SetComponentTickEnabled(bEnabled);
	};
	SetMesh(SourceMesh);
	USkeletalMeshComponent* Preview = GetDevourPreviewMesh();
	if (Preview != SourceMesh)
	{
		SetMesh(Preview);
	}
}

bool AGaspMoverEnemy::WantsHeldRagdollMode() const
{
	if (!bEnableRagdollKit)
	{
		return false;
	}
	if (bPendingRagdoll || bDeathRagdollArmed)
	{
		return true;
	}
	return bCombatKnockdown && !bCombatGetUpRequested;
}

void AGaspMoverEnemy::ForcePhysicalRagdollBodies(bool bDisableCapsule)
{
	auto Force = [](USkeletalMeshComponent* Mesh)
	{
		if (!Mesh)
		{
			return;
		}
		if (UAnimInstance* Anim = Mesh->GetAnimInstance())
		{
			Anim->StopAllMontages(0.f);
		}
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		if (!Mesh->IsSimulatingPhysics() && !Mesh->IsAnySimulatingPhysics())
		{
			Mesh->SetAllBodiesSimulatePhysics(true);
			Mesh->SetSimulatePhysics(true);
		}
		Mesh->WakeAllRigidBodies();
	};
	Force(GetDevourPreviewMesh());
	if (SourceMesh && SourceMesh != GetDevourPreviewMesh())
	{
		Force(SourceMesh);
	}
	if (bDisableCapsule && Capsule)
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

bool AGaspMoverEnemy::ApplyPendingRagdollInput(FCharacterDefaultInputs& Inputs)
{
	if (!WantsHeldRagdollMode())
	{
		return false;
	}
	Inputs.SuggestedMovementMode = FName(TEXT("Ragdoll"));
	Inputs.SetMoveInput(EMoveInputType::DirectionalIntent, FVector::ZeroVector);
	bJumpJustPressed = false;
	bPendingRagdoll = false;
	return true;
}

void AGaspMoverEnemy::KeepDeathRagdollPhysics()
{
	const bool bHoldDeath = bDeathSequence && !bDevouredDeath && bDeathRagdollArmed;
	const bool bHoldCombat = bCombatKnockdown && !bCombatGetUpRequested && !bDeathSequence;
	if (!bHoldDeath && !bHoldCombat)
	{
		return;
	}

	if (Mover && Mover->FindMovementModeByName(FName(TEXT("Ragdoll")))
		&& Mover->GetMovementModeName() != FName(TEXT("Ragdoll")))
	{
		Mover->QueueNextMode(FName(TEXT("Ragdoll")), true);
	}

	const bool bInRagdoll = Mover && Mover->GetMovementModeName() == FName(TEXT("Ragdoll"));
	const float Start = bHoldDeath ? DeathAwaitingRagdollTime : CombatKnockdownStartTime;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : Start;
	const bool bWaited = (Now - Start) >= 0.15f;
	if (!bInRagdoll && !bWaited)
	{
		return;
	}

	ForcePhysicalRagdollBodies(!bInRagdoll);
	if (!bInRagdoll && bWaited && !bMoverSimSuspended)
	{
		SuspendMoverSim();
	}
}

void AGaspMoverEnemy::BeginKnockdownDeath()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CombatGetUpTimer);
		World->GetTimerManager().ClearTimer(DeathRagdollTimer);
	}
	bCombatKnockdown = false;
	bCombatGetUpRequested = false;
	bDeathRagdollArmed = false;
	bDeathAwaitingRagdoll = false;
	StopDeathController();
	SuspendMoverSim();
	EnemyCombat::PrepareGaspDeathPhysics(Capsule, SourceMesh, GetDevourPreviewMesh());
	const float Played = PlayDeathKnockdownMontage();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DeathRagdollTimer,
			this,
			&AGaspMoverEnemy::StartDeathDissolve,
			FMath::Max(Played, 0.2f),
			false);
	}
	else
	{
		StartDeathDissolve();
	}
}

float AGaspMoverEnemy::PlayDeathKnockdownMontage()
{
	UAnimationAsset* Anim = EnemyCombat::LoadGaspDeathKnockdownAnim(
		EnemyCombat::ResolveGaspHitCardinal(this, LastDamageLocation));
	return EnemyCombat::PlayGaspDeathSingleNode(SourceMesh, Anim);
}

void AGaspMoverEnemy::HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	(void)Montage;
	(void)bInterrupted;
	if (!bDeathSequence)
	{
		return;
	}
	auto HoldPose = [](USkeletalMeshComponent* Mesh)
	{
		if (IsValid(Mesh))
		{
			Mesh->bPauseAnims = true;
		}
	};
	HoldPose(SourceMesh);
	if (GetDevourPreviewMesh() != SourceMesh)
	{
		HoldPose(GetDevourPreviewMesh());
	}
}

void AGaspMoverEnemy::QueueDeathRagdoll()
{
	bDeathRagdollArmed = true;
	if (bEnableRagdollKit)
	{
		TriggerSandboxRagdoll();
		return;
	}
	bDeathAwaitingRagdoll = false;
	StopDeathController();
	StartDeathDissolve();
}

void AGaspMoverEnemy::AccrueCombatStun(float Damage)
{
	if (bDeathSequence || bCombatKnockdown || bDevouredDeath || !bEnableRagdollKit)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World || Damage <= 0.f)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	const float Window = FMath::Max(StunWindowSeconds, 0.2f);
	StunHits.RemoveAll([Now, Window](const TPair<float, float>& Hit)
	{
		return (Now - Hit.Key) > Window;
	});
	StunHits.Emplace(Now, Damage);
	float Accrued = 0.f;
	for (const TPair<float, float>& Hit : StunHits)
	{
		Accrued += Hit.Value;
	}
	const float Max = Health ? Health->MaxHP : MaxHP;
	if (Accrued >= StunDamagePercent * FMath::Max(Max, 1.f))
	{
		StunHits.Reset();
		BeginCombatKnockdown();
	}
}

void AGaspMoverEnemy::BeginCombatKnockdown()
{
	if (bCombatKnockdown || bDeathSequence || bDevouredDeath || !bEnableRagdollKit)
	{
		return;
	}
	bCombatKnockdown = true;
	bCombatGetUpRequested = false;
	CombatKnockdownStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	AiMoveIntent = FVector::ZeroVector;
	AiFaceIntent = FVector::ZeroVector;
	CachedMoveIntent = FVector::ZeroVector;
	bWantChaseGait = false;
	if (Combat)
	{
		Combat->InterruptCombat();
	}
	TriggerSandboxRagdoll();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CombatGetUpTimer,
			this,
			&AGaspMoverEnemy::PlayCombatGetUp,
			FMath::Max(GetUpDelaySeconds, 0.1f),
			false);
	}
	else
	{
		PlayCombatGetUp();
	}
}

void AGaspMoverEnemy::PlayCombatGetUp()
{
	if (bDeathSequence || !bCombatKnockdown)
	{
		return;
	}
	static const FName GetUpNames[] = {
		FName(TEXT("Ragdoll_PlayRollingGetups")),
		FName(TEXT("RagdollPlayRollingGetups")),
		FName(TEXT("PlayRollingGetups")),
	};
	bool bCalled = false;
	for (const FName& Name : GetUpNames)
	{
		if (EnemyCombat::CallGaspRagdollFunction(this, Name, nullptr))
		{
			bCalled = true;
			break;
		}
	}
	bCombatGetUpRequested = true;
	CombatGetUpRequestedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (!bCalled)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("GaspMoverEnemy %s: Ragdoll_PlayRollingGetups missing — restore Walking"), *GetName());
		ResumeMoverSim();
		if (Mover && Mover->FindMovementModeByName(FName(TEXT("Walking"))))
		{
			Mover->QueueNextMode(FName(TEXT("Walking")), true);
		}
		if (SourceMesh)
		{
			SourceMesh->SetAllBodiesSimulatePhysics(false);
			SourceMesh->SetSimulatePhysics(false);
		}
		if (Capsule)
		{
			Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		EndCombatKnockdown();
	}
}

void AGaspMoverEnemy::TickCombatKnockdown()
{
	if (!bCombatKnockdown || bDeathSequence)
	{
		return;
	}
	if (!bCombatGetUpRequested)
	{
		if (Mover && Mover->GetMovementModeName() != FName(TEXT("Ragdoll"))
			&& Mover->FindMovementModeByName(FName(TEXT("Ragdoll"))))
		{
			Mover->QueueNextMode(FName(TEXT("Ragdoll")), true);
		}
		return;
	}
	const bool bInRagdoll = Mover && Mover->GetMovementModeName() == FName(TEXT("Ragdoll"));
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : CombatGetUpRequestedTime;
	if (!bInRagdoll || (Now - CombatGetUpRequestedTime) > 4.f)
	{
		EndCombatKnockdown();
	}
}

void AGaspMoverEnemy::EndCombatKnockdown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CombatGetUpTimer);
	}
	ResumeMoverSim();
	bCombatKnockdown = false;
	bCombatGetUpRequested = false;
	StunHits.Reset();
}

void AGaspMoverEnemy::ConfirmDeathRagdollThenStopAI()
{
	if (!bDeathAwaitingRagdoll || bDevouredDeath)
	{
		return;
	}
	const bool bInRagdoll = Mover && Mover->GetMovementModeName() == FName(TEXT("Ragdoll"));
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : DeathAwaitingRagdollTime;
	if (!bInRagdoll && (Now - DeathAwaitingRagdollTime) < 0.75f)
	{
		return;
	}
	bDeathAwaitingRagdoll = false;
	StopDeathController();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DeathRagdollTimer,
			this,
			&AGaspMoverEnemy::StartDeathDissolve,
			FMath::Max(DeathRagdollLingerSeconds, 0.2f),
			false);
	}
	else
	{
		StartDeathDissolve();
	}
}

void AGaspMoverEnemy::StopDeathController()
{
	if (AController* Ctrl = GetController())
	{
		Ctrl->StopMovement();
		Ctrl->SetActorTickEnabled(false);
		if (AAIController* AIC = Cast<AAIController>(Ctrl))
		{
			if (UBrainComponent* Brain = AIC->GetBrainComponent())
			{
				Brain->StopLogic(TEXT("Death"));
			}
		}
		DetachFromControllerPendingDestroy();
	}
}

void AGaspMoverEnemy::StartDeathDissolve()
{
	if (bDevouredDeath)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeathRagdollTimer);
	}
	if (SourceMesh)
	{
		SourceMesh->bPauseAnims = true;
	}
	if (USkeletalMeshComponent* Visual = GetDevourPreviewMesh())
	{
		Visual->bPauseAnims = true;
	}
	if (UMaterialInterface* DissolveMat = DeathDissolveMaterial.LoadSynchronous())
	{
		if (!DeathDissolveMID)
		{
			DeathDissolveMID = UMaterialInstanceDynamic::Create(DissolveMat, this);
		}
		ForEachVisualMesh([this](UMeshComponent* Mesh)
		{
			if (Mesh)
			{
				Mesh->SetOverlayMaterial(DeathDissolveMID);
			}
		});
		EnemyCombat::SetGaspDeathDissolveAmount(DeathDissolveMID, 0.f);
	}
	DeathDissolveElapsed = 0.01f;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DeathDissolveTimer,
			this,
			&AGaspMoverEnemy::TickDeathDissolve,
			0.05f,
			true);
	}
}

void AGaspMoverEnemy::TickDeathDissolve()
{
	DeathDissolveElapsed += 0.05f;
	const float Alpha = 1.f - FMath::Clamp(DeathDissolveElapsed / FMath::Max(DeathDissolveSeconds, 0.2f), 0.f, 1.f);
	EnemyCombat::SetGaspDeathDissolveAmount(DeathDissolveMID, 1.f - Alpha);
	ForEachVisualMesh([Alpha](UMeshComponent* Mesh)
	{
		if (Mesh)
		{
			Mesh->SetVisibility(Alpha > 0.05f);
		}
	});
	if (DeathDissolveElapsed >= DeathDissolveSeconds)
	{
		FinishDeathSequence();
	}
}

void AGaspMoverEnemy::FinishDeathSequence()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeathDissolveTimer);
		World->GetTimerManager().ClearTimer(DeathRagdollTimer);
		World->GetTimerManager().ClearTimer(CombatGetUpTimer);
	}
	Destroy();
}

static int64 MatchGaspMoverEnumByDisplayName(UEnum* Enum, const FString& Needle)
{
	if (!Enum)
	{
		return INDEX_NONE;
	}
	const FString NeedleLower = Needle.ToLower();
	for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
	{
		const FString Name = Enum->GetNameStringByIndex(i);
		const FString Display = Enum->GetDisplayNameTextByIndex(i).ToString();
		if (Name.Equals(Needle, ESearchCase::IgnoreCase)
			|| Display.Equals(Needle, ESearchCase::IgnoreCase)
			|| Name.Contains(Needle, ESearchCase::IgnoreCase)
			|| Display.ToLower().Contains(NeedleLower))
		{
			return Enum->GetValueByIndex(i);
		}
	}
	return INDEX_NONE;
}

void AGaspMoverEnemy::FillGaspAnimPropertiesInternal(UScriptStruct* Struct, void* StructPtr)
{
	if (!Struct || !StructPtr)
	{
		return;
	}

	const FVector Velocity = LastVelocity;
	const FVector Accel = CachedAcceleration;
	const FRotator AimRot = GetControlRotation();

	auto WriteVector = [&](const TCHAR* Prefix, const FVector& V)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop || !Prop->GetName().StartsWith(Prefix))
			{
				continue;
			}
			if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
			{
				if (StructProp->Struct && StructProp->Struct->GetFName() == NAME_Vector)
				{
					*StructProp->ContainerPtrToValuePtr<FVector>(StructPtr) = V;
					return;
				}
			}
			if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
			{
				const FString Name = Prop->GetName();
				float Value = 0.f;
				if (Name.EndsWith(TEXT("_X")) || Name.Contains(TEXT("_X_")))
				{
					Value = V.X;
				}
				else if (Name.EndsWith(TEXT("_Y")) || Name.Contains(TEXT("_Y_")))
				{
					Value = V.Y;
				}
				else if (Name.EndsWith(TEXT("_Z")) || Name.Contains(TEXT("_Z_")))
				{
					Value = V.Z;
				}
				else
				{
					continue;
				}
				FloatProp->SetPropertyValue_InContainer(StructPtr, Value);
			}
		}
	};

	auto WriteRotator = [&](const TCHAR* Prefix, const FRotator& R)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop || !Prop->GetName().StartsWith(Prefix))
			{
				continue;
			}
			if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
			{
				if (StructProp->Struct && StructProp->Struct->GetFName() == NAME_Rotator)
				{
					*StructProp->ContainerPtrToValuePtr<FRotator>(StructPtr) = R;
					return;
				}
			}
		}
	};

	auto WriteEnumByName = [&](const TCHAR* Prefix, const FString& DisplayNeedle)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop || !Prop->GetName().StartsWith(Prefix))
			{
				continue;
			}
			if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
			{
				const int64 Value = MatchGaspMoverEnumByDisplayName(EnumProp->GetEnum(), DisplayNeedle);
				if (Value != INDEX_NONE)
				{
					void* ValuePtr = EnumProp->ContainerPtrToValuePtr<void>(StructPtr);
					EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, Value);
				}
				return;
			}
			if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
			{
				if (UEnum* Enum = ByteProp->GetIntPropertyEnum())
				{
					const int64 Value = MatchGaspMoverEnumByDisplayName(Enum, DisplayNeedle);
					if (Value != INDEX_NONE)
					{
						ByteProp->SetPropertyValue_InContainer(StructPtr, static_cast<uint8>(Value));
					}
				}
				return;
			}
		}
	};

	auto WriteBool = [&](const TCHAR* Prefix, bool bValue)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop || !Prop->GetName().StartsWith(Prefix))
			{
				continue;
			}
			if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
			{
				BoolProp->SetPropertyValue_InContainer(StructPtr, bValue);
				return;
			}
		}
	};

	WriteVector(TEXT("Velocity"), Velocity);
	WriteVector(TEXT("Acceleration"), Accel);
	WriteRotator(TEXT("AimingRotation"), AimRot);

	FString ModeNeedle = TEXT("OnGround");
	if (CachedMovementMode == TEXT("Falling"))
	{
		ModeNeedle = TEXT("InAir");
	}
	else if (CachedMovementMode == TEXT("Flying") || CachedMovementMode == TEXT("Traversing"))
	{
		ModeNeedle = TEXT("InAir");
	}
	else if (CachedMovementMode == TEXT("Sliding"))
	{
		ModeNeedle = TEXT("Grounded");
	}
	WriteEnumByName(TEXT("MovementMode"), ModeNeedle);
	WriteEnumByName(TEXT("Gait"), CachedGaitName);
	WriteEnumByName(TEXT("Stance"), CachedStanceName);
	WriteEnumByName(TEXT("RotationMode"), CachedRotationModeName);

	const bool bSprint = CachedGaitName.Equals(TEXT("Sprint"), ESearchCase::IgnoreCase);
	const bool bWalk = CachedGaitName.Equals(TEXT("Walk"), ESearchCase::IgnoreCase);
	WriteBool(TEXT("Sprint"), bSprint);
	WriteBool(TEXT("Walk"), bWalk);
	WriteBool(TEXT("Crouch"), bWantsCrouch);
	WriteBool(TEXT("WantsToSprint"), bSprint);
	WriteBool(TEXT("WantsToWalk"), bWalk);
	WriteBool(TEXT("WantsToCrouch"), bWantsCrouch);
	WriteBool(TEXT("Strafe"), false);
	WriteBool(TEXT("Aim"), false);
}

void AGaspMoverEnemy::FillGaspTraversalPropertiesInternal(UScriptStruct* Struct, void* StructPtr)
{
	if (!Struct || !StructPtr)
	{
		return;
	}

	auto WriteObject = [&](const TCHAR* Prefix, UObject* Obj)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop || !Prop->GetName().StartsWith(Prefix))
			{
				continue;
			}
			if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
			{
				ObjProp->SetObjectPropertyValue_InContainer(StructPtr, Obj);
				return;
			}
		}
	};

	auto WriteFloat = [&](const TCHAR* Prefix, float Value)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop || !Prop->GetName().StartsWith(Prefix))
			{
				continue;
			}
			if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
			{
				FloatProp->SetPropertyValue_InContainer(StructPtr, Value);
				return;
			}
			if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
			{
				DoubleProp->SetPropertyValue_InContainer(StructPtr, static_cast<double>(Value));
				return;
			}
		}
	};

	auto WriteEnumByName = [&](const TCHAR* Prefix, const FString& DisplayNeedle)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop || !Prop->GetName().StartsWith(Prefix))
			{
				continue;
			}
			if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
			{
				const int64 Value = MatchGaspMoverEnumByDisplayName(EnumProp->GetEnum(), DisplayNeedle);
				if (Value != INDEX_NONE)
				{
					void* ValuePtr = EnumProp->ContainerPtrToValuePtr<void>(StructPtr);
					EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, Value);
				}
				return;
			}
			if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
			{
				if (UEnum* Enum = ByteProp->GetIntPropertyEnum())
				{
					const int64 Value = MatchGaspMoverEnumByDisplayName(Enum, DisplayNeedle);
					if (Value != INDEX_NONE)
					{
						ByteProp->SetPropertyValue_InContainer(StructPtr, static_cast<uint8>(Value));
					}
				}
				return;
			}
		}
	};

	WriteObject(TEXT("Capsule"), Capsule.Get());
	WriteObject(TEXT("Mesh"), SourceMesh.Get());
	WriteObject(TEXT("MotionWarping"), FindComponentByClass<UMotionWarpingComponent>());
	WriteFloat(TEXT("Speed"), LastVelocity.Size2D());

	FString ModeNeedle = TEXT("OnGround");
	if (CachedMovementMode == TEXT("Falling")
		|| CachedMovementMode == TEXT("Flying")
		|| CachedMovementMode == TEXT("Traversing"))
	{
		ModeNeedle = TEXT("InAir");
	}
	WriteEnumByName(TEXT("MovementMode"), ModeNeedle);
	WriteEnumByName(TEXT("Gait"), CachedGaitName);
}

void AGaspMoverEnemy::FillGaspAnimProperties(UPARAM(ref) int32& OutProperties)
{
	(void)OutProperties;
}

DEFINE_FUNCTION(AGaspMoverEnemy::execFillGaspAnimProperties)
{
	Stack.MostRecentProperty = nullptr;
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);
	void* StructPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* StructProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	P_FINISH;
	P_NATIVE_BEGIN;
	if (StructPtr && StructProp && StructProp->Struct)
	{
		P_THIS->FillGaspAnimPropertiesInternal(StructProp->Struct, StructPtr);
	}
	P_NATIVE_END;
}

void AGaspMoverEnemy::WireSandboxAnimInterface()
{
	if (bSandboxAnimInterfaceWired && bSandboxTraversalInterfaceWired)
	{
		return;
	}

	UClass* IfaceClass = LoadClass<UInterface>(
		nullptr,
		TEXT("/Game/Blueprints/BPI_SandboxCharacter_Pawn.BPI_SandboxCharacter_Pawn_C"));
	if (!IfaceClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GaspMoverEnemy: BPI_SandboxCharacter_Pawn missing — AnimBP will stay idle."));
		return;
	}

	UClass* MyClass = GetClass();
	UClass* NativeClass = AGaspMoverEnemy::StaticClass();
	if (!MyClass || !NativeClass)
	{
		return;
	}

	if (!MyClass->ImplementsInterface(IfaceClass))
	{
		FImplementedInterface NewImp;
		NewImp.Class = IfaceClass;
		NewImp.PointerOffset = 0;
		NewImp.bImplementedByK2 = false;
		MyClass->Interfaces.Add(NewImp);
		UE_LOG(LogTemp, Log,
			TEXT("GaspMoverEnemy: registered BPI_SandboxCharacter_Pawn on %s"),
			*MyClass->GetName());
	}

	// Previous builds Rename()'d the BP stub and left an ObjectRedirector; StaticDuplicateObject
	// into the same name then fatal'd ("Cannot replace existing object of a different class").
	// Clear redirectors / void stubs on the BPGC so the native-class UFunction is found.
	auto TrashNamedChild = [](UClass* OuterClass, const TCHAR* Name)
	{
		for (int32 Guard = 0; Guard < 8; ++Guard)
		{
			UObject* Obj = StaticFindObject(nullptr, OuterClass, Name);
			if (!Obj)
			{
				break;
			}
			if (UFunction* AsFn = Cast<UFunction>(Obj))
			{
				OuterClass->RemoveFunctionFromFunctionMap(AsFn);
			}
			const FName TrashName = MakeUniqueObjectName(
				GetTransientPackage(), Obj->GetClass(), TEXT("TrashedGaspAnimFn"));
			Obj->Rename(
				*TrashName.ToString(),
				GetTransientPackage(),
				REN_DontCreateRedirectors | REN_DoNotDirty | REN_NonTransactional);
		}
	};

	auto WireNativeIfaceFn = [&](const TCHAR* FnName, FNativeFuncPtr NativeExec, bool& bWiredFlag)
	{
		if (bWiredFlag)
		{
			return;
		}

		TrashNamedChild(MyClass, FnName);

		UFunction* IfaceFn = IfaceClass->FindFunctionByName(FnName);
		UFunction* Fn = NativeClass->FindFunctionByName(FnName);
		if (!Fn && IfaceFn)
		{
			Fn = Cast<UFunction>(StaticDuplicateObject(
				IfaceFn,
				NativeClass,
				FnName,
				RF_Public | RF_MarkAsNative));
			if (Fn)
			{
				Fn->Script.Empty();
				Fn->FunctionFlags |= FUNC_Native | FUNC_BlueprintCallable | FUNC_BlueprintPure;
				Fn->FunctionFlags &= ~(FUNC_BlueprintEvent | FUNC_BlueprintAuthorityOnly);
				NativeClass->AddFunctionToFunctionMap(Fn, Fn->GetFName());
				UE_LOG(LogTemp, Log,
					TEXT("GaspMoverEnemy: installed %s on native class"), FnName);
			}
		}

		if (!Fn)
		{
			Fn = MyClass->FindFunctionByName(FnName);
		}
		if (!Fn)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("GaspMoverEnemy: %s unavailable on %s"), FnName, *MyClass->GetName());
			return;
		}

		Fn->FunctionFlags |= FUNC_Native | FUNC_BlueprintCallable | FUNC_BlueprintPure;
		Fn->FunctionFlags &= ~FUNC_BlueprintEvent;
		Fn->Script.Empty();
		Fn->SetNativeFunc(NativeExec);
		bWiredFlag = true;
		UE_LOG(LogTemp, Log,
			TEXT("GaspMoverEnemy: %s → native Fill (fn outer=%s)"),
			FnName, *Fn->GetOuter()->GetName());
	};

	WireNativeIfaceFn(
		TEXT("Get_PropertiesForAnimation"),
		&AGaspMoverEnemy::execGet_PropertiesForAnimation,
		bSandboxAnimInterfaceWired);
	WireNativeIfaceFn(
		TEXT("Get_PropertiesForTraversal"),
		&AGaspMoverEnemy::execGet_PropertiesForTraversal,
		bSandboxTraversalInterfaceWired);
}

DEFINE_FUNCTION(AGaspMoverEnemy::execGet_PropertiesForAnimation)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	UScriptStruct* AnimStruct = nullptr;
	if (UFunction* Function = Stack.CurrentNativeFunction)
	{
		if (FStructProperty* StructProp = CastField<FStructProperty>(Function->GetReturnProperty()))
		{
			AnimStruct = StructProp->Struct;
		}
	}
	if (!AnimStruct)
	{
		AnimStruct = LoadObject<UScriptStruct>(
			nullptr,
			TEXT("/Game/Blueprints/Data/S_CharacterPropertiesForAnimation.S_CharacterPropertiesForAnimation"));
	}
	if (AnimStruct && RESULT_PARAM)
	{
		AnimStruct->InitializeStruct(RESULT_PARAM);
		P_THIS->FillGaspAnimPropertiesInternal(AnimStruct, RESULT_PARAM);
	}
	P_NATIVE_END;
}

DEFINE_FUNCTION(AGaspMoverEnemy::execGet_PropertiesForTraversal)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	UScriptStruct* TravStruct = nullptr;
	if (UFunction* Function = Stack.CurrentNativeFunction)
	{
		if (FStructProperty* StructProp = CastField<FStructProperty>(Function->GetReturnProperty()))
		{
			TravStruct = StructProp->Struct;
		}
	}
	if (!TravStruct)
	{
		TravStruct = LoadObject<UScriptStruct>(
			nullptr,
			TEXT("/Game/Blueprints/Data/S_CharacterPropertiesForTraversal.S_CharacterPropertiesForTraversal"));
	}
	if (TravStruct && RESULT_PARAM)
	{
		TravStruct->InitializeStruct(RESULT_PARAM);
		P_THIS->FillGaspTraversalPropertiesInternal(TravStruct, RESULT_PARAM);
	}
	P_NATIVE_END;
}
