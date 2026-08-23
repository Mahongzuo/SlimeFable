#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyEncounterDefinition.h"
#include "EnemyEncounterDirector.generated.h"

class AEnemyCharacter;
class UGameplayEffect;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEnemyEncounterPhaseChanged, int32, PhaseIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEnemyEncounterCompleted);

UCLASS(Blueprintable, meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API AEnemyEncounterDirector : public AActor
{
	GENERATED_BODY()

public:
	AEnemyEncounterDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Encounter",
		meta = (ToolTip = "1945 Boss 遭遇配置；为空时使用默认血量阶段和现有攻击权系统。"))
	TObjectPtr<UEnemyEncounterDefinition> EncounterDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Encounter",
		meta = (ToolTip = "自动发现关卡中带 Combat.Role 标签的敌人。默认开启。"))
	bool bAutoDiscoverEnemies = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Encounter",
		meta = (ClampMin = "0.05", Units = "s", ToolTip = "扫描敌人和阶段的间隔；默认 0.25 秒。"))
	float UpdateInterval = 0.25f;

	UPROPERTY(BlueprintAssignable, Category = "Encounter")
	FEnemyEncounterPhaseChanged OnPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Encounter")
	FEnemyEncounterCompleted OnEncounterCompleted;

	UFUNCTION(BlueprintPure, Category = "Encounter")
	AEnemyCharacter* GetBoss() const { return Boss.Get(); }

	UFUNCTION(BlueprintPure, Category = "Encounter")
	int32 GetCurrentPhase() const { return CurrentPhase; }

	UFUNCTION(BlueprintPure, Category = "Encounter")
	float GetEncounterPressure() const;

	UFUNCTION(BlueprintCallable, Category = "Encounter")
	void RegisterEnemy(AEnemyCharacter* Enemy);

	UFUNCTION(BlueprintCallable, Category = "Encounter")
	void UnregisterEnemy(AEnemyCharacter* Enemy);

	UFUNCTION(BlueprintCallable, Category = "Encounter")
	bool RequestCombatPosition(AEnemyCharacter* Enemy, EEnemyCombatPosition Position);

protected:
	void DiscoverEnemies();
	void UpdateBossPhase();
	void ApplyPhase(const FEnemyEncounterPhaseDef* Phase);
	void SpawnPhaseSupports(const FEnemyEncounterPhaseDef& Phase);
	void BroadcastCombatEvent(const FGameplayTag& EventTag, int32 PhaseIndex) const;
	void CheckCompletion();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AEnemyCharacter>> RegisteredEnemies;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AEnemyCharacter>> SpawnedSupports;

	TWeakObjectPtr<AEnemyCharacter> Boss;
	int32 CurrentPhase = 0;
	float UpdateRemaining = 0.f;
	bool bCompleted = false;
};
