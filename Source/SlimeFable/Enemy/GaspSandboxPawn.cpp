// Copyright Epic Games, Inc. All Rights Reserved.

#include "GaspSandboxPawn.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimationAsset.h"
#include "Backends/MoverBackendLiaison.h"
#include "Backends/MoverStandaloneLiaison.h"
#include "BrainComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/SlimeHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Components/StateTreeComponent.h"
#include "Components/WidgetComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "DefaultMovementSet/NavMoverComponent.h"
#include "EnemyAttributeSet.h"
#include "EnemyCombatComponent.h"
#include "EnemyCombatTypes.h"
#include "SlimeCombatTypes.h"
#include "Engine/CollisionProfile.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GaspEnemyAIController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "MotionWarpingComponent.h"
#include "MovementMode.h"
#include "MoverDataModelTypes.h"
#include "MoverTypes.h"
#include "SlimeFable.h"
#include "StateTree.h"
#include "SlimeFoliageInteractComponent.h"
#include "SlimeLockOnComponent.h"
#include "Slime/SlimeMorphComponent.h"
#include "SlimeStatusComponent.h"
#include "SlimeWorldHealthBar.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "UserDefinedStructSupport.h"

static TAutoConsoleVariable<int32> CVarSlimeGaspMorphTrace(
	TEXT("slime.GaspMorphTrace"),
	0,
	TEXT("When 1, log per-frame Capsule/Mesh/SyncState/Camera deltas for player-controlled GASP pawns."),
	ECVF_Default);

namespace GaspSandboxPrivate
{
	static int64 MatchGaspSandboxEnumByDisplayName(UEnum* Enum, const FString& Needle)
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

	static void WriteCustomInputGait(FMoverInputCmdContext& InputCmdResult, const FString& GaitName)
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

		for (TFieldIterator<FProperty> It(CustomStruct); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop || !Prop->GetName().StartsWith(TEXT("Gait")))
			{
				continue;
			}
			if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
			{
				const int64 Value = MatchGaspSandboxEnumByDisplayName(EnumProp->GetEnum(), GaitName);
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
					const int64 Value = MatchGaspSandboxEnumByDisplayName(Enum, GaitName);
					if (Value != INDEX_NONE)
					{
						ByteProp->SetPropertyValue_InContainer(StructPtr, static_cast<uint8>(Value));
					}
				}
				return;
			}
		}
	}
}

void UGaspMoverInputBridge::Init(AGaspSandboxPawn* InOwner)
{
	OwnerPawn = InOwner;
}

void UGaspMoverInputBridge::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	AGaspSandboxPawn* Owner = OwnerPawn.Get();
	if (!Owner)
	{
		return;
	}

	// Let Blueprint SandboxCharacter_Mover ProduceInput run first (IMC / NavMover).
	if (Owner->GetClass()->ImplementsInterface(UMoverInputProducerInterface::StaticClass()))
	{
		IMoverInputProducerInterface::Execute_ProduceInput(Owner, SimTimeMs, InputCmdResult);
	}

	FCharacterDefaultInputs& Inputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

	if (Owner->ApplyPendingRagdollInput(Inputs))
	{
		return;
	}

	if (Owner->IsMoverFrozen() || Owner->IsInDeathSequence() || Owner->IsCombatKnockdown()
		|| !Owner->GetController())
	{
		// SimpleWalking/FlyingMode reject EMoveInputType::None — use zero DirectionalIntent to stop.
		Inputs.SetMoveInput(EMoveInputType::DirectionalIntent, FVector::ZeroVector);
		Inputs.bIsJumpPressed = false;
		Inputs.bIsJumpJustPressed = false;
		return;
	}

	// Vault/Mantle 成功后 BP Jump 仍会把 Jump 写进 InputCmd —— 清掉以免打断 Traversing。
	if (Owner->bSuppressJumpForTraversal)
	{
		Inputs.bIsJumpPressed = false;
		Inputs.bIsJumpJustPressed = false;
		Owner->bSuppressJumpForTraversal = false;
	}
	else if (UCharacterMoverComponent* Mover = Owner->GetMoverComponent())
	{
		if (Mover->GetMovementModeName() == FName(TEXT("Traversing")))
		{
			Inputs.bIsJumpPressed = false;
			Inputs.bIsJumpJustPressed = false;
		}
	}

	const bool bPlayerControlled = Cast<APlayerController>(Owner->GetController()) != nullptr;
	const bool bCombatLocked = Owner->GetEnemyCombat()
		&& (Owner->GetEnemyCombat()->IsMovementLocked() || Owner->GetEnemyCombat()->IsAttacking());

	if (!bPlayerControlled)
	{
		// Mirror AGaspMoverEnemy::ProduceInput — FeatureLab often has no Recast NavMesh,
		// so BP ProduceInput leaves MoveInput at zero unless we feed AiMoveIntent.
		FVector WorldIntent = FVector::ZeroVector;
		if (!bCombatLocked)
		{
			if (UNavMoverComponent* Nav = Owner->GetNavMoverComponent())
			{
				FVector NavIntent = FVector::ZeroVector;
				FVector NavVelocity = FVector::ZeroVector;
				if (Nav->ConsumeNavMovementData(NavIntent, NavVelocity))
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
			}
			if (WorldIntent.IsNearlyZero() && !Owner->GetAiMoveIntent().IsNearlyZero())
			{
				WorldIntent = Owner->GetAiMoveIntent().GetSafeNormal2D();
			}
		}
		Inputs.SetMoveInput(EMoveInputType::DirectionalIntent, WorldIntent);

		const FVector Face = !Owner->GetAiFaceIntent().IsNearlyZero()
			? Owner->GetAiFaceIntent().GetSafeNormal2D()
			: (!WorldIntent.IsNearlyZero() ? WorldIntent : FVector::ZeroVector);
		if (!Face.IsNearlyZero())
		{
			Inputs.OrientationIntent = Face;
			Inputs.ControlRotation = Face.Rotation();
		}
		if (Owner->WantsChaseGait() && !bCombatLocked)
		{
			GaspSandboxPrivate::WriteCustomInputGait(InputCmdResult, TEXT("Run"));
		}

		// AI-only: stuck Falling with near-zero velocity → force Walking.
		// Do not apply while player-controlled — jump apex would fight BP mode logic.
		if (UCharacterMoverComponent* Mover = Owner->GetMoverComponent())
		{
			const FName ModeName = Mover->GetMovementModeName();
			const bool bStuckFalling = ModeName == FName(TEXT("Falling"))
				&& Mover->GetVelocity().SizeSquared() < 4.f;
			if (bStuckFalling && Mover->FindMovementModeByName(FName(TEXT("Walking"))))
			{
				Inputs.SuggestedMovementMode = FName(TEXT("Walking"));
			}
		}
	}
	else if (bCombatLocked)
	{
		Inputs.SetMoveInput(EMoveInputType::DirectionalIntent, FVector::ZeroVector);
		Inputs.bIsJumpPressed = false;
		Inputs.bIsJumpJustPressed = false;
	}

	if (bPlayerControlled)
	{
		if (UCharacterMoverComponent* Mover = Owner->GetMoverComponent())
		{
			if (Mover->GetMovementModeName() == FName(TEXT("Flying"))
				&& Mover->FindMovementModeByName(FName(TEXT("Walking"))))
			{
				Inputs.SuggestedMovementMode = FName(TEXT("Walking"));
			}
		}
	}

	if (Owner->bForceWalkingAfterRagdollRestore)
	{
		if (UCharacterMoverComponent* Mover = Owner->GetMoverComponent())
		{
			if (Mover->FindMovementModeByName(FName(TEXT("Walking"))))
			{
				Inputs.SuggestedMovementMode = FName(TEXT("Walking"));
			}
		}
		Owner->bForceWalkingAfterRagdollRestore = false;
	}
}

