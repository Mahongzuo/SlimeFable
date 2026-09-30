// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hub/SlimeMuseumHubSubsystem.h"

#include "DayLevel/DayLevelSubsystem.h"
#include "EngineUtils.h"
#include "Farm/SlimeFarmField.h"
#include "Farm/SlimeFarmPlot.h"
#include "Hub/SlimeHubSkyDirector.h"
#include "SlimeFable.h"

namespace SlimeMuseumHubPrivate
{
	// North of the courtyard, on the open terrain, so the starter beds do not sit on the decorative soil blocks.
	static const FVector DefaultFieldLocation(0.f, 4200.f, 20.f);
}

void USlimeMuseumHubSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!IsMuseumWorld(InWorld))
	{
		return;
	}

	UE_LOG(LogSlimeFable, Log, TEXT("[MuseumHub] integrating %s"), *InWorld.GetName());
	EnsureSkyDirector(InWorld);
	EnsureFarmField(InWorld);
}

bool USlimeMuseumHubSubsystem::IsMuseumWorld(const UWorld& World) const
{
	const UGameInstance* GameInstance = World.GetGameInstance();
	const UDayLevelSubsystem* Days = GameInstance ? GameInstance->GetSubsystem<UDayLevelSubsystem>() : nullptr;
	return Days && Days->IsMuseumHubWorld(&World);
}

void USlimeMuseumHubSubsystem::EnsureSkyDirector(UWorld& World)
{
	for (TActorIterator<ASlimeHubSkyDirector> It(&World); It; ++It)
	{
		UE_LOG(LogSlimeFable, Log, TEXT("[MuseumHub] sky director already present"));
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (World.SpawnActor<ASlimeHubSkyDirector>(ASlimeHubSkyDirector::StaticClass(), FTransform::Identity, Params))
	{
		UE_LOG(LogSlimeFable, Log, TEXT("[MuseumHub] spawned sky director"));
	}
	else
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("[MuseumHub] failed to spawn sky director"));
	}
}

void USlimeMuseumHubSubsystem::EnsureFarmField(UWorld& World)
{
	for (TActorIterator<ASlimeFarmField> It(&World); It; ++It)
	{
		UE_LOG(LogSlimeFable, Log, TEXT("[MuseumHub] farm field already present"));
		return;
	}
	for (TActorIterator<ASlimeFarmPlot> It(&World); It; ++It)
	{
		UE_LOG(LogSlimeFable, Log, TEXT("[MuseumHub] farm plots already present, skipping default field"));
		return;
	}

	const FTransform Transform(FRotator::ZeroRotator, SlimeMuseumHubPrivate::DefaultFieldLocation);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (World.SpawnActor<ASlimeFarmField>(ASlimeFarmField::StaticClass(), Transform, Params))
	{
		UE_LOG(LogSlimeFable, Log, TEXT("[MuseumHub] spawned default farm field at %s"),
			*SlimeMuseumHubPrivate::DefaultFieldLocation.ToCompactString());
	}
	else
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("[MuseumHub] failed to spawn default farm field"));
	}
}
