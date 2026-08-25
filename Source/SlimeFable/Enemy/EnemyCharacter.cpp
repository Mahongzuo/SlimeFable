// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyCharacter.h"

#include "AbilitySystemComponent.h"
#include "EnemyAttributeSet.h"
#include "EnemyGameplayEffects.h"
#include "Abilities/EnemySkillAbility.h"
#include "SlimeEnemyGameplayTags.h"

#include "EnemyAllyAIController.h"
#include "EnemyFighter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimationAsset.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "EnemyCombatComponent.h"
#include "EnemyPresenceSubsystem.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "SlimeFableCharacter.h"
#include "Slime/SlimeMorphComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Quest/QuestObjectiveComponent.h"
#include "Quest/QuestSouvenirPickup.h"
#include "Quest/QuestSubsystem.h"
#include "SlimeHealthComponent.h"
#include "SlimeStatusComponent.h"
#include "SlimeLockOnComponent.h"
#include "SlimeCombatTypes.h"
#include "SlimeWorldHealthBar.h"
#include "Inventory/SlimeItemDefinition.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	if (USkeletalMeshComponent* CharMesh = GetMesh())
	{
		CharMesh->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
		CharMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		CharMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CharMesh->SetHiddenInGame(true);
		CharMesh->SetVisibility(false);
	}

	PlaceholderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderMesh"));
	PlaceholderMesh->SetupAttachment(GetCapsuleComponent());
	PlaceholderMesh->SetMobility(EComponentMobility::Movable);
	PlaceholderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderMesh->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PlaceholderMesh->SetStaticMesh(CubeMesh.Object);
		PlaceholderMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.4f));
	}

	Health = CreateDefaultSubobject<USlimeHealthComponent>(TEXT("Health"));
	Health->Team = ESlimeTeam::Enemy;
	Health->bDestroyOnDeath = false;
	Health->bRegenOnDeath = false;
	Health->MaxHP = 200.f;
	Status = CreateDefaultSubobject<USlimeStatusComponent>(TEXT("Status"));
	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("EnemyAbilitySystem"));
	AbilitySystem->SetIsReplicated(false);
	EnemyAttributes = CreateDefaultSubobject<UEnemyAttributeSet>(TEXT("EnemyAttributes"));
	DeathDissolveNiagara = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Dark_Solo_Impact.NS_Dark_Solo_Impact")));
	DeathDissolveMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/_Slime/FX/M_EnemyDeathDissolve.M_EnemyDeathDissolve")));
	HitFlashMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/_Slime/FX/M_EnemyHitFlash.M_EnemyHitFlash")));
	LightningHitOverlay = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/NiagaraExamples/Materials/MI_Mesh_Overlay_TeslaCoil_Player.MI_Mesh_Overlay_TeslaCoil_Player")));
	WindHitOverlay = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/_Slime/FX/MI_EnemyHitOverlay_Wind.MI_EnemyHitOverlay_Wind")));
	SouvenirDropClass = TSoftClassPtr<AActor>(
		FSoftObjectPath(TEXT("/Game/_Slime/Days/08/0815/Y1945/Actors/BP_0815_Souvenir_1945.BP_0815_Souvenir_1945_C")));

	Combat = CreateDefaultSubobject<UEnemyCombatComponent>(TEXT("Combat"));
	Objective = CreateDefaultSubobject<UQuestObjectiveComponent>(TEXT("Objective"));

	HealthBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBar->SetupAttachment(RootComponent);
	HealthBar->SetRelativeLocation(FVector(0.f, 0.f, HealthBarZOffset));
	HealthBar->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBar->SetDrawAtDesiredSize(false);
	HealthBar->SetDrawSize(FVector2D(110.f, 14.f));
	HealthBar->SetPivot(FVector2D(0.5f, 1.f));
	HealthBar->SetWidgetClass(USlimeWorldHealthBar::StaticClass());
	HealthBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = 420.f;
		Move->bOrientRotationToMovement = true;
		Move->RotationRate = FRotator(0.f, 480.f, 0.f);
	}
	bUseControllerRotationYaw = false;
}

void AEnemyCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildMeshParts();
	ApplyHealthBarOffset();
	EnsureDefaultQuestIds();
	EnsureDefaultDisplayName();
}

FText AEnemyCharacter::GetResolvedDisplayName() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}
	return FText::FromString(TEXT("敌人"));
}

void AEnemyCharacter::EnsureDefaultDisplayName()
{
	if (!DisplayName.IsEmpty())
	{
		return;
	}

	const FString ClassName = GetClass()->GetName();
	if (ClassName.Contains(TEXT("Watchdog")) || ClassName.Contains(TEXT("Akita")))
	{
		DisplayName = FText::FromString(TEXT("看门狗"));
	}
	else if (ClassName.Contains(TEXT("Samurai")))
	{
		DisplayName = FText::FromString(TEXT("武士"));
	}
	else if (ClassName.Contains(TEXT("Gunner")))
	{
		DisplayName = FText::FromString(TEXT("机枪手"));
	}
	else if (ClassName.Contains(TEXT("Emperor")))
	{
		DisplayName = FText::FromString(TEXT("天皇"));
	}
}

void AEnemyCharacter::EnsureDefaultQuestIds()
{
	if (!Objective || !Objective->ChapterId.IsNone() || !Objective->QuestId.IsNone() || !Objective->BranchId.IsNone())
	{
		return;
	}

	const FString ClassName = GetClass()->GetName();
	auto Fill = [this](const TCHAR* Chapter, const TCHAR* Quest, const TCHAR* Branch, const TCHAR* Verb)
	{
		Objective->ChapterId = Chapter;
		Objective->QuestId = Quest;
		Objective->BranchId = Branch;
		if (Objective->PromptVerb.IsEmpty())
		{
			Objective->PromptVerb = FText::FromString(Verb);
		}
	};

	if (ClassName.Contains(TEXT("Watchdog")) || ClassName.Contains(TEXT("Akita")))
	{
		Fill(TEXT("1945"), TEXT("Broadcast"), TEXT("Watchdog"), TEXT("打败"));
	}
	else if (ClassName.Contains(TEXT("Samurai")))
	{
		Fill(TEXT("1945"), TEXT("Broadcast"), TEXT("Samurai"), TEXT("打败"));
	}
	else if (ClassName.Contains(TEXT("Gunner")))
	{
		Fill(TEXT("1945"), TEXT("Broadcast"), TEXT("Gunner"), TEXT("打败"));
	}
	else if (ClassName.Contains(TEXT("Emperor")))
	{
		Fill(TEXT("1945"), TEXT("Broadcast"), TEXT("Emperor"), TEXT("打败"));
	}
}

void AEnemyCharacter::ApplyHealthBarOffset()
{
	if (!HealthBar)
	{
		return;
	}

	UpdateHudAnchorCache();
	const FVector Desired(0.f, 0.f, HudAnchorRelZ + HealthBarZOffset);
	if (!HealthBar->GetRelativeLocation().Equals(Desired, 0.05f))
	{
		HealthBar->SetRelativeLocation(Desired);
	}
}

void AEnemyCharacter::RefreshWorldHealthBarVisibility()
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	AActor* Locked = nullptr;
	if (Player)
	{
		if (const USlimeLockOnComponent* Lock = Player->FindComponentByClass<USlimeLockOnComponent>())
		{
			Locked = Lock->GetLockedTarget();
		}
	}
	RefreshWorldHealthBarVisibility(Player, Locked);
}

void AEnemyCharacter::RefreshWorldHealthBarVisibility(const APawn* Player, const AActor* LockedTarget)
{
	if (!HealthBar)
	{
		return;
	}

	ApplyHealthBarOffset();

	if (bDevourLocked || bDevouredDeath || bDeathSequence)
	{
		HealthBar->SetHiddenInGame(true);
		HealthBar->SetVisibility(false);
		return;
	}

	bool bShow = Health && Health->IsAlive()
		&& Presence != EEnemyPresence::Sleep
		&& Presence != EEnemyPresence::Despawned
		&& LockedTarget != this;

	if (bShow)
	{
		if (Player)
		{
			bShow = FVector::DistSquared(Player->GetActorLocation(), GetActorLocation())
				<= FMath::Square(HealthBarVisibleRange);
		}
		else
		{
			bShow = false;
		}
	}

	HealthBar->SetHiddenInGame(!bShow);
	HealthBar->SetVisibility(bShow);
}

void AEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bMorphTarget)
	{
		UpdateMorphSafeTransform();
		return;
	}
	TickOutOfCombatReset(DeltaSeconds);
	TickPoiseRegen(DeltaSeconds);
}

void AEnemyCharacter::UpdateMorphSafeTransform()
{
	if (!bMorphTarget)
	{
		return;
	}

	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move || !Move->IsMovingOnGround())
	{
		return;
	}

	const FTransform CurrentTransform = GetActorTransform();
	if (!CurrentTransform.GetLocation().ContainsNaN())
	{
		MorphSafeTransform = CurrentTransform;
		bHasMorphSafeTransform = true;
	}
}

bool AEnemyCharacter::IsInCombat() const
{
	return Combat && Combat->IsAttacking();
}

void AEnemyCharacter::OnRestoredToSpawn()
{
}

void AEnemyCharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	if (bMorphTarget && Health && Health->IsAlive())
	{
		const FTransform RecoveryTransform = bHasMorphSafeTransform ? MorphSafeTransform : SpawnTransform;
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->StopMovementImmediately();
			Move->Velocity = FVector::ZeroVector;
		}
		TeleportTo(RecoveryTransform.GetLocation(), RecoveryTransform.Rotator(), false, true);

		if (USlimeMorphComponent* MorphComp = MorphMaster.Get()
				? MorphMaster->FindComponentByClass<USlimeMorphComponent>()
				: nullptr)
		{
			MorphComp->ForceUnmorph(false);
		}
		return;
	}

	if (Health && Health->IsAlive())
	{
		RestoreToSpawn();
		return;
	}
	Super::FellOutOfWorld(DmgType);
}

void AEnemyCharacter::RestoreToSpawn()
{
	if (Health && !Health->IsAlive())
	{
		return;
	}

	OutOfCombatSeconds = 0.f;
	TeleportTo(SpawnTransform.GetLocation(), SpawnTransform.Rotator(), false, true);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->Velocity = FVector::ZeroVector;
	}

	if (Health)
	{
		Health->ResetHP();
		if (!bPhantomInstance && DebugStartHealthPercent > KINDA_SMALL_NUMBER)
		{
			Health->CurrentHP = FMath::Clamp(Health->MaxHP * DebugStartHealthPercent, 1.f, Health->MaxHP);
			Health->OnHealthChanged.Broadcast(Health->CurrentHP, Health->MaxHP);
		}
	}
	if (Combat)
	{
		Combat->InterruptCombat();
	}

	OnRestoredToSpawn();
	bHudAnchorCached = false;
}

void AEnemyCharacter::TickOutOfCombatReset(float DeltaSeconds)
{
	if (bMorphTarget)
	{
		return;
	}

	if (Health && !Health->IsAlive())
	{
		return;
	}

	const FVector Location = GetActorLocation();
	if (Location.Z < SpawnTransform.GetLocation().Z - VoidResetDepth)
	{
		RestoreToSpawn();
		return;
	}

	if (IsInCombat())
	{
		OutOfCombatSeconds = 0.f;
		return;
	}

	OutOfCombatSeconds += DeltaSeconds;

	if (bSuppressOutOfCombatReset)
	{
		return;
	}

	const float DistFromSpawn = FVector::Dist2D(Location, SpawnTransform.GetLocation());
	const float HorizSpeed = GetVelocity().Size2D();
	constexpr float StuckSpeed = 15.f;
	constexpr float StuckIdleSeconds = 2.f;
	const bool bStuck = DistFromSpawn > StuckResetDistance
		&& HorizSpeed < StuckSpeed
		&& OutOfCombatSeconds >= StuckIdleSeconds;

	if (bStuck || (OutOfCombatResetSeconds > 0.f && OutOfCombatSeconds >= OutOfCombatResetSeconds))
	{
		RestoreToSpawn();
	}
}

void AEnemyCharacter::BeginPlay()
{
	SpawnTransform = GetActorTransform();
	const FString RuntimeClassName = GetClass()->GetName();
	if (RuntimeClassName.Contains(TEXT("Watchdog")))
	{
		CombatRole = EEnemyCombatRole::Chaser;
	}
	else if (RuntimeClassName.Contains(TEXT("Samurai")))
	{
		CombatRole = EEnemyCombatRole::Duelist;
	}
	else if (RuntimeClassName.Contains(TEXT("Gunner")))
	{
		CombatRole = EEnemyCombatRole::Suppressor;
	}
	else if (RuntimeClassName.Contains(TEXT("Emperor")))
	{
		CombatRole = EEnemyCombatRole::Commander;
	}
	if (bMorphTarget)
	{
		MorphSafeTransform = SpawnTransform;
		bHasMorphSafeTransform = true;
	}

	if (Health)
	{
		Health->Team = (bPhantomInstance || bMorphTarget) ? ESlimeTeam::Player : ESlimeTeam::Enemy;
		Health->bDestroyOnDeath = false;
		Health->bRegenOnDeath = false;
		Health->MaxHP = MaxHP;
		if (bPhantomInstance)
		{
			Health->MaxHP = FMath::Max(MaxHP * 0.35f, 20.f);
		}
		if (SavedHP > 0.f)
		{
			Health->CurrentHP = FMath::Clamp(SavedHP, 1.f, Health->MaxHP);
			Health->OnHealthChanged.Broadcast(Health->CurrentHP, Health->MaxHP);
		}
		else
		{
			Health->ResetHP();
		}
		Health->OnDied.AddDynamic(this, &AEnemyCharacter::HandleDied);
	}

	Super::BeginPlay();
	if (!bPhantomInstance)
	{
		EnsureDefaultQuestIds();
	}
	else if (Objective)
	{
		Objective->SetConsumed(true);
	}
	EnsureDefaultDisplayName();
	RebuildMeshParts();
	UpdateHudAnchorCache();
	if (!bPhantomInstance)
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (const UQuestSubsystem* Quests = GI->GetSubsystem<UQuestSubsystem>())
				{
					ApplyWeekDifficulty(Quests->GetWeekIndex());
				}
			}
		}
	}

	if (HealthBar)
	{
		HealthBar->InitWidget();
		if (USlimeWorldHealthBar* Bar = Cast<USlimeWorldHealthBar>(HealthBar->GetWidget()))
		{
			Bar->SetHealth(Health);
		}
		if (bPhantomInstance)
		{
			HealthBar->SetVisibility(false);
			HealthBar->SetHiddenInGame(true);
		}
		else
		{
			RefreshWorldHealthBarVisibility();
		}
	}

	if (bPhantomInstance)
	{
		ApplyPhantomVisuals();
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				PhantomLifeTimer, this, &AEnemyCharacter::BeginPhantomExpire, FMath::Max(PhantomLifeSeconds, 0.2f), false);
		}
	}
	else if (!bMorphTarget)
	{
		if (UWorld* World = GetWorld())
		{
			if (UEnemyPresenceSubsystem* PresenceSys = World->GetSubsystem<UEnemyPresenceSubsystem>())
			{
				PresenceSys->RegisterEnemy(this);
				bPresenceRegistered = true;
			}
		}
	}
	ApplyLabDummyFlags();
	ApplyHealthOverridesAfterBeginPlay();
	InitAbilitySystem();
}

void AEnemyCharacter::InitAbilitySystem()
{
	if (!AbilitySystem)
	{
		return;
	}
	AbilitySystem->InitAbilityActorInfo(this, this);

	if (EnemyAttributes)
	{
		const float ResolvedMaxHP = Health ? Health->MaxHP : MaxHP;
		EnemyAttributes->InitMaxHealth(FMath::Max(ResolvedMaxHP, 1.f));
		EnemyAttributes->InitHealth(Health ? Health->CurrentHP : ResolvedMaxHP);
		EnemyAttributes->InitMaxPoise(FMath::Max(MaxPoise, 1.f));
		EnemyAttributes->InitPoise(FMath::Max(MaxPoise, 1.f));
		EnemyAttributes->InitGuard(Guard);
		EnemyAttributes->InitMoveSpeed(GetCharacterMovement() ? GetCharacterMovement()->MaxWalkSpeed : 420.f);
		EnemyAttributes->InitDamagePower(1.f);
		EnemyAttributes->InitPhaseIndex(1.f);
	}

	// Role tag lets the encounter director and StateTree query enemies by job.
	AbilitySystem->AddLooseGameplayTag(SlimeEnemyTags::RoleTag(static_cast<uint8>(CombatRole)));
	AbilitySystem->GiveAbility(FGameplayAbilitySpec(UEnemySkillAbility::StaticClass(), 1, INDEX_NONE, this));

	if (Health && !Health->OnHealthChanged.IsAlreadyBound(this, &AEnemyCharacter::HandleHealthFacadeChanged))
	{
		Health->OnHealthChanged.AddDynamic(this, &AEnemyCharacter::HandleHealthFacadeChanged);
	}
}

