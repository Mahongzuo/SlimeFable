// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnemyFighter.h"
#include "PigEnemy.generated.h"

class UAnimMontage;

/** Watchdog-type Chaser pig: wander, chase, three tusk moves, ABP locomotion. */
UCLASS()
class SLIMEFABLE_API APigEnemy : public AEnemyFighter
{
	GENERATED_BODY()

public:
	APigEnemy();

	virtual void EnsureMoveKit() override;
	virtual void EnterStagger(float Duration, AActor* StaggerInstigator) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Pig",
		meta = (ToolTip = "破韧受击 Montage（DefaultSlot）。脚本绑 AM_Pig_GetHit。空则只走通用 stagger。"))
	TSoftObjectPtr<UAnimMontage> HitReactMontage;
};