AGaspSandboxPawn::AGaspSandboxPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AGaspEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	SetReplicatingMovement(false);

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

	// HealthBar is created in PostInitializeComponents once BP Capsule exists as Root.
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
		FSoftObjectPath(TEXT("/Game/Input/IMC_Sandbox.IMC_Sandbox")));
	DeathDissolveMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/_Slime/FX/M_EnemyDeathDissolve.M_EnemyDeathDissolve")));
}

void AGaspSandboxPawn::ResolveBlueprintComponents()
{
	CachedCapsule = FindComponentByClass<UCapsuleComponent>();
	CachedMover = FindComponentByClass<UCharacterMoverComponent>();
	CachedNavMover = FindComponentByClass<UNavMoverComponent>();
	CachedMotionWarping = FindComponentByClass<UMotionWarpingComponent>();

	TArray<USkeletalMeshComponent*> Meshes;
	GetComponents<USkeletalMeshComponent>(Meshes);
	CachedSkeletalMesh = nullptr;
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!Mesh)
		{
			continue;
		}
		const FString Name = Mesh->GetName();
		if (Name.Contains(TEXT("SkeletalMesh"), ESearchCase::IgnoreCase)
			|| Name.Equals(TEXT("Mesh"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("UEFN"), ESearchCase::IgnoreCase))
		{
			CachedSkeletalMesh = Mesh;
			break;
		}
	}
	if (!CachedSkeletalMesh && Meshes.Num() > 0)
	{
		CachedSkeletalMesh = Meshes[0];
	}

	TArray<UChildActorComponent*> ChildActors;
	GetComponents<UChildActorComponent>(ChildActors);
	CachedVisualOverride = nullptr;
	for (UChildActorComponent* Child : ChildActors)
	{
		if (!Child)
		{
			continue;
		}
		const FString Name = Child->GetName();
		if (Name.Contains(TEXT("VisualOverride"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("Visual"), ESearchCase::IgnoreCase))
		{
			CachedVisualOverride = Child;
			break;
		}
	}
	if (!CachedVisualOverride && ChildActors.Num() > 0)
	{
		CachedVisualOverride = ChildActors[0];
	}

	UE_LOG(LogSlimeFable, Log,
		TEXT("GaspSandboxPawn %s: resolved Capsule=%s SkeletalMesh=%s Mover=%s NavMover=%s VisualOverride=%s MotionWarping=%s"),
		*GetName(),
		*GetNameSafe(CachedCapsule),
		*GetNameSafe(CachedSkeletalMesh),
		*GetNameSafe(CachedMover),
		*GetNameSafe(CachedNavMover),
		*GetNameSafe(CachedVisualOverride),
		*GetNameSafe(CachedMotionWarping));
}

void AGaspSandboxPawn::EnsureInputBridge()
{
	if (!InputBridge)
	{
		InputBridge = NewObject<UGaspMoverInputBridge>(this, TEXT("GaspMoverInputBridge"));
		InputBridge->Init(this);
	}
	if (CachedMover && InputBridge)
	{
		UE_LOG(LogSlimeFable, Log, TEXT("GaspSandboxPawn %s: Mover InputProducer was %s; rebinding to bridge."),
			*GetName(), *GetNameSafe(CachedMover->InputProducer.Get()));
		CachedMover->InputProducer = InputBridge;
		// Avoid also gathering other producers that could double-feed or recurse.
		CachedMover->bGatherInputFromAllInputProducerComponents = false;
	}
}

void AGaspSandboxPawn::EnsureMoveKit()
{
	if (Moves.Num() == 0)
	{
		EnemyCombat::FillDefaultGaspMoves(Moves);
	}
}

void AGaspSandboxPawn::EnsureCapsuleIsRoot()
{
	if (!CachedCapsule)
	{
		CachedCapsule = FindComponentByClass<UCapsuleComponent>();
	}
	if (!CachedCapsule)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("GaspSandboxPawn %s: no Capsule — cannot fix Root"), *GetName());
		return;
	}

	// Bad leftover HealthBar (old CreateDefaultSubobject) often becomes Root; Capsule ends up as its child.
	// Detach Capsule BEFORE SetRoot or AttachTo(HealthBar→Capsule) will abort as a cycle.
	if (CachedCapsule->GetAttachParent() != nullptr)
	{
		UE_LOG(LogSlimeFable, Warning,
			TEXT("GaspSandboxPawn %s: Capsule was attached to %s — detaching before promoting Root"),
			*GetName(), *GetNameSafe(CachedCapsule->GetAttachParent()));
		CachedCapsule->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}

	if (GetRootComponent() != CachedCapsule)
	{
		if (USceneComponent* OldRoot = GetRootComponent())
		{
			UE_LOG(LogSlimeFable, Warning,
				TEXT("GaspSandboxPawn %s: Root was %s; forcing Capsule as Root"),
				*GetName(), *OldRoot->GetName());
		}
		SetRootComponent(CachedCapsule);
	}
	CachedCapsule->SetMobility(EComponentMobility::Movable);
}

