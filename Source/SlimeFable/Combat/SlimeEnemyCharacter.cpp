// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeEnemyCharacter.h"

#include "Components/WidgetComponent.h"
#include "SlimeAIController.h"
#include "SlimeElementComponent.h"
#include "SlimeHealthComponent.h"
#include "SlimeWorldHealthBar.h"

ASlimeEnemyCharacter::ASlimeEnemyCharacter()
{
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = ASlimeAIController::StaticClass();

	HealthBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBar->SetupAttachment(RootComponent);
	HealthBar->SetRelativeLocation(FVector(0.f, 0.f, 72.f));
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
	return true;
}

FVector ASlimeEnemyCharacter::GetLockOnLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, 40.f);
}
