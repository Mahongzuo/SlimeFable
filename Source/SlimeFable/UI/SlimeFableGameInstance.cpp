// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeFableGameInstance.h"
#include "UI/SlimeLoadingGateWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "SlimeFable.h"
#include "SlimeFablePlayerController.h"

void USlimeFableGameInstance::Init()
{
	Super::Init();

	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &USlimeFableGameInstance::BeginLoadingScreen);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &USlimeFableGameInstance::EndLoadingScreen);
}

void USlimeFableGameInstance::Shutdown()
{
	if (PreLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
		PreLoadMapHandle.Reset();
	}
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	if (ActiveLoadingGate)
	{
		ActiveLoadingGate->RemoveFromParent();
		ActiveLoadingGate = nullptr;
	}

	Super::Shutdown();
}

void USlimeFableGameInstance::BeginLoadingScreen(const FString& MapName)
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	// No MoviePlayer overlay — it conflicted with LoadingGate's menu background.
	// Only suppress "Preparing Shaders" chrome; the gate restores it when finished.
	bPrevScreenMessagesEnabled = GAreScreenMessagesEnabled;
	GAreScreenMessagesEnabled = false;
	UE_LOG(LogSlimeFable, Log, TEXT("Map travel started for '%s' (LoadingGate after load)"), *MapName);
}

void USlimeFableGameInstance::EndLoadingScreen(UWorld* LoadedWorld)
{
	ShowLoadingGate(LoadedWorld);
}

void USlimeFableGameInstance::ShowLoadingGate(UWorld* LoadedWorld)
{
	if (IsRunningDedicatedServer() || !LoadedWorld)
	{
		GAreScreenMessagesEnabled = bPrevScreenMessagesEnabled;
		return;
	}

	if (ActiveLoadingGate)
	{
		ActiveLoadingGate->RemoveFromParent();
		ActiveLoadingGate = nullptr;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(LoadedWorld, 0);
	if (!PC)
	{
		GAreScreenMessagesEnabled = bPrevScreenMessagesEnabled;
		return;
	}

	ActiveLoadingGate = CreateWidget<USlimeLoadingGateWidget>(PC, USlimeLoadingGateWidget::StaticClass());
	if (!ActiveLoadingGate)
	{
		GAreScreenMessagesEnabled = bPrevScreenMessagesEnabled;
		return;
	}

	ActiveLoadingGate->OnGateFinished.AddDynamic(this, &USlimeFableGameInstance::HandleLoadingGateFinished);
	ActiveLoadingGate->AddToViewport(100);

	if (ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(PC))
	{
		SlimePC->PushUIInput(ESlimeUIInputReason::LoadingGate, ActiveLoadingGate);
	}
	else
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(ActiveLoadingGate->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = false;
	}

	UE_LOG(LogSlimeFable, Log, TEXT("Loading gate shown for world '%s'"), *LoadedWorld->GetMapName());
}

void USlimeFableGameInstance::HandleLoadingGateFinished()
{
	GAreScreenMessagesEnabled = bPrevScreenMessagesEnabled;
	ActiveLoadingGate = nullptr;

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			if (ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(PC))
			{
				SlimePC->PopUIInput(ESlimeUIInputReason::LoadingGate);
			}
			else
			{
				// Main menu stays UI-only.
				FInputModeUIOnly InputMode;
				InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				PC->SetInputMode(InputMode);
				PC->bShowMouseCursor = true;
			}
		}
	}
}