void AGaspSandboxPawn::AttachHealthBarToCapsule()
{
	EnsureCapsuleIsRoot();
	if (!CachedCapsule)
	{
		return;
	}

	const bool bNeedNewBar = !HealthBar
		|| GetRootComponent() == HealthBar
		|| HealthBar->GetAttachParent() != CachedCapsule
		|| CachedCapsule->IsAttachedTo(HealthBar);
	if (bNeedNewBar)
	{
		if (HealthBar)
		{
			UE_LOG(LogSlimeFable, Warning,
				TEXT("GaspSandboxPawn %s: destroying bad HealthBar (parent=%s root=%s)"),
				*GetName(),
				*GetNameSafe(HealthBar->GetAttachParent()),
				*GetNameSafe(GetRootComponent()));
			HealthBar->DestroyComponent();
			HealthBar = nullptr;
		}

		TArray<UWidgetComponent*> Widgets;
		GetComponents<UWidgetComponent>(Widgets);
		for (UWidgetComponent* Widget : Widgets)
		{
			if (Widget && Widget->GetFName().ToString().StartsWith(TEXT("HealthBar")))
			{
				Widget->DestroyComponent();
			}
		}

		HealthBar = NewObject<UWidgetComponent>(this, TEXT("HealthBar"), RF_Transient | RF_TextExportTransient);
		HealthBar->SetWidgetSpace(EWidgetSpace::Screen);
		HealthBar->SetDrawAtDesiredSize(false);
		HealthBar->SetDrawSize(FVector2D(110.f, 14.f));
		HealthBar->SetPivot(FVector2D(0.5f, 1.f));
		HealthBar->SetWidgetClass(USlimeWorldHealthBar::StaticClass());
		HealthBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HealthBar->SetupAttachment(CachedCapsule);
		HealthBar->RegisterComponent();
	}

	RefreshHealthBarAnchor();
	RefreshWorldHealthBarVisibility();
}

void AGaspSandboxPawn::RefreshWorldHealthBarVisibility()
{
	if (!HealthBar)
	{
		return;
	}
	const bool bHide = bMorphTarget || IsPlayerControlled() || bDevourLocked || bDevouredDeath || bDeathSequence;
	HealthBar->SetHiddenInGame(bHide);
	HealthBar->SetVisibility(!bHide);
}

void AGaspSandboxPawn::BindWorldHealthBar()
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
	RefreshWorldHealthBarVisibility();
}

void AGaspSandboxPawn::PostInitializeComponents()
{
	ResolveBlueprintComponents();
	EnsureCapsuleIsRoot();
	EnsureMoveKit();
	Super::PostInitializeComponents();
	ResolveBlueprintComponents();
	EnsureCapsuleIsRoot();
	EnsureInputBridge();
	AttachHealthBarToCapsule();
	if (SpawnOrigin.IsNearlyZero())
	{
		SpawnOrigin = GetActorLocation();
	}
	if (Health)
	{
		Health->MaxHP = FMath::Max(MaxHP, 1.f);
	}
	if (CachedMover && CachedCapsule)
	{
		CachedMover->SetUpdatedComponent(CachedCapsule);
	}
	EnsureMoverModes();
}

void AGaspSandboxPawn::BeginPlay()
{
	ResolveBlueprintComponents();
	EnsureCapsuleIsRoot();
	EnsureMoveKit();
	EnsureInputBridge();
	AttachHealthBarToCapsule();
	ApplyActiveVisualOnly();
	Super::BeginPlay();
	EnsureMoverModes();
	SpawnOrigin = GetActorLocation();
	BindWorldHealthBar();
	EnsureMeshTickAfterMover();
	if (CachedMover)
	{
		CachedMover->OnPostFinalize.AddDynamic(this, &AGaspSandboxPawn::HandleMoverPostFinalize);
	}
	if (Health)
	{
		Health->OnDied.AddDynamic(this, &AGaspSandboxPawn::HandleDied);
		Health->MaxHP = FMath::Max(MaxHP, 1.f);
		if (DebugStartHealthPercent > KINDA_SMALL_NUMBER)
		{
			Health->CurrentHP = Health->MaxHP * FMath::Clamp(DebugStartHealthPercent, 0.01f, 1.f);
		}
	}
}

void AGaspSandboxPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetMorphLocomotionTicksEnabled(false);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CombatGetUpTimer);
		World->GetTimerManager().ClearTimer(DeathRagdollTimer);
		World->GetTimerManager().ClearTimer(DeathDissolveTimer);
	}
	if (CachedMover)
	{
		CachedMover->OnPostFinalize.RemoveDynamic(this, &AGaspSandboxPawn::HandleMoverPostFinalize);
	}
	RemoveSandboxMappingFromController(GetController());
	if (AActor* Master = MorphMaster.Get())
	{
		if (APlayerController* MasterPC = Cast<APlayerController>(Master->GetInstigatorController()))
		{
			RemoveSandboxMappingFromController(MasterPC);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AGaspSandboxPawn::UnPossessed()
{
	UnbindMorphLocomotionActions();
	SetSmartObjectPlayerLogic(false);
	AController* Previous = GetController();
	Super::UnPossessed();
	RemoveSandboxMappingFromController(Previous);
	RefreshWorldHealthBarVisibility();
}

void AGaspSandboxPawn::PawnClientRestart()
{
	Super::PawnClientRestart();
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EIC->ClearDebugKeyBindings();
	}
}

void AGaspSandboxPawn::RemoveSandboxMappingFromController(AController* OldController)
{
	APlayerController* PC = Cast<APlayerController>(OldController);
	if (!PC)
	{
		return;
	}
	UInputMappingContext* IMC = SandboxMapping.LoadSynchronous();
	if (!IMC)
	{
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* Sub =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Sub->RemoveMappingContext(IMC);
		UE_LOG(LogSlimeFable, Log, TEXT("GaspSandboxPawn %s: removed %s from %s"),
			*GetName(), *IMC->GetName(), *PC->GetName());
	}
}

void AGaspSandboxPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (AGaspEnemyAIController* AIC = Cast<AGaspEnemyAIController>(GetController()))
	{
		AIC->DriveCombatAI(DeltaSeconds);
	}
	// Keep NavMover fed when AI sets a direct intent (official ProduceInput consumes it).
	if (CachedNavMover && !AiMoveIntent.IsNearlyZero() && Cast<APlayerController>(GetController()) == nullptr)
	{
		CachedNavMover->RequestPathMove(AiMoveIntent.GetSafeNormal2D());
	}
	RestoreUnexpectedRagdoll();
	TickCombatKnockdown();
	ConfirmDeathRagdollThenStopAI();
	KeepDeathRagdollPhysics();
	TickMorphTrace(DeltaSeconds);
}

