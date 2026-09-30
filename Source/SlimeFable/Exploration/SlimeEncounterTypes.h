#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/SaveGame.h"
#include "SlimeEncounterTypes.generated.h"

USTRUCT(BlueprintType)
struct FSlimeEncounterEnemyRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config", meta=(ToolTip="现有可战斗敌人蓝图；使用软引用，进入探索图时预载。"))
	TSoftClassPtr<APawn> EnemyClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config", meta=(ToolTip="是否加入每日候选池。关闭不会修改原敌人蓝图。"))
	bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config", meta=(ClampMin="0.0", ToolTip="每日种类和替补怪的抽取权重；默认1，0不抽取。"))
	float Weight = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config", meta=(ToolTip="共享数量组。Lyra系自动归入Lyra，无法通过留空绕过限制；其他敌人默认空。"))
	FName LimitGroup;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config", meta=(ClampMin="0.01", ToolTip="相对原敌人基础生命倍率，默认1；再乘当天强化倍率。"))
	float HealthScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config", meta=(ClampMin="0.01", ToolTip="相对原敌人基础攻击倍率，默认1；再乘当天强化倍率。"))
	float DamageScale = 1.f;
};

USTRUCT()
struct FSlimeEncounterRecord
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FGuid Id;
	UPROPERTY(SaveGame) FName Species;
	UPROPERTY(SaveGame) FName PointId;
	UPROPERTY(SaveGame) int32 Level = 0;
	UPROPERTY(SaveGame) float HealthFraction = 1.f;
	UPROPERTY(SaveGame) int32 HealthPhase = 1;
	UPROPERTY(SaveGame) bool bPendingReplacement = false;
	UPROPERTY(SaveGame) int64 DueUtcTicks = 0;
};

UCLASS()
class SLIMEFABLE_API USlimeEncounterSave : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) int32 Version = 1;
	UPROPERTY(SaveGame) FString LocalDate;
	UPROPERTY(SaveGame) int32 KillLevel = 0;
	UPROPERTY(SaveGame) TArray<FName> DailySpecies;
	UPROPERTY(SaveGame) TArray<FSlimeEncounterRecord> Records;
};

namespace SlimeEncounterRules
{
	inline float Scale(int32 Level, float PerKill) { return 1.f + FMath::Max(Level, 0) * FMath::Max(PerKill, 0.f); }
	inline bool IsDue(int64 Due, int64 Now) { return Due <= Now; }
}
