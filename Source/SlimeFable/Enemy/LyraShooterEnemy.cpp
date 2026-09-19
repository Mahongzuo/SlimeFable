// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraShooterEnemy.h"

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "GameFramework/InputSettings.h"
#include "Engine/LocalPlayer.h"
#include "GenericTeamAgentInterface.h"
#include "Player/LyraPlayerState.h"
#include "Settings/SlimeInputSettings.h"
#include "SlimeLyraQuickBarComponent.h"
#include "UIExtensionSystem.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Camera/LyraCameraComponent.h"
#include "Camera/LyraCameraMode.h"
#include "Character/LyraHealthComponent.h"
#include "Character/LyraPawnData.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Combat/SlimeDodgeComponent.h"
#include "Combat/SlimeHealthComponent.h"
#include "Combat/SlimeLockOnComponent.h"
#include "Combat/SlimeStatusComponent.h"
#include "Combat/SlimeWorldHealthBar.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Enemy/EnemyCombatComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Equipment/LyraEquipmentDefinition.h"
#include "Equipment/LyraEquipmentInstance.h"
#include "Equipment/LyraEquipmentManagerComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Input/LyraInputConfig.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Inventory/InventoryFragment_EquippableItem.h"
#include "Inventory/LyraInventoryItemDefinition.h"
#include "Inventory/LyraInventoryItemInstance.h"
#include "Inventory/LyraInventoryManagerComponent.h"
#include "Kismet/GameplayStatics.h"
#include "LyraGameplayTags.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "SlimeFable.h"
#include "Slime/SlimeMorphComponent.h"
#include "TimerManager.h"
#include "Weapons/LyraRangedWeaponInstance.h"
#include "Weapons/LyraWeaponInstance.h"

namespace LyraShooterTags
{
	static FGameplayTag Get(const TCHAR* Name)
	{
		return FGameplayTag::RequestGameplayTag(FName(Name), false);
	}
	static FGameplayTag FireAuto() { return Get(TEXT("InputTag.Weapon.FireAuto")); }
	static FGameplayTag Fire() { return Get(TEXT("InputTag.Weapon.Fire")); }
	static FGameplayTag ADS() { return Get(TEXT("InputTag.Weapon.ADS")); }
	static FGameplayTag HudReticle() { return Get(TEXT("HUD.Slot.Reticle")); }
	static FGameplayTag HudEquipment() { return Get(TEXT("HUD.Slot.Equipment")); }
	static FGameplayTag MagazineSize() { return Get(TEXT("Lyra.ShooterGame.Weapon.MagazineSize")); }
	static FGameplayTag SpareAmmo() { return Get(TEXT("Lyra.ShooterGame.Weapon.SpareAmmo")); }
}

ALyraShooterEnemy::ALyraShooterEnemy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// ALyraCharacter turns bStartWithTickEnabled off; without this Tick (chase + weapon input) never runs.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();
	bReplicates = true;
	SetReplicateMovement(true);

	Health = CreateDefaultSubobject<USlimeHealthComponent>(TEXT("Health"));
	Health->Team = ESlimeTeam::Enemy;
	Health->MaxHP = 120.f;
	Health->bDestroyOnDeath = false;
	// Death is driven by HandleDeath (ragdoll → dissolve); a regen-on-death enemy could never be killed.
	Health->bRegenOnDeath = false;

	Status = CreateDefaultSubobject<USlimeStatusComponent>(TEXT("Status"));
	Combat = CreateDefaultSubobject<UEnemyCombatComponent>(TEXT("Combat"));

	// Property name must match B_Hero_Default's Get AIPerceptionStimuliSource pin.
	AIPerceptionStimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("AIPerceptionStimuliSource"));
	AIPerceptionStimuliSource->bAutoRegister = true;
	AIPerceptionStimuliSource->RegisterForSense(UAISense_Sight::StaticClass());

	// Lyra keeps these on the PlayerController (added by the ShooterCore experience). We run no
	// experience and the body must work for AI too, so they live on the pawn.
	Inventory = CreateDefaultSubobject<ULyraInventoryManagerComponent>(TEXT("LyraInventory"));
	EquipmentManager = CreateDefaultSubobject<ULyraEquipmentManagerComponent>(TEXT("LyraEquipment"));

	// Same head-bar as ASlimeEnemyCharacter / AGaspSandboxPawn. Anchor is refreshed from the capsule at runtime.
	HealthBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBar->SetupAttachment(RootComponent);
	HealthBar->SetRelativeLocation(FVector(0.f, 0.f, 90.f + HealthBarZOffset));
	HealthBar->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBar->SetDrawAtDesiredSize(false);
	HealthBar->SetDrawSize(FVector2D(72.f, 8.f));
	HealthBar->SetPivot(FVector2D(0.5f, 1.f));
	HealthBar->SetWidgetClass(USlimeWorldHealthBar::StaticClass());
	HealthBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthBar->SetHiddenInGame(true);

	StandalonePawnData = TSoftObjectPtr<ULyraPawnData>(FSoftObjectPath(
		TEXT("/Game/Characters/Heroes/SimplePawnData/SimplePawnData.SimplePawnData")));
	StandaloneInputConfig = TSoftObjectPtr<ULyraInputConfig>(FSoftObjectPath(
		TEXT("/Game/Input/InputData_Hero.InputData_Hero")));
	StandaloneMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(
		TEXT("/Game/Input/Mappings/IMC_Default.IMC_Default")));
	StandaloneCameraMode = TSoftClassPtr<ULyraCameraMode>(FSoftObjectPath(
		TEXT("/Game/Characters/Cameras/CM_ThirdPerson.CM_ThirdPerson_C")));
	ADSCameraMode = TSoftClassPtr<ULyraCameraMode>(FSoftObjectPath(
		TEXT("/ShooterCore/Camera/CM_ThirdPersonADS.CM_ThirdPersonADS_C")));
	// SimplePawnData grants nothing; the ShooterCore hero set is where jump / ADS / dash / melee live.
	StandaloneAbilitySets.Add(TSoftObjectPtr<ULyraAbilitySet>(FSoftObjectPath(
		TEXT("/ShooterCore/Game/AbilitySet_ShooterHero.AbilitySet_ShooterHero"))));
	StandaloneExcludedAbilities.Add(TSoftClassPtr<ULyraGameplayAbility>(FSoftObjectPath(
		TEXT("/Game/Characters/Heroes/Abilities/GA_Hero_Death.GA_Hero_Death_C"))));
	StandaloneExcludedAbilities.Add(TSoftClassPtr<ULyraGameplayAbility>(FSoftObjectPath(
		TEXT("/ShooterCore/Game/Respawn/GA_SpawnEffect.GA_SpawnEffect_C"))));
	// GA_ADS needs ALyraPlayerController (GetLocalPlayerSubSystemFromPlayerController) and a LyraHeroComponent;
	// with our plain PC it throws "Accessed None" every aim. SetADSActive does its two jobs (camera + IMC_ADS_Speed) natively.
	StandaloneExcludedAbilities.Add(TSoftClassPtr<ULyraGameplayAbility>(FSoftObjectPath(
		TEXT("/ShooterCore/Input/Abilities/GA_ADS.GA_ADS_C"))));
	ADSMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(
		TEXT("/ShooterCore/Input/Mappings/IMC_ADS_Speed.IMC_ADS_Speed")));
	StandaloneHUDLayoutClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(
		TEXT("/ShooterCore/UserInterface/W_ShooterHUDLayout.W_ShooterHUDLayout_C")));
	{
		FSlimeLyraHUDWidgetEntry Reticle;
		Reticle.WidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(
			TEXT("/ShooterCore/UserInterface/HUD/W_WeaponReticleHost.W_WeaponReticleHost_C")));
		Reticle.SlotTag = LyraShooterTags::HudReticle();
		StandaloneHUDWidgets.Add(Reticle);
		FSlimeLyraHUDWidgetEntry QuickBar;
		QuickBar.WidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(
			TEXT("/ShooterCore/UserInterface/HUD/W_QuickBar.W_QuickBar_C")));
		QuickBar.SlotTag = LyraShooterTags::HudEquipment();
		StandaloneHUDWidgets.Add(QuickBar);
	}
	StandaloneHUDRemovedWidgets.Add(TSoftClassPtr<UUserWidget>(FSoftObjectPath(
		TEXT("/Game/UI/Hud/W_Healthbar.W_Healthbar_C"))));
	DefaultWeaponItem = TSoftClassPtr<ULyraInventoryItemDefinition>(FSoftObjectPath(
		TEXT("/ShooterCore/Weapons/Rifle/ID_Rifle.ID_Rifle_C")));
	// ABP_Mannequin_Base is layer-driven; with nothing linked it outputs a broken pose.
	// Lyra links this from B_MannequinPawnCosmetics / weapon OnEquipped; we have neither on the AI body.
	DefaultAnimLayers = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(
		TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Unarmed/ABP_UnarmedAnimLayers.ABP_UnarmedAnimLayers_C")));

	// Official B_Hero_Default mesh is empty (cosmetics). Lab needs a visible body.
	// SOFT refs only. Do NOT ConstructorHelpers-load ABP_Mannequin_Base here: the game module's CDOs are
	// built before Default-phase plugins (MovieSceneAnimMixer) register their script packages, so the ABP's
	// AnimSubsystem_SequencerMixer property degrades to FallbackStruct and every Slot node then hits
	// check(Subsystem) in AnimInstance.h:937 on the first evaluate.
	DefaultBodyMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
		TEXT("/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny")));
	DefaultAnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(
		TEXT("/Game/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Base.ABP_Mannequin_Base_C")));
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		// Capsule is 40x90 (ALyraCharacter); feet must sit at the capsule bottom.
		// B_Hero_Default leaves Mesh at the capsule center because its body is a cosmetics child actor.
		MeshComp->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
		MeshComp->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		MeshComp->SetVisibility(true);
		MeshComp->SetHiddenInGame(false);
	}
}