void AGaspSandboxPawn::RestoreUnexpectedRagdoll()
{
	if (bPendingRagdoll || bDeathSequence || bDevouredDeath || bMoverFrozen || bCombatKnockdown)
	{
		return;
	}

	const bool bInRagdoll = CachedMover
		&& CachedMover->GetMovementModeName() == FName(TEXT("Ragdoll"));
	if (!bInRagdoll)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = CachedSkeletalMesh;
	const bool bMeshSim = Mesh && (Mesh->IsSimulatingPhysics() || Mesh->IsAnySimulatingPhysics());
	const bool bCapsuleOff = CachedCapsule
		&& CachedCapsule->GetCollisionEnabled() == ECollisionEnabled::NoCollision;
	if (!bMeshSim && !bCapsuleOff)
	{
		return;
	}

	if (Mesh && bMeshSim)
	{
		Mesh->SetAllBodiesSimulatePhysics(false);
		Mesh->SetSimulatePhysics(false);
		Mesh->PutAllRigidBodiesToSleep();
	}
	if (CachedCapsule && bCapsuleOff)
	{
		CachedCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		if (CachedCapsule->GetCollisionProfileName() == NAME_None
			|| CachedCapsule->GetCollisionProfileName() == UCollisionProfile::NoCollision_ProfileName)
		{
			CachedCapsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
		}
	}
	bForceWalkingAfterRagdollRestore = true;
	UE_LOG(LogSlimeFable, Warning, TEXT("GaspSandboxPawn %s: restored stray ragdoll (meshSim=%d capsuleOff=%d)"),
		*GetName(), bMeshSim ? 1 : 0, bCapsuleOff ? 1 : 0);
}

void AGaspSandboxPawn::SetSmartObjectPlayerLogic(bool bEnable)
{
	static const TCHAR* TreePath = TEXT("/Game/Blueprints/AI/StateTree/ST_Player_SandboxCharacter_SmartObject.ST_Player_SandboxCharacter_SmartObject");

	bool bStartedTree = false;
	TArray<UStateTreeComponent*> Trees;
	GetComponents<UStateTreeComponent>(Trees);
	for (UStateTreeComponent* Tree : Trees)
	{
		if (!Tree)
		{
			continue;
		}
		if (bEnable)
		{
			if (!Tree->IsRunning())
			{
				if (UStateTree* Asset = LoadObject<UStateTree>(nullptr, TreePath))
				{
					Tree->SetStateTree(Asset);
				}
				Tree->StartLogic();
			}
			bStartedTree = Tree->IsRunning();
		}
		else if (Tree->IsRunning())
		{
			Tree->StopLogic(TEXT("UnPossess"));
		}
	}

	if (!bEnable || bStartedTree)
	{
		return;
	}

	TArray<UActorComponent*> Comps;
	GetComponents(Comps);
	for (UActorComponent* Comp : Comps)
	{
		if (!Comp)
		{
			continue;
		}
		if (!Comp->GetClass()->GetName().Contains(TEXT("AC_SmartObjectAnimation")))
		{
			continue;
		}
		if (UFunction* Fn = Comp->FindFunction(TEXT("ReceiveBeginPlay")))
		{
			Comp->ProcessEvent(Fn, nullptr);
			UE_LOG(LogSlimeFable, Log, TEXT("GaspSandboxPawn %s: re-ran SmartObject BeginPlay on %s"),
				*GetName(), *Comp->GetName());
		}
	}
}

void AGaspSandboxPawn::SetAiMoveIntent(const FVector& WorldIntent)
{
	AiMoveIntent = WorldIntent;
	if (CachedNavMover && !WorldIntent.IsNearlyZero())
	{
		CachedNavMover->RequestPathMove(WorldIntent.GetSafeNormal2D());
	}
}

void AGaspSandboxPawn::ClearAiMoveIntent()
{
	AiMoveIntent = FVector::ZeroVector;
}

bool AGaspSandboxPawn::IsCameraAttachedMesh(const UMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return false;
	}
	for (const USceneComponent* Walk = Mesh; Walk; Walk = Walk->GetAttachParent())
	{
		if (Walk->IsA<UCameraComponent>())
		{
			return true;
		}
	}
	return false;
}

USkeletalMeshComponent* AGaspSandboxPawn::FindChildActorVisualMesh() const
{
	if (!CachedVisualOverride)
	{
		return nullptr;
	}
	AActor* Child = CachedVisualOverride->GetChildActor();
	if (!Child)
	{
		return nullptr;
	}
	TArray<USkeletalMeshComponent*> Meshes;
	Child->GetComponents<USkeletalMeshComponent>(Meshes);
	USkeletalMeshComponent* Fallback = nullptr;
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!Mesh || !Mesh->GetSkeletalMeshAsset() || Mesh->IsVisualizationComponent() || IsCameraAttachedMesh(Mesh))
		{
			continue;
		}
		if (Mesh->IsVisible() && !Mesh->bHiddenInGame)
		{
			return Mesh;
		}
		if (!Fallback)
		{
			Fallback = Mesh;
		}
	}
	return Fallback;
}

USkeletalMeshComponent* AGaspSandboxPawn::GetDevourPreviewMesh() const
{
	if (USkeletalMeshComponent* Vis = FindChildActorVisualMesh())
	{
		return Vis;
	}
	return CachedSkeletalMesh;
}

void AGaspSandboxPawn::ForEachVisualMesh(TFunctionRef<void(UMeshComponent*)> Fn) const
{
	// Visible Echo (and extra hair/cloth parts). Hidden UEFN source is skipped.
	bool bAny = false;
	if (AActor* Child = CachedVisualOverride ? CachedVisualOverride->GetChildActor() : nullptr)
	{
		TArray<USkeletalMeshComponent*> Meshes;
		Child->GetComponents<USkeletalMeshComponent>(Meshes);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (!Mesh || !Mesh->GetSkeletalMeshAsset() || Mesh->IsVisualizationComponent()
				|| IsCameraAttachedMesh(Mesh) || Mesh->bHiddenInGame)
			{
				continue;
			}
			Fn(Mesh);
			bAny = true;
		}
	}
	if (!bAny)
	{
		USkeletalMeshComponent* Active = GetDevourPreviewMesh();
		if (Active && !Active->IsA<UWidgetComponent>() && !Active->IsVisualizationComponent()
			&& !IsCameraAttachedMesh(Active))
		{
			Fn(Active);
		}
	}
}

void AGaspSandboxPawn::ApplyActiveVisualOnly()
{
	USkeletalMeshComponent* OverrideMesh = FindChildActorVisualMesh();
	if (OverrideMesh && CachedSkeletalMesh && CachedSkeletalMesh != OverrideMesh)
	{
		CachedSkeletalMesh->SetHiddenInGame(true, false);
		OverrideMesh->SetHiddenInGame(false, false);
	}
}

FText AGaspSandboxPawn::GetResolvedDisplayName() const
{
	return DisplayName.IsEmpty() ? FText::FromString(TEXT("动作试样")) : DisplayName;
}

