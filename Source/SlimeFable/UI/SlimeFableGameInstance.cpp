// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeFableGameInstance.h"
#include "UI/SSlimeLoadingScreen.h"
#include "UI/SlimeLoadingGateWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "MoviePlayer.h"
#include "SlimeFable.h"
#include "SlimeFablePlayerController.h"

void USlimeFableGameInstance::Init()
{
	Super::Init();

	EnsureLoadingBackgroundBrush();
	EnsureLoadingStatusFont();

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

	CachedLoadingBackground.Reset();
	bHasCachedLoadingStatusFont = false;
	CachedLoadingStatusFont = FSlateFontInfo();
	Super::Shutdown();
}

void USlimeFableGameInstance::EnsureLoadingBackgroundBrush()
{
	if (!CachedLoadingBackground.IsValid())
	{
		CachedLoadingBackground = SSlimeLoadingScreen::CreateBackgroundBrushOnGameThread();
	}
}

void USlimeFableGameInstance::EnsureLoadingStatusFont()
{
	if (!bHasCachedLoadingStatusFont)
	{
		CachedLoadingStatusFont = SSlimeLoadingScreen::CreateStatusFontOnGameThread(22.f);
		bHasCachedLoadingStatusFont = true;
	}
}

void USlimeFableGameInstance::BeginLoadingScreen(const FString& MapName)
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	EnsureLoadingBackgroundBrush();
	EnsureLoadingStatusFont();

	// Hide "Preparing Shaders" chrome for the whole travel; gate restores later.
	bPrevScreenMessagesEnabled = GAreScreenMessagesEnabled;
	GAreScreenMessagesEnabled = false;

	FLoadingScreenAttributes Attributes;
	Attributes.bAutoCompleteWhenLoadingCompletes = true;
	// Never tick the engine under MoviePlayer when RayTracing is on.
	Attributes.bAllowEngineTick = false;
	Attributes.bWaitForManualStop = false;
	Attributes.MinimumLoadingScreenDisplayTime = 0.2f;
	Attributes.WidgetLoadingScreen = SNew(SSlimeLoadingScreen)
		.BackgroundBrush(CachedLoadingBackground)
		.StatusFont(CachedLoadingStatusFont);

	if (IGameMoviePlayer* MoviePlayer = GetMoviePlayer())
	{
		MoviePlayer->SetupLoadingScreen(Attributes);
		UE_LOG(LogSlimeFable, Log, TEXT("Loading screen started for '%s'"), *MapName);
	}
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