void ALyraShooterEnemy::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Editor placement / level load: plugins are registered by now, safe to load the ABP.
	EnsureBodyVisuals();

#if WITH_EDITOR
	// Placed in a level: run the anim graph in the editor too, otherwise the mannequin sits in its
	// A-pose ref pose until PIE. Needs the unarmed layers linked or the base graph outputs junk.
	if (UWorld* World = GetWorld(); World && !World->IsGameWorld())
	{
		if (USkeletalMeshComponent* MeshComp = GetMesh(); MeshComp && MeshComp->GetAnimClass())
		{
			MeshComp->SetUpdateAnimationInEditor(true);
			if (!MeshComp->GetAnimInstance())
			{
				MeshComp->InitAnim(true);
			}
			EnsureAnimLayersLinked();
		}
	}
#endif
}

void ALyraShooterEnemy::PostInitializeComponents()
{
	EnsureBodyVisuals();
	// Before Super: APawn::PostInitializeComponents spawns the AI controller and Lyra refuses to
	// change the team of a possessed character. Morph bodies were flagged in InitAsMorphTarget.
	TrySetTeam(bMorphTarget ? PlayerTeamId : EnemyTeamId);
	Super::PostInitializeComponents();
}

void ALyraShooterEnemy::TrySetTeam(uint8 TeamId)
{
	if (!HasAuthority() || GetController() != nullptr)
	{
		return;
	}
	SetGenericTeamId(FGenericTeamId(TeamId));
}

void ALyraShooterEnemy::EnsureBodyVisuals()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}
	if (!MeshComp->GetSkeletalMeshAsset() && !DefaultBodyMesh.IsNull())
	{
		if (USkeletalMesh* Body = DefaultBodyMesh.LoadSynchronous())
		{
			MeshComp->SetSkeletalMeshAsset(Body);
		}
	}
	if (!MeshComp->GetAnimClass() && !DefaultAnimClass.IsNull())
	{
		if (UClass* AnimClass = DefaultAnimClass.LoadSynchronous())
		{
			MeshComp->SetAnimInstanceClass(AnimClass);
		}
	}
}

void ALyraShooterEnemy::BeginPlay()
{
	Super::BeginPlay();
	// PawnExt->InitializeAbilitySystem (inside GrantStandaloneAbilities) fires
	// ALyraCharacter::OnAbilitySystemInitialized, which already inits LyraHealthComponent.
	// Only fall back to a manual init if PawnData failed to load.
	GrantStandaloneAbilities();
	if (ULyraHealthComponent* LyraHealth = ULyraHealthComponent::FindHealthComponent(this))
	{
		// GetMaxHealth() returns 0 while the component has no HealthSet bound.
		if (LyraHealth->GetMaxHealth() <= 0.f)
		{
			if (ULyraAbilitySystemComponent* LyraASC = Cast<ULyraAbilitySystemComponent>(GetAbilitySystemComponent()))
			{
				LyraHealth->InitializeWithAbilitySystem(LyraASC);
			}
		}
	}
	BindLyraHealthMirror();
	if (Health)
	{
		Health->OnDied.AddUniqueDynamic(this, &ALyraShooterEnemy::HandleSlimeDied);
		Health->OnHealthChanged.AddUniqueDynamic(this, &ALyraShooterEnemy::HandleSlimeHealthChanged);
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		// B_Hero_Default overrides the C++ defaults (orient to movement); remember them for the morphed player.
		bSavedOrientToMovement = Move->bOrientRotationToMovement;
		bSavedUseControllerDesiredRotation = Move->bUseControllerDesiredRotation;
	}
	bSavedUseControllerRotationYaw = bUseControllerRotationYaw;
	BindWorldHealthBar();
	EnsureAnimLayersLinked();
	if (HasAuthority())
	{
		EnsureDefaultWeapon();
	}
}

void ALyraShooterEnemy::BindWorldHealthBar()
{
	if (!HealthBar)
	{
		return;
	}
	RefreshHealthBarAnchor();
	HealthBar->InitWidget();
	if (USlimeWorldHealthBar* Bar = Cast<USlimeWorldHealthBar>(HealthBar->GetWidget()))
	{
		Bar->SetHealth(Health);
	}
	RefreshWorldHealthBarVisibility();
}

void ALyraShooterEnemy::RefreshHealthBarAnchor()
{
	if (!HealthBar)
	{
		return;
	}
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float Half = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
	HealthBar->SetRelativeLocation(FVector(0.f, 0.f, Half + HealthBarZOffset));
}

void ALyraShooterEnemy::RefreshWorldHealthBarVisibility()
{
	if (!HealthBar)
	{
		return;
	}
	bool bShow = Health && Health->IsAlive()
		&& !bMorphTarget && !IsPlayerControlled() && !bDevourLocked && !bDevouredDeath && !bDeathSequence;
	if (bShow)
	{
		if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			// The lock-on HUD bar takes over while this body is the locked target.
			bShow = !USlimeLockOnComponent::IsLockedByLocalPlayer(this, this);
			if (bShow && !Health->IsWorldHealthBarRevealed())
			{
				bShow = FVector::DistSquared(Player->GetActorLocation(), GetActorLocation())
					<= FMath::Square(HealthBarVisibleRange);
			}
		}
		else
		{
			bShow = false;
		}
	}
	HealthBar->SetHiddenInGame(!bShow);
	HealthBar->SetVisibility(bShow);
}

bool ALyraShooterEnemy::CanBeLockedOn() const
{
	return Health && Health->IsAlive() && !bMorphTarget && !bDeathSequence && !bDevouredDeath && !IsPlayerControlled();
}

void ALyraShooterEnemy::BindLyraHealthMirror()
{
	if (bLyraHealthBound)
	{
		return;
	}
	if (ULyraHealthComponent* LyraHealth = ULyraHealthComponent::FindHealthComponent(this))
	{
		LyraHealth->OnHealthChanged.AddUniqueDynamic(this, &ALyraShooterEnemy::HandleLyraHealthChanged);
		bLyraHealthBound = true;
	}
}

void ALyraShooterEnemy::HandleLyraHealthChanged(ULyraHealthComponent* LyraHealthComp, float OldValue, float NewValue, AActor* DamageInstigator)
{
	// Lyra weapon hits (GE_Damage_*) land on LyraHealthSet. Mirror the delta into the slime health,
	// which drives devour threshold / HUD / death. Melee never touches Lyra health, so no feedback loop.
	if (!Health || !LyraHealthComp || bSyncingLyraHealth)
	{
		return;
	}
	const float MaxLyra = LyraHealthComp->GetMaxHealth();
	if (MaxLyra <= 0.f)
	{
		return;
	}
	float Delta = (OldValue - NewValue) / MaxLyra * Health->MaxHP;
	if (Delta > 0.f && IsPlayerControlled())
	{
		// Lyra rifles do ~10 per hit against 100 HP; the morphed player should not evaporate in one burst.
		Delta *= MorphIncomingGunDamageScale;
	}
	UE_LOG(LogSlimeFable, Verbose, TEXT("LyraShooterEnemy %s: Lyra health %.1f -> %.1f (slime delta %.1f) by %s"),
		*GetName(), OldValue, NewValue, Delta, *GetNameSafe(DamageInstigator));
	if (Delta > 0.f)
	{
		Health->ApplyDamage(Delta, DamageInstigator, GetActorLocation(), FVector::ZeroVector);
	}
	else if (Delta < 0.f)
	{
		Health->ApplyHealing(-Delta);
	}
	if (IsPlayerMorphBody())
	{
		// Refill Lyra's pool right after the hit so it never reaches 0 and GA_Hero_Death never fires;
		// deferred one tick because the ASC is mid-execution here.
		bLyraHealthSyncPending = true;
	}
}

void ALyraShooterEnemy::HandleSlimeHealthChanged(float CurrentHP, float MaxHP)
{
	if (IsPlayerMorphBody())
	{
		bLyraHealthSyncPending = true;
	}
}

