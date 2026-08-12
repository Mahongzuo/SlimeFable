// Copyright Epic Games, Inc. All Rights Reserved.


#include "CombatEnemySpawner.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ArrowComponent.h"
#include "TimerManager.h"
#include "CombatEnemy.h"

ACombatEnemySpawner::ACombatEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	// create the root
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the reference spawn capsule
	SpawnCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Spawn Capsule"));
	SpawnCapsule->SetupAttachment(RootComponent);

	SpawnCapsule->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	SpawnCapsule->SetCapsuleSize(35.0f, 90.0f);
	SpawnCapsule->SetCollisionProfileName(FName("NoCollision"));

	SpawnDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("Spawn Direction"));
	SpawnDirection->SetupAttachment(RootComponent);
}

void ACombatEnemySpawner::BeginPlay()
{
	Super::BeginPlay();
	
	// should we spawn an enemy right away?
	if (bShouldSpawnEnemiesImmediately)
	{
		// schedule the first enemy spawn
		GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &ACombatEnemySpawner::SpawnEnemy, InitialSpawnDelay);
	}

}

void ACombatEnemySpawner::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the spawn timer
	GetWorld()->GetTimerManager().ClearTimer(SpawnTimer);
}

void ACombatEnemySpawner::SpawnEnemy()
{
	// ensure the enemy class is valid
	if (IsValid(EnemyClass))
	{
		// spawn the enemy at the reference capsule's transform
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ACombatEnemy* SpawnedEnemy = GetWorld()->SpawnActor<ACombatEnemy>(EnemyClass, SpawnCapsule->GetComponentTransform(), SpawnParams);

		// was the enemy successfully created?
		if (SpawnedEnemy)
		{
			// subscribe to the death delegate
			SpawnedEnemy->OnEnemyDied.AddDynamic(this, &ACombatEnemySpawner::OnEnemyDied);
		}
	}
}

void ACombatEnemySpawner::OnEnemyDied()
{
	// decrease the spawn counter
	--SpawnCount;

	// is this the last enemy we should spawn?
	if (SpawnCount <= 0)
	{
		// schedule the activation on depleted message
		GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &ACombatEnemySpawner::SpawnerDepleted, ActivationDelay);
		return;
	}

	// schedule the next enemy spawn
	GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &ACombatEnemySpawner::SpawnEnemy, RespawnDelay);
}

void ACombatEnemySpawner::SpawnerDepleted()
{
	// process the actors to activate list
	for (AActor* CurrentActor : ActorsToActivateWhenDepleted)
	{
		// check if the actor is activatable
		if (ICombatActivatable* CombatActivatable = Cast<ICombatActivatable>(CurrentActor))
		{
			// activate the actor
			CombatActivatable->ActivateInteraction(this);
		}
	}
}

void ACombatEnemySpawner::ToggleInteraction(AActor* ActivationInstigator)
{
	// stub
}

void ACombatEnemySpawner::ActivateInteraction(AActor* ActivationInstigator)
{
	// ensure we're only activated once, and only if we've deferred enemy spawning
	if (bHasBeenActivated || bShouldSpawnEnemiesImmediately)
	{
		return;
	}

	// raise the activation flag
	bHasBeenActivated = true;

	// spawn the first enemy
	SpawnEnemy();
}

void ACombatEnemySpawner::DeactivateInteraction(AActor* ActivationInstigator)
{
	// stub
}
