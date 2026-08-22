// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyPresenceSubsystem.h"

#include "EnemyCharacter.h"
#include "EnemyCombatComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SlimeHealthComponent.h"
#include "SlimeLockOnComponent.h"
#include "GameFramework/Pawn.h"

void UEnemyPresenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	HeartbeatRemaining = 0.f;
	Cursor = 0;
}

void UEnemyPresenceSubsystem::Deinitialize()
{
	Enemies.Reset();
	Despawned.Reset();
	Super::Deinitialize();
}

TStatId UEnemyPresenceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEnemyPresenceSubsystem, STATGROUP_Tickables);
}

void UEnemyPresenceSubsystem::RegisterEnemy(AEnemyCharacter* Enemy)
{
	if (!Enemy)
	{
		return;
	}
	Enemies.AddUnique(Enemy);
}

void UEnemyPresenceSubsystem::UnregisterEnemy(AEnemyCharacter* Enemy)
{
	Enemies.RemoveAll([Enemy](const TWeakObjectPtr<AEnemyCharacter>& Ptr)
	{
		return !Ptr.IsValid() || Ptr.Get() == Enemy;
	});
}

void UEnemyPresenceSubsystem::Tick(float DeltaTime)
{
	HeartbeatRemaining -= DeltaTime;
	if (HeartbeatRemaining > 0.f)
	{
		return;
	}
	HeartbeatRemaining = HeartbeatInterval;
	ProcessHeartbeat();
}

void UEnemyPresenceSubsystem::ProcessHeartbeat()
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player)
	{
		return;
	}
	const FVector PlayerLoc = Player->GetActorLocation();
	const USlimeLockOnComponent* Lock = Player->FindComponentByClass<USlimeLockOnComponent>();
	AActor* LockedTarget = Lock ? Lock->GetLockedTarget() : nullptr;

	Enemies.RemoveAll([](const TWeakObjectPtr<AEnemyCharacter>& Ptr) { return !Ptr.IsValid(); });

	struct FCandidate
	{
		AEnemyCharacter* Enemy = nullptr;
		float DistSq = 0.f;
	};

	const int32 Count = Enemies.Num();
	if (Count == 0)
	{
		ProcessDespawnRecords(PlayerLoc);
		return;
	}

	TArray<FCandidate> ActivePool;
	ActivePool.Reserve(Count);

	for (int32 Index = Enemies.Num() - 1; Index >= 0; --Index)
	{
		AEnemyCharacter* Enemy = Enemies[Index].Get();
		if (!Enemy || Enemy->IsInDeathSequence())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(PlayerLoc, Enemy->GetActorLocation());
		EEnemyPresence Desired = EvaluatePresence(Enemy, DistSq, Enemy->GetEnemyPresence());

		const bool bForceActive = (Enemy->GetEnemyCombat() && Enemy->GetEnemyCombat()->IsAttacking())
			|| (LockedTarget == Enemy);
		if (bForceActive)
		{
			Desired = EEnemyPresence::Active;
		}

		if (Desired == EEnemyPresence::Despawned && Enemy->bAllowDespawn)
		{
			FEnemyDespawnRecord Rec;
			Rec.Class = Enemy->GetClass();
			Rec.Transform = Enemy->GetSpawnTransform();
			Rec.RemainingHP = Enemy->GetEnemyHealth() ? Enemy->GetEnemyHealth()->CurrentHP : -1.f;
			Rec.bAllowDespawn = true;
			Rec.ActiveRangeOverride = Enemy->ActiveRangeOverride;
			Rec.SleepRangeOverride = Enemy->SleepRangeOverride;
			Rec.DespawnRangeOverride = Enemy->DespawnRangeOverride;
			Despawned.Add(Rec);
			UnregisterEnemy(Enemy);
			Enemy->Destroy();
			continue;
		}

		if (Desired == EEnemyPresence::Active)
		{
			ActivePool.Add({Enemy, DistSq});
		}
		else
		{
			Enemy->SetEnemyPresence(Desired);
		}

		Enemy->RefreshWorldHealthBarVisibility(Player, LockedTarget);
	}

	ActivePool.Sort([](const FCandidate& A, const FCandidate& B) { return A.DistSq < B.DistSq; });
	for (int32 Index = 0; Index < ActivePool.Num(); ++Index)
	{
		AEnemyCharacter* Enemy = ActivePool[Index].Enemy;
		if (!Enemy)
		{
			continue;
		}
		Enemy->SetEnemyPresence(Index < MaxActiveCombat ? EEnemyPresence::Active : EEnemyPresence::Idle);
	}

	ProcessDespawnRecords(PlayerLoc);
}