void ALyraShooterEnemy::SyncLyraHealthFromSlime()
{
	bLyraHealthSyncPending = false;
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || !Health || !HasAuthority() || bSyncingLyraHealth)
	{
		return;
	}
	const ULyraHealthSet* Set = ASC->GetSet<ULyraHealthSet>();
	if (!Set)
	{
		return;
	}
	const float Fraction = FMath::Clamp(Health->GetHealthPercent(), 0.f, 1.f);
	const float LyraMax = FMath::Max(1.f, Set->GetMaxHealth());
	// Floor at 1: Lyra's death cascade (GA_Hero_Death -> destroy pawn) must never run on the morph body.
	const float Target = FMath::Max(1.f, LyraMax * Fraction);
	if (FMath::IsNearlyEqual(Target, Set->GetHealth(), 0.01f))
	{
		return;
	}
	TGuardValue<bool> Guard(bSyncingLyraHealth, true);
	ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), Target);
}

void ALyraShooterEnemy::HandleSlimeDied()
{
	HandleDeath();
}

bool ALyraShooterEnemy::IsPlayerMorphBody() const
{
	return bMorphTarget && Cast<APlayerController>(GetController()) != nullptr;
}

void ALyraShooterEnemy::EnsureAnimLayersLinked()
{
	if (EquippedWeapon)
	{
		RelinkWeaponAnimLayers();
		return;
	}
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp || !MeshComp->GetAnimInstance() || DefaultAnimLayers.IsNull())
	{
		return;
	}
	UClass* LayerClass = DefaultAnimLayers.LoadSynchronous();
	if (!LayerClass)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("LyraShooterEnemy %s: DefaultAnimLayers failed to load."), *GetName());
		return;
	}
	if (MeshComp->GetLinkedAnimLayerInstanceByClass(LayerClass))
	{
		return;
	}
	MeshComp->LinkAnimClassLayers(LayerClass);
}

void ALyraShooterEnemy::RelinkWeaponAnimLayers()
{
	// B_WeaponInstance_Base::OnEquipped normally does this in Blueprint; do it from C++ as well so the
	// body never stays in the unarmed layers with a rifle in hand.
	USkeletalMeshComponent* MeshComp = GetMesh();
	ULyraWeaponInstance* Weapon = Cast<ULyraWeaponInstance>(EquippedWeapon);
	if (!MeshComp || !MeshComp->GetAnimInstance() || !Weapon)
	{
		return;
	}
	TSubclassOf<UAnimInstance> LayerClass = Weapon->PickBestAnimLayer(true, FGameplayTagContainer());
	if (!LayerClass || MeshComp->GetLinkedAnimLayerInstanceByClass(LayerClass))
	{
		return;
	}
	MeshComp->LinkAnimClassLayers(LayerClass);
}

void ALyraShooterEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING
	TickDebugLogClock += DeltaSeconds;
	if (TickDebugLogClock >= 2.f)
	{
		TickDebugLogClock = 0.f;
		UE_LOG(LogSlimeFable, Verbose, TEXT("LyraShooterEnemy %s: tick ctrl=%s morph=%d death=%d hp=%.0f player=%s"),
			*GetName(), *GetNameSafe(GetController()), bMorphTarget, bDeathSequence, Health ? Health->CurrentHP : -1.f,
			*GetNameSafe(UGameplayStatics::GetPlayerPawn(this, 0)));
	}
#endif
	if (!bMorphTarget && !bDevouredDeath && !bDeathSequence)
	{
		if (bDevourLocked)
		{
			// Latched by the slime: no shooting / turning while the devour plays out.
			SetAIFiring(false);
			SetAIFacingMode(false);
		}
		else
		{
			TickSimpleChase(DeltaSeconds);
		}
	}
	RefreshWorldHealthBarVisibility();
	if (IsPlayerMorphBody())
	{
		TickRightMouse();
		TickQuickSlotKeys();
		TickCrouch();
		if (bLyraHealthSyncPending)
		{
			SyncLyraHealthFromSlime();
		}
	}
	// Lyra normally does this in ALyraPlayerController::PostProcessInput against the PlayerState ASC.
	// Our ASC lives on the pawn and the AI has no player controller, so drive it here for both.
	ProcessLyraAbilityInput(DeltaSeconds);
}

void ALyraShooterEnemy::ProcessLyraAbilityInput(float DeltaSeconds)
{
	if (ULyraAbilitySystemComponent* LyraASC = Cast<ULyraAbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		LyraASC->ProcessAbilityInput(DeltaSeconds, false);
	}
}

void ALyraShooterEnemy::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	SetAIFiring(false);
	SetAIFacingMode(false);
	if (APlayerController* PC = Cast<APlayerController>(NewController))
	{
		if (bMorphTarget)
		{
			ActivateStandaloneForPlayer(PC);
			// PC->GetPawn() is still unset here (see SetupPlayerQuickBar); the morph component calls again right after
			// Possess, but a plain possess (PIE / debug) needs this second pass for the quick bar + HUD.
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
				{
					if (APlayerController* NowPC = Cast<APlayerController>(GetController()))
					{
						if (bMorphTarget)
						{
							ActivateStandaloneForPlayer(NowPC);
						}
					}
				}));
			}
		}
	}
	ApplyWeaponDamageOverrides();
}

void ALyraShooterEnemy::UnPossessed()
{
	SetAIFiring(false);
	SetAIFacingMode(false);
	AController* OldController = GetController();
	if (Cast<APlayerController>(OldController))
	{
		// Controller still owns the pawn here, so the quick bar can still reach the EquipmentManager.
		DeactivateStandaloneForPlayer(OldController);
	}
	Super::UnPossessed();
	ApplyWeaponDamageOverrides();
}

void ALyraShooterEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Body destroyed while the player still had it (level travel, debug destroy): do not leave the Lyra HUD
	// or a quick bar pointing at us on the controller.
	if (HUDLayoutWidget || PlayerQuickBar.IsValid() || bStandaloneMappingAdded)
	{
		DeactivateStandaloneForPlayer(GetController());
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ALyraShooterEnemy::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		BindStandaloneInput(PC);
	}
}

void ALyraShooterEnemy::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	if (Health)
	{
		Health->ApplyDamage(Damage, DamageCauser, DamageLocation, DamageImpulse);
	}
}

void ALyraShooterEnemy::HandleDeath()
{
	if (bDeathSequence)
	{
		return;
	}
	if (Health && Health->IsAlive())
	{
		// Route through the health component so OnDied fires once and HandleSlimeDied lands here again.
		Health->ApplyDamage(FMath::Max(Health->CurrentHP, 1.f), this, GetActorLocation(), FVector::ZeroVector);
		return;
	}

	if (bMorphTarget)
	{
		// The morphed body died: hand control back to the slime, same as the GASP bodies.
		if (AActor* Master = MorphMaster.Get())
		{
			if (USlimeMorphComponent* Morph = Master->FindComponentByClass<USlimeMorphComponent>())
			{
				Morph->ForceUnmorph(true);
			}
		}
		return;
	}

	bDeathSequence = true;
	bDevourable = false;
	SetAIFiring(false);
	SetAIFacingMode(false);
	RefreshWorldHealthBarVisibility();
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
		AI->ClearFocus(EAIFocusPriority::Gameplay);
	}
	if (EquippedWeapon && EquipmentManager)
	{
		// Drops the B_Rifle actor with the body instead of leaving a floating gun.
		EquipmentManager->UnequipItem(EquippedWeapon);
		EquippedWeapon = nullptr;
		EquippedWeaponItem = nullptr;
	}

	if (bDevouredDeath)
	{
		SetActorEnableCollision(false);
		SetActorHiddenInGame(true);
		return;
	}

	// Ragdoll, then let the shared dissolve destroy the actor.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
		MeshComp->SetAllBodiesSimulatePhysics(true);
		MeshComp->SetSimulatePhysics(true);
		MeshComp->WakeAllRigidBodies();
	}
	if (Health)
	{
		TWeakObjectPtr<USlimeHealthComponent> WeakHealth(Health);
		GetWorldTimerManager().SetTimer(DeathDissolveHandle, FTimerDelegate::CreateLambda([WeakHealth]()
		{
			if (USlimeHealthComponent* Alive = WeakHealth.Get())
			{
				Alive->BeginDeathDissolve();
			}
		}), DeathRagdollSeconds, false);
	}
}

void ALyraShooterEnemy::ApplyHealing(float Healing, AActor* Healer)
{
	(void)Healer;
	if (Health)
	{
		Health->ApplyHealing(Healing);
	}
}

void ALyraShooterEnemy::NotifyDanger(const FVector& DangerLocation, AActor* DangerSource)
{
	(void)DangerLocation;
	(void)DangerSource;
}

bool ALyraShooterEnemy::IsDevourableNow() const
{
	// Do NOT gate on bDevourLocked: FreezeDevourTarget sets it true and TickPhase re-checks
	// CanDevourTarget every frame; gating here aborts the devour before the shrink starts
	// (same rule as AGaspSandboxPawn / AGaspMoverEnemy).
	return bDevourable && !bMorphTarget && !bDeathSequence && !bDevouredDeath && !IsPlayerControlled()
		&& Health && Health->IsAlive();
}

float ALyraShooterEnemy::GetHealthPercent() const
{
	return Health ? Health->GetHealthPercent() : 0.f;
}

