// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeFableMenuPlayerController.h"
#include "UI/MainMenuWidget.h"
#include "Blueprint/UserWidget.h"
#include "UObject/SoftObjectPath.h"

ASlimeFableMenuPlayerController::ASlimeFableMenuPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	MainMenuClass = nullptr;
}

void ASlimeFableMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;

	TSubclassOf<UMainMenuWidget> ClassToSpawn = MainMenuClass;
	if (!ClassToSpawn)
	{
		const FSoftClassPath SoftPath(TEXT("/Game/UI/WBP_MainMenu.WBP_MainMenu_C"));
		ClassToSpawn = SoftPath.TryLoadClass<UMainMenuWidget>();
	}
	if (!ClassToSpawn)
	{
		ClassToSpawn = UMainMenuWidget::StaticClass();
	}

	MainMenuWidget = CreateWidget<UMainMenuWidget>(this, ClassToSpawn);
	if (MainMenuWidget)
	{
		MainMenuWidget->AddToViewport(0);
		InputMode.SetWidgetToFocus(MainMenuWidget->TakeWidget());
		SetInputMode(InputMode);
	}
}
