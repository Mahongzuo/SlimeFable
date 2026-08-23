#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EnemyEncounterSubsystem.generated.h"

class AEnemyCharacter;

UENUM(BlueprintType)
enum class EEnemyCombatRole : uint8
{
	Chaser,
	Duelist,
	Suppressor,
	Commander
};

USTRUCT()
struct FEnemyAttackSlot
{
	GENERATED_BODY()
	TWeakObjectPtr<AEnemyCharacter> Owner;
	EEnemyCombatRole Role = EEnemyCombatRole::Duelist;
	float ExpireAt = 0.f;
};

UCLASS()
class SLIMEFABLE_API UEnemyEncounterSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	bool TryAcquireAttackSlot(AEnemyCharacter* Enemy, EEnemyCombatRole Role, float Duration);
	void ReleaseAttackSlot(AEnemyCharacter* Enemy);
	int32 GetBossPhase() const;
	int32 GetActiveAttackCount(EEnemyCombatRole Role) const;
	float GetPressure() const;

private:
	void CleanupExpiredSlots() const;
	mutable TArray<FEnemyAttackSlot> ActiveSlots;
};