FText ALyraShooterEnemy::GetResolvedDisplayName() const
{
	return DisplayName.IsEmpty() ? NSLOCTEXT("LyraShooter", "DefaultName", "射击兵") : DisplayName;
}

FLinearColor ALyraShooterEnemy::ResolveDevourWheelTint() const
{
	return FLinearColor(0.35f, 0.45f, 0.28f);
}

USkeletalMeshComponent* ALyraShooterEnemy::GetPrimarySkeletalMesh() const
{
	return GetMesh();
}

USkeletalMeshComponent* ALyraShooterEnemy::GetDevourPreviewMesh() const
{
	return GetMesh();
}

UCapsuleComponent* ALyraShooterEnemy::GetDevourCapsule() const
{
	return GetCapsuleComponent();
}

void ALyraShooterEnemy::ForEachVisualMesh(TFunctionRef<void(UMeshComponent*)> Fn) const
{
	TArray<UMeshComponent*> MeshComps;
	GetComponents<UMeshComponent>(MeshComps);
	for (UMeshComponent* MeshComp : MeshComps)
	{
		if (!MeshComp || MeshComp->IsVisualizationComponent() || Cast<UCameraComponent>(MeshComp->GetAttachParent()))
		{
			continue;
		}
		Fn(MeshComp);
	}
}

void ALyraShooterEnemy::InitAsMorphTarget(AActor* Master)
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
		Health->bRegenOnDeath = false;
	}
	TrySetTeam(PlayerTeamId);
}

void ALyraShooterEnemy::InitAsPhantom(float LifeSeconds, AActor* Master)
{
	(void)LifeSeconds;
	bDevourable = false;
	MorphMaster = Master;
	if (Health)
	{
		Health->Team = ESlimeTeam::Player;
	}
	TrySetTeam(PlayerTeamId);
}

void ALyraShooterEnemy::BeginDevouredDeath(AActor* Devourer)
{
	(void)Devourer;
	bDevouredDeath = true;
	bDeathSequence = true;
	bDevourable = false;
	SetAIFiring(false);
}

void ALyraShooterEnemy::FreezeForDevour()
{
	SetAIFiring(false);
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
}

void ALyraShooterEnemy::RestoreFromDevour()
{
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetDefaultMovementMode();
	}
}

void ALyraShooterEnemy::StopMeshAnimation()
{
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->bPauseAnims = true;
	}
}

FVector ALyraShooterEnemy::GetVisualBoundsCenter() const
{
	if (const USkeletalMeshComponent* MeshComp = GetMesh())
	{
		return MeshComp->Bounds.Origin;
	}
	return GetActorLocation();
}

FVector ALyraShooterEnemy::GetHudAnchorLocation() const
{
	if (const UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		return Capsule->GetComponentLocation() + FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + HealthBarZOffset);
	}
	return GetActorLocation();
}

bool ALyraShooterEnemy::GetStableMeshBounds(FBox& OutBox) const
{
	if (const USkeletalMeshComponent* MeshComp = GetMesh())
	{
		OutBox = MeshComp->Bounds.GetBox();
		return true;
	}
	return false;
}

void ALyraShooterEnemy::SetMorphGameplayEnabled(bool bEnabled)
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		if (bEnabled)
		{
			Movement->SetDefaultMovementMode();
		}
		else
		{
			Movement->DisableMovement();
		}
	}
}

void ALyraShooterEnemy::ActivateStandaloneForPlayer(APlayerController* PC)
{
	if (!PC)
	{
		return;
	}
	// Called from PossessedBy and again from USlimeMorphComponent::PossessMorphTarget; every step is idempotent.
	GrantStandaloneAbilities();
	EnsurePlayerTeam(PC);
	GrantStandaloneHeroAbilities();
	AddStandaloneMappingContext(PC);
	EnsurePlayerCrouch();
	BindStandaloneInput(PC);
	BindStandaloneCamera();
	SetupPlayerQuickBar(PC);
	ShowStandaloneHUD(PC);
	ApplyWeaponDamageOverrides();
	bLyraHealthSyncPending = true;
}

void ALyraShooterEnemy::DeactivateStandaloneForPlayer(AController* OldController)
{
	SetADSActive(false);
	bRightMouseHeld = false;
	HideStandaloneHUD();
	TeardownPlayerQuickBar();
	RemoveStandaloneMappingContext(OldController);
	if (USlimeDodgeComponent* Dodge = CachedMorphDodge.Get())
	{
		Dodge->bPollRightMouse = true;
	}
	CachedMorphDodge = nullptr;
}

void ALyraShooterEnemy::EnsurePlayerTeam(APlayerController* PC)
{
	if (!HasAuthority())
	{
		return;
	}
	// Lyra's damage execution compares the instigator pawn's team with the target's; with NoTeam on either
	// side CanCauseDamage() returns false and every bullet the player fires is a blank.
	if (ALyraPlayerState* LyraPS = PC ? PC->GetPlayerState<ALyraPlayerState>() : nullptr)
	{
		if (LyraPS->GetGenericTeamId() == FGenericTeamId::NoTeam)
		{
			LyraPS->SetGenericTeamId(FGenericTeamId(PlayerTeamId));
		}
	}
	if (GetGenericTeamId() != FGenericTeamId(PlayerTeamId))
	{
		TrySetTeam(PlayerTeamId);
	}
}

void ALyraShooterEnemy::GrantStandaloneHeroAbilities()
{
	if (bHeroAbilitiesGranted || !HasAuthority())
	{
		return;
	}
	ULyraAbilitySystemComponent* LyraASC = Cast<ULyraAbilitySystemComponent>(GetAbilitySystemComponent());
	if (!LyraASC)
	{
		return;
	}
	TArray<TSubclassOf<ULyraGameplayAbility>> Excluded;
	for (const TSoftClassPtr<ULyraGameplayAbility>& Soft : StandaloneExcludedAbilities)
	{
		if (UClass* Loaded = Soft.LoadSynchronous())
		{
			Excluded.Add(Loaded);
		}
	}
	for (const TSoftObjectPtr<ULyraAbilitySet>& Soft : StandaloneAbilitySets)
	{
		if (const ULyraAbilitySet* Set = Soft.LoadSynchronous())
		{
			FLyraAbilitySet_GrantedHandles Handles;
			Set->GiveToAbilitySystemFiltered(LyraASC, &Handles, this, Excluded);
			GrantedHandles.Add(MoveTemp(Handles));
		}
	}
	bHeroAbilitiesGranted = true;
}

void ALyraShooterEnemy::AddStandaloneMappingContext(APlayerController* PC)
{
	if (bStandaloneMappingAdded || !PC || !PC->IsLocalController())
	{
		return;
	}
	ULocalPlayer* LP = PC->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}
	if (UInputMappingContext* IMC = StandaloneMappingContext.LoadSynchronous())
	{
		if (!Subsystem->HasMappingContext(IMC))
		{
			Subsystem->AddMappingContext(IMC, 2);
		}
		bStandaloneMappingAdded = true;
	}
}

void ALyraShooterEnemy::RemoveStandaloneMappingContext(AController* OldController)
{
	if (!bStandaloneMappingAdded)
	{
		return;
	}
	bStandaloneMappingAdded = false;
	APlayerController* PC = Cast<APlayerController>(OldController);
	ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}
	if (UInputMappingContext* IMC = StandaloneMappingContext.Get())
	{
		// Lyra's IMC_Default maps WASD / Space / LMB too; left behind it would fight the slime's own bindings.
		Subsystem->RemoveMappingContext(IMC);
	}
}

// ---------------------------------------------------------------------------------------------
// Quick bar (player morph): 1..N keys, W_QuickBar ammo / weapon name
// ---------------------------------------------------------------------------------------------

