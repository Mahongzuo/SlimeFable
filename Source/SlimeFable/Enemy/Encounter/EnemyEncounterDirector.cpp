#include "EnemyEncounterDirector.h"

#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "EnemyCharacter.h"
#include "EnemyEncounterSubsystem.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "SlimeEnemyGameplayTags.h"
#include "SlimeHealthComponent.h"

AEnemyEncounterDirector::AEnemyEncounterDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
}

void AEnemyEncounterDirector::BeginPlay()
{
	Super::BeginPlay();
	if (bAutoDiscoverEnemies)
	{
		DiscoverEnemies();
	}
	UpdateBossPhase();
}

void AEnemyEncounterDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateRemaining -= DeltaSeconds;
	if (UpdateRemaining > 0.f)
	{
		return;
	}
	UpdateRemaining = FMath::Max(UpdateInterval, 0.05f);
	if (bAutoDiscoverEnemies)
	{
		DiscoverEnemies();
	}
	UpdateBossPhase();
	CheckCompletion();
}

void AEnemyEncounterDirector::DiscoverEnemies()
{
	RegisteredEnemies.RemoveAll([](const TObjectPtr<AEnemyCharacter>& Enemy)
	{
		return !IsValid(Enemy);
	});

	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		AEnemyCharacter* Enemy = *It;
		if (IsValid(Enemy) && !RegisteredEnemies.Contains(Enemy))
		{
			RegisterEnemy(Enemy);
		}
	}

	if (!Boss.IsValid())
	{
		for (AEnemyCharacter* Enemy : RegisteredEnemies)
		{
			if (Enemy && Enemy->GetCombatRole() == EEnemyCombatRole::Commander)
			{
				Boss = Enemy;
				break;
			}
		}
	}
}

void AEnemyEncounterDirector::RegisterEnemy(AEnemyCharacter* Enemy)
{
	if (!IsValid(Enemy))
	{
		return;
	}
	RegisteredEnemies.AddUnique(Enemy);
	if (!Boss.IsValid() && Enemy->GetCombatRole() == EEnemyCombatRole::Commander)
	{
		Boss = Enemy;
	}
}

void AEnemyEncounterDirector::UnregisterEnemy(AEnemyCharacter* Enemy)
{
	RegisteredEnemies.Remove(Enemy);
	SpawnedSupports.Remove(Enemy);
	if (Boss.Get() == Enemy)
	{
		Boss.Reset();
	}
}

void AEnemyEncounterDirector::UpdateBossPhase()
{
	AEnemyCharacter* BossActor = Boss.Get();
	if (!IsValid(BossActor))
	{
		return;
	}

	const float HealthRatio = BossActor->GetHealthPercent();
	int32 NewPhase = 1;
	if (EncounterDefinition && EncounterDefinition->Phases.Num() > 0)
	{
		NewPhase = 1;
		float BestThreshold = -1.f;
		for (const FEnemyEncounterPhaseDef& Phase : EncounterDefinition->Phases)
		{
			if (HealthRatio <= Phase.MaxHealthRatio && Phase.MaxHealthRatio >= BestThreshold)
			{
				NewPhase = Phase.PhaseIndex;
				BestThreshold = Phase.MaxHealthRatio;
			}
		}
	}
	else
	{
		NewPhase = HealthRatio > 0.7f ? 1 : (HealthRatio > 0.35f ? 2 : 3);
	}

	if (NewPhase != CurrentPhase)
	{
		CurrentPhase = NewPhase;
		ApplyPhase(EncounterDefinition ? EncounterDefinition->FindPhase(CurrentPhase) : nullptr);
	}
}

void AEnemyEncounterDirector::ApplyPhase(const FEnemyEncounterPhaseDef* Phase)
{
	BroadcastCombatEvent(SlimeEnemyTags::Event_PhaseChanged, CurrentPhase);
	OnPhaseChanged.Broadcast(CurrentPhase);
	if (Phase)
	{
		SpawnPhaseSupports(*Phase);
	}
}

