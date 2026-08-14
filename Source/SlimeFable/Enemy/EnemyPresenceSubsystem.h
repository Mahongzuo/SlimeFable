// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnemyCombatTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "EnemyPresenceSubsystem.generated.h"

class AEnemyCharacter;

USTRUCT()
struct FEnemyDespawnRecord
{
	GENERATED_BODY()

	UPROPERTY()
	TSubclassOf<AEnemyCharacter> Class;

	UPROPERTY()
	FTransform Transform = FTransform::Identity;

	UPROPERTY()
	float RemainingHP = -1.f;

	UPROPERTY()
	bool bAllowDespawn = false;

	UPROPERTY()
	float ActiveRangeOverride = 0.f;

	UPROPERTY()
	float SleepRangeOverride = 0.f;

	UPROPERTY()
	float DespawnRangeOverride = 0.f;
};

UCLASS()
class SLIMEFABLE_API UEnemyPresenceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

	void RegisterEnemy(AEnemyCharacter* Enemy);
	void UnregisterEnemy(AEnemyCharacter* Enemy);

	UPROPERTY(EditAnywhere, Category = "Presence", meta = (ClampMin = "100.0", Units = "cm"))
	float ActiveRange = 2500.f;

	UPROPERTY(EditAnywhere, Category = "Presence", meta = (ClampMin = "100.0", Units = "cm"))
	float IdleRange = 5000.f;

	UPROPERTY(EditAnywhere, Category = "Presence", meta = (ClampMin = "100.0", Units = "cm"))
	float SleepRange = 15000.f;

	UPROPERTY(EditAnywhere, Category = "Presence", meta = (ClampMin = "100.0", Units = "cm"))
	float DespawnRange = 15000.f;

	UPROPERTY(EditAnywhere, Category = "Presence", meta = (ClampMin = "100.0", Units = "cm"))
	float RespawnRange = 8000.f;

	UPROPERTY(EditAnywhere, Category = "Presence", meta = (ClampMin = "1"))
	int32 MaxActiveCombat = 8;

	UPROPERTY(EditAnywhere, Category = "Presence", meta = (ClampMin = "0.05", Units = "s"))
	float HeartbeatInterval = 0.2f;

protected:
	void ProcessHeartbeat();
	void ProcessDespawnRecords(const FVector& PlayerLoc);
	EEnemyPresence EvaluatePresence(AEnemyCharacter* Enemy, float DistSq, EEnemyPresence Current) const;
	float ResolveActiveRange(const AEnemyCharacter* Enemy) const;
	float ResolveSleepRange(const AEnemyCharacter* Enemy) const;
	float ResolveDespawnRange(const AEnemyCharacter* Enemy) const;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AEnemyCharacter>> Enemies;

	UPROPERTY(Transient)
	TArray<FEnemyDespawnRecord> Despawned;

	float HeartbeatRemaining = 0.f;
	int32 Cursor = 0;
};