void AEnemyCharacter::ApplyLabDummyFlags()
{
	static const FName DummyTag(TEXT("SlimeLabDevourDummy"));
	static const FName PassiveTag(TEXT("SlimeLabPassive"));
	const bool bLabDummy = ActorHasTag(DummyTag) || ActorHasTag(PassiveTag);
	if (bLabDummy)
	{
		bSuppressOutOfCombatReset = true;
	}
	if (!ActorHasTag(PassiveTag))
	{
		return;
	}
	bHarmless = true;
	if (AEnemyFighter* Fighter = Cast<AEnemyFighter>(this))
	{
		Fighter->bPassive = true;
		Fighter->bWanderWhenIdle = false;
	}
}

void AEnemyCharacter::ApplyHealthOverridesAfterBeginPlay()
{
	if (!Health || bPhantomInstance)
	{
		return;
	}
	if (SavedHP > 0.f)
	{
		Health->CurrentHP = FMath::Clamp(SavedHP, 1.f, Health->MaxHP);
		Health->OnHealthChanged.Broadcast(Health->CurrentHP, Health->MaxHP);
		return;
	}
	if (DebugStartHealthPercent > KINDA_SMALL_NUMBER)
	{
		Health->CurrentHP = FMath::Clamp(Health->MaxHP * DebugStartHealthPercent, 1.f, Health->MaxHP);
		Health->OnHealthChanged.Broadcast(Health->CurrentHP, Health->MaxHP);
	}
}

void AEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhantomLifeTimer);
	}
	if (bPresenceRegistered)
	{
		if (UWorld* World = GetWorld())
		{
			if (UEnemyPresenceSubsystem* PresenceSys = World->GetSubsystem<UEnemyPresenceSubsystem>())
			{
				PresenceSys->UnregisterEnemy(this);
			}
		}
		bPresenceRegistered = false;
	}
	Super::EndPlay(EndPlayReason);
}

void AEnemyCharacter::ClearGeneratedParts()
{
	for (USceneComponent* Comp : GeneratedParts)
	{
		if (Comp)
		{
			Comp->DestroyComponent();
		}
	}
	GeneratedParts.Reset();
}

USceneComponent* AEnemyCharacter::ResolveAttachParent(const FEnemyMeshPart& Part) const
{
	if (Part.AttachToPart != NAME_None)
	{
		for (USceneComponent* Comp : GeneratedParts)
		{
			if (Comp && Comp->GetFName() == Part.AttachToPart)
			{
				return Comp;
			}
		}
	}
	if (GetMesh() && GetMesh()->GetSkeletalMeshAsset())
	{
		return GetMesh();
	}
	return GetCapsuleComponent();
}

void AEnemyCharacter::ApplyPlaceholderVisual()
{
	const bool bHasPrimary = PrimarySkeletalMesh.LoadSynchronous() != nullptr;
	bool bHasParts = false;
	for (const FEnemyMeshPart& Part : MeshParts)
	{
		if (!Part.SkeletalMesh.IsNull() || !Part.StaticMesh.IsNull())
		{
			bHasParts = true;
			break;
		}
	}
	if (PlaceholderMesh)
	{
		PlaceholderMesh->SetVisibility(!bHasPrimary && !bHasParts);
		PlaceholderMesh->SetHiddenInGame(bHasPrimary || bHasParts);
	}
}

void AEnemyCharacter::FitCapsuleToMesh()
{
	if (!bAutoFitCapsuleToMesh)
	{
		return;
	}
	USkeletalMeshComponent* Skel = GetMesh();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Skel || !Capsule)
	{
		return;
	}
	USkeletalMesh* MeshAsset = Skel->GetSkeletalMeshAsset();
	if (!MeshAsset)
	{
		return;
	}

	const FBoxSphereBounds Bounds = MeshAsset->GetBounds();
	const float Height = FMath::Max(Bounds.BoxExtent.Z * 2.f, 80.f);
	const float Radius = FMath::Clamp(FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y), 18.f, 80.f);
	const float HalfHeight = FMath::Clamp(Height * 0.5f, Radius + 8.f, 180.f);
	Capsule->SetCapsuleSize(Radius, HalfHeight);
	FVector Rel = Skel->GetRelativeLocation();
	Rel.Z = -HalfHeight;
	Skel->SetRelativeLocation(Rel);
	bHudAnchorCached = false;
}

void AEnemyCharacter::RebuildMeshParts()
{
	ClearGeneratedParts();

	if (USkeletalMesh* Primary = PrimarySkeletalMesh.LoadSynchronous())
	{
		GetMesh()->SetSkeletalMesh(Primary);
		GetMesh()->SetHiddenInGame(false);
		GetMesh()->SetVisibility(true);
		if (UClass* AnimClass = PrimaryAnimClass.LoadSynchronous())
		{
			GetMesh()->SetAnimInstanceClass(AnimClass);
		}
		ApplySingleNodeAnimModeIfNeeded();
	}
	else
	{
		GetMesh()->SetSkeletalMesh(nullptr);
		GetMesh()->SetHiddenInGame(true);
		GetMesh()->SetVisibility(false);
	}

	for (int32 Index = 0; Index < MeshParts.Num(); ++Index)
	{
		const FEnemyMeshPart& Part = MeshParts[Index];
		const FName CompName = Part.PartName.IsNone()
			? FName(*FString::Printf(TEXT("EnemyPart_%d"), Index))
			: Part.PartName;

		USceneComponent* Parent = ResolveAttachParent(Part);
		if (!Parent)
		{
			continue;
		}

		if (Part.Kind == EEnemyMeshPartKind::Skeletal)
		{
			if (USkeletalMesh* Skel = Part.SkeletalMesh.LoadSynchronous())
			{
				USkeletalMeshComponent* Comp = NewObject<USkeletalMeshComponent>(this, CompName);
				Comp->SetupAttachment(Parent, Part.AttachSocket);
				Comp->SetSkeletalMesh(Skel);
				Comp->SetRelativeTransform(Part.RelativeTransform);
				Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				if (UMaterialInterface* Mat = Part.MaterialOverride.LoadSynchronous())
				{
					Comp->SetMaterial(0, Mat);
				}
				Comp->RegisterComponent();
				GeneratedParts.Add(Comp);
			}
		}
		else
		{
			if (UStaticMesh* Stat = Part.StaticMesh.LoadSynchronous())
			{
				UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, CompName);
				Comp->SetupAttachment(Parent, Part.AttachSocket);
				Comp->SetStaticMesh(Stat);
				Comp->SetRelativeTransform(Part.RelativeTransform);
				Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				if (UMaterialInterface* Mat = Part.MaterialOverride.LoadSynchronous())
				{
					Comp->SetMaterial(0, Mat);
				}
				Comp->RegisterComponent();
				GeneratedParts.Add(Comp);
			}
		}
	}

	ApplyPlaceholderVisual();
	FitCapsuleToMesh();
}

bool AEnemyCharacter::CanBeLockedOn() const
{
	if (bPhantomInstance || bDevourLocked)
	{
		return false;
	}
	if (Presence == EEnemyPresence::Sleep || Presence == EEnemyPresence::Despawned)
	{
		return false;
	}
	return Health && Health->IsAlive();
}

void AEnemyCharacter::SetDevourLocked(bool bLocked)
{
	bDevourLocked = bLocked;
}