void ALyraShooterEnemy::SetupPlayerQuickBar(APlayerController* PC)
{
	if (!HasAuthority() || !PC || !Inventory || !EquipmentManager)
	{
		return;
	}
	// ULyraQuickBarComponent::FindEquipmentManager goes through Controller->GetPawn(), which APlayerController::OnPossess
	// only sets after PossessedBy returns. Until then the quick bar could not equip anything, so wait for the next call.
	if (PC->GetPawn() != this)
	{
		return;
	}
	USlimeLyraQuickBarComponent* QuickBar = PlayerQuickBar.Get();
	if (QuickBar && QuickBar->GetOwner() != PC)
	{
		TeardownPlayerQuickBar();
		QuickBar = nullptr;
	}
	if (QuickBar && EquippedWeapon && EquippedWeaponItem && QuickBar->GetActiveSlotItem() == EquippedWeaponItem)
	{
		// Already handed over and in sync; re-running would respawn the weapon actor.
		return;
	}
	if (!QuickBar)
	{
		QuickBar = PC->FindComponentByClass<USlimeLyraQuickBarComponent>();
	}
	if (!QuickBar)
	{
		QuickBar = NewObject<USlimeLyraQuickBarComponent>(PC, USlimeLyraQuickBarComponent::StaticClass(), TEXT("SlimeLyraQuickBar"));
		QuickBar->ConfigureSlotCount(WeaponQuickSlots);
		QuickBar->RegisterComponent();
	}
	PlayerQuickBar = QuickBar;

	// Hand the equip over to the quick bar: it owns the equipped instance from here on
	// (ULyraQuickBarComponent::EquipItemInSlot checks its own EquippedItem is null).
	ULyraInventoryItemInstance* WantedItem = EquippedWeaponItem;
	if (EquippedWeapon)
	{
		EquipmentManager->UnequipItem(EquippedWeapon);
		EquippedWeapon = nullptr;
		EquippedWeaponItem = nullptr;
	}
	QuickBar->ClearAllSlots();

	int32 WantedSlot = INDEX_NONE;
	for (ULyraInventoryItemInstance* Item : Inventory->GetAllItems())
	{
		if (!Item || !Item->FindFragmentByClass<UInventoryFragment_EquippableItem>())
		{
			continue;
		}
		const int32 Slot = QuickBar->GetNextFreeItemSlot();
		if (Slot == INDEX_NONE)
		{
			break;
		}
		QuickBar->AddItemToSlot(Slot, Item);
		if (Item == WantedItem)
		{
			WantedSlot = Slot;
		}
	}
	if (WantedSlot == INDEX_NONE)
	{
		const TArray<ULyraInventoryItemInstance*> Slots = QuickBar->GetSlots();
		WantedSlot = Slots.IndexOfByPredicate([](const ULyraInventoryItemInstance* Item) { return Item != nullptr; });
	}
	if (WantedSlot != INDEX_NONE)
	{
		QuickBar->SetActiveSlotIndex(WantedSlot);
	}
	SyncEquippedWeaponFromManager();
}

void ALyraShooterEnemy::TeardownPlayerQuickBar()
{
	USlimeLyraQuickBarComponent* QuickBar = PlayerQuickBar.Get();
	PlayerQuickBar = nullptr;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(QuickBarRebroadcastHandle);
	}
	if (!QuickBar)
	{
		return;
	}
	ULyraInventoryItemInstance* KeepItem = QuickBar->GetActiveSlotItem();
	QuickBar->ClearAllSlots();
	QuickBar->DestroyComponent();
	EquippedWeapon = nullptr;
	EquippedWeaponItem = nullptr;
	// The body keeps its gun after the player leaves (AI fallback / phantom copy / next morph).
	if (KeepItem && HasAuthority() && !IsActorBeingDestroyed() && !bDeathSequence && !bDevouredDeath)
	{
		EquipInventoryItem(KeepItem);
	}
}

void ALyraShooterEnemy::RebroadcastQuickBar()
{
	if (USlimeLyraQuickBarComponent* QuickBar = PlayerQuickBar.Get())
	{
		QuickBar->RebroadcastState();
	}
}

void ALyraShooterEnemy::SyncEquippedWeaponFromManager()
{
	if (!EquipmentManager)
	{
		return;
	}
	ULyraEquipmentInstance* Current = EquipmentManager->GetFirstInstanceOfType<ULyraWeaponInstance>();
	if (Current == EquippedWeapon)
	{
		return;
	}
	EquippedWeapon = Current;
	EquippedWeaponItem = Current ? Cast<ULyraInventoryItemInstance>(Current->GetInstigator()) : nullptr;
	ApplyWeaponDamageOverrides();
	if (EquippedWeapon)
	{
		RelinkWeaponAnimLayers();
	}
	else
	{
		EnsureAnimLayersLinked();
	}
}

void ALyraShooterEnemy::InputQuickSlot(int32 SlotIndex)
{
	USlimeLyraQuickBarComponent* QuickBar = PlayerQuickBar.Get();
	if (!QuickBar || !HasAuthority())
	{
		return;
	}
	const TArray<ULyraInventoryItemInstance*> Slots = QuickBar->GetSlots();
	if (!Slots.IsValidIndex(SlotIndex) || !Slots[SlotIndex] || QuickBar->GetActiveSlotIndex() == SlotIndex)
	{
		return;
	}
	QuickBar->SetActiveSlotIndex(SlotIndex);
	SyncEquippedWeaponFromManager();
}

void ALyraShooterEnemy::InputCycleSlot(bool bForward)
{
	USlimeLyraQuickBarComponent* QuickBar = PlayerQuickBar.Get();
	if (!QuickBar || !HasAuthority())
	{
		return;
	}
	if (bForward)
	{
		QuickBar->CycleActiveSlotForward();
	}
	else
	{
		QuickBar->CycleActiveSlotBackward();
	}
	SyncEquippedWeaponFromManager();
}

// ---------------------------------------------------------------------------------------------
// Lyra HUD (player morph): reticle + ammo via UIExtension slots of W_ShooterHUDLayout
// ---------------------------------------------------------------------------------------------

void ALyraShooterEnemy::ShowStandaloneHUD(APlayerController* PC)
{
	if (HUDLayoutWidget || !PC || !PC->IsLocalController() || StandaloneHUDLayoutClass.IsNull())
	{
		return;
	}
	UClass* LayoutClass = StandaloneHUDLayoutClass.LoadSynchronous();
	if (!LayoutClass)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("LyraShooterEnemy %s: HUD layout '%s' failed to load."), *GetName(), *StandaloneHUDLayoutClass.ToString());
		return;
	}
	HUDLayoutWidget = CreateWidget<UUserWidget>(PC, LayoutClass);
	if (!HUDLayoutWidget)
	{
		return;
	}
	// Drop built-in children we replace with slime HUD (the Lyra health bar).
	if (StandaloneHUDRemovedWidgets.Num() > 0 && HUDLayoutWidget->WidgetTree)
	{
		TArray<UClass*> RemovedClasses;
		for (const TSoftClassPtr<UUserWidget>& Soft : StandaloneHUDRemovedWidgets)
		{
			if (UClass* Loaded = Soft.LoadSynchronous())
			{
				RemovedClasses.Add(Loaded);
			}
		}
		TArray<UWidget*> ToRemove;
		HUDLayoutWidget->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			for (UClass* Removed : RemovedClasses)
			{
				if (Widget && Widget->IsA(Removed))
				{
					ToRemove.Add(Widget);
					return;
				}
			}
		});
		for (UWidget* Widget : ToRemove)
		{
			Widget->RemoveFromParent();
		}
	}
	// Not pushed on the CommonUI Game layer: SlimeFable has no PrimaryGameLayout. AddToPlayerScreen builds the
	// Slate tree now, which registers the layout's UIExtensionPointWidgets for this local player.
	HUDLayoutWidget->AddToPlayerScreen(StandaloneHUDZOrder);

	UWorld* World = GetWorld();
	UUIExtensionSubsystem* Extensions = World ? World->GetSubsystem<UUIExtensionSubsystem>() : nullptr;
	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (Extensions && LP)
	{
		for (const FSlimeLyraHUDWidgetEntry& Entry : StandaloneHUDWidgets)
		{
			if (!Entry.SlotTag.IsValid() || Entry.WidgetClass.IsNull())
			{
				continue;
			}
			UClass* WidgetClass = Entry.WidgetClass.LoadSynchronous();
			if (!WidgetClass)
			{
				UE_LOG(LogSlimeFable, Warning, TEXT("LyraShooterEnemy %s: HUD widget '%s' failed to load."), *GetName(), *Entry.WidgetClass.ToString());
				continue;
			}
			// Same context Lyra's GameFeatureAction_AddWidgets uses (the LocalPlayer), so the point widgets match.
			FUIExtensionHandle Handle = Extensions->RegisterExtensionAsWidgetForContext(Entry.SlotTag, LP, WidgetClass, -1);
			if (Handle.IsValid())
			{
				HUDExtensionHandles.Add(MoveTemp(Handle));
			}
		}
	}
	// W_QuickBar / W_WeaponReticleHost were created after the quick bar already fired its messages; resend next tick.
	if (World)
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ALyraShooterEnemy::RebroadcastQuickBar);
	}
}

void ALyraShooterEnemy::HideStandaloneHUD()
{
	for (FUIExtensionHandle& Handle : HUDExtensionHandles)
	{
		if (Handle.IsValid())
		{
			Handle.Unregister();
		}
	}
	HUDExtensionHandles.Reset();
	if (HUDLayoutWidget)
	{
		HUDLayoutWidget->RemoveFromParent();
		HUDLayoutWidget = nullptr;
	}
}

// ---------------------------------------------------------------------------------------------
// Right mouse (player morph): tap = slime dodge, hold = ADS
// ---------------------------------------------------------------------------------------------

USlimeDodgeComponent* ALyraShooterEnemy::FindMorphDodge()
{
	if (USlimeDodgeComponent* Cached = CachedMorphDodge.Get())
	{
		return Cached;
	}
	// USlimeMorphComponent::EnsureMorphDodge adds "MorphSlimeDodge" after ActivateStandaloneForPlayer, so look it up lazily.
	USlimeDodgeComponent* Found = FindComponentByClass<USlimeDodgeComponent>();
	CachedMorphDodge = Found;
	return Found;
}

