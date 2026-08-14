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

	/** Max distance to start attacking (10 m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "50.0", Units = "cm"))
	float AttackRange = 1000.f;

	/** Path-following stop distance / preferred melee spacing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "40.0", Units = "cm"))
	float ApproachAcceptRadius = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.1", Units = "s"))
	float AttackInterval = 0.55f;

	/** How often to repath while chasing. Avoids RestartMove every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0.05", Units = "s"))
	float PathRefreshInterval = 0.25f;

protected:
	APawn* FindPlayerPawn() const;
	void UpdateChase(APawn* MyPawn, APawn* Player, float Dist);
	void ChaseDirect(APawn* MyPawn, APawn* Player);

	UPROPERTY(Transient)
	TObjectPtr<USlimeCombatComponent> Combat;

	float AttackCooldown = 0.f;
	float PathRefreshRemaining = 0.f;
	bool bUseDirectChase = false;
};