void AEnemyCharacter::PlayHitFlash()
{
	if (bDeathSequence || DeathDissolveElapsed > 0.f)
	{
		return;
	}
	// Prefer keeping the elemental aura tint when re-flashing from damage.
	if (bAuraFlashActive && HitFlashRemaining > HitFlashDuration)
	{
		return;
	}

	UMaterialInterface* FlashMat = HitFlashMaterial.LoadSynchronous();
	if (!FlashMat)
	{
		return;
	}

	// Specialty aura overlays do not expose HitFlash — leave them alone during damage pulses.
	if (bAuraFlashActive && !bAuraOverlayUsesHitFlashParams)
	{
		return;
	}

	if (!HitFlashMID || HitFlashMID->Parent != FlashMat)
	{
		HitFlashMID = UMaterialInstanceDynamic::Create(FlashMat, this);
	}
	if (!HitFlashMID)
	{
		return;
	}

	ApplyHitFlashToMesh(GetMesh());
	for (USceneComponent* Part : GeneratedParts)
	{
		ApplyHitFlashToMesh(Cast<UMeshComponent>(Part));
	}
	if (PlaceholderMesh && PlaceholderMesh->IsVisible())
	{
		ApplyHitFlashToMesh(PlaceholderMesh);
	}

	const UWorld* World = GetWorld();
	// If an aura flash is already running, only pulse intensity — do not reset to short red hit.
	if (bAuraFlashActive)
	{
		DriveAuraOverlayIntensity(1.f);
		return;
	}

	bAuraOverlayUsesHitFlashParams = true;
	HitFlashStartTime = World ? World->GetTimeSeconds() : 0.f;
	ActiveFlashDuration = HitFlashDuration;
	ActiveFlashFrequency = HitFlashFrequency;
	bAuraFlashActive = false;
	HitFlashRemaining = ActiveFlashDuration;
	DriveAuraOverlayIntensity(1.f);

	if (World)
	{
		World->GetTimerManager().ClearTimer(HitFlashTimer);
		World->GetTimerManager().SetTimer(
			HitFlashTimer,
			this,
			&AEnemyCharacter::TickHitFlash,
			0.016f,
			true);
	}
}

void AEnemyCharacter::PlayElementHitFlash(FLinearColor FlashColor)
{
	PlayElementAuraFlashByColor(FlashColor, 8.f);
}

UMaterialInterface* AEnemyCharacter::ResolveElementHitOverlay(ESlimeElement Element) const
{
	switch (Element)
	{
	case ESlimeElement::Lightning:
		if (UMaterialInterface* Lightning = LightningHitOverlay.LoadSynchronous())
		{
			return Lightning;
		}
		break;
	case ESlimeElement::Wind:
		if (UMaterialInterface* Wind = WindHitOverlay.LoadSynchronous())
		{
			return Wind;
		}
		break;
	default:
		break;
	}
	return HitFlashMaterial.LoadSynchronous();
}

void AEnemyCharacter::BeginAuraOverlay(UMaterialInterface* FlashMat, bool bUsesHitFlashParams, float Duration)
{
	if (!FlashMat)
	{
		return;
	}

	if (!HitFlashMID || HitFlashMID->Parent != FlashMat)
	{
		HitFlashMID = UMaterialInstanceDynamic::Create(FlashMat, this);
	}
	if (!HitFlashMID)
	{
		return;
	}

	bAuraOverlayUsesHitFlashParams = bUsesHitFlashParams;
	if (!bUsesHitFlashParams)
	{
		float Mul = 2.f;
		FlashMat->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Opacity Multiplier")), Mul);
		AuraOverlayOpacityMul = Mul > KINDA_SMALL_NUMBER ? Mul : 2.f;
	}

	const UWorld* World = GetWorld();
	HitFlashStartTime = World ? World->GetTimeSeconds() : 0.f;
	ActiveFlashDuration = Duration;
	bAuraFlashActive = true;
	HitFlashRemaining = ActiveFlashDuration;
	DriveAuraOverlayIntensity(1.f);
	ApplyHitFlashToAllMeshes();

	if (World)
	{
		World->GetTimerManager().ClearTimer(HitFlashTimer);
		World->GetTimerManager().SetTimer(
			HitFlashTimer,
			this,
			&AEnemyCharacter::TickHitFlash,
			0.016f,
			true);
	}
}

void AEnemyCharacter::DriveAuraOverlayIntensity(float Pulse)
{
	if (!HitFlashMID)
	{
		return;
	}
	if (bAuraOverlayUsesHitFlashParams)
	{
		HitFlashMID->SetScalarParameterValue(TEXT("HitFlash"), Pulse);
	}
	else
	{
		HitFlashMID->SetScalarParameterValue(TEXT("Opacity Multiplier"), AuraOverlayOpacityMul * Pulse);
	}
	HitFlashMID->SetScalarParameterValue(TEXT("HitTime"), HitFlashStartTime);
}

void AEnemyCharacter::PlayElementAuraFlash(ESlimeElement Element, float Duration)
{
	if (bDeathSequence || DeathDissolveElapsed > 0.f)
	{
		return;
	}
	if (Duration <= KINDA_SMALL_NUMBER)
	{
		ClearElementAuraFlash();
		return;
	}

	UMaterialInterface* FlashMat = ResolveElementHitOverlay(Element);
	if (!FlashMat)
	{
		return;
	}

	const bool bUsesHitParams =
		FlashMat == HitFlashMaterial.LoadSynchronous()
		|| Element == ESlimeElement::Water
		|| Element == ESlimeElement::Fire
		|| Element == ESlimeElement::Dark
		|| Element == ESlimeElement::Physical;

	BeginAuraOverlay(FlashMat, bUsesHitParams, Duration);
	if (HitFlashMID && bUsesHitParams)
	{
		HitFlashMID->SetVectorParameterValue(TEXT("HitColor"), SlimeCombat::GetElementVfxColor(Element));
	}
}

void AEnemyCharacter::PlayElementAuraFlashByColor(FLinearColor FlashColor, float Duration)
{
	if (bDeathSequence || DeathDissolveElapsed > 0.f)
	{
		return;
	}
	if (Duration <= KINDA_SMALL_NUMBER)
	{
		ClearElementAuraFlash();
		return;
	}

	UMaterialInterface* FlashMat = HitFlashMaterial.LoadSynchronous();
	if (!FlashMat)
	{
		return;
	}

	BeginAuraOverlay(FlashMat, true, Duration);
	if (HitFlashMID)
	{
		HitFlashMID->SetVectorParameterValue(TEXT("HitColor"), FlashColor);
	}
}

void AEnemyCharacter::ClearElementAuraFlash()
{
	if (!bAuraFlashActive)
	{
		return;
	}
	bAuraFlashActive = false;
	HitFlashRemaining = 0.f;
	ClearHitFlashOverlay();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitFlashTimer);
	}
}

void AEnemyCharacter::ApplyHitFlashToMesh(UMeshComponent* MeshComp)
{
	if (!MeshComp || !HitFlashMID)
	{
		return;
	}
	MeshComp->SetOverlayMaterial(HitFlashMID);
}

void AEnemyCharacter::ApplyHitFlashToAllMeshes()
{
	ApplyHitFlashToMesh(GetMesh());
	for (USceneComponent* Part : GeneratedParts)
	{
		ApplyHitFlashToMesh(Cast<UMeshComponent>(Part));
	}
	if (PlaceholderMesh && PlaceholderMesh->IsVisible())
	{
		ApplyHitFlashToMesh(PlaceholderMesh);
	}
}

void AEnemyCharacter::ClearHitFlashFromMeshes()
{
	auto ClearMesh = [](UMeshComponent* MeshComp)
	{
		if (MeshComp)
		{
			MeshComp->SetOverlayMaterial(nullptr);
		}
	};
	ClearMesh(GetMesh());
	for (USceneComponent* Part : GeneratedParts)
	{
		ClearMesh(Cast<UMeshComponent>(Part));
	}
	ClearMesh(PlaceholderMesh);
}

void AEnemyCharacter::ClearHitFlashOverlay()
{
	ClearHitFlashFromMeshes();
	HitFlashMID = nullptr;
	bAuraFlashActive = false;
	bAuraOverlayUsesHitFlashParams = true;
}

