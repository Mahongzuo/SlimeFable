#include "EnemyEncounterSubsystem.h"

#include "EnemyCharacter.h"
#include "SlimeHealthComponent.h"
#include "EngineUtils.h"

void UEnemyEncounterSubsystem::CleanupExpiredSlots() const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	ActiveSlots.RemoveAll([Now](const FEnemyAttackSlot& Slot)
	{
		return !Slot.Owner.IsValid() || Slot.ExpireAt <= Now;
	});
}

bool UEnemyEncounterSubsystem::TryAcquireAttackSlot(AEnemyCharacter* Enemy, EEnemyCombatRole Role, float Duration)
{
	if (!Enemy || !GetWorld())
	{
		return false;
	}
	CleanupExpiredSlots();
	for (const FEnemyAttackSlot& Slot : ActiveSlots)
	{
		if (Slot.Owner.Get() == Enemy)
		{
			return true;
		}
	}
	const int32 MaxForRole = Role == EEnemyCombatRole::Commander ? 1
		: (Role == EEnemyCombatRole::Suppressor ? 1 : 1);
	if (GetActiveAttackCount(Role) >= MaxForRole)
	{
		return false;
	}
	FEnemyAttackSlot& NewSlot = ActiveSlots.AddDefaulted_GetRef();
	NewSlot.Owner = Enemy;
	NewSlot.Role = Role;
	NewSlot.ExpireAt = GetWorld()->GetTimeSeconds() + FMath::Max(Duration, 0.2f);
	return true;
}

void UEnemyEncounterSubsystem::ReleaseAttackSlot(AEnemyCharacter* Enemy)
{
	if (!Enemy)
	{
		return;
	}
	ActiveSlots.RemoveAll([Enemy](const FEnemyAttackSlot& Slot) { return Slot.Owner.Get() == Enemy; });
}

int32 UEnemyEncounterSubsystem::GetActiveAttackCount(EEnemyCombatRole Role) const
{
	CleanupExpiredSlots();
	int32 Count = 0;
	for (const FEnemyAttackSlot& Slot : ActiveSlots)
	{
		if (Slot.Role == Role)
		{
			++Count;
		}
	}
	return Count;
}

int32 UEnemyEncounterSubsystem::GetBossPhase() const
{
	if (!GetWorld())
	{
		return 1;
	}
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		const AEnemyCharacter* Enemy = *It;
		if (!Enemy || !Enemy->GetClass()->GetName().Contains(TEXT("Emperor")))
		{
			continue;
		}
		const USlimeHealthComponent* Health = Enemy->GetEnemyHealth();
		const float Ratio = Health && Health->MaxHP > 0.f ? Health->CurrentHP / Health->MaxHP : 1.f;
		return Ratio > 0.7f ? 1 : (Ratio > 0.35f ? 2 : 3);
	}
	return 1;
}

float UEnemyEncounterSubsystem::GetPressure() const
{
	CleanupExpiredSlots();
	return FMath::Clamp(static_cast<float>(ActiveSlots.Num()) / 3.f, 0.f, 1.f);
}
