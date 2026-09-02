// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeFablePlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Components/InputComponent.h"
#include "DayLevel/DayLevelSubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Inventory/SlimeInteractComponent.h"
#include "Inventory/SlimeInventorySubsystem.h"
#include "Slime/SlimeAbilityComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SlimeFable.h"
#include "Quest/QuestSubsystem.h"
#include "Settings/SlimeInputSettings.h"
#include "Settings/SlimeCheatComponent.h"
#include "UI/PauseMenuWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SVirtualJoystick.h"

namespace
{
	bool IsOverlayReason(ESlimeUIInputReason Reason)
	{
		return Reason == ESlimeUIInputReason::QuestLog
			|| Reason == ESlimeUIInputReason::AltCursor
			|| Reason == ESlimeUIInputReason::CheatConsole;
	}

	bool ShouldPauseReason(ESlimeUIInputReason Reason)
	{
		return Reason == ESlimeUIInputReason::Pause
			|| Reason == ESlimeUIInputReason::WeekSelect
			|| Reason == ESlimeUIInputReason::Inventory
			|| Reason == ESlimeUIInputReason::Souvenir
			|| Reason == ESlimeUIInputReason::ElementFormation
			|| Reason == ESlimeUIInputReason::HotbarConfirm
			|| Reason == ESlimeUIInputReason::LoadingGate;
	}

	bool ShouldShowCursor(ESlimeUIInputReason Reason)
	{
		return Reason != ESlimeUIInputReason::LoadingGate;
	}
}

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
		UIInputStack.Reset();
		bPausedByUIInput = false;
		ApplyTopUIInput();
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

void ASlimeFablePlayerController::RetargetUIFocus(UUserWidget* FocusWidget)
{
	if (!IsLocalPlayerController() || !FocusWidget || UIInputStack.IsEmpty())
	{
		return;
	}
	UIInputStack.Last().FocusWidget = FocusWidget;
	ApplyTopUIInput();
	FocusWidget->SetIsFocusable(true);
	FocusWidget->SetKeyboardFocus();
}

void ASlimeFablePlayerController::PushUIInput(ESlimeUIInputReason Reason, UUserWidget* FocusWidget)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	for (int32 Index = UIInputStack.Num() - 1; Index >= 0; --Index)
	{
		if (UIInputStack[Index].Reason == Reason)
		{
			UIInputStack[Index].FocusWidget = FocusWidget;
			if (Index != UIInputStack.Num() - 1)
			{
				const FSlimeUIInputEntry Entry = UIInputStack[Index];
				UIInputStack.RemoveAt(Index);
				UIInputStack.Add(Entry);
			}
			ApplyTopUIInput();
			return;
		}
	}

	FSlimeUIInputEntry Entry;
	Entry.Reason = Reason;
	Entry.FocusWidget = FocusWidget;
	UIInputStack.Add(Entry);
	ApplyTopUIInput();
}

void ASlimeFablePlayerController::PopUIInput(ESlimeUIInputReason Reason)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	UIInputStack.RemoveAll([Reason](const FSlimeUIInputEntry& Entry)
	{
		return Entry.Reason == Reason;
	});
	ApplyTopUIInput();
}

bool ASlimeFablePlayerController::HasUIInput(ESlimeUIInputReason Reason) const
{
	for (const FSlimeUIInputEntry& Entry : UIInputStack)
	{
		if (Entry.Reason == Reason)
		{
			return true;
		}
	}
	return false;
}

bool ASlimeFablePlayerController::HasModalUI() const
{
	for (const FSlimeUIInputEntry& Entry : UIInputStack)
	{
		if (!IsOverlayReason(Entry.Reason))
		{
			return true;
		}
	}
	return false;
}

void ASlimeFablePlayerController::RestoreGameplayInput()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (UIInputStack.IsEmpty())
	{
		if (bPausedByUIInput)
		{
			UGameplayStatics::SetGamePaused(this, false);
			bPausedByUIInput = false;
		}
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
		}
		return;
	}

	ApplyTopUIInput();
}

void ASlimeFablePlayerController::ApplyTopUIInput()
{
	if (UIInputStack.IsEmpty())
	{
		if (bPausedByUIInput)
		{
			UGameplayStatics::SetGamePaused(this, false);
			bPausedByUIInput = false;
		}
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
		return;
	}

	const FSlimeUIInputEntry& Top = UIInputStack.Last();
	if (ShouldPauseReason(Top.Reason))
	{
		UGameplayStatics::SetGamePaused(this, true);
		bPausedByUIInput = true;
	}
	else if (bPausedByUIInput)
	{
		UGameplayStatics::SetGamePaused(this, false);
		bPausedByUIInput = false;
	}

	if (IsOverlayReason(Top.Reason))
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		if (UUserWidget* Focus = Top.FocusWidget.Get())
		{
			InputMode.SetWidgetToFocus(Focus->TakeWidget());
		}
		SetInputMode(InputMode);
		bShowMouseCursor = true;
		return;
	}

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	if (UUserWidget* Focus = Top.FocusWidget.Get())
	{
		InputMode.SetWidgetToFocus(Focus->TakeWidget());
	}
	SetInputMode(InputMode);
	bShowMouseCursor = ShouldShowCursor(Top.Reason);
}