EEnemyPresence UEnemyPresenceSubsystem::EvaluatePresence(AEnemyCharacter* Enemy, float DistSq, EEnemyPresence Current) const
{
	const float ActiveR = ResolveActiveRange(Enemy);
	const float IdleR = FMath::Max(IdleRange, ActiveR + 100.f);
	const float SleepR = ResolveSleepRange(Enemy);
	const float DespawnR = ResolveDespawnRange(Enemy);

	const float ActiveIn = ActiveR;
	const float ActiveOut = ActiveR * 0.9f;
	const float IdleIn = IdleR;
	const float IdleOut = IdleR * 0.9f;
	const float SleepIn = SleepR;
	const float SleepOut = SleepR * 0.9f;

	auto InBand = [](float DistSqValue, float Radius) { return DistSqValue <= FMath::Square(Radius); };

	if (Enemy->bAllowDespawn && DistSq > FMath::Square(DespawnR))
	{
		return EEnemyPresence::Despawned;
	}

	switch (Current)
	{
	case EEnemyPresence::Active:
		if (!InBand(DistSq, IdleIn))
		{
			return InBand(DistSq, SleepIn) ? EEnemyPresence::Idle : EEnemyPresence::Sleep;
		}
		if (!InBand(DistSq, ActiveIn))
		{
			return EEnemyPresence::Idle;
		}
		return EEnemyPresence::Active;

	case EEnemyPresence::Idle:
		if (InBand(DistSq, ActiveOut))
		{
			return EEnemyPresence::Active;
		}
		if (!InBand(DistSq, SleepIn))
		{
			return EEnemyPresence::Sleep;
		}
		return EEnemyPresence::Idle;

	case EEnemyPresence::Sleep:
		if (InBand(DistSq, IdleOut))
		{
			return InBand(DistSq, ActiveOut) ? EEnemyPresence::Active : EEnemyPresence::Idle;
		}
		return EEnemyPresence::Sleep;

	default:
		if (InBand(DistSq, ActiveOut))
		{
			return EEnemyPresence::Active;
		}
		if (InBand(DistSq, IdleOut))
		{
			return EEnemyPresence::Idle;
		}
		if (InBand(DistSq, SleepOut))
		{
			return EEnemyPresence::Sleep;
		}
		return EEnemyPresence::Sleep;
	}
}

float UEnemyPresenceSubsystem::ResolveActiveRange(const AEnemyCharacter* Enemy) const
{
	return (Enemy && Enemy->ActiveRangeOverride > 0.f) ? Enemy->ActiveRangeOverride : ActiveRange;
}

float UEnemyPresenceSubsystem::ResolveSleepRange(const AEnemyCharacter* Enemy) const
{
	return (Enemy && Enemy->SleepRangeOverride > 0.f) ? Enemy->SleepRangeOverride : SleepRange;
}

float UEnemyPresenceSubsystem::ResolveDespawnRange(const AEnemyCharacter* Enemy) const
{
	return (Enemy && Enemy->DespawnRangeOverride > 0.f) ? Enemy->DespawnRangeOverride : DespawnRange;
}

void UEnemyPresenceSubsystem::ProcessDespawnRecords(const FVector& PlayerLoc)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (int32 Index = Despawned.Num() - 1; Index >= 0; --Index)
	{
		const FEnemyDespawnRecord& Rec = Despawned[Index];
		if (!Rec.Class)
		{
			Despawned.RemoveAtSwap(Index);
			continue;
		}
		if (FVector::DistSquared(PlayerLoc, Rec.Transform.GetLocation()) > FMath::Square(RespawnRange))
		{
			continue;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (AEnemyCharacter* Spawned = World->SpawnActor<AEnemyCharacter>(Rec.Class, Rec.Transform, Params))
		{
			Spawned->bAllowDespawn = Rec.bAllowDespawn;
			Spawned->ActiveRangeOverride = Rec.ActiveRangeOverride;
			Spawned->SleepRangeOverride = Rec.SleepRangeOverride;
			Spawned->DespawnRangeOverride = Rec.DespawnRangeOverride;
			Spawned->SetSavedHP(Rec.RemainingHP);
			if (Spawned->GetEnemyHealth() && Rec.RemainingHP > 0.f)
			{
				Spawned->GetEnemyHealth()->CurrentHP = Rec.RemainingHP;
				Spawned->GetEnemyHealth()->OnHealthChanged.Broadcast(
					Spawned->GetEnemyHealth()->CurrentHP, Spawned->GetEnemyHealth()->MaxHP);
			}
		}
		Despawned.RemoveAtSwap(Index);
	}
}
