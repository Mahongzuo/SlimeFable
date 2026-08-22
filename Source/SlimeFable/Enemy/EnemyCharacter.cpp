// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyCharacter.h"

#include "EnemyAllyAIController.h"
#include "EnemyFighter.h"
#include "EnemyTower.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimationAsset.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
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
#include "SlimeLockOnComponent.h"
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
	DeathDissolveNiagara = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(TEXT("/Game/Mixed_Magic_VFX_Pack/VFX/NS_Dark_Solo_Impact.NS_Dark_Solo_Impact")));
	DeathDissolveMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/_Slime/FX/M_EnemyDeathDissolve.M_EnemyDeathDissolve")));
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
	if (Health)
	{
		Health->ApplyDamage(Damage, DamageCauser, DamageLocation, DamageImpulse);
	}
}

void AEnemyCharacter::HandleDeath()
{
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
	if (Cast<AEnemyTower>(this))
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