FLinearColor AGaspSandboxPawn::ResolveDevourWheelTint() const
{
	return FLinearColor(0.35f, 0.55f, 0.75f);
}

bool AGaspSandboxPawn::IsDevourableNow() const
{
	return bDevourable && !bMorphTarget && !bPhantomInstance && !bDeathSequence;
}

float AGaspSandboxPawn::GetHealthPercent() const
{
	return Health ? Health->GetHealthPercent() : 0.f;
}

bool AGaspSandboxPawn::CanBeLockedOn() const
{
	return Health && Health->IsAlive() && !bDeathSequence;
}

FVector AGaspSandboxPawn::GetLockOnLocation() const
{
	return GetHudAnchorLocation();
}

bool AGaspSandboxPawn::GetFoliageInteractVolume(FVector& OutLocation, float& OutRadius) const
{
	OutLocation = GetActorLocation();
	OutRadius = CachedCapsule ? CachedCapsule->GetScaledCapsuleRadius() * 1.4f : 50.f;
	return true;
}

bool AGaspSandboxPawn::ShouldSuppressFoliageInteract() const
{
	return bDeathSequence || bDevouredDeath || (Health && !Health->IsAlive());
}

FVector AGaspSandboxPawn::GetVisualBoundsCenter() const
{
	if (USkeletalMeshComponent* Mesh = GetDevourPreviewMesh())
	{
		return Mesh->Bounds.Origin;
	}
	return GetActorLocation();
}

FVector AGaspSandboxPawn::GetHudAnchorLocation() const
{
	const float Half = CachedCapsule ? CachedCapsule->GetScaledCapsuleHalfHeight() : 96.f;
	return GetActorLocation() + FVector(0.f, 0.f, Half + HealthBarZOffset);
}

void AGaspSandboxPawn::RefreshHealthBarAnchor()
{
	if (HealthBar)
	{
		const float Half = CachedCapsule ? CachedCapsule->GetScaledCapsuleHalfHeight() : 96.f;
		HealthBar->SetRelativeLocation(FVector(0.f, 0.f, Half + HealthBarZOffset));
	}
}

bool AGaspSandboxPawn::GetStableMeshBounds(FBox& OutBox) const
{
	if (USkeletalMeshComponent* Mesh = GetDevourPreviewMesh())
	{
		OutBox = Mesh->Bounds.GetBox();
		return true;
	}
	return false;
}

void AGaspSandboxPawn::InitAsMorphTarget(AActor* Master)
{
	bMorphTarget = true;
	bDevourable = false;
	MorphMaster = Master;
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;
	ClearAiMoveIntent();
	ClearAiFaceIntent();
	SetWantChaseGait(false);
	if (Health)
	{
		Health->Team = ESlimeTeam::Player;
		Health->bDestroyOnDeath = false;
	}
	ResolveBlueprintComponents();
	ApplyActiveVisualOnly();
	if (!MorphLockOn)
	{
		MorphLockOn = NewObject<USlimeLockOnComponent>(this, TEXT("MorphLockOn"));
		MorphLockOn->bPollLockOnKey = true;
		MorphLockOn->RegisterComponent();
	}
	RefreshWorldHealthBarVisibility();
	// Do NOT add IMC or create cameras — BP ReceivePossessed adds IMC_Sandbox + GameplayCamera.
}

void AGaspSandboxPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	RefreshWorldHealthBarVisibility();
	if (APlayerController* PC = Cast<APlayerController>(NewController))
	{
		BindMorphLocomotionActions(PC);
		SetSmartObjectPlayerLogic(true);
	}
	else
	{
		SetSmartObjectPlayerLogic(false);
	}
}

void AGaspSandboxPawn::InitAsPhantom(float LifeSeconds, AActor* Master)
{
	bPhantomInstance = true;
	bDevourable = false;
	MorphMaster = Master;
	(void)LifeSeconds;
}

void AGaspSandboxPawn::BeginDevouredDeath(AActor* Devourer)
{
	(void)Devourer;
	bDevouredDeath = true;
	bDeathSequence = true;
	bDevourable = false;
}

void AGaspSandboxPawn::FreezeForDevour()
{
	AiMoveIntent = FVector::ZeroVector;
	AiFaceIntent = FVector::ZeroVector;
	bWantChaseGait = false;
	if (CachedNavMover)
	{
		CachedNavMover->SetComponentTickEnabled(false);
	}
	SuspendMoverSim();
}

void AGaspSandboxPawn::RestoreFromDevour()
{
	if (CachedMover && CachedCapsule)
	{
		CachedMover->SetUpdatedComponent(CachedCapsule);
	}
	ResumeMoverSim();
	if (CachedNavMover)
	{
		CachedNavMover->SetComponentTickEnabled(true);
	}
}

void AGaspSandboxPawn::SetMorphGameplayEnabled(bool bEnabled)
{
	if (bEnabled)
	{
		ResumeMoverSim();
	}
	else
	{
		AiMoveIntent = FVector::ZeroVector;
		AiFaceIntent = FVector::ZeroVector;
		SuspendMoverSim();
	}
}

void AGaspSandboxPawn::SuspendMoverSim()
{
	bMoverFrozen = true;
	if (bMoverSimSuspended)
	{
		return;
	}
	bMoverSimSuspended = true;

	if (CachedMover)
	{
		CachedMover->SetGravityOverride(true, FVector::ZeroVector);
		CachedMover->SetComponentTickEnabled(false);
	}

	// Standalone Mover simulation ticks live on BackendLiaison, not MoverComponent.
	if (UActorComponent* Liaison = FindComponentByInterface(UMoverBackendLiaisonInterface::StaticClass()))
	{
		Liaison->RegisterAllComponentTickFunctions(false);
	}
	SetMorphLocomotionTicksEnabled(false);
}

void AGaspSandboxPawn::ResumeMoverSim()
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

	if (CachedMover)
	{
		CachedMover->SetComponentTickEnabled(true);
		CachedMover->SetGravityOverride(false);
		// Accept the actor location pinned during devour / morph freeze.
		TSharedPtr<FTeleportEffect> Teleport = MakeShared<FTeleportEffect>();
		Teleport->TargetLocation = GetActorLocation();
		Teleport->bUseActorRotation = true;
		CachedMover->QueueInstantMovementEffect(Teleport);
	}
	SetMorphLocomotionTicksEnabled(true);
}

