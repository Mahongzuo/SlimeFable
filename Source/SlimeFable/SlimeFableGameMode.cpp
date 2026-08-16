// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeFableGameMode.h"
#include "EngineUtils.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/PackageName.h"

namespace
{
	bool IsHostMapPlayerStart(const APlayerStart* Start, const UWorld* World)
	{
		if (!Start || !World)
		{
			return false;
		}

		const ULevel* Level = Start->GetLevel();
		if (!Level)
		{
			return false;
		}

		const FString Pkg = Level->GetOutermost() ? Level->GetOutermost()->GetName() : FString();
		if (Pkg.Contains(TEXT("Showcase"))
			|| Pkg.Contains(TEXT("Demonstration"))
			|| Pkg.Contains(TEXT("OperaHouse"))
			|| Pkg.Contains(TEXT("Venice")))
		{
			return false;
		}

		if (Level == World->PersistentLevel)
		{
			return true;
		}

		const FString Short = FPackageName::GetShortName(Pkg);
		if (Short.StartsWith(TEXT("SL_")))
		{
			return true;
		}
		return Short.Len() == 4 && Short.IsNumeric();
	}
}

ASlimeFableGameMode::ASlimeFableGameMode()
{
}

AActor* ASlimeFableGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	TArray<APlayerStart*> HostStarts;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (IsHostMapPlayerStart(*It, World))
		{
			HostStarts.Add(*It);
		}
	}

	if (HostStarts.Num() > 0)
	{
		return HostStarts[0];
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}
