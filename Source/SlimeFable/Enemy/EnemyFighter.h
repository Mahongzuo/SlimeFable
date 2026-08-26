// Copyright Epic Games, Inc. All Rights Reserved.



#pragma once



#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "EnemyCharacter.h"
#include "EnemyCombatTypes.h"
#include "EnemyFighter.generated.h"



class USphereComponent;



UCLASS()

class SLIMEFABLE_API AEnemyFighter : public AEnemyCharacter

{

	GENERATED_BODY()



public:

	AEnemyFighter();



	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void BeginPlay() override;

	virtual bool IsInCombat() const override;

	virtual void OnRestoredToSpawn() override;
	virtual void ApplyDifficultyToCombat(float DamageMul, float IntervalMul) override;



	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter", meta = (ClampMin = "100.0", Units = "cm"))

	float DetectRange = 1000.f;



	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter", meta = (ClampMin = "100.0", Units = "cm"))

	float LeashRange = 1500.f;



	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter", meta = (ClampMin = "50.0", Units = "cm"))

	float PreferredDistance = 160.f;



	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter")

	TArray<FEnemyMoveDef> Moves;



	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter", meta = (ClampMin = "100.0"))
	float WalkSpeed = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter",
		meta = (ToolTip = "勾选后只用撕咬近战，不用默认的砍/冲/弹/砸。看门狗勾上。"))
	bool bBiteOnlyKit = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter",
		meta = (ToolTip = "未进战时绕出生点闲逛。看门狗勾上，武士等保持关。"))
	bool bWanderWhenIdle = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Debug",
		meta = (ToolTip = "勾选后不主动进战，仍可闲逛或被打。SlimeLab 测试桩勾上。"))
	bool bPassive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter",
		meta = (ClampMin = "50.0", Units = "cm",
			ToolTip = "闲逛半径，绕出生点。默认 500（5 米）。"))
	float WanderRadius = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter",
		meta = (ClampMin = "0.2", Units = "s",
			ToolTip = "两次闲逛/待机之间的最短停顿。"))
	float WanderPauseMin = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter",
		meta = (ClampMin = "0.2", Units = "s",
			ToolTip = "两次闲逛/待机之间的最长停顿。"))
	float WanderPauseMax = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter",
		meta = (ToolTip = "闲逛停下时随机播一条。坐下、嗅、甩头等，空则只站着。"))
	TArray<TSoftObjectPtr<UAnimMontage>> IdleMontages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter",
		meta = (ToolTip = "勾选后用单节点播动画，不依赖 AnimBP 的 DefaultSlot。看门狗勾上。"))
	bool bUseSingleNodeAnims = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter",
		meta = (ToolTip = "闲逛/走近时循环播。看门狗绑原地走路（Walk_F_IP）。"))
	TSoftObjectPtr<UAnimMontage> WalkMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter",
		meta = (ToolTip = "追逐时循环播（bABPDrivenLocomotion / 幻形冲刺）。看门狗绑跑步（Run_F_IP）。空则回退 WalkMontage 加速。"))
	TSoftObjectPtr<UAnimMontage> RunMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter",
		meta = (ToolTip = "玩家幻形起跳时单次播。看门狗绑 Jump_IP。空则跳跃无动作。"))
	TSoftObjectPtr<UAnimMontage> JumpMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter",
		meta = (ToolTip = "勾选后按 MaxWalkSpeed/WalkSpeed 匹配 Walk 蒙太奇 PlayRate（高速加速=跑），并在追逐时把 MaxWalkSpeed 提到 ChaseSpeed。看门狗勾上。"))
	bool bABPDrivenLocomotion = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter",
		meta = (ClampMin = "100.0", ToolTip = "追逐玩家时的 MaxWalkSpeed，配合 ABP RunThreshold 触发跑步。仅 bABPDrivenLocomotion 生效。默认 600。"))
	float ChaseSpeed = 600.f;

	const TArray<FEnemyMoveDef>& GetMoves() const { return Moves; }

	virtual bool UsesSingleNodeAnims() const override { return bUseSingleNodeAnims; }

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void EnsureMoveKit();

	TArray<float> DifficultyBaseDamages;
	TArray<float> DifficultyBaseRecoveries;
	TArray<float> DifficultyBaseCooldowns;
	bool bCombatBasesCaptured = false;



protected:

	void SyncRangeVisuals();



	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)

	TObjectPtr<USphereComponent> DetectRangeVisual;



	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)

	TObjectPtr<USphereComponent> LeashRangeVisual;

};