void AGaspSandboxPawn::EnsureMeshTickAfterMover()
{
	if (bMeshTickAfterMoverWired)
	{
		return;
	}
	UMoverStandaloneLiaisonComponent* Standalone =
		Cast<UMoverStandaloneLiaisonComponent>(
			FindComponentByInterface(UMoverBackendLiaisonInterface::StaticClass()));
	if (!Standalone)
	{
		return;
	}

	auto WireMesh = [Standalone](UActorComponent* Comp)
	{
		if (Comp)
		{
			Standalone->AddTickDependency(Comp, EMoverTickDependencyOrder::After, EMoverTickPhase::ApplyState);
		}
	};

	WireMesh(CachedSkeletalMesh);
	if (CachedVisualOverride)
	{
		WireMesh(CachedVisualOverride);
		if (AActor* Child = CachedVisualOverride->GetChildActor())
		{
			TArray<USkeletalMeshComponent*> ChildMeshes;
			Child->GetComponents<USkeletalMeshComponent>(ChildMeshes);
			for (USkeletalMeshComponent* Mesh : ChildMeshes)
			{
				WireMesh(Mesh);
			}
		}
	}
	bMeshTickAfterMoverWired = true;
}

void AGaspSandboxPawn::HandleMoverPostFinalize(const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	(void)SyncState;
	(void)AuxState;
	++MorphTraceFinalizeCount;
}

void AGaspSandboxPawn::TickMorphTrace(float DeltaSeconds)
{
	(void)DeltaSeconds;
	if (CVarSlimeGaspMorphTrace.GetValueOnGameThread() == 0)
	{
		MorphTraceFinalizeCount = 0;
		return;
	}
	if (Cast<APlayerController>(GetController()) == nullptr)
	{
		MorphTraceFinalizeCount = 0;
		return;
	}

	const FVector CapsuleLoc = CachedCapsule ? CachedCapsule->GetComponentLocation() : GetActorLocation();
	USkeletalMeshComponent* Mesh = GetDevourPreviewMesh();
	if (!Mesh)
	{
		Mesh = CachedSkeletalMesh;
	}
	const FVector MeshLoc = Mesh ? Mesh->GetComponentLocation() : CapsuleLoc;

	FVector CameraLoc = FVector::ZeroVector;
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			CameraLoc = PC->PlayerCameraManager->GetCameraLocation();
		}
	}
	if (CameraLoc.IsNearlyZero())
	{
		if (const UCameraComponent* Cam = FindComponentByClass<UCameraComponent>())
		{
			CameraLoc = Cam->GetComponentLocation();
		}
	}

	FVector SyncLoc = CapsuleLoc;
	FName ModeName = NAME_None;
	if (CachedMover)
	{
		ModeName = CachedMover->GetMovementModeName();
		const FMoverSyncState& Sync = CachedMover->GetSyncState();
		if (const FMoverDefaultSyncState* DefaultSync =
			Sync.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
		{
			SyncLoc = DefaultSync->GetLocation_WorldSpace();
		}
	}

	const int32 FinalizeCount = MorphTraceFinalizeCount;
	MorphTraceFinalizeCount = 0;

	if (!bMorphTraceHasPrev)
	{
		MorphTracePrevCapsule = CapsuleLoc;
		MorphTracePrevMesh = MeshLoc;
		MorphTracePrevCamera = CameraLoc;
		bMorphTraceHasPrev = true;
		return;
	}

	const FVector CapDelta = CapsuleLoc - MorphTracePrevCapsule;
	const FVector MeshDelta = MeshLoc - MorphTracePrevMesh;
	const FVector CamDelta = CameraLoc - MorphTracePrevCamera;
	const FVector SyncGap = SyncLoc - CapsuleLoc;
	const FVector MeshVsCap = MeshLoc - CapsuleLoc;

	UE_LOG(LogSlimeFable, Log,
		TEXT("GaspMorphTrace %s frame=%d mode=%s finalize=%d capΔ=(%.2f,%.2f,%.2f) meshΔ=(%.2f,%.2f,%.2f) camΔ=(%.2f,%.2f,%.2f) syncGap=(%.2f,%.2f,%.2f) meshVsCap=(%.2f,%.2f,%.2f)"),
		*GetName(),
		GFrameNumber,
		*ModeName.ToString(),
		FinalizeCount,
		CapDelta.X, CapDelta.Y, CapDelta.Z,
		MeshDelta.X, MeshDelta.Y, MeshDelta.Z,
		CamDelta.X, CamDelta.Y, CamDelta.Z,
		SyncGap.X, SyncGap.Y, SyncGap.Z,
		MeshVsCap.X, MeshVsCap.Y, MeshVsCap.Z);

	MorphTracePrevCapsule = CapsuleLoc;
	MorphTracePrevMesh = MeshLoc;
	MorphTracePrevCamera = CameraLoc;
}

void AGaspSandboxPawn::StopMeshAnimation()
{
	auto StopOn = [](USkeletalMeshComponent* Mesh)
	{
		if (UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr)
		{
			Anim->StopAllMontages(0.1f);
		}
	};
	StopOn(GetDevourPreviewMesh());
	if (CachedSkeletalMesh && CachedSkeletalMesh != GetDevourPreviewMesh())
	{
		StopOn(CachedSkeletalMesh);
	}
}

namespace GaspSandboxPrivate
{
	bool ProcessBoolFunction(UObject* Target, UFunction* Fn)
	{
		if (!Target || !Fn)
		{
			return false;
		}

		// Allocate full parm block — TryTraversalAction may have extra bool out-pins.
		uint8* Buffer = Fn->ParmsSize > 0
			? static_cast<uint8*>(FMemory_Alloca(Fn->ParmsSize))
			: nullptr;
		if (Buffer)
		{
			FMemory::Memzero(Buffer, Fn->ParmsSize);
		}
		Target->ProcessEvent(Fn, Buffer);

		if (FProperty* RetProp = Fn->GetReturnProperty())
		{
			if (FBoolProperty* BoolRet = CastField<FBoolProperty>(RetProp))
			{
				return Buffer && BoolRet->GetPropertyValue_InContainer(Buffer);
			}
		}
		return false;
	}
}

bool AGaspSandboxPawn::CallBoolFunctionByName(FName FunctionName)
{
	// Components first — AC_TraversalLogic owns the real vault check.
	if (DispatchBoolEventOnComponents(FunctionName))
	{
		return true;
	}
	if (UFunction* Fn = FindFunction(FunctionName))
	{
		return GaspSandboxPrivate::ProcessBoolFunction(this, Fn);
	}
	return false;
}

