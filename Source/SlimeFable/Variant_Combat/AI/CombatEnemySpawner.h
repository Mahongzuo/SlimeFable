// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatActivatable.h"
#include "CombatEnemySpawner.generated.h"

class UCapsuleComponent;
class UArrowComponent;
class ACombatEnemy;

/**
 *  A basic Actor in charge of spawning Enemy Characters and monitoring their deaths.
 *  Enemies will be spawned one by one, and the spawner will wait until the enemy dies before spawning a new one.
 *  The spawner can be remotely activated through the ICombatActivatable interface
 *  When the last spawned enemy dies, the spawner can also activate other ICombatActivatables
 */
UCLASS(abstract)
class ACombatEnemySpawner : public AActor, public ICombatActivatable
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCapsuleComponent* SpawnCapsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UArrowComponent* SpawnDirection;

protected:

	/** Type of enemy to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner")
	TSubclassOf<ACombatEnemy> EnemyClass;

	/** If true, the first enemy will be spawned as soon as the game starts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner")
	bool bShouldSpawnEnemiesImmediately = true;

	/** Time to wait before spawning the first enemy on game start */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner", meta = (ClampMin = 0, ClampMax = 10))
	float InitialSpawnDelay = 5.0f;

	/** Number of enemies to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner", meta = (ClampMin = 0, ClampMax = 100))
	int32 SpawnCount = 1;

	/** Time to wait before spawning the next enemy after the current one dies */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy Spawner", meta = (ClampMin = 0, ClampMax = 10))
	float RespawnDelay = 5.0f;

	/** Time to wait after this spawner is depleted before activating the actor list */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Activation", meta = (ClampMin = 0, ClampMax = 10))
	float ActivationDelay = 1.0f;

	/** List of actors to activate after the last enemy dies */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Activation")
	TArray<AActor*> ActorsToActivateWhenDepleted;

	/** Flag to ensure this is only activated once */
	bool bHasBeenActivated = false;

	/** Timer to spawn enemies after a delay */
	FTimerHandle SpawnTimer;

public:	
	
	/** Constructor */
	ACombatEnemySpawner();

public:

	/** Initialization */
	virtual void BeginPlay() override;

	/** Cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

protected:

	/** Spawn an enemy and subscribe to its death event */
	void SpawnEnemy();

	/** Called when the spawned enemy has died */
	UFUNCTION()
	void OnEnemyDied();

	/** Called after the last spawned enemy has died */
	void SpawnerDepleted();

public:

	// ~begin ICombatActivatable interface

	/** Toggles the Spawner */
	UFUNCTION(BlueprintCallable, Category="Activatable")
	virtual void ToggleInteraction(AActor* ActivationInstigator) override;

	/** Activates the Spawner */
	UFUNCTION(BlueprintCallable, Category="Activatable")
	virtual void ActivateInteraction(AActor* ActivationInstigator) override;

	/** Deactivates the Spawner */
	UFUNCTION(BlueprintCallable, Category="Activatable")
	virtual void DeactivateInteraction(AActor* ActivationInstigator) override;

	// ~end IActivatable interface
};
