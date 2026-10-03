#pragma once

#include "CoreMinimal.h"
#include "MiniGame/SlimeMiniGameDirector.h"
#include "SlimeRunnerDirector.generated.h"

class ACameraActor;
class AActor;
class UNiagaraSystem;
class USoundBase;
class USlimeRunnerHUDWidget;

/** Side-scrolling run: plane-locked slime, side camera, flags, checkpoints and a finale shot. */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeRunnerDirector : public ASlimeMiniGameDirector
{
	GENERATED_BODY()

public:
	ASlimeRunnerDirector();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Course",
		meta = (ToolTip = "赛道所在平面的世界 Y。进入时玩家被放到这个 Y 并锁在 XZ 平面里移动。"))
	float PlaneY = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Course",
		meta = (ToolTip = "起点世界 X，HUD 进度条 0%，也是相机左边界。"))
	float CourseStartX = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Course",
		meta = (ToolTip = "终点世界 X，HUD 进度条 100%，也是相机右边界。"))
	float CourseEndX = 10000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Course",
		meta = (ToolTip = "HUD 进度条两端的地名，如 胡同 / 天安门。"))
	FText StartLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Course",
		meta = (ToolTip = "HUD 进度条两端的地名，如 胡同 / 天安门。"))
	FText EndLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Camera",
		meta = (ClampMin = "200", ToolTip = "侧视相机离赛道平面的距离（相机在 +Y 侧朝 -Y 看，远景要摆在更小的 Y），默认 1400。越大看得越广。"))
	float CameraDistance = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Camera",
		meta = (ToolTip = "相机比玩家高多少，默认 220。"))
	float CameraHeight = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Camera",
		meta = (ToolTip = "相机朝前进方向多看多少（按朝向自动左右），默认 320。"))
	float CameraLookAhead = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Camera",
		meta = (ToolTip = "相机俯角（负为向下看），默认 -8。"))
	float CameraPitch = -8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Camera",
		meta = (ClampMin = "20", ClampMax = "120", ToolTip = "侧视相机视角，默认 55。"))
	float CameraFOV = 55.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Rules",
		meta = (ClampMin = "0", ToolTip = "红旗总数。0 = 开局自动数场景里的 SlimeRunnerPickup。"))
	int32 TotalFlags = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Rules",
		meta = (ClampMin = "0", ToolTip = "碰到墨团/尖刺扣的血（史莱姆满血 100），默认 20。"))
	float HazardDamage = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Rules",
		meta = (ClampMin = "0", ToolTip = "掉坑回检查点时扣的血，默认 15。"))
	float FallDamage = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Rules",
		meta = (ClampMin = "0", ToolTip = "受伤后无敌秒数，默认 1.2。"))
	float HurtInvulnerableSeconds = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Week",
		meta = (ClampMin = "0.5", ToolTip = "二、三周目墨团巡逻速度倍率，默认 1.35。"))
	float HardPatrolSpeedScale = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Week",
		meta = (ClampMin = "0", ToolTip = "三周目限时秒数，0 = 不限时。超时按失败重来本章。默认 240。"))
	float Week3TimeLimit = 240.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Finale",
		meta = (MakeEditWidget, ToolTip = "终点广角镜头的位置（相对本 Actor）。在视口拖菱形调整。"))
	FVector FinaleCameraLocation = FVector(0.f, 3000.f, 900.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Finale",
		meta = (ToolTip = "终点广角镜头的朝向。侧视相机在 +Y 侧朝 -Y 看，Yaw 一般为 -90。"))
	FRotator FinaleCameraRotation = FRotator(-6.f, -90.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Finale",
		meta = (ClampMin = "0.1", ToolTip = "镜头从跟随切到广角的秒数，默认 2.5。"))
	float FinaleBlendSeconds = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Finale",
		meta = (ToolTip = "终点要升起的旗子 Actor（挂在旗杆底部），留空则不升旗。"))
	TObjectPtr<AActor> RaisedFlag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Finale",
		meta = (ToolTip = "升旗总高度（厘米），默认 1800。"))
	float FlagRiseHeight = 1800.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Finale",
		meta = (ClampMin = "0.5", ToolTip = "升旗用时秒数，默认 8。"))
	float FlagRiseSeconds = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Finale",
		meta = (ToolTip = "礼炮/烟花特效。每收集一面红旗放一次，至少放 1 次。"))
	TObjectPtr<UNiagaraSystem> SaluteFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Finale",
		meta = (ToolTip = "每次礼炮的音效，可空。"))
	TObjectPtr<USoundBase> SaluteSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Finale",
		meta = (MakeEditWidget, ToolTip = "礼炮爆点（相对本 Actor），轮流使用并加少量随机偏移。"))
	TArray<FVector> SaluteSpots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Finale",
		meta = (ClampMin = "0.05", ToolTip = "两次礼炮间隔秒数，默认 0.35。"))
	float SaluteInterval = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Finale",
		meta = (ToolTip = "终点横幅小标题。"))
	FText FinaleKicker;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Finale",
		meta = (MultiLine = true, ToolTip = "终点横幅正文，写史实。"))
	FText FinaleText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Finale",
		meta = (ClampMin = "0", ToolTip = "升旗和礼炮都结束后再等几秒才算通关，默认 2。"))
	float FinaleTailSeconds = 2.f;

	void RegisterPickup() { ++AutoFlagCount; }
	void CollectFlag(int32 Value);
	void SetCheckpoint(const FVector& Location);
	/** Damage + knockback from a hazard, respecting hurt invulnerability. */
	void HurtPlayer(AActor* Source);
	/** Fell into a pit: back to the last checkpoint. */
	void RespawnPlayer();
	void BeginFinale();

	UFUNCTION(BlueprintPure, Category = "MiniGame")
	int32 GetFlagsCollected() const { return FlagsCollected; }

	UFUNCTION(BlueprintPure, Category = "MiniGame")
	int32 GetFlagsTotal() const { return TotalFlags > 0 ? TotalFlags : AutoFlagCount; }

	float GetPatrolSpeedScale() const { return GetWeekIndex() >= 2 ? HardPatrolSpeedScale : 1.f; }
	bool IsInFinale() const { return bFinale; }

	static ASlimeRunnerDirector* Find(const UObject* WorldContext);

protected:
	virtual void EnterMode(ACharacter* Character, APlayerController* PC) override;
	virtual void ExitMode(ACharacter* Character, APlayerController* PC) override;

private:
	void UpdateFollowCamera(float DeltaSeconds, bool bSnap);
	void TickFinale(float DeltaSeconds);
	void FireSalute();
	void UpdateHUD();

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> RunnerCamera;

	UPROPERTY(Transient)
	TObjectPtr<USlimeRunnerHUDWidget> HUD;

	FVector CheckpointLocation = FVector::ZeroVector;
	FVector FlagStart = FVector::ZeroVector;
	FVector FinaleFromLocation = FVector::ZeroVector;
	FRotator FinaleFromRotation = FRotator::ZeroRotator;
	TWeakObjectPtr<AActor> PreviousViewTarget;
	int32 AutoFlagCount = 0;
	int32 FlagsCollected = 0;
	int32 SalutesFired = 0;
	float LookAheadSign = 1.f;
	float CurrentLookAhead = 0.f;
	float ElapsedSeconds = 0.f;
	float FinaleSeconds = 0.f;
	float SaluteTimer = 0.f;
	double InvulnerableUntil = 0.0;
	bool bFinale = false;
	bool bBannerShown = false;
	bool bSavedConstrain = false;
	FVector SavedPlaneNormal = FVector::ZeroVector;
};