bool ASlimeFablePlayerController::DismissOverlayUI()
{
	UGameInstance* GI = GetGameInstance();
	UQuestSubsystem* Quests = GI ? GI->GetSubsystem<UQuestSubsystem>() : nullptr;
	USlimeInventorySubsystem* Inventory = GI ? GI->GetSubsystem<USlimeInventorySubsystem>() : nullptr;

	if (HasUIInput(ESlimeUIInputReason::WeekSelect))
	{
		if (Quests)
		{
			Quests->CloseWeekSelect();
		}
		return true;
	}
	if (HasUIInput(ESlimeUIInputReason::Souvenir))
	{
		if (Inventory)
		{
			Inventory->CloseSouvenir();
		}
		return true;
	}
	if (HasUIInput(ESlimeUIInputReason::Inventory))
	{
		if (APawn* ControlledPawn = GetPawn())
		{
			if (USlimeInteractComponent* Interact = ControlledPawn->FindComponentByClass<USlimeInteractComponent>())
			{
				Interact->CloseInventory();
			}
		}
		return true;
	}
	if (HasUIInput(ESlimeUIInputReason::ElementFormation))
	{
		if (APawn* ControlledPawn = GetPawn())
		{
			if (USlimeAbilityComponent* Ability = ControlledPawn->FindComponentByClass<USlimeAbilityComponent>())
			{
				Ability->CloseFormation();
			}
		}
		else
		{
			PopUIInput(ESlimeUIInputReason::ElementFormation);
		}
		return true;
	}
	if (HasUIInput(ESlimeUIInputReason::HotbarConfirm))
	{
		if (APawn* ControlledPawn = GetPawn())
		{
			if (USlimeAbilityComponent* Ability = ControlledPawn->FindComponentByClass<USlimeAbilityComponent>())
			{
				Ability->CloseHotbarConfirm();
			}
		}
		else
		{
			PopUIInput(ESlimeUIInputReason::HotbarConfirm);
		}
		return true;
	}
	if (HasUIInput(ESlimeUIInputReason::QuestLog))
	{
		if (Quests)
		{
			Quests->CloseQuestLog();
		}
		return true;
	}
	if (HasUIInput(ESlimeUIInputReason::CheatConsole))
	{
		if (APawn* ControlledPawn = GetPawn())
		{
			if (USlimeCheatComponent* Cheat = ControlledPawn->FindComponentByClass<USlimeCheatComponent>())
			{
				Cheat->CloseConsole();
			}
		}
		else
		{
			PopUIInput(ESlimeUIInputReason::CheatConsole);
		}
		return true;
	}
	return false;
}

void ASlimeFablePlayerController::TogglePauseMenu()
{
	if (DismissOverlayUI())
	{
		return;
	}

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
		PauseMenuWidget->OnResetDayRequested.AddDynamic(this, &ASlimeFablePlayerController::HandlePauseResetDay);
	}

	if (!PauseMenuWidget->IsInViewport())
	{
		PauseMenuWidget->AddToViewport(10);
	}
	PauseMenuWidget->SetVisibility(ESlateVisibility::Visible);
	PauseMenuWidget->RefreshHubButtonVisibility();
	PushUIInput(ESlimeUIInputReason::Pause, PauseMenuWidget);
}

void ASlimeFablePlayerController::ClosePauseMenu()
{
	if (PauseMenuWidget)
	{
		PauseMenuWidget->TryHandleEscape();
		PauseMenuWidget->RemoveFromParent();
	}

	PopUIInput(ESlimeUIInputReason::Pause);
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

void ASlimeFablePlayerController::HandlePauseResetDay()
{
	UGameInstance* GI = GetGameInstance();
	UQuestSubsystem* Quests = GI ? GI->GetSubsystem<UQuestSubsystem>() : nullptr;
	ClosePauseMenu();
	if (Quests)
	{
		Quests->ResetDayProgressAndReload();
	}
}

void ASlimeFablePlayerController::UpdateAltCursor()
{
	if (!IsLocalPlayerController() || HasModalUI())
	{
		return;
	}

	bool bShowCursorDown = false;
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const USlimeInputSettings* InputSettings = GI->GetSubsystem<USlimeInputSettings>())
		{
			bShowCursorDown = InputSettings->IsKeyDown(this, ESlimeInputAction::ShowCursor);
			if (InputSettings->GetKey(ESlimeInputAction::ShowCursor) == EKeys::LeftAlt)
			{
				bShowCursorDown = bShowCursorDown || IsInputKeyDown(EKeys::RightAlt);
			}
		}
		else
		{
			bShowCursorDown = IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt);
		}
	}
	if (bShowCursorDown)
	{
		if (!HasUIInput(ESlimeUIInputReason::AltCursor))
		{
			PushUIInput(ESlimeUIInputReason::AltCursor, nullptr);
		}
	}
	else if (HasUIInput(ESlimeUIInputReason::AltCursor))
	{
		PopUIInput(ESlimeUIInputReason::AltCursor);
	}
}

bool ASlimeFablePlayerController::IsPauseMenuOpen() const
{
	return PauseMenuWidget
		&& PauseMenuWidget->IsInViewport()
		&& PauseMenuWidget->GetVisibility() != ESlateVisibility::Collapsed
		&& PauseMenuWidget->GetVisibility() != ESlateVisibility::Hidden;
}