void AEnemyEncounterDirector::SpawnPhaseSupports(const FEnemyEncounterPhaseDef& Phase)
{
	if (!GetWorld() || !Phase.bAllowSupportRefresh)
	{
		return;
	}
	for (int32 Index = 0; Index < Phase.SupportClasses.Num(); ++Index)
	{
		TSubclassOf<AEnemyCharacter> SupportClass = Phase.SupportClasses[Index];
		if (!SupportClass)
		{
			continue;
		}
		bool bAlreadyPresent = false;
		for (AEnemyCharacter* Existing : RegisteredEnemies)
		{
			if (Existing && Existing->IsA(SupportClass))
			{
				bAlreadyPresent = true;
				break;
			}
		}
		if (bAlreadyPresent)
		{
			continue;
		}
		FTransform SpawnTransform = GetActorTransform();
		if (Phase.SupportSpawnPoints.IsValidIndex(Index))
		{
			SpawnTransform = Phase.SupportSpawnPoints[Index] * GetActorTransform();
		}
		else
		{
			SpawnTransform.AddToTranslation(FVector(250.f + Index * 100.f, Index % 2 ? 180.f : -180.f, 0.f));
		}
		if (AEnemyCharacter* Support = GetWorld()->SpawnActorDeferred<AEnemyCharacter>(SupportClass, SpawnTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
		{
			Support->FinishSpawning(SpawnTransform);
			RegisterEnemy(Support);
			SpawnedSupports.Add(Support);
			BroadcastCombatEvent(SlimeEnemyTags::Event_SupportCalled, CurrentPhase);
		}
	}
}

void AEnemyEncounterDirector::BroadcastCombatEvent(const FGameplayTag& EventTag, int32 PhaseIndex) const
{
	for (AEnemyCharacter* Enemy : RegisteredEnemies)
	{
		if (Enemy && Enemy->GetEnemyAbilitySystem())
		{
			FGameplayEventData Payload;
			Payload.EventTag = EventTag;
			Payload.Target = Enemy;
			Payload.EventMagnitude = static_cast<float>(PhaseIndex);
			Enemy->GetEnemyAbilitySystem()->HandleGameplayEvent(EventTag, &Payload);
		}
	}
}

float AEnemyEncounterDirector::GetEncounterPressure() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const UEnemyEncounterSubsystem* Subsystem = World->GetSubsystem<UEnemyEncounterSubsystem>())
		{
			return Subsystem->GetPressure();
		}
	}
	return 0.f;
}

bool AEnemyEncounterDirector::RequestCombatPosition(AEnemyCharacter* Enemy, EEnemyCombatPosition Position)
{
	if (!IsValid(Enemy))
	{
		return false;
	}
	const FVector Origin = GetActorLocation();
	const FVector Offset = Position == EEnemyCombatPosition::Melee ? FVector(220.f, 0.f, 0.f)
		: (Position == EEnemyCombatPosition::Ranged ? FVector(-260.f, 0.f, 0.f)
		: (Position == EEnemyCombatPosition::Flank ? FVector(0.f, 320.f, 0.f) : FVector(-350.f, 0.f, 0.f)));
	Enemy->SetActorLocation(Origin + Offset, true);
	return true;
}

void AEnemyEncounterDirector::CheckCompletion()
{
	if (bCompleted || !Boss.IsValid())
	{
		return;
	}
	if (Boss->GetEnemyHealth() && !Boss->GetEnemyHealth()->IsAlive())
	{
		bCompleted = true;
		OnEncounterCompleted.Broadcast();
		if (EncounterDefinition && EncounterDefinition->CompletionRewardClass && GetWorld())
		{
			GetWorld()->SpawnActor<AActor>(EncounterDefinition->CompletionRewardClass, GetActorTransform());
		}
	}
}
