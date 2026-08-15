// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "EnemyCombatComponent.h"
#include "EnemyPresenceSubsystem.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "SlimeHealthComponent.h"
#include "SlimeLockOnComponent.h"
#include "SlimeWorldHealthBar.h"
#include "Quest/QuestObjectiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

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
	PlaceholderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderMesh->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PlaceholderMesh->SetStaticMesh(CubeMesh.Object);
		PlaceholderMesh->SetWorldScale3D(FVector(0.7f, 0.7f, 1.4f));
	}

	Health = CreateDefaultSubobject<USlimeHealthComponent>(TEXT("Health"));
	Health->Team = ESlimeTeam::Enemy;
	Health->bDestroyOnDeath = true;
	Health->bRegenOnDeath = false;
	Health->MaxHP = 200.f;

	Combat = CreateDefaultSubobject<UEnemyCombatComponent>(TEXT("Combat"));

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
}

void AEnemyCharacter::ApplyHealthBarOffset()
{
	if (HealthBar)
	{
		HealthBar->SetRelativeLocation(FVector(0.f, 0.f, HealthBarZOffset));
	}
}

void AEnemyCharacter::RefreshWorldHealthBarVisibility()
{
	if (!HealthBar)
	{
		return;
	}

	bool bShow = Health && Health->IsAlive()
		&& Presence != EEnemyPresence::Sleep
		&& Presence != EEnemyPresence::Despawned
		&& !USlimeLockOnComponent::IsLockedByLocalPlayer(this, this);

	if (bShow)
	{
		if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
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
	RefreshWorldHealthBarVisibility();
	TickOutOfCombatReset(DeltaSeconds);
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
	}
	if (Combat)
	{
		Combat->InterruptCombat();
	}

	OnRestoredToSpawn();
}

void AEnemyCharacter::TickOutOfCombatReset(float DeltaSeconds)
{
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

	const float DistFromSpawn = FVector::Dist2D(Location, SpawnTransform.GetLocation());
	const float HorizSpeed = GetVelocity().Size2D();
	constexpr float StuckSpeed = 15.f;
	const bool bStuck = DistFromSpawn > StuckResetDistance && HorizSpeed < StuckSpeed;

	if (bStuck || (OutOfCombatResetSeconds > 0.f && OutOfCombatSeconds >= OutOfCombatResetSeconds))
	{
		RestoreToSpawn();
	}
}

void AEnemyCharacter::BeginPlay()
{
	SpawnTransform = GetActorTransform();

	if (Health)
	{
		Health->Team = ESlimeTeam::Enemy;
		Health->bDestroyOnDeath = true;
		Health->bRegenOnDeath = false;
		Health->MaxHP = MaxHP;
		if (SavedHP > 0.f)
		{
			Health->CurrentHP = FMath::Clamp(SavedHP, 1.f, MaxHP);
			Health->OnHealthChanged.Broadcast(Health->CurrentHP, Health->MaxHP);
		}
		else
		{
			Health->ResetHP();
		}
		Health->OnDied.AddDynamic(this, &AEnemyCharacter::HandleDied);
	}

	Super::BeginPlay();
	RebuildMeshParts();

	if (HealthBar)
	{
		HealthBar->InitWidget();
		if (USlimeWorldHealthBar* Bar = Cast<USlimeWorldHealthBar>(HealthBar->GetWidget()))
		{
			Bar->SetHealth(Health);
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (UEnemyPresenceSubsystem* PresenceSys = World->GetSubsystem<UEnemyPresenceSubsystem>())
		{
			PresenceSys->RegisterEnemy(this);
			bPresenceRegistered = true;
		}
	}
}

void AEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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

void AEnemyCharacter::RebuildMeshParts()
{
	ClearGeneratedParts();

	if (USkeletalMesh* Primary = PrimarySkeletalMesh.LoadSynchronous())
	{
		GetMesh()->SetSkeletalMesh(Primary);
		GetMesh()->SetHiddenInGame(false);
		GetMesh()->SetVisibility(true);
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
}

bool AEnemyCharacter::CanBeLockedOn() const
{
	if (Presence == EEnemyPresence::Sleep || Presence == EEnemyPresence::Despawned)
	{
		return false;
	}
	return Health && Health->IsAlive();
}

FVector AEnemyCharacter::GetLockOnLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, 60.f);
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
	if (UQuestObjectiveComponent* Objective = FindComponentByClass<UQuestObjectiveComponent>())
	{
		Objective->TryContribute();
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
	if (UAnimMontage* Montage = DeathMontage.LoadSynchronous())
	{
		if (UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			Anim->Montage_Play(Montage);
		}
	}
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
			Move->DisableMovement();
		}
		else
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}

	RefreshWorldHealthBarVisibility();

	SetActorHiddenInGame(Presence == EEnemyPresence::Despawned);
	SetActorEnableCollision(Presence != EEnemyPresence::Despawned);
}