void AEnemyCharacter::TickHitFlash()
{
	if (!HitFlashMID || HitFlashRemaining <= 0.f)
	{
		ClearHitFlashOverlay();
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(HitFlashTimer);
		}
		return;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : HitFlashStartTime;
	const float Age = FMath::Max(Now - HitFlashStartTime, 0.f);
	const float Duration = ActiveFlashDuration > 0.f ? ActiveFlashDuration : HitFlashDuration;
	const float Envelope = Duration > 0.f
		? FMath::Clamp(1.f - Age / Duration, 0.f, 1.f)
		: 0.f;

	float Pulse = 0.f;
	if (bAuraFlashActive)
	{
		// 0.5s tint on, 1.0s natural color off, repeat for the aura window.
		const float OnSec = FMath::Max(AuraFlashOnSeconds, 0.05f);
		const float OffSec = FMath::Max(AuraFlashOffSeconds, 0.05f);
		const float Cycle = OnSec + OffSec;
		const float Phase = FMath::Fmod(Age, Cycle);
		const bool bOn = Phase < OnSec;
		Pulse = bOn ? 1.f : 0.f;
		if (bOn)
		{
			ApplyHitFlashToAllMeshes();
			DriveAuraOverlayIntensity(Pulse);
		}
		else
		{
			ClearHitFlashFromMeshes();
		}
	}
	else
	{
		Pulse = Envelope * FMath::Abs(FMath::Sin(Age * ActiveFlashFrequency * PI));
		DriveAuraOverlayIntensity(Pulse);
	}

	HitFlashRemaining = Envelope * Duration;

	if (Envelope <= KINDA_SMALL_NUMBER)
	{
		ClearHitFlashOverlay();
		if (UWorld* WorldMutable = GetWorld())
		{
			WorldMutable->GetTimerManager().ClearTimer(HitFlashTimer);
		}
	}
}

FVector AEnemyCharacter::GetLockOnLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, 60.f);
}

bool AEnemyCharacter::GetStableMeshBounds(FBox& OutBox) const
{
	OutBox = FBox(ForceInit);
	bool bAny = false;

	auto Accumulate = [&](UPrimitiveComponent* Prim)
	{
		if (!Prim || Prim->bHiddenInGame || !Prim->IsVisible())
		{
			return;
		}
		const FBox Local = Prim->GetLocalBounds().GetBox();
		OutBox += Local.TransformBy(Prim->GetComponentTransform());
		bAny = true;
	};

	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		if (Skel->GetSkeletalMeshAsset() != nullptr)
		{
			Accumulate(Skel);
		}
	}

	if (!bAny && PlaceholderMesh)
	{
		Accumulate(PlaceholderMesh);
	}

	return bAny;
}

void AEnemyCharacter::UpdateHudAnchorCache() const
{
	const FVector CapsuleLoc = GetCapsuleComponent()
		? GetCapsuleComponent()->GetComponentLocation()
		: GetActorLocation();

	float TargetRelZ = 60.f;
	FBox Box;
	if (GetStableMeshBounds(Box))
	{
		TargetRelZ = Box.Max.Z - CapsuleLoc.Z;
	}
	else if (const UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		TargetRelZ = Capsule->GetScaledCapsuleHalfHeight();
	}

	constexpr float DeadzoneCm = 24.f;
	if (!bHudAnchorCached)
	{
		HudAnchorRelZ = TargetRelZ;
		bHudAnchorCached = true;
		return;
	}

	if (FMath::Abs(TargetRelZ - HudAnchorRelZ) > DeadzoneCm)
	{
		HudAnchorRelZ = TargetRelZ;
	}
}

FVector AEnemyCharacter::GetHudAnchorLocation() const
{
	UpdateHudAnchorCache();
	const FVector Loc = GetCapsuleComponent()
		? GetCapsuleComponent()->GetComponentLocation()
		: GetActorLocation();
	return FVector(Loc.X, Loc.Y, Loc.Z + HudAnchorRelZ);
}

FVector AEnemyCharacter::GetVisualBoundsCenter() const
{
	FBox Combined(ForceInit);
	bool bAny = false;

	auto Accumulate = [&](UPrimitiveComponent* Prim)
	{
		if (!Prim || Prim->bHiddenInGame || !Prim->IsVisible())
		{
			return;
		}
		Combined += Prim->Bounds.GetBox();
		bAny = true;
	};

	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		if (Skel->GetSkeletalMeshAsset() != nullptr)
		{
			Accumulate(Skel);
		}
	}

	for (const TObjectPtr<USceneComponent>& Part : GeneratedParts)
	{
		Accumulate(Cast<UPrimitiveComponent>(Part.Get()));
	}

	if (PlaceholderMesh)
	{
		Accumulate(PlaceholderMesh);
	}

	if (bAny)
	{
		return Combined.GetCenter();
	}
	return GetActorLocation();
}

void AEnemyCharacter::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	if (Damage > 0.f)
	{
		OutOfCombatSeconds = 0.f;
	}

	// Legacy fallback: no ability system (e.g. CDO / early spawn) or already inside the GAS callback.
	if (bRoutingGasDamage || !AbilitySystem || !EnemyAttributes)
	{
		if (Health)
		{
			Health->ApplyDamage(Damage, DamageCauser, DamageLocation, DamageImpulse);
		}
		return;
	}

	if (Damage > 0.f && HasCombatStateTag(SlimeEnemyTags::State_Invulnerable))
	{
		return;
	}

	float FinalDamage = Damage;
	if (Damage > 0.f && HasCombatStateTag(SlimeEnemyTags::State_Guarding))
	{
		FinalDamage = Damage * FMath::Clamp(1.f - GuardDamageReduction, 0.f, 1.f);
		const float NewGuard = EnemyAttributes->GetGuard() - Damage;
		EnemyAttributes->SetGuard(FMath::Max(NewGuard, 0.f));
		if (NewGuard <= 0.f)
		{
			OnGuardBroken(DamageCauser);
		}
	}

	const bool bSuperArmor = HasCombatStateTag(SlimeEnemyTags::State_SuperArmor);
	const float PoiseDamage = (FinalDamage > 0.f && !bSuperArmor) ? FinalDamage * PoiseDamageRatio : 0.f;

	PendingDamageLocation = DamageLocation;
	PendingDamageImpulse = DamageImpulse;
	PoiseRegenBlockedUntil = GetWorld() ? GetWorld()->GetTimeSeconds() + 2.f : 0.f;

	FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
	Context.AddInstigator(DamageCauser, DamageCauser);
	const FGameplayEffectSpecHandle Spec =
		AbilitySystem->MakeOutgoingSpec(UGE_EnemyDamage::StaticClass(), 1.f, Context);
	if (!Spec.IsValid())
	{
		if (Health)
		{
			Health->ApplyDamage(FinalDamage, DamageCauser, DamageLocation, DamageImpulse);
		}
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(SlimeEnemyTags::Data_Damage, FinalDamage);
	Spec.Data->SetSetByCallerMagnitude(SlimeEnemyTags::Data_PoiseDamage, PoiseDamage);
	AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data);
}

void AEnemyCharacter::OnGasDamageApplied(float Damage, AActor* DamageInstigator)
{
	if (!Health || Damage <= 0.f)
	{
		return;
	}
	TGuardValue<bool> GasRouteGuard(bRoutingGasDamage, true);
	TGuardValue<bool> FacadeGuard(bSyncingHealthFacade, true);
	Health->ApplyDamage(Damage, DamageInstigator, PendingDamageLocation, PendingDamageImpulse);
}

void AEnemyCharacter::OnGasHealingApplied(float Healing, AActor* HealingInstigator)
{
	if (!Health || Healing <= 0.f)
	{
		return;
	}
	TGuardValue<bool> FacadeGuard(bSyncingHealthFacade, true);
	Health->ApplyHealing(Healing);
}

void AEnemyCharacter::OnPoiseBroken(AActor* PoiseInstigator)
{
	EnterStagger(StaggerDuration, PoiseInstigator);
}

void AEnemyCharacter::OnGuardBroken(AActor* GuardInstigator)
{
	if (AbilitySystem)
	{
		FGameplayEventData Payload;
		Payload.EventTag = SlimeEnemyTags::Event_GuardBreak;
		Payload.Instigator = GuardInstigator;
		Payload.Target = this;
		AbilitySystem->HandleGameplayEvent(SlimeEnemyTags::Event_GuardBreak, &Payload);
		AbilitySystem->RemoveActiveEffectsWithGrantedTags(
			FGameplayTagContainer(SlimeEnemyTags::State_Guarding));
	}
	EnterStagger(StaggerDuration, GuardInstigator);
}