void ALyraShooterEnemy::TickRightMouse()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	UWorld* World = GetWorld();
	if (!PC || !PC->IsLocalController() || !World)
	{
		return;
	}
	USlimeDodgeComponent* Dodge = FindMorphDodge();
	if (Dodge && Dodge->bPollRightMouse)
	{
		// We own the button now; the dodge component would otherwise roll on every ADS press.
		Dodge->bPollRightMouse = false;
	}

	bool bDown = false;
	const UGameInstance* GI = World->GetGameInstance();
	const USlimeInputSettings* InputSettings = GI ? GI->GetSubsystem<USlimeInputSettings>() : nullptr;
	if (InputSettings)
	{
		bDown = InputSettings->IsKeyDown(PC, ESlimeInputAction::Dodge);
	}
	else
	{
		bDown = PC->IsInputKeyDown(EKeys::RightMouseButton);
	}

	const float Now = World->GetTimeSeconds();
	if (bDown && !bRightMouseHeld)
	{
		bRightMouseHeld = true;
		RightMousePressTime = Now;
	}
	else if (bDown && bRightMouseHeld && !bADSActive && (Now - RightMousePressTime) >= ADSHoldSeconds)
	{
		SetADSActive(true);
	}
	else if (!bDown && bRightMouseHeld)
	{
		bRightMouseHeld = false;
		if (bADSActive)
		{
			SetADSActive(false);
		}
		else if (Dodge)
		{
			// Tap: blink dash out of threat range, roll / perfect dodge (0.5 s i-frames) inside it.
			Dodge->TryHandleRightClick();
		}
	}
}

void ALyraShooterEnemy::TickCrouch()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const USlimeInputSettings* InputSettings = GI ? GI->GetSubsystem<USlimeInputSettings>() : nullptr;
	const bool bFlattenPressed = InputSettings
		? InputSettings->WasKeyPressed(PC, ESlimeInputAction::Flatten)
		: PC->WasInputKeyJustPressed(EKeys::C);
	const bool bCtrlPressed = PC->WasInputKeyJustPressed(EKeys::LeftControl);
	if (bFlattenPressed || bCtrlPressed)
	{
		TryToggleCrouch();
	}
}

void ALyraShooterEnemy::TickQuickSlotKeys()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController() || !PlayerQuickBar.IsValid())
	{
		return;
	}
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const USlimeInputSettings* InputSettings = GI ? GI->GetSubsystem<USlimeInputSettings>() : nullptr;

	// Slime abilities are locked while morphed (USlimeAbilityComponent::PollAbilityKeys), so 1..6 are free.
	static const ESlimeInputAction SlotActions[6] = {
		ESlimeInputAction::Element1, ESlimeInputAction::Element2, ESlimeInputAction::Element3,
		ESlimeInputAction::Element4, ESlimeInputAction::Element5, ESlimeInputAction::Element6
	};
	static const FKey SlotFallbacks[6] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six };
	const int32 Count = FMath::Min(WeaponQuickSlots, 6);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const bool bPressed = InputSettings
			? InputSettings->WasKeyPressed(PC, SlotActions[Index])
			: PC->WasInputKeyJustPressed(SlotFallbacks[Index]);
		if (bPressed)
		{
			InputQuickSlot(Index);
			return;
		}
	}
	// Lyra habit: mouse wheel cycles the quick bar.
	if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp))
	{
		InputCycleSlot(false);
	}
	else if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown))
	{
		InputCycleSlot(true);
	}
}

void ALyraShooterEnemy::SetADSActive(bool bActive)
{
	if (bADSActive == bActive)
	{
		return;
	}
	bADSActive = bActive;
	// Native stand-in for GA_ADS (excluded by default, see constructor):
	//  1. camera: DetermineStandaloneCameraMode now returns ADSCameraMode. CM_ThirdPersonADS carries
	//     Lyra.Weapon.SteadyAimingCamera, which ULyraRangedWeaponInstance reads to tighten the spread as the blend lands.
	//  2. IMC_ADS_Speed: lower look sensitivity while aiming.
	APlayerController* PC = Cast<APlayerController>(GetController());
	ULocalPlayer* LP = (PC && PC->IsLocalController()) ? PC->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (Subsystem && !ADSMappingContext.IsNull())
	{
		if (UInputMappingContext* IMC = bActive ? ADSMappingContext.LoadSynchronous() : ADSMappingContext.Get())
		{
			if (bActive)
			{
				Subsystem->AddMappingContext(IMC, 3);
			}
			else
			{
				Subsystem->RemoveMappingContext(IMC);
			}
		}
	}
	// Still let a Blueprint GA bound to InputTag.Weapon.ADS react (e.g. a project-specific replacement for GA_ADS).
	if (ULyraAbilitySystemComponent* LyraASC = Cast<ULyraAbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		const FGameplayTag ADSTag = LyraShooterTags::ADS();
		if (ADSTag.IsValid())
		{
			if (bActive)
			{
				LyraASC->AbilityInputTagPressed(ADSTag);
			}
			else
			{
				LyraASC->AbilityInputTagReleased(ADSTag);
			}
		}
	}
}

void ALyraShooterEnemy::BindStandaloneCamera()
{
	// ULyraCameraComponent::GetCameraView evaluates its camera-mode stack; nothing ever pushes a mode
	// unless DetermineCameraModeDelegate is bound (Lyra binds it from ULyraHeroComponent once the
	// PlayerState / experience init chain completes, which a morph body never runs). With an empty stack
	// the view is the default FLyraCameraModeView: world origin, i.e. "camera stuck underground".
	ULyraCameraComponent* Camera = ULyraCameraComponent::FindCameraComponent(this);
	if (!Camera)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("LyraShooterEnemy %s: no LyraCameraComponent for the morphed player."), *GetName());
		return;
	}
	if (!Camera->DetermineCameraModeDelegate.IsBound())
	{
		Camera->DetermineCameraModeDelegate.BindUObject(this, &ALyraShooterEnemy::DetermineStandaloneCameraMode);
	}
}

TSubclassOf<ULyraCameraMode> ALyraShooterEnemy::DetermineStandaloneCameraMode() const
{
	if (bADSActive && !ADSCameraMode.IsNull())
	{
		if (UClass* ADSClass = ADSCameraMode.LoadSynchronous())
		{
			return ADSClass;
		}
	}
	if (const ULyraPawnExtensionComponent* Ext = ULyraPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		if (const ULyraPawnData* PawnData = Ext->GetPawnData<ULyraPawnData>())
		{
			if (PawnData->DefaultCameraMode)
			{
				return PawnData->DefaultCameraMode;
			}
		}
	}
	return StandaloneCameraMode.IsNull() ? nullptr : StandaloneCameraMode.LoadSynchronous();
}

void ALyraShooterEnemy::GrantStandaloneAbilities()
{
	if (bStandaloneReady)
	{
		return;
	}

	ULyraAbilitySystemComponent* LyraASC = Cast<ULyraAbilitySystemComponent>(GetAbilitySystemComponent());
	if (!LyraASC)
	{
		return;
	}

	if (ULyraPawnExtensionComponent* Ext = ULyraPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		if (const ULyraPawnData* PawnData = StandalonePawnData.LoadSynchronous())
		{
			Ext->SetPawnData(PawnData);
			Ext->InitializeAbilitySystem(LyraASC, this);
			for (const ULyraAbilitySet* Set : PawnData->AbilitySets)
			{
				if (Set)
				{
					FLyraAbilitySet_GrantedHandles Handles;
					Set->GiveToAbilitySystem(LyraASC, &Handles, this);
					GrantedHandles.Add(MoveTemp(Handles));
				}
			}
		}
	}

	bStandaloneReady = true;
}

// ---------------------------------------------------------------------------------------------
// Weapons
// ---------------------------------------------------------------------------------------------

void ALyraShooterEnemy::EnsureDefaultWeapon()
{
	if (EquippedWeapon || DefaultWeaponItem.IsNull())
	{
		return;
	}
	UClass* ItemClass = DefaultWeaponItem.LoadSynchronous();
	if (!ItemClass)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("LyraShooterEnemy %s: DefaultWeaponItem '%s' failed to load."), *GetName(), *DefaultWeaponItem.ToString());
		return;
	}
	GiveWeaponItem(ItemClass, true);
}

ULyraInventoryItemInstance* ALyraShooterEnemy::FindInventoryItemOfDef(TSubclassOf<ULyraInventoryItemDefinition> ItemDef) const
{
	if (!Inventory || !ItemDef)
	{
		return nullptr;
	}
	for (ULyraInventoryItemInstance* Item : Inventory->GetAllItems())
	{
		if (Item && Item->GetItemDef() == ItemDef)
		{
			return Item;
		}
	}
	return nullptr;
}

