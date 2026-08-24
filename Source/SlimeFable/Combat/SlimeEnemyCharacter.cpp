// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeEnemyCharacter.h"

#include "Components/WidgetComponent.h"
#include "SlimeAIController.h"
#include "SlimeElementComponent.h"
#include "SlimeHealthComponent.h"
#include "SlimeLockOnComponent.h"
#include "SlimeWorldHealthBar.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

ASlimeEnemyCharacter::ASlimeEnemyCharacter()
{
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = ASlimeAIController::StaticClass();

	HealthBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBar->SetupAttachment(RootComponent);
	HealthBar->SetRelativeLocation(FVector(0.f, 0.f, HealthBarZOffset));
	PrimaryActorTick.bCanEverTick = true;
	HealthBar->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBar->SetDrawAtDesiredSize(false);
	HealthBar->SetDrawSize(FVector2D(96.f, 12.f));
	HealthBar->SetPivot(FVector2D(0.5f, 1.f));
	HealthBar->SetWidgetClass(USlimeWorldHealthBar::StaticClass());
	HealthBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASlimeEnemyCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyStartingElement();
	ApplyHealthBarOffset();
}

void ASlimeEnemyCharacter::ApplyHealthBarOffset()
{
	if (HealthBar)
	{
		HealthBar->SetRelativeLocation(FVector(0.f, 0.f, HealthBarZOffset));
	}
}

void ASlimeEnemyCharacter::RefreshWorldHealthBarVisibility()
{
	if (!HealthBar)
	{
		return;
	}

	const USlimeHealthComponent* Health = GetSlimeHealth();
	bool bShow = Health && Health->IsAlive()
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

void ASlimeEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshWorldHealthBarVisibility();
}

void ASlimeEnemyCharacter::BeginPlay()
{
	if (USlimeHealthComponent* Health = GetSlimeHealth())
	{
		Health->Team = ESlimeTeam::Enemy;
		Health->bDestroyOnDeath = !bStationaryTraining;
		Health->bRegenOnDeath = bStationaryTraining;
		if (bStationaryTraining)
		{
			Health->MaxHP = 400.f;
			Health->ResetHP();
		}
		Health->OnDied.AddDynamic(this, &ASlimeEnemyCharacter::HandleEnemyDied);
	}

	Super::BeginPlay();

	if (USlimeElementComponent* Element = GetSlimeElement())
	{
		Element->SetElement(StartingElement, true);
	}

	if (HealthBar)
	{
		HealthBar->InitWidget();
		if (USlimeWorldHealthBar* Bar = Cast<USlimeWorldHealthBar>(HealthBar->GetWidget()))
		{
			Bar->SetHealth(GetSlimeHealth());
		}
	}
}

void ASlimeEnemyCharacter::HandleEnemyDied()
{
	if (HealthBar)
	{
		HealthBar->SetVisibility(false);
		HealthBar->SetHiddenInGame(true);
	}
}

void ASlimeEnemyCharacter::ApplyStartingElement()
{
	if (USlimeElementComponent* Element = GetSlimeElement())
	{
		Element->CurrentElement = StartingElement;
	}
}

bool ASlimeEnemyCharacter::CanBeLockedOn() const
{
	if (const USlimeHealthComponent* Health = GetSlimeHealth())
	{
		return Health->IsAlive();
	}
	return false;
}

FVector ASlimeEnemyCharacter::GetLockOnLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, 40.f);
}