void AEnemyCharacter::EnterStagger(float Duration, AActor* StaggerInstigator)
{
	if (bDeathSequence || Duration <= 0.f)
	{
		return;
	}
	if (Combat)
	{
		Combat->InterruptCombat();
	}
	ReleaseAttackSlot();
	ApplyTimedState(UGE_EnemyStagger::StaticClass(), Duration);
	if (EnemyAttributes)
	{
		// Refill poise so the enemy does not chain-stagger forever.
		EnemyAttributes->SetPoise(EnemyAttributes->GetMaxPoise());
	}
	if (AbilitySystem)
	{
		FGameplayEventData Payload;
		Payload.EventTag = SlimeEnemyTags::Event_Hit;
		Payload.Instigator = StaggerInstigator;
		Payload.Target = this;
		AbilitySystem->HandleGameplayEvent(SlimeEnemyTags::Event_Hit, &Payload);
	}
}

bool AEnemyCharacter::ApplyTimedState(TSubclassOf<UGameplayEffect> EffectClass, float Duration, float Power)
{
	if (!AbilitySystem || !EffectClass)
	{
		return false;
	}
	const FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(EffectClass, 1.f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}
	Spec.Data->SetSetByCallerMagnitude(SlimeEnemyTags::Data_Duration, FMath::Max(Duration, 0.05f));
	Spec.Data->SetSetByCallerMagnitude(SlimeEnemyTags::Data_Power, Power);
	return AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data).IsValid();
}

bool AEnemyCharacter::HasCombatStateTag(FGameplayTag Tag) const
{
	return AbilitySystem && Tag.IsValid() && AbilitySystem->HasMatchingGameplayTag(Tag);
}

bool AEnemyCharacter::IsStaggered() const
{
	return HasCombatStateTag(SlimeEnemyTags::State_Staggered);
}

float AEnemyCharacter::GetPoisePercent() const
{
	if (!EnemyAttributes || EnemyAttributes->GetMaxPoise() <= 0.f)
	{
		return 1.f;
	}
	return FMath::Clamp(EnemyAttributes->GetPoise() / EnemyAttributes->GetMaxPoise(), 0.f, 1.f);
}

float AEnemyCharacter::GetHealthPercent() const
{
	return Health ? Health->GetHealthPercent() : 1.f;
}

void AEnemyCharacter::HandleHealthFacadeChanged(float CurrentHP, float FacadeMaxHP)
{
	if (bSyncingHealthFacade || !EnemyAttributes)
	{
		return;
	}
	// Damage applied straight to the legacy component (old Blueprints, fall damage, quests):
	// mirror it so GAS stays the single source of truth for AI queries.
	EnemyAttributes->SetMaxHealth(FMath::Max(FacadeMaxHP, 1.f));
	EnemyAttributes->SetHealth(FMath::Clamp(CurrentHP, 0.f, FMath::Max(FacadeMaxHP, 1.f)));
}

void AEnemyCharacter::TickPoiseRegen(float DeltaSeconds)
{
	if (!EnemyAttributes || PoiseRegenPerSecond <= 0.f || bDeathSequence)
	{
		return;
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (Now < PoiseRegenBlockedUntil || IsStaggered())
	{
		return;
	}
	const float MaxValue = EnemyAttributes->GetMaxPoise();
	const float Current = EnemyAttributes->GetPoise();
	if (Current < MaxValue)
	{
		EnemyAttributes->SetPoise(FMath::Min(Current + PoiseRegenPerSecond * DeltaSeconds, MaxValue));
	}
}

bool AEnemyCharacter::RequestAttackSlot(float Duration)
{
	if (bDeathSequence || !GetWorld())
	{
		return false;
	}
	if (UEnemyEncounterSubsystem* Director = GetWorld()->GetSubsystem<UEnemyEncounterSubsystem>())
	{
		return Director->TryAcquireAttackSlot(this, CombatRole, Duration);
	}
	return true;
}

void AEnemyCharacter::ReleaseAttackSlot()
{
	if (GetWorld())
	{
		GetWorld()->GetSubsystem<UEnemyEncounterSubsystem>()->ReleaseAttackSlot(this);
	}
}

int32 AEnemyCharacter::GetEncounterPhase() const
{
	return GetWorld() ? GetWorld()->GetSubsystem<UEnemyEncounterSubsystem>()->GetBossPhase() : 1;
}

void AEnemyCharacter::HandleDeath()
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

void AEnemyCharacter::ApplyHealing(float Healing, AActor* Healer)
{
	if (Health)
	{
		Health->ApplyHealing(Healing);
	}
}

void AEnemyCharacter::NotifyDanger(const FVector& DangerLocation, AActor* DangerSource)
{
	if (bDeathSequence || CombatRole != EEnemyCombatRole::Duelist || Guard <= 0.f || IsStaggered())
	{
		return;
	}
	// Duelists answer a readable incoming attack with a short guard stance.
	ApplyTimedState(UGE_EnemyGuard::StaticClass(), 0.75f);
}

void AEnemyCharacter::HandleDied()
{
	if (bDeathSequence)
	{
		return;
	}

	// Morph target death: force the slime back without killing it. The morph component
	// owns the unpossess + cleanup; we must not run the normal death dissolve because the
	// player controller is still on this pawn and needs to be handed back first.
	if (bMorphTarget)
	{
		if (USlimeMorphComponent* MorphComp = MorphMaster.Get()
				? MorphMaster->FindComponentByClass<USlimeMorphComponent>()
				: nullptr)
		{
			MorphComp->ForceUnmorph(true);
		}
		return;
	}

	ClearElementAuraFlash();
	if (Status)
	{
		Status->ClearAllAuras();
	}

	bDeathSequence = true;
	SetActorTickEnabled(false);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhantomLifeTimer);
	}

	if (!bPhantomInstance)
	{
		if (Objective)
		{
			Objective->TryContribute();
		}
		DropSouvenirReward();
	}

	if (HealthBar)
	{
		HealthBar->SetVisibility(false);
		HealthBar->SetHiddenInGame(true);
	}
	if (Combat)
	{
		Combat->InterruptCombat();
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	if (AController* AI = GetController())
	{
		AI->StopMovement();
		AI->SetActorTickEnabled(false);
		if (AAIController* AIC = Cast<AAIController>(AI))
		{
			if (UBrainComponent* Brain = AIC->GetBrainComponent())
			{
				Brain->StopLogic(TEXT("Death"));
			}
		}
	}

	if (bDevouredDeath)
	{
		SetActorEnableCollision(false);
		SetActorHiddenInGame(true);
		if (bPresenceRegistered)
		{
			if (UWorld* World = GetWorld())
			{
				if (UEnemyPresenceSubsystem* PresenceSys = World->GetSubsystem<UEnemyPresenceSubsystem>())
				{
					PresenceSys->UnregisterEnemy(this);
				}
			}
			bPresenceRegistered = false;
		}
		return;
	}

	PlayDeathMontageThenDissolve();
}

void AEnemyCharacter::ApplySingleNodeAnimModeIfNeeded()
{
	if (!UsesSingleNodeAnims())
	{
		return;
	}
	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		Skel->SetAnimInstanceClass(nullptr);
		Skel->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	}
}

void AEnemyCharacter::PlayMeshAnimation(UAnimationAsset* Asset, bool bLoop)
{
	USkeletalMeshComponent* Skel = GetMesh();
	if (!Skel || !Asset)
	{
		return;
	}
	if (UsesSingleNodeAnims())
	{
		Skel->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Skel->PlayAnimation(Asset, bLoop);
		return;
	}
	if (UAnimMontage* Montage = Cast<UAnimMontage>(Asset))
	{
		if (UAnimInstance* Anim = Skel->GetAnimInstance())
		{
			Anim->Montage_Play(Montage);
		}
	}
}

void AEnemyCharacter::StopMeshAnimation()
{
	USkeletalMeshComponent* Skel = GetMesh();
	if (!Skel)
	{
		return;
	}
	if (UsesSingleNodeAnims())
	{
		Skel->Stop();
		return;
	}
	if (UAnimInstance* Anim = Skel->GetAnimInstance())
	{
		Anim->Montage_Stop(0.15f);
	}
}

