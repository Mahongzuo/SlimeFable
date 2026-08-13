// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/KeybindSettingsWidget.h"
#include "UI/MenuUIStyle.h"
#include "Settings/SlimeInputSettings.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

void USlimeKeybindRowProxy::HandleClicked()
{
	if (Owner)
	{
		Owner->BeginRebind(Action);
	}
}

void UKeybindSettingsWidget::BeginRebind(ESlimeInputAction Action)
{
	ListeningAction = Action;
	SetIsFocusable(true);
	SetKeyboardFocus();
	if (StatusText)
	{
		StatusText->SetVisibility(ESlateVisibility::Visible);
		StatusText->SetText(FText::FromString(TEXT("请按下新按键…（Esc 取消）")));
		FMenuUIStyle::ApplyBrushCJKFont(StatusText, 16.f, FMenuUIStyle::WarmMutedTextColor());
	}
}

TSharedRef<SWidget> UKeybindSettingsWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void UKeybindSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyLook();
	SetIsFocusable(true);

	if (ResetButton)
	{
		ResetButton->OnClicked.AddUniqueDynamic(this, &UKeybindSettingsWidget::OnResetClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked.AddUniqueDynamic(this, &UKeybindSettingsWidget::OnBackClicked);
	}

	RefreshList();
}

void UKeybindSettingsWidget::SetReturnTarget(UUserWidget* InTarget)
{
	ReturnTarget = InTarget;
}

USlimeInputSettings* UKeybindSettingsWidget::GetInputSettings() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<USlimeInputSettings>();
	}
	return nullptr;
}

void UKeybindSettingsWidget::BuildLayoutIfNeeded()
{
	if (TitleText && BindScroll && ResetButton && BackButton)
	{
		bBuiltInCode = false;
		return;
	}

	bBuiltInCode = true;
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = Root;

	BackgroundImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BackgroundImage"));
	if (UCanvasPanelSlot* BgSlot = Root->AddChildToCanvas(BackgroundImage))
	{
		BgSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BgSlot->SetOffsets(FMargin(0.f));
	}

	DimOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DimOverlay"));
	if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(DimOverlay))
	{
		DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimSlot->SetOffsets(FMargin(0.f));
	}

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuBox"));
	if (UCanvasPanelSlot* VBoxSlot = Root->AddChildToCanvas(VBox))
	{
		VBoxSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		VBoxSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		VBoxSlot->SetAutoSize(true);
	}

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("自定义按键")));
	if (UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
	}

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* StatusSlot = VBox->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		StatusSlot->SetHorizontalAlignment(HAlign_Center);
	}

	USizeBox* ScrollSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ScrollSize"));
	ScrollSize->SetWidthOverride(520.f);
	ScrollSize->SetHeightOverride(420.f);
	if (UVerticalBoxSlot* ScrollSizeSlot = VBox->AddChildToVerticalBox(ScrollSize))
	{
		ScrollSizeSlot->SetPadding(FMargin(0.f, 4.f));
		ScrollSizeSlot->SetHorizontalAlignment(HAlign_Center);
	}

	BindScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BindScroll"));
	ScrollSize->AddChild(BindScroll);

	BindList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BindList"));
	BindScroll->AddChild(BindList);

	auto AddButton = [this, VBox](const FName& Name, const FText& Label) -> UButton*
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			*FString::Printf(TEXT("%s_Size"), *Name.ToString()));
		SizeBox->SetWidthOverride(320.f);
		SizeBox->SetHeightOverride(56.f);
		UVerticalBoxSlot* SizeSlot = VBox->AddChildToVerticalBox(SizeBox);
		SizeSlot->SetPadding(FMargin(0.f, 8.f));
		SizeSlot->SetHorizontalAlignment(HAlign_Center);

		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		SizeBox->AddChild(Button);
		UTextBlock* LabelBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			*FString::Printf(TEXT("%s_Label"), *Name.ToString()));
		LabelBlock->SetText(Label);
		LabelBlock->SetJustification(ETextJustify::Center);
		Button->AddChild(LabelBlock);
		return Button;
	};

	ResetButton = AddButton(TEXT("ResetButton"), FText::FromString(TEXT("恢复默认")));
	BackButton = AddButton(TEXT("BackButton"), FText::FromString(TEXT("返回")));
}

void UKeybindSettingsWidget::ApplyLook()
{
	FMenuUIStyle::ApplyMenuBackground(BackgroundImage);
	if (DimOverlay)
	{
		FSlateBrush DimBrush;
		DimBrush.DrawAs = ESlateBrushDrawType::Box;
		DimBrush.TintColor = FSlateColor(FLinearColor(0.04f, 0.03f, 0.02f, 0.5f));
		DimBrush.Margin = FMargin(0.f);
		DimBrush.ImageSize = FVector2D(32.f, 32.f);
		DimOverlay->SetBrush(DimBrush);
	}

	FMenuUIStyle::ApplyBrushCJKFont(TitleText, 36.f, FMenuUIStyle::WarmTitleColor());
	UMaterialInterface* BrushBtn = FMenuUIStyle::LoadButtonMaterial();
	FMenuUIStyle::ApplyMaterialButtonStyle(ResetButton, BrushBtn, FVector2D(320.f, 56.f));
	FMenuUIStyle::ApplyMaterialButtonStyle(BackButton, BrushBtn, FVector2D(320.f, 56.f));

	auto StyleChildLabel = [](UButton* Button, float Size)
	{
		if (Button)
		{
			if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
			{
				FMenuUIStyle::ApplyBrushCJKFont(Label, Size, FMenuUIStyle::WarmTextColor());
			}
		}
	};
	StyleChildLabel(ResetButton, 20.f);
	StyleChildLabel(BackButton, 20.f);
}

