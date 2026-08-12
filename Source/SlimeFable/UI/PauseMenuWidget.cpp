// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PauseMenuWidget.h"
#include "UI/MenuUIStyle.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"

TSharedRef<SWidget> UPauseMenuWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyLook();
	SetIsFocusable(true);

	if (ContinueButton)
	{
		ContinueButton->OnClicked.AddUniqueDynamic(this, &UPauseMenuWidget::OnContinueClicked);
	}
	if (LevelSelectButton)
	{
		LevelSelectButton->OnClicked.AddUniqueDynamic(this, &UPauseMenuWidget::OnLevelSelectClicked);
	}
	if (MainMenuButton)
	{
		MainMenuButton->OnClicked.AddUniqueDynamic(this, &UPauseMenuWidget::OnMainMenuClicked);
	}
}

FReply UPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnContinueClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UPauseMenuWidget::BuildLayoutIfNeeded()
{
	if (TitleText && ContinueButton && LevelSelectButton && MainMenuButton)
	{
		bBuiltInCode = false;
		return;
	}

	bBuiltInCode = true;
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = Root;

	DimOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DimOverlay"));
	if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(DimOverlay))
	{
		DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimSlot->SetOffsets(FMargin(0.f));
	}

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PauseBox"));
	if (UCanvasPanelSlot* VBoxSlot = Root->AddChildToCanvas(VBox))
	{
		VBoxSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		VBoxSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		VBoxSlot->SetAutoSize(true);
	}

	auto AddText = [this, VBox](const FName& Name, const FText& Text) -> UTextBlock*
	{
		UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Block->SetText(Text);
		UVerticalBoxSlot* Slot = VBox->AddChildToVerticalBox(Block);
		Slot->SetPadding(FMargin(0.f, 12.f));
		Slot->SetHorizontalAlignment(HAlign_Center);
		return Block;
	};

	auto AddButton = [this, VBox](const FName& Name, const FText& Label) -> UButton*
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			*FString::Printf(TEXT("%s_Size"), *Name.ToString()));
		SizeBox->SetWidthOverride(320.f);
		SizeBox->SetHeightOverride(60.f);
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

	TitleText = AddText(TEXT("TitleText"), FText::FromString(TEXT("暂停")));
	ContinueButton = AddButton(TEXT("ContinueButton"), FText::FromString(TEXT("继续游戏")));
	LevelSelectButton = AddButton(TEXT("LevelSelectButton"), FText::FromString(TEXT("返回选关")));
	MainMenuButton = AddButton(TEXT("MainMenuButton"), FText::FromString(TEXT("返回主菜单")));
}

void UPauseMenuWidget::ApplyLook()
{
	if (DimOverlay)
	{
		FSlateBrush DimBrush;
		DimBrush.DrawAs = ESlateBrushDrawType::Box;
		DimBrush.TintColor = FSlateColor(FLinearColor(0.04f, 0.03f, 0.02f, 0.62f));
		DimBrush.Margin = FMargin(0.f);
		DimBrush.ImageSize = FVector2D(32.f, 32.f);
		DimOverlay->SetBrush(DimBrush);
		DimOverlay->SetColorAndOpacity(FLinearColor::White);
	}

	FMenuUIStyle::ApplyTitleFont(TitleText, 48.f, FMenuUIStyle::WarmTitleColor());
	UMaterialInterface* BrushBtn = FMenuUIStyle::LoadButtonMaterial();
	FMenuUIStyle::ApplyMaterialButtonStyle(ContinueButton, BrushBtn, FVector2D(320.f, 60.f));
	FMenuUIStyle::ApplyMaterialButtonStyle(LevelSelectButton, BrushBtn, FVector2D(320.f, 60.f));
	FMenuUIStyle::ApplyMaterialButtonStyle(MainMenuButton, BrushBtn, FVector2D(320.f, 60.f));

	auto StyleLabel = [](UButton* Button)
	{
		if (!Button || Button->GetChildrenCount() == 0)
		{
			return;
		}
		if (UTextBlock* Label = Cast<UTextBlock>(Button->GetChildAt(0)))
		{
			FMenuUIStyle::ApplyMixedMenuFont(Label, 22.f, FMenuUIStyle::WarmTextColor());
		}
	};
	StyleLabel(ContinueButton);
	StyleLabel(LevelSelectButton);
	StyleLabel(MainMenuButton);
}

void UPauseMenuWidget::OnContinueClicked()
{
	OnContinueRequested.Broadcast();
}

void UPauseMenuWidget::OnLevelSelectClicked()
{
	OnLevelSelectRequested.Broadcast();
}

void UPauseMenuWidget::OnMainMenuClicked()
{
	OnMainMenuRequested.Broadcast();
}
