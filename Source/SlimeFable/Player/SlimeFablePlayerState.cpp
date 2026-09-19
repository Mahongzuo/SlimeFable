// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeFablePlayerState.h"

#include "GenericTeamAgentInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SlimeFablePlayerState)

void ASlimeFablePlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Authority only (ALyraPlayerState logs an error otherwise); replicates to clients via MyTeamID.
	if (HasAuthority() && GetGenericTeamId() == FGenericTeamId::NoTeam)
	{
		SetGenericTeamId(FGenericTeamId(DefaultTeamId));
	}
}
