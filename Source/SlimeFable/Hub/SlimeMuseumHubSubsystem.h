// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SlimeMuseumHubSubsystem.generated.h"

/** Makes sure a museum world has a sky director and a farm, even if the map save missed them. */
UCLASS()
class SLIMEFABLE_API USlimeMuseumHubSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	bool IsMuseumWorld(const UWorld& World) const;
	void EnsureSkyDirector(UWorld& World);
	void EnsureFarmField(UWorld& World);
};