ULyraInventoryItemInstance* ALyraShooterEnemy::GiveWeaponItem(TSubclassOf<ULyraInventoryItemDefinition> ItemDef, bool bEquip)
{
	if (!HasAuthority() || !Inventory || !ItemDef)
	{
		return nullptr;
	}

	ULyraInventoryItemInstance* Item = FindInventoryItemOfDef(ItemDef);
	if (Item)
	{
		// Same gun again = ammo box.
		RefillSpareAmmo(Item, 2);
	}
	else
	{
		Item = Inventory->AddItemDefinition(ItemDef, 1);
		if (!Item)
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("LyraShooterEnemy %s: AddItemDefinition(%s) failed."), *GetName(), *GetNameSafe(ItemDef));
			return nullptr;
		}
	}

	// Player morph: the quick bar owns equips (W_QuickBar shows the new gun, 1..N switch to it).
	if (USlimeLyraQuickBarComponent* QuickBar = PlayerQuickBar.Get())
	{
		int32 Slot = QuickBar->FindSlotOfItem(Item);
		if (Slot == INDEX_NONE)
		{
			Slot = QuickBar->GetNextFreeItemSlot();
			if (Slot != INDEX_NONE)
			{
				QuickBar->AddItemToSlot(Slot, Item);
			}
		}
		if (bEquip && Slot != INDEX_NONE && QuickBar->GetActiveSlotIndex() != Slot)
		{
			QuickBar->SetActiveSlotIndex(Slot);
		}
		SyncEquippedWeaponFromManager();
		return Item;
	}

	if (bEquip && Item != EquippedWeaponItem)
	{
		EquipInventoryItem(Item);
	}
	return Item;
}

bool ALyraShooterEnemy::EquipInventoryItem(ULyraInventoryItemInstance* Item)
{
	if (!HasAuthority() || !EquipmentManager || !Item)
	{
		return false;
	}
	if (USlimeLyraQuickBarComponent* QuickBar = PlayerQuickBar.Get())
	{
		// Quick bar owns the equipped instance while the player drives; equip by selecting its slot.
		const int32 Slot = QuickBar->FindSlotOfItem(Item);
		if (Slot == INDEX_NONE)
		{
			return false;
		}
		if (QuickBar->GetActiveSlotIndex() != Slot)
		{
			QuickBar->SetActiveSlotIndex(Slot);
		}
		SyncEquippedWeaponFromManager();
		return EquippedWeapon != nullptr;
	}
	const UInventoryFragment_EquippableItem* EquipInfo = Item->FindFragmentByClass<UInventoryFragment_EquippableItem>();
	if (!EquipInfo || !EquipInfo->EquipmentDefinition)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("LyraShooterEnemy %s: item %s has no EquippableItem fragment."), *GetName(), *GetNameSafe(Item->GetItemDef()));
		return false;
	}

	if (EquippedWeapon)
	{
		EquipmentManager->UnequipItem(EquippedWeapon);
		EquippedWeapon = nullptr;
		EquippedWeaponItem = nullptr;
	}

	// Same as ULyraQuickBarComponent::EquipItemInSlot: the equipment's Instigator is the inventory item,
	// which is what GA_Weapon_Fire's ItemTagStack ammo cost reads. Without it CommitAbility fails.
	EquippedWeapon = EquipmentManager->EquipItem(EquipInfo->EquipmentDefinition);
	if (!EquippedWeapon)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("LyraShooterEnemy %s: EquipItem(%s) failed."), *GetName(), *GetNameSafe(EquipInfo->EquipmentDefinition));
		return false;
	}
	EquippedWeapon->SetInstigator(Item);
	EquippedWeaponItem = Item;

	ApplyWeaponDamageOverrides();
	RelinkWeaponAnimLayers();
	UE_LOG(LogSlimeFable, Log, TEXT("LyraShooterEnemy %s: equipped %s via %s (spawned actors=%d)"),
		*GetName(), *GetNameSafe(Item->GetItemDef()), *GetNameSafe(EquipInfo->EquipmentDefinition), EquippedWeapon->GetSpawnedActors().Num());
	return true;
}

int32 ALyraShooterEnemy::RefillSpareAmmo(ULyraInventoryItemInstance* Item, int32 Magazines)
{
	if (!Item || Magazines <= 0)
	{
		return 0;
	}
	const FGameplayTag SpareTag = LyraShooterTags::SpareAmmo();
	const FGameplayTag SizeTag = LyraShooterTags::MagazineSize();
	if (!SpareTag.IsValid() || !SizeTag.IsValid())
	{
		return 0;
	}
	const int32 MagazineSize = FMath::Max(1, Item->GetStatTagStackCount(SizeTag));
	const int32 Rounds = MagazineSize * Magazines;
	Item->AddStatTagStack(SpareTag, Rounds);
	return Rounds;
}

void ALyraShooterEnemy::ApplyWeaponDamageOverrides()
{
	if (ULyraRangedWeaponInstance* Ranged = Cast<ULyraRangedWeaponInstance>(EquippedWeapon))
	{
		Ranged->PlainHitDamage = IsPlayerControlled() ? PlayerGunDamagePerHit : AIGunDamagePerHit;
	}
}

// ---------------------------------------------------------------------------------------------
// Player input (morph)
// ---------------------------------------------------------------------------------------------

void ALyraShooterEnemy::BindStandaloneInput(APlayerController* PC)
{
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	AddStandaloneMappingContext(PC);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		EIC = Cast<UEnhancedInputComponent>(PC->InputComponent);
	}
	if (!EIC)
	{
		return;
	}
	// SetupPlayerInputComponent (fresh component per possess) and ActivateStandaloneForPlayer both land here;
	// binding the same component twice fires every ability press twice.
	if (BoundStandaloneInput.Get() == EIC)
	{
		return;
	}
	BoundStandaloneInput = EIC;

	const ULyraInputConfig* InputConfig = StandaloneInputConfig.LoadSynchronous();
	if (!InputConfig)
	{
		if (const ULyraPawnData* PawnData = StandalonePawnData.LoadSynchronous())
		{
			InputConfig = PawnData->InputConfig;
		}
	}
	if (!InputConfig)
	{
		return;
	}

	if (const UInputAction* MoveAction = InputConfig->FindNativeInputActionForTag(LyraGameplayTags::InputTag_Move, false))
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ALyraShooterEnemy::InputMove);
	}
	if (const UInputAction* LookAction = InputConfig->FindNativeInputActionForTag(LyraGameplayTags::InputTag_Look_Mouse, false))
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ALyraShooterEnemy::InputLook);
	}
	const UInputAction* CrouchAction = InputConfig->FindNativeInputActionForTag(LyraGameplayTags::InputTag_Crouch, true);
	if (!CrouchAction)
	{
		for (const FLyraInputAction& AbilityBind : InputConfig->AbilityInputActions)
		{
			if (AbilityBind.InputAction && AbilityBind.InputTag == LyraGameplayTags::InputTag_Crouch)
			{
				CrouchAction = AbilityBind.InputAction;
				break;
			}
		}
	}
	if (!CrouchAction)
	{
		CrouchAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(
			TEXT("/Game/Input/Actions/IA_Crouch.IA_Crouch"))).LoadSynchronous();
	}
	if (CrouchAction)
	{
		// IMC_Default: C / Left Ctrl. Started matches InputTriggerPressed (Triggered would also work).
		EIC->BindAction(CrouchAction, ETriggerEvent::Started, this, &ALyraShooterEnemy::InputCrouch);
	}
	else
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("LyraShooterEnemy %s: no IA_Crouch to bind; TickCrouch still polls C / Left Ctrl."),
			*GetName());
	}

	const FGameplayTag ADSTag = LyraShooterTags::ADS();
	for (const FLyraInputAction& AbilityBind : InputConfig->AbilityInputActions)
	{
		if (AbilityBind.InputAction && AbilityBind.InputTag.IsValid())
		{
			if (AbilityBind.InputTag == LyraGameplayTags::InputTag_Crouch)
			{
				// No GA_Hero_Crouch; ToggleCrouch is bound above.
				continue;
			}
			if (ADSTag.IsValid() && AbilityBind.InputTag == ADSTag)
			{
				// Right mouse is tap-dodge / hold-ADS, owned by TickRightMouse.
				continue;
			}
			EIC->BindAction(AbilityBind.InputAction, ETriggerEvent::Triggered, this, &ALyraShooterEnemy::InputAbilityPressed, AbilityBind.InputTag);
			EIC->BindAction(AbilityBind.InputAction, ETriggerEvent::Completed, this, &ALyraShooterEnemy::InputAbilityReleased, AbilityBind.InputTag);
		}
	}
}

void ALyraShooterEnemy::InputMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Controller)
	{
		const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
		AddMovementInput(Yaw.RotateVector(FVector::ForwardVector), Axis.Y);
		AddMovementInput(Yaw.RotateVector(FVector::RightVector), Axis.X);
	}
}

void ALyraShooterEnemy::InputLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	// Lyra's IMC_Default already negates mouse Y for a project with bEnableLegacyInputScales=False.
	// SlimeFable keeps the legacy scales on (the slime needs them), and the legacy InputPitchScale is -2.5,
	// so the same value would flip twice and mouse-up would look down. Undo the second flip here.
	float Pitch = Axis.Y;
	if (GetDefault<UInputSettings>()->bEnableLegacyInputScales)
	{
		if (const APlayerController* PC = Cast<APlayerController>(Controller))
		{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			const float LegacyPitchScale = PC->GetDeprecatedInputPitchScale();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			if (LegacyPitchScale < 0.f)
			{
				Pitch = -Pitch;
			}
		}
	}
	AddControllerPitchInput(Pitch);
}

