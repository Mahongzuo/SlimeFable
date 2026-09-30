#include "SlimeEncounterMember.h"
#include "SlimeEncounterDirector.h"
#include "Combat/SlimeDevourTarget.h"
#include "Combat/SlimeHealthComponent.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/EnemyCombatComponent.h"
#include "Enemy/EnemyPresenceSubsystem.h"
#include "Enemy/GaspEnemyAIController.h"
#include "Enemy/LyraShooterEnemy.h"
#include "Enemy/LyraXinShooterEnemy.h"
#include "UObject/UnrealType.h"
#include "Quest/QuestObjectiveComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"

USlimeEncounterMember::USlimeEncounterMember() { PrimaryComponentTick.bCanEverTick = false; }

float USlimeEncounterMember::DamageMultiplier(const AActor* Actor)
{
	const auto* Member = Actor ? Actor->FindComponentByClass<USlimeEncounterMember>() : nullptr;
	return Member ? Member->GetDamageMultiplier() : 1.f;
}

float USlimeEncounterMember::GetDamageMultiplier() const
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	const auto* Target = SlimeDevourUtil::As(GetOwner());
	return (Pawn && Pawn->IsPlayerControlled()) || (Target && Target->IsMorphTarget()) ? 1.f : DamageMultiplierValue;
}

void USlimeEncounterMember::Initialize(ASlimeEncounterDirector* InDirector, FGuid InId, float HealthScale, float DamageScale, float HealthFraction, int32 HealthPhase)
{
	Director = InDirector;
	RecordId = InId;
	DamageMultiplierValue = DamageScale;
	auto* Target = SlimeDevourUtil::As(GetOwner());
	USlimeHealthComponent* Health = Target ? Target->GetEnemyHealth() : nullptr;
	if (!Health) return;
	if (auto* Enemy = Cast<AEnemyCharacter>(GetOwner()))
	{
		// This population owns lifetime and difficulty; the day-level subsystem must not respawn it too.
		if (auto* Presence = GetWorld()->GetSubsystem<UEnemyPresenceSubsystem>()) Presence->UnregisterEnemy(Enemy);
		Enemy->bAllowDespawn = false;
		Enemy->bSuppressOutOfCombatReset = true;
		Enemy->DebugStartHealthPercent = 0.f;
		Enemy->ApplyWeekDifficulty(2);
		Enemy->MaxHP = FMath::Max(1.f, Health->MaxHP * HealthScale);
		Enemy->SetEnemyPresence(EEnemyPresence::Active);
	}
	if (auto* Objective = GetOwner()->FindComponentByClass<UQuestObjectiveComponent>())
	{
		Objective->ChapterId = NAME_None;
		Objective->QuestId = NAME_None;
		Objective->BranchId = NAME_None;
		Objective->SetConsumed(true);
	}
	Health->bRegenOnDeath = false;
	float NewMaxHP = FMath::Max(1.f, Health->MaxHP * HealthScale);
	if (auto* Xin = Cast<ALyraXinShooterEnemy>(GetOwner()))
	{
		Xin->Phase1MaxHP *= HealthScale;
		Xin->Phase2MaxHP *= HealthScale;
		Xin->RestoreExplorationPhase(HealthPhase);
		NewMaxHP = Xin->GetHealthPhase() >= 2 ? Xin->Phase2MaxHP : Xin->Phase1MaxHP;
	}
	if (auto* Prop = FindFProperty<FFloatProperty>(GetOwner()->GetClass(), TEXT("MaxHP")))
		Prop->SetPropertyValue_InContainer(GetOwner(), NewMaxHP);
	Health->SetMaxAndRefill(NewMaxHP);
	Health->CurrentHP = FMath::Clamp(Health->MaxHP * HealthFraction, 1.f, Health->MaxHP);
	Health->OnHealthChanged.Broadcast(Health->CurrentHP, Health->MaxHP);
	if (auto* Lyra = Cast<ALyraShooterEnemy>(GetOwner())) Lyra->RefreshExplorationStats(DamageScale);
	LastHP = Health->CurrentHP;
	Health->OnDied.AddUniqueDynamic(this, &ThisClass::HandleDied);
	Health->OnHealthChanged.AddUniqueDynamic(this, &ThisClass::HandleHealth);
}