void AEnemyCharacter::PlayDeathMontageThenDissolve()
{
	UAnimMontage* Montage = DeathMontage.LoadSynchronous();
	USkeletalMeshComponent* Skel = GetMesh();
	if (Montage && UsesSingleNodeAnims())
	{
		PlayMeshAnimation(Montage, false);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				DeathMontageFallbackTimer,
				this,
				&AEnemyCharacter::FreezeDeathPoseAndDissolve,
				Montage->GetPlayLength() + 0.05f,
				false);
		}
		return;
	}
	if (Montage && Skel)
	{
		if (UAnimInstance* Anim = Skel->GetAnimInstance())
		{
			const float Played = Anim->Montage_Play(Montage);
			const float BlendOut = Montage->GetDefaultBlendOutTime();
			const float Wait = FMath::Max((Played > 0.f ? Played : Montage->GetPlayLength()) - BlendOut - 0.03f, 0.08f);
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					DeathMontageFallbackTimer,
					this,
					&AEnemyCharacter::FreezeDeathPoseAndDissolve,
					Wait,
					false);
			}
			return;
		}
	}
	StartDeathDissolve();
}

void AEnemyCharacter::FreezeDeathPoseAndDissolve()
{
	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		if (UAnimInstance* Anim = Skel->GetAnimInstance())
		{
			if (UAnimMontage* Montage = DeathMontage.Get())
			{
				Anim->Montage_Pause(Montage);
				Anim->Montage_SetPosition(Montage, Montage->GetPlayLength());
			}
		}
		Skel->bPauseAnims = true;
	}
	StartDeathDissolve();
}

void AEnemyCharacter::StartDeathDissolve()
{
	if (DeathDissolveElapsed > 0.f)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeathMontageFallbackTimer);
	}

	if (UNiagaraSystem* FX = DeathDissolveNiagara.LoadSynchronous())
	{
		FVector FxLoc = GetVisualBoundsCenter();
		if (USkeletalMeshComponent* Skel = GetMesh())
		{
			static const FName HeadSocket(TEXT("head"));
			if (Skel->DoesSocketExist(HeadSocket))
			{
				FxLoc = Skel->GetSocketLocation(HeadSocket);
			}
		}
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FX, FxLoc, GetActorRotation());
	}

	if (UMaterialInterface* DissolveMat = DeathDissolveMaterial.LoadSynchronous())
	{
		// Death dissolve owns the overlay slot — cancel any active hit flash first.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(HitFlashTimer);
		}
		HitFlashRemaining = 0.f;
		HitFlashMID = nullptr;
		if (USkeletalMeshComponent* Skel = GetMesh())
		{
			DeathDissolveMID = UMaterialInstanceDynamic::Create(DissolveMat, this);
			if (DeathDissolveMID)
			{
				Skel->SetOverlayMaterial(DeathDissolveMID);
			}
			else
			{
				Skel->SetOverlayMaterial(DissolveMat);
			}
		}
	}

	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		Skel->bPauseAnims = true;
	}

	DeathDissolveElapsed = 0.01f;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DeathDissolveTimer,
			this,
			&AEnemyCharacter::TickDeathDissolve,
			0.05f,
			true);
	}
}

void AEnemyCharacter::TickDeathDissolve()
{
	DeathDissolveElapsed += 0.05f;
	const float Alpha = 1.f - FMath::Clamp(DeathDissolveElapsed / FMath::Max(DeathDissolveSeconds, 0.2f), 0.f, 1.f);
	ApplyDeathDissolveVisual(Alpha);
	if (DeathDissolveElapsed >= DeathDissolveSeconds)
	{
		FinishDeathSequence();
	}
}

void AEnemyCharacter::ApplyDeathDissolveVisual(float Alpha)
{
	auto HasScalar = [](UMaterialInterface* Mat, FName ParamName) -> bool
	{
		if (!Mat)
		{
			return false;
		}
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Ids;
		Mat->GetAllScalarParameterInfo(Infos, Ids);
		for (const FMaterialParameterInfo& Info : Infos)
		{
			if (Info.Name == ParamName)
			{
				return true;
			}
		}
		return false;
	};

	if (DeathDissolveMID)
	{
		DeathDissolveMID->SetScalarParameterValue(TEXT("DissolveAmount"), 1.f - Alpha);
	}

	auto FadeMesh = [Alpha, &HasScalar](UMeshComponent* Comp)
	{
		if (!Comp)
		{
			return;
		}
		const int32 Mats = Comp->GetNumMaterials();
		for (int32 Index = 0; Index < Mats; ++Index)
		{
			UMaterialInterface* Base = Comp->GetMaterial(Index);
			if (!Base)
			{
				continue;
			}
			if (!HasScalar(Base, TEXT("DissolveLine")) && !HasScalar(Base, TEXT("Opacity")))
			{
				continue;
			}
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Base);
			if (!MID)
			{
				MID = Comp->CreateAndSetMaterialInstanceDynamic(Index);
			}
			if (MID)
			{
				if (HasScalar(MID, TEXT("DissolveLine")))
				{
					MID->SetScalarParameterValue(TEXT("DissolveLine"), 1.f - Alpha);
				}
				if (HasScalar(MID, TEXT("Opacity")))
				{
					MID->SetScalarParameterValue(TEXT("Opacity"), Alpha);
				}
			}
		}
		Comp->SetVisibility(Alpha > 0.05f);
	};

	FadeMesh(GetMesh());
	FadeMesh(PlaceholderMesh);
	for (USceneComponent* Part : GeneratedParts)
	{
		FadeMesh(Cast<UMeshComponent>(Part));
	}
}

void AEnemyCharacter::FinishDeathSequence()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeathDissolveTimer);
		World->GetTimerManager().ClearTimer(DeathMontageFallbackTimer);
	}
	Destroy();
}

void AEnemyCharacter::DropSouvenirReward()
{
	if (SouvenirReward.IsNull())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	UClass* DropClass = SouvenirDropClass.LoadSynchronous();
	if (!DropClass)
	{
		DropClass = AQuestSouvenirPickup::StaticClass();
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AActor* Drop = World->SpawnActor<AActor>(DropClass, GetActorLocation() + FVector(0.f, 0.f, 40.f), GetActorRotation(), Params);
	if (AQuestSouvenirPickup* Souvenir = Cast<AQuestSouvenirPickup>(Drop))
	{
		Souvenir->SouvenirDefinition = SouvenirReward;
	}
}

void AEnemyCharacter::CaptureDifficultyBases()
{
	if (bDifficultyBasesCaptured)
	{
		return;
	}
	DifficultyBaseMaxHP = MaxHP;
	bDifficultyBasesCaptured = true;
}

void AEnemyCharacter::ApplyWeekDifficulty(int32 InWeekIndex)
{
	CaptureDifficultyBases();
	float HpMul = 1.f;
	float DmgMul = 1.f;
	float IntervalMul = 1.f;
	switch (InWeekIndex)
	{
	case 1:
		HpMul = 0.85f;
		DmgMul = 0.70f;
		IntervalMul = 1.35f;
		break;
	case 3:
		HpMul = 1.40f;
		DmgMul = 1.35f;
		IntervalMul = 0.75f;
		break;
	default:
		break;
	}

	MaxHP = FMath::Max(DifficultyBaseMaxHP * HpMul, 1.f);
	if (Health)
	{
		const float Ratio = Health->MaxHP > 0.f ? Health->CurrentHP / Health->MaxHP : 1.f;
		Health->MaxHP = MaxHP;
		Health->CurrentHP = FMath::Clamp(MaxHP * Ratio, 1.f, MaxHP);
		Health->OnHealthChanged.Broadcast(Health->CurrentHP, Health->MaxHP);
	}
	ApplyDifficultyToCombat(DmgMul, IntervalMul);
}

void AEnemyCharacter::ApplyDifficultyToCombat(float DamageMul, float IntervalMul)
{
}

bool AEnemyCharacter::WantsCombatBudget() const
{
	return Presence == EEnemyPresence::Active || Presence == EEnemyPresence::Idle;
}

void AEnemyCharacter::SetEnemyPresence(EEnemyPresence NewPresence)
{
	if (Presence == NewPresence)
	{
		return;
	}
	Presence = NewPresence;

	const bool bSleeping = Presence == EEnemyPresence::Sleep || Presence == EEnemyPresence::Despawned;
	if (AController* AI = GetController())
	{
		AI->SetActorTickEnabled(!bSleeping);
		if (bSleeping)
		{
			AI->StopMovement();
		}
	}

	if (Combat)
	{
		Combat->SetComponentTickEnabled(Presence == EEnemyPresence::Active);
		if (bSleeping)
		{
			Combat->InterruptCombat();
		}
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		if (bSleeping)
		{
			Move->StopMovementImmediately();
			if (!Move->IsFalling())
			{
				Move->DisableMovement();
			}
		}
		else
		{
			Move->SetMovementMode(Move->IsFalling() ? MOVE_Falling : MOVE_Walking);
		}
	}

	RefreshWorldHealthBarVisibility();

	if (!bDeathSequence)
	{
		SetActorHiddenInGame(Presence == EEnemyPresence::Despawned);
		SetActorEnableCollision(Presence != EEnemyPresence::Despawned);
	}
}

bool AEnemyCharacter::IsDevourableNow() const
{
	if (!bDevourable || bPhantomInstance || bDeathSequence)
	{
		return false;
	}
	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		if (Skel->GetSkeletalMeshAsset())
		{
			return true;
		}
	}
	if (GeneratedParts.Num() > 0)
	{
		return true;
	}
	if (PlaceholderMesh && PlaceholderMesh->GetStaticMesh())
	{
		return true;
	}
	return false;
}