void ALyraShooterEnemy::EnsurePlayerCrouch()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move)
	{
		return;
	}
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	float StandingHalf = 90.f;
	if (const UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		StandingHalf = Capsule->GetUnscaledCapsuleHalfHeight();
	}
	if (StandingHalf <= Move->GetCrouchedHalfHeight())
	{
		Move->SetCrouchedHalfHeight(StandingHalf * 0.7f);
	}
}

void ALyraShooterEnemy::TryToggleCrouch()
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	// Enhanced Input Started and TickCrouch can see the same press; two toggles = stand back up.
	if (LastCrouchToggleTime >= 0.f && (Now - LastCrouchToggleTime) < 0.08f)
	{
		return;
	}
	LastCrouchToggleTime = Now;
	ToggleCrouch();
}

void ALyraShooterEnemy::InputCrouch(const FInputActionValue& Value)
{
	(void)Value;
	// Same as ULyraHeroComponent::Input_Crouch; the morph body never gets a hero component.
	TryToggleCrouch();
}

void ALyraShooterEnemy::InputAbilityPressed(FGameplayTag InputTag)
{
	if (ULyraAbilitySystemComponent* LyraASC = Cast<ULyraAbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		LyraASC->AbilityInputTagPressed(InputTag);
	}
}

void ALyraShooterEnemy::InputAbilityReleased(FGameplayTag InputTag)
{
	if (ULyraAbilitySystemComponent* LyraASC = Cast<ULyraAbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		LyraASC->AbilityInputTagReleased(InputTag);
	}
}

// ---------------------------------------------------------------------------------------------
// AI
// ---------------------------------------------------------------------------------------------

void ALyraShooterEnemy::TickSimpleChase(float DeltaSeconds)
{
	if (Cast<APlayerController>(GetController()))
	{
		return;
	}
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	AAIController* AI = Cast<AAIController>(GetController());
	if (!PlayerPawn || PlayerPawn == this)
	{
		SetAIFiring(false);
		SetAIFacingMode(false);
		if (AI)
		{
			AI->ClearFocus(EAIFocusPriority::Gameplay);
		}
		return;
	}
	const FVector ToPlayer = PlayerPawn->GetActorLocation() - GetActorLocation();
	const float Distance = ToPlayer.Size();
	if (ToPlayer.Size2D() > AIPreferredRange)
	{
		AddMovementInput(ToPlayer.GetSafeNormal2D(), 1.f);
	}
	const FVector AimAt = PlayerPawn->GetActorLocation() + FVector(0.f, 0.f, AIAimHeightOffset);
	if (AI)
	{
		// Lyra's ranged targeting for AI starts at BaseEyeHeight and fires along the control rotation.
		// Focal point makes AAIController::UpdateControlRotation keep aiming here every tick instead of
		// resetting the control rotation to the pawn's facing.
		AI->SetFocalPoint(AimAt, EAIFocusPriority::Gameplay);
		const FVector EyeLoc = GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight);
		AI->SetControlRotation((AimAt - EyeLoc).Rotation());
	}
	// Engaged = inside fire range: square up to the target even while strafing / standing still.
	const bool bEngaged = Distance <= AIFireRange;
	SetAIFacingMode(bEngaged);
	if (bEngaged)
	{
		FaceTarget(AimAt, DeltaSeconds);
	}
	TickAIWeapon(DeltaSeconds, PlayerPawn, Distance);
}

void ALyraShooterEnemy::SetAIFacingMode(bool bFaceTarget)
{
	if (bAIFacingTarget == bFaceTarget)
	{
		return;
	}
	bAIFacingTarget = bFaceTarget;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move)
	{
		return;
	}
	if (bFaceTarget)
	{
		// Orient-to-movement (hero BP default) points the body along the velocity, so a soldier walking
		// toward the slime looks right but one holding position at AIPreferredRange never turns. Let the
		// movement component rotate toward the control rotation instead; FaceTarget drives the yaw.
		Move->bOrientRotationToMovement = false;
		Move->bUseControllerDesiredRotation = false;
		bUseControllerRotationYaw = false;
	}
	else
	{
		Move->bOrientRotationToMovement = bSavedOrientToMovement;
		Move->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
		bUseControllerRotationYaw = bSavedUseControllerRotationYaw;
	}
}

void ALyraShooterEnemy::FaceTarget(const FVector& TargetLocation, float DeltaSeconds)
{
	const FVector Flat = (TargetLocation - GetActorLocation()).GetSafeNormal2D();
	if (Flat.IsNearlyZero())
	{
		return;
	}
	const float TargetYaw = Flat.Rotation().Yaw;
	FRotator Rot = GetActorRotation();
	Rot.Yaw = AIFaceTargetYawRate > 0.f
		? FMath::FixedTurn(Rot.Yaw, TargetYaw, AIFaceTargetYawRate * DeltaSeconds)
		: TargetYaw;
	Rot.Pitch = 0.f;
	Rot.Roll = 0.f;
	SetActorRotation(Rot);
}

void ALyraShooterEnemy::TickAIWeapon(float DeltaSeconds, APawn* Target, float Distance)
{
	AAIController* AI = Cast<AAIController>(GetController());
	const bool bCanShoot = EquippedWeapon && AI && Target && Distance <= AIFireRange && AI->LineOfSightTo(Target);
#if !UE_BUILD_SHIPPING
	AIDebugLogClock += DeltaSeconds;
	if (AIDebugLogClock >= 2.f)
	{
		AIDebugLogClock = 0.f;
		UE_LOG(LogSlimeFable, Verbose, TEXT("LyraShooterEnemy %s: AI weapon tick weapon=%d ai=%s dist=%.0f los=%d canShoot=%d"),
			*GetName(), EquippedWeapon != nullptr, *GetNameSafe(GetController()), Distance,
			(AI && Target) ? AI->LineOfSightTo(Target) : -1, bCanShoot);
	}
#endif
	if (!bCanShoot)
	{
		SetAIFiring(false);
		AIBurstClock = 0.f;
		AILastTelegraphCycle = -1;
		return;
	}

	AIBurstClock += DeltaSeconds;
	// Cycle = [telegraph][burst][pause]. The telegraph opens the slime's perfect-dodge window
	// (USlimeDodgeComponent::PerfectWindow, 1 s) before the first round leaves the barrel, so RMB timed
	// on the raise beats the burst the same way it beats a melee wind-up.
	const float Telegraph = FMath::Max(0.f, AIAimTelegraphSeconds);
	const float Cycle = FMath::Max(0.05f, Telegraph + AIBurstSeconds + AIBurstPauseSeconds);
	const int32 CycleIndex = FMath::FloorToInt(AIBurstClock / Cycle);
	const float Phase = AIBurstClock - CycleIndex * Cycle;
	if (CycleIndex != AILastTelegraphCycle)
	{
		AILastTelegraphCycle = CycleIndex;
		USlimeDodgeComponent::NotifyPlayerIncomingAttack(this, this);
	}
	SetAIFiring(Phase >= Telegraph && Phase < Telegraph + AIBurstSeconds);

	if (bAIInfiniteAmmo && EquippedWeaponItem)
	{
		const FGameplayTag SpareTag = LyraShooterTags::SpareAmmo();
		const FGameplayTag SizeTag = LyraShooterTags::MagazineSize();
		if (SpareTag.IsValid() && SizeTag.IsValid())
		{
			const int32 MagazineSize = FMath::Max(1, EquippedWeaponItem->GetStatTagStackCount(SizeTag));
			if (EquippedWeaponItem->GetStatTagStackCount(SpareTag) < MagazineSize)
			{
				EquippedWeaponItem->AddStatTagStack(SpareTag, MagazineSize * 3);
			}
		}
	}
}

void ALyraShooterEnemy::SetAIFiring(bool bFire)
{
	if (bAIFiring == bFire)
	{
		return;
	}
	bAIFiring = bFire;
	ULyraAbilitySystemComponent* LyraASC = Cast<ULyraAbilitySystemComponent>(GetAbilitySystemComponent());
	if (!LyraASC)
	{
		return;
	}
	UE_LOG(LogSlimeFable, Verbose, TEXT("LyraShooterEnemy %s: AI fire %s"), *GetName(), bFire ? TEXT("start") : TEXT("stop"));
	// Rifle = InputTag.Weapon.FireAuto (WhileInputActive); pistol / shotgun = InputTag.Weapon.Fire. Press both;
	// only the granted one matches. ProcessLyraAbilityInput in Tick turns held → activation.
	for (const FGameplayTag& Tag : { LyraShooterTags::FireAuto(), LyraShooterTags::Fire() })
	{
		if (!Tag.IsValid())
		{
			continue;
		}
		if (bFire)
		{
			LyraASC->AbilityInputTagPressed(Tag);
		}
		else
		{
			LyraASC->AbilityInputTagReleased(Tag);
		}
	}
}
