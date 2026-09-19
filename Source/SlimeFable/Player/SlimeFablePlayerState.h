// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Player/LyraPlayerState.h"
#include "SlimeFablePlayerState.generated.h"

/**
 * Play maps inherit Lyra PlayerState so session + GAS replication stay available.
 *
 * Team: Lyra's damage execution (ULyraTeamSubsystem::CanCauseDamage) refuses damage when the instigator
 * has no team. ALyraCharacter copies its team from the possessing controller, and the controller reads
 * it from this PlayerState, so a morphed Lyra body driven by the player would otherwise deal 0 damage.
 * We run no LyraTeamCreationComponent, so hand out the player team here.
 */
UCLASS()
class SLIMEFABLE_API ASlimeFablePlayerState : public ALyraPlayerState
{
	GENERATED_BODY()

public:
	virtual void PostInitializeComponents() override;

	UPROPERTY(EditDefaultsOnly, Category = "0_Config|Team",
		meta = (ClampMin = "0", ClampMax = "254",
			ToolTip = "玩家 PlayerState 的 Lyra TeamId。要和 ALyraShooterEnemy::PlayerTeamId 一致（默认 1），敌兵是 2。"))
	uint8 DefaultTeamId = 1;
};
