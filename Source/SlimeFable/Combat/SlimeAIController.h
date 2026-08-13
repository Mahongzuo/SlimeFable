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
	float AttackRange = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.1", Units = "s"))
	float AttackInterval = 0.55f;

protected:
	APawn* FindPlayerPawn() const;

	UPROPERTY(Transient)
	TObjectPtr<USlimeCombatComponent> Combat;

	float AttackCooldown = 0.f;
};
