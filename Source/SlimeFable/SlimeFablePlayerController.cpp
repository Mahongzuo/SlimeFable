// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeFablePlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Components/InputComponent.h"
#include "DayLevel/DayLevelSubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "SlimeFable.h"
#include "Quest/QuestSubsystem.h"
#include "UI/PauseMenuWidget.h"
#include "Widgets/Input/SVirtualJoystick.h"

void ASlimeFablePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (PauseMenuClassPath.IsNull())
	{
		PauseMenuClassPath = TSoftClassPtr<UPauseMenuWidget>(
			FSoftObjectPath(TEXT("/Game/UI/WBP_PauseMenu.WBP_PauseMenu_C")));
	}

	// Menu sets FInputModeUIOnly; the viewport IgnoreInput flag survives hard level travel.
	// Always reclaim game input when a gameplay controller starts.
	if (IsLocalPlayerController())
	{
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
	}

	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);
		if (MobileControlsWidget)
		{
			MobileControlsWidget->AddToPlayerScreen(0);
		}
		else
		{
			UE_LOG(LogSlimeFable, Error, TEXT("Could not spawn mobile controls widget."));
		}
	}
}

void ASlimeFablePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateAltCursor();
}

void ASlimeFablePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalPlayerController())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
		{
			Subsystem->AddMappingContext(CurrentContext, 0);
		}

		if (!ShouldUseTouchControls())
		{
			for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}
		}
	}

	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ASlimeFablePlayerController::TogglePauseMenu);
	}
}

bool ASlimeFablePlayerController::ShouldUseTouchControls() const
{
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void ASlimeFablePlayerController::TogglePauseMenu()
{
	if (PauseMenuWidget && PauseMenuWidget->IsInViewport())
	{
		if (PauseMenuWidget->TryHandleEscape())
		{
			return;
		}
		ClosePauseMenu();
	}
	else
	{
		OpenPauseMenu();
	}
}

void ASlimeFablePlayerController::OpenPauseMenu()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (!PauseMenuWidget)
	{
		TSubclassOf<UPauseMenuWidget> ClassToSpawn = PauseMenuClass;
		if (!ClassToSpawn && !PauseMenuClassPath.IsNull())
		{
			ClassToSpawn = PauseMenuClassPath.LoadSynchronous();
		}
		if (!ClassToSpawn)
		{
			ClassToSpawn = UPauseMenuWidget::StaticClass();
		}

		PauseMenuWidget = CreateWidget<UPauseMenuWidget>(this, ClassToSpawn);
		if (!PauseMenuWidget)
		{
			UE_LOG(LogSlimeFable, Error, TEXT("Failed to create pause menu widget."));
			return;
		}

		PauseMenuWidget->OnContinueRequested.AddDynamic(this, &ASlimeFablePlayerController::HandlePauseContinue);
		PauseMenuWidget->OnLevelSelectRequested.AddDynamic(this, &ASlimeFablePlayerController::HandlePauseLevelSelect);
		PauseMenuWidget->OnMainMenuRequested.AddDynamic(this, &ASlimeFablePlayerController::HandlePauseMainMenu);
		PauseMenuWidget->OnReturnToHubRequested.AddDynamic(this, &ASlimeFablePlayerController::HandlePauseReturnToHub);
	}

	if (!PauseMenuWidget->IsInViewport())
	{
		PauseMenuWidget->AddToViewport(10);
	}
	PauseMenuWidget->SetVisibility(ESlateVisibility::Visible);
	PauseMenuWidget->RefreshHubButtonVisibility();

	UGameplayStatics::SetGamePaused(this, true);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(PauseMenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void ASlimeFablePlayerController::ClosePauseMenu()
{
	if (PauseMenuWidget)
	{
		PauseMenuWidget->TryHandleEscape();
		PauseMenuWidget->RemoveFromParent();
	}

	UGameplayStatics::SetGamePaused(this, false);

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	bShowMouseCursor = false;
}

void ASlimeFablePlayerController::HandlePauseContinue()
{
	ClosePauseMenu();
}

void ASlimeFablePlayerController::HandlePauseLevelSelect()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDayLevelSubsystem* Days = GI->GetSubsystem<UDayLevelSubsystem>())
		{
			Days->TravelToLevelSelect(this);
			return;
		}
	}
	ClosePauseMenu();
}

void ASlimeFablePlayerController::HandlePauseMainMenu()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDayLevelSubsystem* Days = GI->GetSubsystem<UDayLevelSubsystem>())
		{
			Days->TravelToMainMenu(this);
			return;
		}
	}
	ClosePauseMenu();
}

void ASlimeFablePlayerController::HandlePauseReturnToHub()
{
	UGameInstance* GI = GetGameInstance();
	UQuestSubsystem* Quests = GI ? GI->GetSubsystem<UQuestSubsystem>() : nullptr;
	const FName HubDayId = Quests ? Quests->GetHostDayId() : NAME_None;
	ClosePauseMenu();
	if (Quests && !HubDayId.IsNone())
	{
		Quests->TravelToHub(HubDayId);
	}
}

void ASlimeFablePlayerController::UpdateAltCursor()
{
	if (!IsLocalPlayerController() || IsPauseMenuOpen())
	{
		return;
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UQuestSubsystem* Quests = GI->GetSubsystem<UQuestSubsystem>())
		{
			if (Quests->IsQuestLogOpen())
			{
				return;
			}
		}
	}

	const bool bAltDown = IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt);
	if (bAltDown)
	{
		if (!bShowMouseCursor)
		{
			FInputModeGameAndUI InputMode;
			InputMode.SetHideCursorDuringCapture(false);
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(InputMode);
			bShowMouseCursor = true;
		}
	}
	else if (bShowMouseCursor)
	{
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
	}
}

bool ASlimeFablePlayerController::IsPauseMenuOpen() const
{
	return PauseMenuWidget
		&& PauseMenuWidget->IsInViewport()
		&& PauseMenuWidget->GetVisibility() != ESlateVisibility::Collapsed
		&& PauseMenuWidget->GetVisibility() != ESlateVisibility::Hidden;
}
