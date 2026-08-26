// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SlimeAIController.generated.h"

class USlimeCombatComponent;

UCLASS()
class SLIMEFABLE_API ASlimeAIController : public AAIController
{
	GENERATED_BODY()

public:
	ASlimeAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "100.0", Units = "cm"))
	float AggroRange = 1800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "50.0", Units = "cm"))
	float AttackRange = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "40.0", Units = "cm"))
	float ApproachAcceptRadius = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.1", Units = "s"))
	float AttackInterval = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.05", Units = "s"))
	float PathRefreshInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "50.0", Units = "cm",
		ToolTip = "卡住时朝玩家方向绕障采样距离。默认 160。"))
	float SideStepOffset = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.1", Units = "s",
		ToolTip = "Nav 全失败后短直推最长秒数。默认 0.75。"))
	float DirectChaseMaxSeconds = 0.75f;

protected:
	APawn* FindPlayerPawn() const;
	void UpdateChase(APawn* MyPawn, APawn* Player, float Dist);
	bool TryMoveToNavLocation(const FVector& Dest);
	bool RequestNavDetourTowardFocus(APawn* Player);
	void ResetChaseFallback();

	UPROPERTY(Transient)
	TObjectPtr<USlimeCombatComponent> Combat;

	float AttackCooldown = 0.f;
	float PathRefreshRemaining = 0.f;
	float ChaseStalledSeconds = 0.f;
	FVector ChaseLastPos = FVector::ZeroVector;
	bool bDirectChaseFallback = false;
	float DirectChaseActiveSeconds = 0.f;
	int32 DetourSampleIndex = 0;
};
