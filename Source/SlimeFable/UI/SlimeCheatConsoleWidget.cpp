// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeCheatConsoleWidget.h"

#include "UI/MenuUIStyle.h"
#include "Settings/SlimeCheatComponent.h"
#include "SlimeFablePlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableText.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

TSharedRef<SWidget> USlimeCheatConsoleWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void USlimeCheatConsoleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	BuildLayoutIfNeeded();
	ApplyLook();
	if (CommandInput)
	{
		CommandInput->OnTextCommitted.AddUniqueDynamic(this, &USlimeCheatConsoleWidget::OnCommandCommitted);
	}
}

void USlimeCheatConsoleWidget::NativeDestruct()
{
	if (CommandInput)
	{
		CommandInput->OnTextCommitted.RemoveDynamic(this, &USlimeCheatConsoleWidget::OnCommandCommitted);
	}
	Super::NativeDestruct();
}

void USlimeCheatConsoleWidget::Setup(USlimeCheatComponent* InOwner)
{
	OwnerCheat = InOwner;
	ToastRemaining = 0.f;
	bWantFocus = true;
	if (const UWorld* World = GetWorld())
	{
		IgnoreCommitUntil = World->GetTimeSeconds() + 0.2f;
	}
	if (CommandInput)
	{
		CommandInput->SetText(FText::GetEmpty());
	}
	if (ToastPanel)
	{
		ToastPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USlimeCheatConsoleWidget::SetIgnoreCommitUntil(float WorldTimeSeconds)
{
	IgnoreCommitUntil = WorldTimeSeconds;
}

bool USlimeCheatConsoleWidget::ShouldIgnoreCommit() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < IgnoreCommitUntil;
}

void USlimeCheatConsoleWidget::ShowInputBar(bool bShow)
{
	if (InputSize)
	{
		InputSize->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	bWantFocus = bShow;
	if (!bShow)
	{
		bWantFocus = false;
		if (CommandInput)
		{
			CommandInput->SetText(FText::GetEmpty());
		}
	}
}

void USlimeCheatConsoleWidget::ShowToast(const FString& Message, float Duration)
{
	if (!ToastText || !ToastPanel || Message.IsEmpty())
	{
		return;
	}
	bWantFocus = false;
	ToastText->SetText(FText::FromString(Message));
	ToastPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	SetVisibility(ESlateVisibility::HitTestInvisible);
	ToastRemaining = FMath::Max(Duration, 0.1f);
}

void USlimeCheatConsoleWidget::FocusInput()
{
	if (!CommandInput || (InputSize && InputSize->GetVisibility() == ESlateVisibility::Collapsed))
	{
		return;
	}

	bWantFocus = true;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	CommandInput->SetKeyboardFocus();
	if (TSharedPtr<SWidget> Slate = CommandInput->GetCachedWidget())
	{
		FSlateApplication::Get().SetKeyboardFocus(Slate, EFocusCause::SetDirectly);
	}

	// GameAndUI keeps LMB/MMB combat polling alive; only soft-focus the text field.
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(CommandInput->TakeWidget());
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}
}

void USlimeCheatConsoleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bWantFocus && CommandInput && InputSize
		&& InputSize->GetVisibility() != ESlateVisibility::Collapsed)
	{
		const bool bHasFocus = CommandInput->HasKeyboardFocus()
			|| (CommandInput->GetCachedWidget().IsValid()
				&& FSlateApplication::Get().GetKeyboardFocusedWidget() == CommandInput->GetCachedWidget());
		if (!bHasFocus)
		{
			FocusInput();
		}
		else
		{
			bWantFocus = false;
		}
	}

	if (ToastRemaining > 0.f)
	{
		ToastRemaining -= InDeltaTime;
		if (ToastRemaining <= 0.f)
		{
			ToastRemaining = 0.f;
			if (ToastPanel)
			{
				ToastPanel->SetVisibility(ESlateVisibility::Collapsed);
			}
			if (InputSize && InputSize->GetVisibility() == ESlateVisibility::Collapsed)
			{
				RemoveFromParent();
				if (ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(GetOwningPlayer()))
				{
					SlimePC->RestoreGameplayInput();
				}
			}
		}
	}
}

