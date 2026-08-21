// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnemyFighterAIController.h"
#include "EnemyAllyAIController.generated.h"

class AActor;

UCLASS()
class SLIMEFABLE_API AEnemyAllyAIController : public AEnemyFighterAIController
{
	GENERATED_BODY()

public:
	AEnemyAllyAIController();

	virtual void OnPossess(APawn* InPawn) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "200.0", Units = "cm",
		ToolTip = "友军寻找敌对目标的半径。默认 2000cm。"))
	float AllySeekRange = 2000.f;

	void SetMaster(AActor* InMaster);

protected:
	virtual APawn* FindCombatFocus() const override;
	virtual void TickIdle(float DeltaSeconds, float Dist) override;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> Master;
};