void AEnemyCharacter::InitAsPhantom(float LifeSeconds, AActor* Master)
{
	bPhantomInstance = true;
	bDevourable = false;
	PhantomLifeSeconds = LifeSeconds;
	PhantomMaster = Master;
	AIControllerClass = AEnemyAllyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	if (AEnemyFighter* Fighter = Cast<AEnemyFighter>(this))
	{
		Fighter->bPassive = false;
		Fighter->bWanderWhenIdle = false;
		Fighter->LeashRange = 10000.f;
		Fighter->DetectRange = 2000.f;
	}
}

void AEnemyCharacter::InitAsMorphTarget(AActor* Master)
{
	bMorphTarget = true;
	bDevourable = false;
	MorphMaster = Master;

	// No AI — the player will possess this pawn.
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

	// The presence subsystem may Destroy() enemies it considers far away. This pawn is the
	// one the player is driving, so it must never be culled out from under them.
	bAllowDespawn = false;

	// Morph targets bypass every spawn-based enemy reset. Actual KillZ recovery uses their
	// most recent grounded transform and unmorphs without consuming the capture.
	bSuppressOutOfCombatReset = true;

	// Player team so hostile enemies can damage the morph body.
	if (Health)
	{
		Health->Team = ESlimeTeam::Player;
		Health->bDestroyOnDeath = false;
		Health->bRegenOnDeath = false;
	}

	// Match the slime's movement feel: orient to movement, not to controller yaw.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	}

	// Copy InputAction assets from the slime so Enhanced Input bindings work.
	if (ASlimeFableCharacter* Slime = Cast<ASlimeFableCharacter>(Master))
	{
		MorphMoveAction = Slime->GetMoveAction();
		MorphLookAction = Slime->GetLookAction();
		MorphMouseLookAction = Slime->GetMouseLookAction();
		MorphJumpAction = Slime->GetJumpAction();
	}

	// Temporary third-person camera (enemies don't have one by default).
	MorphCameraBoom = NewObject<USpringArmComponent>(this, TEXT("MorphCameraBoom"));
	MorphCameraBoom->SetupAttachment(RootComponent);
	MorphCameraBoom->TargetArmLength = 300.f;
	MorphCameraBoom->bUsePawnControlRotation = true;
	MorphCameraBoom->RegisterComponent();

	MorphFollowCamera = NewObject<UCameraComponent>(this, TEXT("MorphFollowCamera"));
	MorphFollowCamera->SetupAttachment(MorphCameraBoom, USpringArmComponent::SocketName);
	MorphFollowCamera->bUsePawnControlRotation = false;
	MorphFollowCamera->RegisterComponent();

	// Play an initial idle animation so single-node-anim enemies (e.g. watchdog) don't
	// stand in T-pose or stuck in a prone pose when the morph body first appears.
	if (UsesSingleNodeAnims())
	{
		if (const AEnemyFighter* Fighter = Cast<AEnemyFighter>(this))
		{
			if (Fighter->IdleMontages.Num() > 0)
			{
				if (UAnimMontage* Idle = Fighter->IdleMontages[0].LoadSynchronous())
				{
					PlayMeshAnimation(Idle, false);
				}
			}
			else if (UAnimMontage* Walk = Fighter->WalkMontage.LoadSynchronous())
			{
				// No idle montages — fall back to a looping walk so at least the model is posed standing.
				PlayMeshAnimation(Walk, true);
			}
		}
	}
}

void AEnemyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!bMorphTarget || !PlayerInputComponent)
	{
		return;
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MorphJumpAction)
		{
			EnhancedInput->BindAction(MorphJumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
			EnhancedInput->BindAction(MorphJumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}
		if (MorphMoveAction)
		{
			EnhancedInput->BindAction(MorphMoveAction, ETriggerEvent::Triggered, this, &AEnemyCharacter::MorphMove);
		}
		if (MorphMouseLookAction)
		{
			EnhancedInput->BindAction(MorphMouseLookAction, ETriggerEvent::Triggered, this, &AEnemyCharacter::MorphLook);
		}
		if (MorphLookAction)
		{
			EnhancedInput->BindAction(MorphLookAction, ETriggerEvent::Triggered, this, &AEnemyCharacter::MorphLook);
		}
	}
}

void AEnemyCharacter::MorphMove(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	if (GetController())
	{
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AEnemyCharacter::MorphLook(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	if (GetController())
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AEnemyCharacter::BeginDevouredDeath(AActor* Devourer)
{
	if (bDeathSequence || !Health)
	{
		return;
	}
	ClearElementAuraFlash();
	if (Status)
	{
		Status->ClearAllAuras();
	}
	bDevouredDeath = true;
	Health->ApplyDamage(FMath::Max(Health->CurrentHP, 1.f), Devourer, GetActorLocation(), FVector::ZeroVector);
}

void AEnemyCharacter::BeginPhantomExpire()
{
	if (bDeathSequence)
	{
		return;
	}
	if (Health && Health->IsAlive())
	{
		Health->ApplyDamage(FMath::Max(Health->CurrentHP, 1.f), this, GetActorLocation(), FVector::ZeroVector);
	}
	else if (!bDeathSequence)
	{
		PlayDeathMontageThenDissolve();
	}
}

FLinearColor AEnemyCharacter::ResolveDevourWheelTint() const
{
	const uint32 Hash = GetTypeHash(GetClass()->GetName());
	static const FLinearColor Palette[] = {
		FLinearColor(0.45f, 0.32f, 0.18f),
		FLinearColor(0.35f, 0.42f, 0.22f),
		FLinearColor(0.50f, 0.38f, 0.22f),
		FLinearColor(0.28f, 0.30f, 0.22f),
		FLinearColor(0.42f, 0.28f, 0.20f),
		FLinearColor(0.38f, 0.34f, 0.28f)
	};
	return Palette[Hash % 6];
}

void AEnemyCharacter::ApplyPhantomVisuals()
{
	auto Ghost = [](UMeshComponent* Comp)
	{
		if (!Comp)
		{
			return;
		}
		const int32 Mats = Comp->GetNumMaterials();
		for (int32 Index = 0; Index < Mats; ++Index)
		{
			UMaterialInterface* Base = Comp->GetMaterial(Index);
			if (!Base)
			{
				continue;
			}
			UMaterialInstanceDynamic* MID = Comp->CreateAndSetMaterialInstanceDynamic(Index);
			if (MID)
			{
				MID->SetScalarParameterValue(TEXT("Opacity"), 0.55f);
				MID->SetScalarParameterValue(TEXT("EmissiveStrength"), 0.8f);
				MID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.55f, 0.72f, 0.42f));
			}
		}
	};
	Ghost(GetMesh());
	Ghost(PlaceholderMesh);
	for (USceneComponent* Part : GeneratedParts)
	{
		Ghost(Cast<UMeshComponent>(Part));
	}
}
