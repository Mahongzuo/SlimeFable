// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimePlayGameMode.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "SlimeCharacter.h"
#include "SlimeElementComponent.h"
#include "SlimeEnemyCharacter.h"
#include "SlimeFable.h"

ASlimePlayGameMode::ASlimePlayGameMode()
{
	DefaultPawnClass = ASlimeCharacter::StaticClass();
	// Concrete fallback: the abstract ASlimeFablePlayerController cannot be spawned.
	PlayerControllerClass = APlayerController::StaticClass();

	SlimePawnClassPath = TSoftClassPtr<APawn>(
		FSoftObjectPath(TEXT("/Game/Characters/Slime/BP_SlimeCharacter.BP_SlimeCharacter_C")));
	PlayerControllerClassPath = TSoftClassPtr<APlayerController>(
		FSoftObjectPath(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C")));

	// Resolve in the constructor so World Settings / CDO show the real Blueprint, not a stub.
	if (UClass* ControllerClass = PlayerControllerClassPath.LoadSynchronous())
	{
		PlayerControllerClass = ControllerClass;
	}
	if (UClass* PawnClass = SlimePawnClassPath.LoadSynchronous())
	{
		DefaultPawnClass = PawnClass;
	}
}

void ASlimePlayGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (!SlimePawnClassPath.IsNull())
	{
		if (UClass* PawnClass = SlimePawnClassPath.LoadSynchronous())
		{
			DefaultPawnClass = PawnClass;
		}
		else
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("SlimePlayGameMode: '%s' is missing; falling back to ASlimeCharacter without Blueprint input bindings."), *SlimePawnClassPath.ToString());
		}
	}

	if (!PlayerControllerClassPath.IsNull())
	{
		if (UClass* ControllerClass = PlayerControllerClassPath.LoadSynchronous())
		{
			PlayerControllerClass = ControllerClass;
		}
		else
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("SlimePlayGameMode: '%s' is missing; falling back to APlayerController (no DefaultMappingContexts)."), *PlayerControllerClassPath.ToString());
			PlayerControllerClass = APlayerController::StaticClass();
		}
	}
}

void ASlimePlayGameMode::StartPlay()
{
	Super::StartPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FString MapName = World->GetMapName();
	if (!MapName.Contains(TEXT("SlimeLab")))
	{
		return;
	}

	TArray<AActor*> Existing;
	UGameplayStatics::GetAllActorsOfClass(World, ASlimeEnemyCharacter::StaticClass(), Existing);
	if (Existing.Num() > 0)
	{
		return;
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	const FVector Origin = Player ? Player->GetActorLocation() : FVector::ZeroVector;
	const FVector Forward = Player ? Player->GetActorForwardVector() : FVector::ForwardVector;
	const FVector Right = Player ? Player->GetActorRightVector() : FVector::RightVector;

	auto SpawnEnemy = [&](const FVector& Offset, ESlimeElement Element, bool bTraining)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		ASlimeEnemyCharacter* Enemy = World->SpawnActor<ASlimeEnemyCharacter>(Origin + Offset, Forward.Rotation(), Params);
		if (Enemy)
		{
			Enemy->StartingElement = Element;
			Enemy->bStationaryTraining = bTraining;
			if (USlimeElementComponent* ElementComp = Enemy->GetSlimeElement())
			{
				ElementComp->SetElement(Element, true);
			}
			TWeakObjectPtr<ASlimeEnemyCharacter> WeakEnemy(Enemy);
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakEnemy]()
			{
				ASlimeEnemyCharacter* Alive = WeakEnemy.Get();
				if (!Alive)
				{
					return;
				}
				if (USlimeElementComponent* ElementComp = Alive->GetSlimeElement())
				{
					ElementComp->SetElement(Alive->StartingElement, true);
				}
			}));
		}
	};

	SpawnEnemy(Forward * 700.f, ESlimeElement::Wind, false);
	SpawnEnemy(Forward * 900.f + Right * 400.f, ESlimeElement::Wind, false);
	SpawnEnemy(Forward * 900.f - Right * 400.f, ESlimeElement::Fire, true);
}