void UKeybindSettingsWidget::RefreshList()
{
	USlimeInputSettings* Settings = GetInputSettings();
	if (!BindList && BindScroll)
	{
		BindList = Cast<UVerticalBox>(BindScroll->GetChildAt(0));
		if (!BindList)
		{
			BindList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BindList"));
			BindScroll->ClearChildren();
			BindScroll->AddChild(BindList);
		}
	}
	if (!Settings || !BindList)
	{
		return;
	}

	BindList->ClearChildren();
	RowProxies.Reset();
	ActionKeyLabels.Reset();

	UMaterialInterface* BrushBtn = FMenuUIStyle::LoadButtonMaterial();

	for (ESlimeInputAction Action : USlimeInputSettings::GetAllActions())
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			*FString::Printf(TEXT("Row_%d"), static_cast<int32>(Action)));
		if (UVerticalBoxSlot* RowSlot = BindList->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, 4.f));
		}

		UTextBlock* NameLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			*FString::Printf(TEXT("Name_%d"), static_cast<int32>(Action)));
		NameLabel->SetText(Settings->GetActionDisplayName(Action));
		FMenuUIStyle::ApplyBrushCJKFont(NameLabel, 18.f, FMenuUIStyle::WarmTextColor());
		if (UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(NameLabel))
		{
			NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			NameSlot->SetVerticalAlignment(VAlign_Center);
			NameSlot->SetPadding(FMargin(8.f, 0.f));
		}

		USizeBox* KeySize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			*FString::Printf(TEXT("KeySize_%d"), static_cast<int32>(Action)));
		KeySize->SetWidthOverride(160.f);
		KeySize->SetHeightOverride(44.f);
		Row->AddChildToHorizontalBox(KeySize);

		UButton* KeyButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			*FString::Printf(TEXT("KeyBtn_%d"), static_cast<int32>(Action)));
		KeySize->AddChild(KeyButton);
		FMenuUIStyle::ApplyMaterialButtonStyle(KeyButton, BrushBtn, FVector2D(160.f, 44.f));

		UTextBlock* KeyLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			*FString::Printf(TEXT("KeyLbl_%d"), static_cast<int32>(Action)));
		KeyLabel->SetText(Settings->GetKeyDisplayName(Action));
		KeyLabel->SetJustification(ETextJustify::Center);
		FMenuUIStyle::ApplyMarkerFont(KeyLabel, 18.f, FMenuUIStyle::WarmTextColor());
		KeyButton->AddChild(KeyLabel);
		ActionKeyLabels.Add(Action, KeyLabel);

		USlimeKeybindRowProxy* Proxy = NewObject<USlimeKeybindRowProxy>(this);
		Proxy->Action = Action;
		Proxy->Owner = this;
		KeyButton->OnClicked.AddUniqueDynamic(Proxy, &USlimeKeybindRowProxy::HandleClicked);
		RowProxies.Add(Proxy);
	}
}

bool UKeybindSettingsWidget::CaptureKey(FKey Key)
{
	if (!ListeningAction.IsSet())
	{
		return false;
	}

	if (Key == EKeys::Escape)
	{
		ListeningAction.Reset();
		if (StatusText)
		{
			StatusText->SetVisibility(ESlateVisibility::Collapsed);
		}
		return true;
	}

	USlimeInputSettings* Settings = GetInputSettings();
	if (!Settings)
	{
		ListeningAction.Reset();
		return true;
	}

	FText Error;
	const ESlimeInputAction Action = ListeningAction.GetValue();
	if (!Settings->TrySetKey(Action, Key, Error))
	{
		if (StatusText)
		{
			StatusText->SetVisibility(ESlateVisibility::Visible);
			StatusText->SetText(Error);
			FMenuUIStyle::ApplyBrushCJKFont(StatusText, 16.f, FMenuUIStyle::WarmMutedTextColor());
		}
		return true;
	}

	ListeningAction.Reset();
	if (StatusText)
	{
		StatusText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (TObjectPtr<UTextBlock>* Label = ActionKeyLabels.Find(Action))
	{
		if (*Label)
		{
			(*Label)->SetText(Settings->GetKeyDisplayName(Action));
		}
	}

	if (APlayerController* PC = GetOwningPlayer())
	{
		Settings->ApplyEnhancedInputRemaps(PC);
	}
	return true;
}

FReply UKeybindSettingsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (ListeningAction.IsSet())
	{
		CaptureKey(InKeyEvent.GetKey());
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UKeybindSettingsWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (ListeningAction.IsSet())
	{
		CaptureKey(InMouseEvent.GetEffectingButton());
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UKeybindSettingsWidget::OnResetClicked()
{
	if (USlimeInputSettings* Settings = GetInputSettings())
	{
		Settings->ResetToDefaults();
		if (APlayerController* PC = GetOwningPlayer())
		{
			Settings->ApplyEnhancedInputRemaps(PC);
		}
	}
	ListeningAction.Reset();
	if (StatusText)
	{
		StatusText->SetVisibility(ESlateVisibility::Collapsed);
	}
	RefreshList();
}

void UKeybindSettingsWidget::OnBackClicked()
{
	ListeningAction.Reset();
	RemoveFromParent();
	if (UUserWidget* Target = ReturnTarget.Get())
	{
		Target->SetVisibility(ESlateVisibility::Visible);
	}
}