void USlimeEncounterMember::HandleDied()
{
	if (bDefeatReported) return;
	bDefeatReported = true;
	if (Director.IsValid()) Director->ReportDefeat(RecordId);
}

void USlimeEncounterMember::HandleHealth(float Current, float Max)
{
	if (Current < LastHP) { LastHitTime = GetWorld()->GetTimeSeconds(); SetSleeping(false); }
	LastHP = Current;
	if (Director.IsValid()) Director->MarkDirty();
}

bool USlimeEncounterMember::IsEngaged() const
{
	const auto* Target = SlimeDevourUtil::As(GetOwner());
	if (!Target || !Target->GetEnemyHealth() || !Target->GetEnemyHealth()->IsAlive()) return false;
	if (Target->IsDevourLocked() || Target->IsMorphTarget()) return true;
	if (GetWorld()->GetTimeSeconds() - LastHitTime < 10.f) return true;
	if (bSleeping) return false;
	if (Target->GetEnemyCombat() && Target->GetEnemyCombat()->IsAttacking()) return true;
	if (const auto* Enemy = Cast<AEnemyCharacter>(GetOwner())) return Enemy->IsInCombat();
	if (const auto* Pawn = Cast<APawn>(GetOwner()))
	{
		if (Pawn->IsPlayerControlled()) return true;
		if (const auto* AI = Cast<AGaspEnemyAIController>(Pawn->GetController())) return AI->IsEngaged();
	}
	// Lyra's original simple AI pursues its player continuously, including outside firing range.
	// Once woken, preserve that pursuit even if navigation is temporarily blocked.
	if (Cast<ALyraShooterEnemy>(GetOwner())) return UGameplayStatics::GetPlayerPawn(this, 0) != nullptr;
	return GetOwner()->GetVelocity().SizeSquared() > 100.f;
}

void USlimeEncounterMember::SetSleeping(bool bSleep)
{
	if (bSleeping == bSleep) return;
	auto* Pawn = Cast<APawn>(GetOwner());
	auto* Target = SlimeDevourUtil::As(GetOwner());
	if (bSleep && (!Target || Target->IsMorphTarget() || Target->IsDevourLocked() || Target->IsInDeathSequence() || (Pawn && Pawn->IsPlayerControlled()))) return;
	bSleeping = bSleep;
	if (bSleep)
	{
		TArray<AActor*> Actors;
		Actors.Add(GetOwner());
		TArray<AActor*> Attached;
		GetOwner()->GetAttachedActors(Attached, true, true);
		Actors.Append(Attached);
		if (Pawn && Pawn->GetController()) Actors.AddUnique(Pawn->GetController());
		if (Pawn && Pawn->GetMovementComponent()) Pawn->GetMovementComponent()->StopMovementImmediately();
		if (auto* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr)
		{
			AI->StopMovement();
			if (AI->BrainComponent) AI->BrainComponent->PauseLogic(TEXT("Exploration distance sleep"));
		}
		for (AActor* Actor : Actors)
		{
			if (!IsValid(Actor)) continue;
			ActorStates.Add({Actor, Actor->IsHidden(), Actor->IsActorTickEnabled(), Actor->GetActorEnableCollision()});
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
			Actor->SetActorTickEnabled(false);
			TInlineComponentArray<UActorComponent*> Components(Actor);
			for (UActorComponent* C : Components)
			{
				ComponentStates.Add({C, C->IsComponentTickEnabled()});
				C->SetComponentTickEnabled(false);
			}
		}
	}
	else
	{
		for (const auto& S : ActorStates) if (auto* A = S.Actor.Get())
		{
			A->SetActorHiddenInGame(S.bHidden); A->SetActorEnableCollision(S.bCollision); A->SetActorTickEnabled(S.bTick);
		}
		for (const auto& S : ComponentStates) if (auto* C = S.Component.Get()) C->SetComponentTickEnabled(S.bTick);
		ActorStates.Reset(); ComponentStates.Reset();
		if (auto* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr)
			if (AI->BrainComponent) AI->BrainComponent->ResumeLogic(TEXT("Exploration proximity wake"));
	}
}
