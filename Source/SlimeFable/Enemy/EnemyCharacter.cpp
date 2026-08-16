// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
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
		Health->bDestroyOnDeath = false;
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
		if (UClass* AnimClass = PrimaryAnimClass.LoadSynchronous())
		{
			GetMesh()->SetAnimInstanceClass(AnimClass);
		}
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
	if (bDeathSequence)
	{
		return;
	}
	bDeathSequence = true;

	if (UQuestObjectiveComponent* Objective = FindComponentByClass<UQuestObjectiveComponent>())
	{
		Objective->TryContribute();
	}
	DropSouvenirReward();

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
		if (AAIController* AIC = Cast<AAIController>(AI))
		{
			if (UBrainComponent* Brain = AIC->GetBrainComponent())
			{
				Brain->StopLogic(TEXT("Death"));
			}
		}
	}

	PlayDeathMontageThenDissolve();
}

void AEnemyCharacter::PlayDeathMontageThenDissolve()
{
	UAnimMontage* Montage = DeathMontage.LoadSynchronous();
	USkeletalMeshComponent* Skel = GetMesh();
	UAnimInstance* Anim = Skel ? Skel->GetAnimInstance() : nullptr;
	if (Montage && Anim)
	{
		const float Played = Anim->Montage_Play(Montage);
		FOnMontageBlendingOutStarted BlendOut;
		BlendOut.BindUObject(this, &AEnemyCharacter::HandleDeathMontageEnded);
		Anim->Montage_SetBlendingOutDelegate(BlendOut, Montage);
		if (UWorld* World = GetWorld())
		{
			const float Wait = FMath::Max(Played, Montage->GetPlayLength()) + 0.05f;
			World->GetTimerManager().SetTimer(
				DeathMontageFallbackTimer,
				this,
				&AEnemyCharacter::StartDeathDissolve,
				Wait,
				false);
		}
		return;
	}
	StartDeathDissolve();
}

void AEnemyCharacter::HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		if (Montage)
		{
			Anim->Montage_Pause(Montage);
			Anim->Montage_SetPosition(Montage, Montage->GetPlayLength());
		}
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
		const FVector Head = GetMesh()
			? GetMesh()->GetSocketLocation(TEXT("head"))
			: GetActorLocation() + FVector(0.f, 0.f, 90.f);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FX, Head, GetActorRotation());
	}

	if (UMaterialInterface* DissolveMat = DeathDissolveMaterial.LoadSynchronous())
	{
		if (USkeletalMeshComponent* Skel = GetMesh())
		{
			Skel->SetOverlayMaterial(DissolveMat);
		}
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
	auto FadeMesh = [Alpha](UMeshComponent* Comp)
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
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Base);
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
		Comp->SetVisibility(Alpha > 0.05f);
	};

	FadeMesh(GetMesh());
	FadeMesh(PlaceholderMesh);
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
			Move->DisableMovement();
		}
		else
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}

	RefreshWorldHealthBarVisibility();

	if (!bDeathSequence)
	{
		SetActorHiddenInGame(Presence == EEnemyPresence::Despawned);
		SetActorEnableCollision(Presence != EEnemyPresence::Despawned);
	}
}