void AGaspSandboxPawn::BindMorphLocomotionActions(APlayerController* PC)
{
	UnbindMorphLocomotionActions();
	if (!PC)
	{
		return;
	}
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		EIC = Cast<UEnhancedInputComponent>(PC->InputComponent);
	}
	if (!EIC)
	{
		return;
	}

	if (!MorphJumpAction)
	{
		MorphJumpAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/IA_Jump.IA_Jump"));
	}
	if (!MorphJumpAction)
	{
		return;
	}

	FEnhancedInputActionEventBinding& Started = EIC->BindAction(
		MorphJumpAction, ETriggerEvent::Started, this, &AGaspSandboxPawn::MorphJumpStarted);
	FEnhancedInputActionEventBinding& Completed = EIC->BindAction(
		MorphJumpAction, ETriggerEvent::Completed, this, &AGaspSandboxPawn::MorphJumpReleased);
	MorphJumpBindingHandles.Add(Started.GetHandle());
	MorphJumpBindingHandles.Add(Completed.GetHandle());
}

void AGaspSandboxPawn::UnbindMorphLocomotionActions()
{
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			EIC = Cast<UEnhancedInputComponent>(PC->InputComponent);
		}
	}
	if (EIC)
	{
		for (const uint32 Handle : MorphJumpBindingHandles)
		{
			EIC->RemoveBindingByHandle(Handle);
		}
	}
	MorphJumpBindingHandles.Reset();
}

void AGaspSandboxPawn::MorphJumpStarted()
{
	// Vault is owned by BP Jump → AC_TraversalLogic::TryTraversalAction.
	// InputBridge clears Jump while movement mode is Traversing.
}

void AGaspSandboxPawn::MorphJumpReleased()
{
}

bool AGaspSandboxPawn::DispatchBoolEventOnComponents(FName FunctionName)
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
		if (GaspSandboxPrivate::ProcessBoolFunction(Comp, Fn))
		{
			return true;
		}
	}
	return false;
}

void AGaspSandboxPawn::EnsureMoverModes()
{
	if (!CachedMover)
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
		if (CachedMover->FindMovementModeByName(Spec.Name))
		{
			continue;
		}
		UClass* ModeClass = LoadClass<UBaseMovementMode>(nullptr, Spec.Path);
		if (!ModeClass)
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("GaspSandboxPawn %s: failed to load movement mode %s from %s"),
				*GetName(), *Spec.Name.ToString(), Spec.Path);
			continue;
		}
		UBaseMovementMode* NewMode = NewObject<UBaseMovementMode>(CachedMover, ModeClass, Spec.Name);
		if (!CachedMover->AddMovementModeFromObject(Spec.Name, NewMode))
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("GaspSandboxPawn %s: failed to register movement mode %s"),
				*GetName(), *Spec.Name.ToString());
			continue;
		}
		UE_LOG(LogSlimeFable, Log, TEXT("GaspSandboxPawn %s: registered movement mode %s"),
			*GetName(), *Spec.Name.ToString());
	}

	if (CachedMover->StartingMovementMode.IsNone() || !CachedMover->FindMovementModeByName(CachedMover->StartingMovementMode))
	{
		CachedMover->StartingMovementMode = CachedMover->FindMovementModeByName(FName(TEXT("Walking")))
			? FName(TEXT("Walking"))
			: FName(TEXT("Falling"));
	}

	// TMap may already contain Ragdoll from an earlier call that ran before
	// Simulation existed. Re-add so QueueNextMode can find it.
	if (UBaseMovementMode* RagdollMode = CachedMover->FindMovementModeByName(FName(TEXT("Ragdoll"))))
	{
		CachedMover->AddMovementModeFromObject(FName(TEXT("Ragdoll")), RagdollMode);
	}
}

void AGaspSandboxPawn::TriggerSandboxRagdoll()
{
	if (!bEnableRagdollKit || bDeathSequence)
	{
		return;
	}
	EnsureMoverModes();
	bPendingRagdoll = true;
	if (CachedMover && CachedMover->FindMovementModeByName(FName(TEXT("Ragdoll"))))
	{
		CachedMover->QueueNextMode(FName(TEXT("Ragdoll")), true);
	}
	DispatchBoolEventOnComponents(TEXT("TriggerRagdoll"));
	if (UFunction* Fn = FindFunction(TEXT("TriggerRagdoll")))
	{
		// The official event takes (bool, FMontageBlendSettings, injury enum): a null parm
		// block would make the VM read from address 0.
		GaspSandboxPrivate::ProcessBoolFunction(this, Fn);
	}
}

bool AGaspSandboxPawn::WantsHeldRagdollMode() const
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

void AGaspSandboxPawn::ForcePhysicalRagdollBodies(bool bDisableCapsule)
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
	if (CachedSkeletalMesh && CachedSkeletalMesh != GetDevourPreviewMesh())
	{
		Force(CachedSkeletalMesh);
	}
	if (bDisableCapsule && CachedCapsule)
	{
		CachedCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

bool AGaspSandboxPawn::ApplyPendingRagdollInput(FCharacterDefaultInputs& Inputs)
{
	if (!WantsHeldRagdollMode())
	{
		return false;
	}
	Inputs.SuggestedMovementMode = FName(TEXT("Ragdoll"));
	Inputs.SetMoveInput(EMoveInputType::DirectionalIntent, FVector::ZeroVector);
	Inputs.bIsJumpPressed = false;
	Inputs.bIsJumpJustPressed = false;
	bPendingRagdoll = false;
	return true;
}

void AGaspSandboxPawn::SetMorphLocomotionTicksEnabled(bool bEnabled)
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
	SetMesh(CachedSkeletalMesh);
	USkeletalMeshComponent* Preview = GetDevourPreviewMesh();
	if (Preview != CachedSkeletalMesh)
	{
		SetMesh(Preview);
	}
}

void AGaspSandboxPawn::KeepDeathRagdollPhysics()
{
	const bool bHoldDeath = bDeathSequence && !bDevouredDeath && bDeathRagdollArmed;
	const bool bHoldCombat = bCombatKnockdown && !bCombatGetUpRequested && !bDeathSequence;
	if (!bHoldDeath && !bHoldCombat)
	{
		return;
	}

	if (CachedMover && CachedMover->FindMovementModeByName(FName(TEXT("Ragdoll")))
		&& CachedMover->GetMovementModeName() != FName(TEXT("Ragdoll")))
	{
		CachedMover->QueueNextMode(FName(TEXT("Ragdoll")), true);
	}

	const bool bInRagdoll = CachedMover
		&& CachedMover->GetMovementModeName() == FName(TEXT("Ragdoll"));
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
		// Official Ragdoll mode never activated — stop Walking from snapping the mesh upright.
		SuspendMoverSim();
	}
}