FReply USlimeCheatConsoleWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (USlimeCheatComponent* Cheat = OwnerCheat.Get())
		{
			Cheat->CloseConsole();
		}
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::Enter && !ShouldIgnoreCommit())
	{
		TrySubmitFromEnterKey();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void USlimeCheatConsoleWidget::OnCommandCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod != ETextCommit::OnEnter)
	{
		return;
	}
	if (ShouldIgnoreCommit())
	{
		return;
	}
	SubmitCommand();
}

void USlimeCheatConsoleWidget::TrySubmitFromEnterKey()
{
	if (ShouldIgnoreCommit())
	{
		return;
	}
	SubmitCommand();
}

void USlimeCheatConsoleWidget::SubmitCommand()
{
	USlimeCheatComponent* Cheat = OwnerCheat.Get();
	if (!Cheat || !CommandInput)
	{
		return;
	}

	const FString Raw = CommandInput->GetText().ToString().TrimStartAndEnd();
	CommandInput->SetText(FText::GetEmpty());

	if (Raw.IsEmpty())
	{
		Cheat->CloseConsole();
		return;
	}

	Cheat->HandleCommand(Raw);
}

void USlimeCheatConsoleWidget::BuildLayoutIfNeeded()
{
	if (CommandInput && ToastText)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	ToastPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ToastPanel"));
	ToastPanel->SetPadding(FMargin(8.f, 4.f));
	ToastPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* ToastSlot = Root->AddChildToCanvas(ToastPanel))
	{
		ToastSlot->SetAnchors(FAnchors(0.f, 1.f));
		ToastSlot->SetAlignment(FVector2D(0.f, 1.f));
		ToastSlot->SetOffsets(FMargin(16.f, 0.f, 0.f, 52.f));
		ToastSlot->SetAutoSize(true);
	}
	ToastText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ToastText"));
	ToastPanel->SetContent(ToastText);

	InputSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("InputSize"));
	InputSize->SetWidthOverride(340.f);
	if (UCanvasPanelSlot* InputSlot = Root->AddChildToCanvas(InputSize))
	{
		InputSlot->SetAnchors(FAnchors(0.f, 1.f));
		InputSlot->SetAlignment(FVector2D(0.f, 1.f));
		InputSlot->SetOffsets(FMargin(16.f, 0.f, 0.f, 18.f));
		InputSlot->SetAutoSize(true);
	}

	InputPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InputPanel"));
	InputPanel->SetPadding(FMargin(6.f, 3.f));
	InputSize->AddChild(InputPanel);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	InputPanel->SetContent(Row);

	PromptText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PromptText"));
	PromptText->SetText(FText::FromString(TEXT(">")));
	if (UHorizontalBoxSlot* PromptSlot = Row->AddChildToHorizontalBox(PromptText))
	{
		PromptSlot->SetVerticalAlignment(VAlign_Center);
		PromptSlot->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
	}

	CommandInput = WidgetTree->ConstructWidget<UEditableText>(UEditableText::StaticClass(), TEXT("CommandInput"));
	CommandInput->SetHintText(FText::FromString(TEXT("")));
	CommandInput->SetIsReadOnly(false);
	CommandInput->SetClearKeyboardFocusOnCommit(false);
	if (UHorizontalBoxSlot* EditSlot = Row->AddChildToHorizontalBox(CommandInput))
	{
		EditSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		EditSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void USlimeCheatConsoleWidget::ApplyLook()
{
	const FLinearColor PanelFill(0.02f, 0.03f, 0.02f, 0.55f);
	const FLinearColor ToastFill(0.02f, 0.03f, 0.02f, 0.4f);
	const FLinearColor CliGreen(0.45f, 0.85f, 0.45f, 0.85f);
	const FLinearColor CliMuted(0.55f, 0.7f, 0.55f, 0.75f);

	if (InputPanel)
	{
		InputPanel->SetBrushColor(PanelFill);
	}
	if (ToastPanel)
	{
		ToastPanel->SetBrushColor(ToastFill);
	}
	if (PromptText)
	{
		FMenuUIStyle::ApplyMarkerFont(PromptText, 13.f, CliGreen);
	}
	if (ToastText)
	{
		FMenuUIStyle::ApplyBrushCJKFont(ToastText, 13.f, CliMuted);
	}
	if (CommandInput)
	{
		CommandInput->SetFont(FMenuUIStyle::MakeMarkerFont(13.f));
	}
}
