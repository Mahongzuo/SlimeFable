// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/GraphicsSettingsWidget.h"
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
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"

TSharedRef<SWidget> UGraphicsSettingsWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void UGraphicsSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyLook();

	if (Quality0Button)
	{
		Quality0Button->OnClicked.AddUniqueDynamic(this, &UGraphicsSettingsWidget::OnQuality0Clicked);
	}
	if (Quality1Button)
	{
		Quality1Button->OnClicked.AddUniqueDynamic(this, &UGraphicsSettingsWidget::OnQuality1Clicked);
	}
	if (Quality2Button)
	{
		Quality2Button->OnClicked.AddUniqueDynamic(this, &UGraphicsSettingsWidget::OnQuality2Clicked);
	}
	if (Quality3Button)
	{
		Quality3Button->OnClicked.AddUniqueDynamic(this, &UGraphicsSettingsWidget::OnQuality3Clicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked.AddUniqueDynamic(this, &UGraphicsSettingsWidget::OnBackClicked);
	}

	RefreshSelection();
}

void UGraphicsSettingsWidget::SetReturnTarget(UUserWidget* InTarget)
{
	ReturnTarget = InTarget;
}

void UGraphicsSettingsWidget::BuildLayoutIfNeeded()
{
	if (TitleText && Quality0Button && Quality1Button && Quality2Button && Quality3Button && BackButton)
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
	TitleText->SetText(FText::FromString(TEXT("画质选择")));
	if (UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
	}

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	if (UVerticalBoxSlot* StatusSlot = VBox->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		StatusSlot->SetHorizontalAlignment(HAlign_Center);
	}

	auto AddButton = [this, VBox](const FName& Name, const FText& Label) -> UButton*
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			*FString::Printf(TEXT("%s_Size"), *Name.ToString()));
		SizeBox->SetWidthOverride(360.f);
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

	Quality0Button = AddButton(TEXT("Quality0Button"), FText::FromString(TEXT("流畅")));
	Quality1Button = AddButton(TEXT("Quality1Button"), FText::FromString(TEXT("均衡")));
	Quality2Button = AddButton(TEXT("Quality2Button"), FText::FromString(TEXT("高清")));
	Quality3Button = AddButton(TEXT("Quality3Button"), FText::FromString(TEXT("极致")));
	BackButton = AddButton(TEXT("BackButton"), FText::FromString(TEXT("返回")));
}

void UGraphicsSettingsWidget::ApplyLook()
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
	const FVector2D Size(360.f, 60.f);
	FMenuUIStyle::ApplyMaterialButtonStyle(Quality0Button, BrushBtn, Size);
	FMenuUIStyle::ApplyMaterialButtonStyle(Quality1Button, BrushBtn, Size);
	FMenuUIStyle::ApplyMaterialButtonStyle(Quality2Button, BrushBtn, Size);
	FMenuUIStyle::ApplyMaterialButtonStyle(Quality3Button, BrushBtn, Size);
	FMenuUIStyle::ApplyMaterialButtonStyle(BackButton, BrushBtn, Size);

	auto StyleChildLabel = [](UButton* Button, float FontSize)
	{
		if (Button)
		{
			if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
			{
				FMenuUIStyle::ApplyBrushCJKFont(Label, FontSize, FMenuUIStyle::WarmTextColor());
			}
		}
	};
	StyleChildLabel(Quality0Button, 22.f);
	StyleChildLabel(Quality1Button, 22.f);
	StyleChildLabel(Quality2Button, 22.f);
	StyleChildLabel(Quality3Button, 22.f);
	StyleChildLabel(BackButton, 20.f);
}

void UGraphicsSettingsWidget::RefreshSelection()
{
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	const int32 Level = Settings ? Settings->GetOverallScalabilityLevel() : 1;

	static const TCHAR* Names[] = { TEXT("流畅"), TEXT("均衡"), TEXT("高清"), TEXT("极致") };
	const int32 Clamped = FMath::Clamp(Level, 0, 3);
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("当前：%s"), Names[Clamped])));
		FMenuUIStyle::ApplyBrushCJKFont(StatusText, 18.f, FMenuUIStyle::WarmMutedTextColor());
	}

	UButton* Buttons[] = { Quality0Button, Quality1Button, Quality2Button, Quality3Button };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (!Buttons[Index])
		{
			continue;
		}
		if (UTextBlock* Label = Cast<UTextBlock>(Buttons[Index]->GetContent()))
		{
			const FString Text = Index == Clamped
				? FString::Printf(TEXT("● %s"), Names[Index])
				: FString(Names[Index]);
			Label->SetText(FText::FromString(Text));
			FMenuUIStyle::ApplyBrushCJKFont(
				Label,
				22.f,
				Index == Clamped ? FMenuUIStyle::WarmTitleColor() : FMenuUIStyle::WarmTextColor());
		}
	}
}

void UGraphicsSettingsWidget::ApplyQuality(int32 Level)
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetOverallScalabilityLevel(FMath::Clamp(Level, 0, 3));
		Settings->ApplySettings(false);
		Settings->SaveSettings();
	}
	RefreshSelection();
}

void UGraphicsSettingsWidget::OnQuality0Clicked() { ApplyQuality(0); }
void UGraphicsSettingsWidget::OnQuality1Clicked() { ApplyQuality(1); }
void UGraphicsSettingsWidget::OnQuality2Clicked() { ApplyQuality(2); }
void UGraphicsSettingsWidget::OnQuality3Clicked() { ApplyQuality(3); }

void UGraphicsSettingsWidget::OnBackClicked()
{
	RemoveFromParent();
	if (UUserWidget* Target = ReturnTarget.Get())
	{
		Target->SetVisibility(ESlateVisibility::Visible);
	}
}
