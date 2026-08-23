#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EnemyCombatTypes.h"
#include "EnemyEncounterDefinition.generated.h"

class AEnemyCharacter;

UENUM(BlueprintType)
enum class EEnemyCombatPosition : uint8
{
	Melee,
	Ranged,
	Flank,
	Retreat
};

USTRUCT(BlueprintType)
struct SLIMEFABLE_API FEnemyEncounterPhaseDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase",
		meta = (ToolTip = "阶段编号，从 1 开始；Boss 血量阈值会自动选择最高匹配阶段。"))
	int32 PhaseIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase",
		meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "进入该阶段所需的最高血量比例。1.0 为满血，0.35 表示血量低于 35% 时进入。"))
	float MaxHealthRatio = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase",
		meta = (ToolTip = "Boss 在本阶段可选的 Ability 定义；实际激活仍受距离、冷却和攻击权限制。"))
	TArray<FEnemyAbilityDef> BossAbilities;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase|Support",
		meta = (ToolTip = "进入本阶段时按顺序补入的支援敌人类。已存在同类支援时不会重复生成。"))
	TArray<TSubclassOf<AEnemyCharacter>> SupportClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase|Support",
		meta = (ToolTip = "支援生成点；为空时使用 Director 自身位置周围的环形点。"))
	TArray<FTransform> SupportSpawnPoints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase|Rules",
		meta = (ToolTip = "本阶段同时允许的攻击者数量。0 表示沿用全局默认预算。"))
	int32 MaxSimultaneousAttackers = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase|Rules",
		meta = (ToolTip = "是否允许继续刷新支援。终战阶段通常关闭。"))
	bool bAllowSupportRefresh = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase|Broadcast",
		meta = (ToolTip = "阶段切换时广播的文本；为空则不显示 UI 文本，由关卡 Cue 负责表现。"))
	FText BroadcastText;
};

UCLASS(BlueprintType)
class SLIMEFABLE_API UEnemyEncounterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter",
		meta = (ToolTip = "Boss 遭遇名称，用于调试与奖励记录。"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Phase",
		meta = (ToolTip = "按 MaxHealthRatio 从高到低排序；至少配置 1/2/3 三个阶段。"))
	TArray<FEnemyEncounterPhaseDef> Phases;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Budget",
		meta = (ClampMin = "1", ToolTip = "全局同时攻击预算，默认 3（Boss、近战支援、远程压制各 1）。"))
	int32 DefaultAttackBudget = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter|Reward",
		meta = (ToolTip = "遭遇完成时生成的奖励 Actor；为空时只广播完成事件。"))
	TSubclassOf<AActor> CompletionRewardClass;

	const FEnemyEncounterPhaseDef* FindPhase(int32 PhaseIndex) const;
};