void AGaspSandboxPawn::BeginKnockdownDeath()
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
	EnemyCombat::PrepareGaspDeathPhysics(CachedCapsule, CachedSkeletalMesh, GetDevourPreviewMesh());
	const float Played = PlayDeathKnockdownMontage();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DeathRagdollTimer,
			this,
			&AGaspSandboxPawn::StartDeathDissolve,
			FMath::Max(Played, 0.2f),
			false);
	}
	else
	{
		StartDeathDissolve();
	}
}

float AGaspSandboxPawn::PlayDeathKnockdownMontage()
{
	UAnimationAsset* Anim = EnemyCombat::LoadGaspDeathKnockdownAnim(
		EnemyCombat::ResolveGaspHitCardinal(this, LastDamageLocation));
	return EnemyCombat::PlayGaspDeathSingleNode(CachedSkeletalMesh, Anim);
}

void AGaspSandboxPawn::HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted)
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
	HoldPose(CachedSkeletalMesh);
	if (GetDevourPreviewMesh() != CachedSkeletalMesh)
	{
		HoldPose(GetDevourPreviewMesh());
	}
}

void AGaspSandboxPawn::QueueDeathRagdoll()
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

void AGaspSandboxPawn::AccrueCombatStun(float Damage)
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

void AGaspSandboxPawn::BeginCombatKnockdown()
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
			&AGaspSandboxPawn::PlayCombatGetUp,
			FMath::Max(GetUpDelaySeconds, 0.1f),
			false);
	}
	else
	{
		PlayCombatGetUp();
	}
}

void AGaspSandboxPawn::PlayCombatGetUp()
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
		UE_LOG(LogSlimeFable, Warning, TEXT("GaspSandboxPawn %s: Ragdoll_PlayRollingGetups missing — restore Walking"), *GetName());
		ResumeMoverSim();
		if (CachedMover && CachedMover->FindMovementModeByName(FName(TEXT("Walking"))))
		{
			CachedMover->QueueNextMode(FName(TEXT("Walking")), true);
		}
		if (CachedSkeletalMesh)
		{
			CachedSkeletalMesh->SetAllBodiesSimulatePhysics(false);
			CachedSkeletalMesh->SetSimulatePhysics(false);
		}
		if (CachedCapsule)
		{
			CachedCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		bForceWalkingAfterRagdollRestore = true;
		EndCombatKnockdown();
	}
}

void AGaspSandboxPawn::TickCombatKnockdown()
{
	if (!bCombatKnockdown || bDeathSequence)
	{
		return;
	}
	if (!bCombatGetUpRequested)
	{
		if (CachedMover && CachedMover->GetMovementModeName() != FName(TEXT("Ragdoll"))
			&& CachedMover->FindMovementModeByName(FName(TEXT("Ragdoll"))))
		{
			CachedMover->QueueNextMode(FName(TEXT("Ragdoll")), true);
		}
		return;
	}
	const bool bInRagdoll = CachedMover
		&& CachedMover->GetMovementModeName() == FName(TEXT("Ragdoll"));
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : CombatGetUpRequestedTime;
	if (!bInRagdoll || (Now - CombatGetUpRequestedTime) > 4.f)
	{
		EndCombatKnockdown();
	}
}

void AGaspSandboxPawn::EndCombatKnockdown()
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

void AGaspSandboxPawn::ConfirmDeathRagdollThenStopAI()
{
	if (!bDeathAwaitingRagdoll || bDevouredDeath)
	{
		return;
	}
	const bool bInRagdoll = CachedMover
		&& CachedMover->GetMovementModeName() == FName(TEXT("Ragdoll"));
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
			&AGaspSandboxPawn::StartDeathDissolve,
			FMath::Max(DeathRagdollLingerSeconds, 0.2f),
			false);
	}
	else
	{
		StartDeathDissolve();
	}
}

void AGaspSandboxPawn::StopDeathController()
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

void AGaspSandboxPawn::StartDeathDissolve()
{
	if (bDevouredDeath)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeathRagdollTimer);
	}

	if (CachedSkeletalMesh)
	{
		CachedSkeletalMesh->bPauseAnims = true;
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
			&AGaspSandboxPawn::TickDeathDissolve,
			0.05f,
			true);
	}
}

void AGaspSandboxPawn::TickDeathDissolve()
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

void AGaspSandboxPawn::FinishDeathSequence()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeathDissolveTimer);
		World->GetTimerManager().ClearTimer(DeathRagdollTimer);
		World->GetTimerManager().ClearTimer(CombatGetUpTimer);
	}
	Destroy();
}

void AGaspSandboxPawn::PlayHitFlash()
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
			HitFlashTimer, this, &AGaspSandboxPawn::TickHitFlash, 0.016f, true);
	}
}

void AGaspSandboxPawn::PlayElementAuraFlash(ESlimeElement Element, float Duration)
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
			HitFlashTimer, this, &AGaspSandboxPawn::TickHitFlash, 0.016f, true);
	}
}

void AGaspSandboxPawn::PlayElementAuraFlashByColor(FLinearColor Color, float Duration)
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
			HitFlashTimer, this, &AGaspSandboxPawn::TickHitFlash, 0.016f, true);
	}
}

void AGaspSandboxPawn::ClearElementAuraFlash()
{
	bAuraFlashActive = false;
	ClearHitFlash();
}

void AGaspSandboxPawn::TickHitFlash()
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

void AGaspSandboxPawn::ClearHitFlash()
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

UAnimMontage* AGaspSandboxPawn::ResolveHitReactMontage(const FVector& DamageLocation) const
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

void AGaspSandboxPawn::PlayHitReact(const FVector& DamageLocation)
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
	UAnimInstance* Anim = CachedSkeletalMesh ? CachedSkeletalMesh->GetAnimInstance() : nullptr;
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
			&AGaspSandboxPawn::StopHitReact,
			FMath::Max(HitReactSeconds, 0.05f),
			false);
	}
}

void AGaspSandboxPawn::StopHitReact()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitReactTimer);
	}
	if (UAnimMontage* Montage = ActiveHitReactMontage.Get())
	{
		if (UAnimInstance* Anim = CachedSkeletalMesh ? CachedSkeletalMesh->GetAnimInstance() : nullptr)
		{
			Anim->Montage_Stop(0.25f, Montage);
		}
	}
	ActiveHitReactMontage.Reset();
}

void AGaspSandboxPawn::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
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

void AGaspSandboxPawn::ApplyHealing(float Healing, AActor* Healer)
{
	(void)Healer;
	if (Health)
	{
		Health->ApplyHealing(Healing);
	}
}

void AGaspSandboxPawn::NotifyDanger(const FVector& DangerLocation, AActor* DangerSource)
{
	(void)DangerLocation;
	(void)DangerSource;
}

void AGaspSandboxPawn::HandleDeath()
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

void AGaspSandboxPawn::HandleDied()
{
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
