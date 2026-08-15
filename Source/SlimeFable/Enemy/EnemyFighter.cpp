// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyFighter.h"

#include "Components/SphereComponent.h"
#include "EnemyFighterAIController.h"
#include "GameFramework/CharacterMovementComponent.h"

AEnemyFighter::AEnemyFighter()
{
	AIControllerClass = AEnemyFighterAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	MaxHP = 220.f;
	bAllowDespawn = false;
	DetectRange = 1000.f;
	LeashRange = 1500.f;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = WalkSpeed;
		Move->bOrientRotationToMovement = true;
	}

	DetectRangeVisual = CreateDefaultSubobject<USphereComponent>(TEXT("DetectRangeVisual"));
	DetectRangeVisual->SetupAttachment(RootComponent);
	DetectRangeVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DetectRangeVisual->SetHiddenInGame(true);
	DetectRangeVisual->ShapeColor = FColor(80, 180, 255);
	DetectRangeVisual->bDrawOnlyIfSelected = true;

	LeashRangeVisual = CreateDefaultSubobject<USphereComponent>(TEXT("LeashRangeVisual"));
	LeashRangeVisual->SetupAttachment(RootComponent);
	LeashRangeVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeashRangeVisual->SetHiddenInGame(true);
	LeashRangeVisual->ShapeColor = FColor(255, 180, 60);
	LeashRangeVisual->bDrawOnlyIfSelected = true;

	EnemyCombat::FillDefaultFighterMoves(Moves);
}

void AEnemyFighter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SyncRangeVisuals();
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = WalkSpeed;
	}
}

void AEnemyFighter::BeginPlay()
{
	if (LeashRange < DetectRange)
	{
		LeashRange = DetectRange;
	}
	Super::BeginPlay();
	SyncRangeVisuals();
	if (Moves.Num() == 0)
	{
		EnemyCombat::FillDefaultFighterMoves(Moves);
	}
}

bool AEnemyFighter::IsInCombat() const
{
	if (const AEnemyFighterAIController* AI = Cast<AEnemyFighterAIController>(GetController()))
	{
		return AI->IsEngaged();
	}
	return Super::IsInCombat();
}

void AEnemyFighter::OnRestoredToSpawn()
{
	if (AEnemyFighterAIController* AI = Cast<AEnemyFighterAIController>(GetController()))
	{
		AI->ReturnToIdle();
	}
}

void AEnemyFighter::SyncRangeVisuals()
{
	if (DetectRangeVisual)
	{
		DetectRangeVisual->SetSphereRadius(DetectRange);
	}
	if (LeashRangeVisual)
	{
		LeashRangeVisual->SetSphereRadius(LeashRange);
	}
}
