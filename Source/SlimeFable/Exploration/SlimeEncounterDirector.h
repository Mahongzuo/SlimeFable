#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeEncounterTypes.h"
#include "SlimeEncounterDirector.generated.h"

class USlimeEncounterMember;
struct FStreamableHandle;

UCLASS(meta=(PrioritizeCategories="0_Config"))
class SLIMEFABLE_API ASlimeEncounterSpawnPoint : public AActor
{
	GENERATED_BODY()
public:
	ASlimeEncounterSpawnPoint();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config", meta=(ToolTip="出生点唯一编号。摆放后不要重命名，否则旧存档需要重新选点。"))
	FName PointId;
};

UCLASS(meta=(PrioritizeCategories="0_Config"))
class SLIMEFABLE_API ASlimeEncounterDirector : public AActor
{
	GENERATED_BODY()
public:
	ASlimeEncounterDirector();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Enemies", meta=(ToolTip="敌人种类表，行类型SlimeEncounterEnemyRow；新增已有基类的敌人只需加行。"))
	TObjectPtr<UDataTable> EnemyTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Enemies", meta=(ClampMin="1", ToolTip="每天抽取的不同种类数，默认5。"))
	int32 DailySpeciesCount = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Enemies", meta=(ToolTip="共享种类组的存活上限；默认Lyra=2，休眠和预留名额也计数。"))
	TMap<FName, int32> GroupLimits;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Respawn", meta=(ClampMin="1", ToolTip="击败后等待秒数，默认60；离线也计时。安全点不可用时继续等待。"))
	float RespawnSeconds = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Respawn", meta=(ClampMin="0", ToolTip="每次全图击杀，新怪生命增加基础值比例；默认0.2。已有活怪不变。"))
	float HealthPerKill = .2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Respawn", meta=(ClampMin="0", ToolTip="每次全图击杀，新怪伤害增加基础值比例；默认0.1。已有活怪不变。"))
	float DamagePerKill = .1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Performance", meta=(ClampMin="100", ToolTip="玩家进入此距离激活，单位厘米，默认3000（30米）。"))
	float WakeDistance = 3000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Performance", meta=(ClampMin="100", ToolTip="超过此距离且脱战才休眠，默认4000（40米），应大于激活距离。追击中的Lyra不休眠。"))
	float SleepDistance = 4000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Respawn", meta=(ClampMin="0", ToolTip="玩家与补怪点的最小距离，默认1000（10米），避免贴脸刷新。"))
	float SafeSpawnDistance = 1000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Save", meta=(ToolTip="独立探索存档名，默认RuinEncounter。不会覆盖日关卡存档；PIE自动使用独立测试槽。"))
	FString SaveSlot = TEXT("RuinEncounter");
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="0_Config|Runtime") int32 KillLevel = 0;
	void ReportDefeat(FGuid Id);
	void MarkDirty() { bDirty = true; }
	UFUNCTION(BlueprintCallable, Category="Exploration") FString GetEncounterDiagnostics() const;
	/** Editor batch setup: nav-connected, capsule-clear, farthest-point sampling. */
	UFUNCTION(BlueprintCallable, Category="Exploration") int32 BuildValidatedSpawnPoints(int32 Count = 15);
	UFUNCTION(BlueprintCallable, Category="Exploration") int32 ValidateSpawnPoints() const;
private:
	void FinishLoading();
	void StartNewDay();
	void Heartbeat();
	void SaveState();
	void LoadState();
	void TrySpawn(FSlimeEncounterRecord& Record);
	void UpdatePresence();
	FName ChooseSpecies(const TArray<FName>& Pool, const FGuid& Ignore = FGuid()) const;
	bool CanReserve(FName Species, const FGuid& Ignore = FGuid()) const;
	FName GetGroup(FName Species) const;
	float PlayerDistance(const FVector& Position) const;
	FString ResolvedSlot() const;
	bool AnyEngaged() const;
	UPROPERTY() TObjectPtr<USlimeEncounterSave> State;
	UPROPERTY() TArray<TObjectPtr<ASlimeEncounterSpawnPoint>> Points;
	UPROPERTY() TMap<FGuid, TObjectPtr<APawn>> Live;
	TArray<FName> ValidSpecies;
	TSharedPtr<FStreamableHandle> Preload;
	FTimerHandle HeartbeatTimer;
	bool bReady = false;
	bool bDirty = false;
	bool bEnding = false;
	float LastSaveTime = 0.f;
#if WITH_EDITOR
	void VerifyRuntime();
	int32 VerificationPhase = 0;
	double VerificationStarted = 0;
	FGuid VerificationVictim;
	bool bVerificationFailed = false;
#endif
};
