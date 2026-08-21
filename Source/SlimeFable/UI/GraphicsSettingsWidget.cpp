// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/GraphicsSettingsWidget.h"
#include "UI/MenuUIStyle.h"
#include "Settings/SlimeGraphicsSettings.h"
#include "Settings/SlimeGraphicsTypes.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

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
	if (UpscalerButton)
	{
		UpscalerButton->OnClicked.AddUniqueDynamic(this, &UGraphicsSettingsWidget::OnUpscalerClicked);
	}
	if (DLSSQualityButton)
	{
		DLSSQualityButton->OnClicked.AddUniqueDynamic(this, &UGraphicsSettingsWidget::OnDLSSQualityClicked);
	}
	if (FrameGenButton)
	{
		FrameGenButton->OnClicked.AddUniqueDynamic(this, &UGraphicsSettingsWidget::OnFrameGenClicked);
	}
	if (AutoDetectButton)
	{
		AutoDetectButton->OnClicked.AddUniqueDynamic(this, &UGraphicsSettingsWidget::OnAutoDetectClicked);
	}
	if (PixelStreamingButton)
	{
		PixelStreamingButton->OnClicked.AddUniqueDynamic(this, &UGraphicsSettingsWidget::OnPixelStreamingClicked);
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

USlimeGraphicsSettings* UGraphicsSettingsWidget::GetGraphicsSettings() const
{
	if (UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<USlimeGraphicsSettings>();
	}
	return nullptr;
}

void UGraphicsSettingsWidget::BuildLayoutIfNeeded()
{
	if (TitleText && Quality0Button && Quality1Button && Quality2Button && Quality3Button
		&& UpscalerButton && DLSSQualityButton && FrameGenButton && AutoDetectButton
		&& PixelStreamingButton && BackButton)
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

	USizeBox* ScrollSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MenuScrollSize"));
	ScrollSize->SetWidthOverride(400.f);
	ScrollSize->SetHeightOverride(720.f);
	if (UCanvasPanelSlot* SizeSlot = Root->AddChildToCanvas(ScrollSize))
	{
		SizeSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		SizeSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		SizeSlot->SetAutoSize(true);
	}

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("MenuScroll"));
	ScrollSize->AddChild(Scroll);

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuBox"));
	if (UScrollBoxSlot* BoxSlot = Cast<UScrollBoxSlot>(Scroll->AddChild(VBox)))
	{
		BoxSlot->SetHorizontalAlignment(HAlign_Center);
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
		SizeBox->SetHeightOverride(52.f);
		UVerticalBoxSlot* SizeSlot = VBox->AddChildToVerticalBox(SizeBox);
		SizeSlot->SetPadding(FMargin(0.f, 6.f));
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
	UpscalerButton = AddButton(TEXT("UpscalerButton"), FText::FromString(TEXT("超分：关(TSR)")));
	DLSSQualityButton = AddButton(TEXT("DLSSQualityButton"), FText::FromString(TEXT("超分档位：质量")));
	FrameGenButton = AddButton(TEXT("FrameGenButton"), FText::FromString(TEXT("帧生成：关")));
	AutoDetectButton = AddButton(TEXT("AutoDetectButton"), FText::FromString(TEXT("自动检测")));
	PixelStreamingButton = AddButton(TEXT("PixelStreamingButton"), FText::FromString(TEXT("像素流送：关")));
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
	const FVector2D Size(360.f, 52.f);
	UButton* AllButtons[] = {
		Quality0Button, Quality1Button, Quality2Button, Quality3Button,
		UpscalerButton, DLSSQualityButton, FrameGenButton, AutoDetectButton, PixelStreamingButton, BackButton
	};
	for (UButton* Button : AllButtons)
	{
		FMenuUIStyle::ApplyMaterialButtonStyle(Button, BrushBtn, Size);
	}

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
	StyleChildLabel(UpscalerButton, 20.f);
	StyleChildLabel(DLSSQualityButton, 20.f);
	StyleChildLabel(FrameGenButton, 20.f);
	StyleChildLabel(AutoDetectButton, 20.f);
	StyleChildLabel(PixelStreamingButton, 20.f);
	StyleChildLabel(BackButton, 20.f);
}

void UGraphicsSettingsWidget::SetButtonLabel(UButton* Button, const FText& Label, bool bSelected, float FontSize)
{
	if (!Button)
	{
		return;
	}
	if (UTextBlock* Text = Cast<UTextBlock>(Button->GetContent()))
	{
		Text->SetText(Label);
		FMenuUIStyle::ApplyBrushCJKFont(
			Text,
			FontSize,
			bSelected ? FMenuUIStyle::WarmTitleColor() : FMenuUIStyle::WarmTextColor());
	}
}

void UGraphicsSettingsWidget::RefreshSelection()
{
	USlimeGraphicsSettings* Graphics = GetGraphicsSettings();
	const int32 Level = Graphics ? Graphics->GetQualityLevel() : 1;
	static const TCHAR* Names[] = { TEXT("流畅"), TEXT("均衡"), TEXT("高清"), TEXT("极致") };
	const int32 Clamped = FMath::Clamp(Level, 0, 3);

	if (StatusText)
	{
		const FText Status = Graphics
			? Graphics->GetStatusText()
			: FText::FromString(FString::Printf(TEXT("当前：%s"), Names[Clamped]));
		StatusText->SetText(Status);
		FMenuUIStyle::ApplyBrushCJKFont(StatusText, 16.f, FMenuUIStyle::WarmMutedTextColor());
	}

	UButton* QualityButtons[] = { Quality0Button, Quality1Button, Quality2Button, Quality3Button };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FString Text = Index == Clamped
			? FString::Printf(TEXT("● %s"), Names[Index])
			: FString(Names[Index]);
		SetButtonLabel(QualityButtons[Index], FText::FromString(Text), Index == Clamped, 22.f);
	}

	const ESlimeUpscaler Mode = Graphics ? Graphics->GetUpscaler() : ESlimeUpscaler::Off;
	FString UpscalerLabel = FString::Printf(TEXT("超分：%s"), Graphics ? *Graphics->GetUpscalerDisplayName().ToString() : TEXT("关(TSR)"));
	if (Graphics && !Graphics->IsDlssSupported() && !Graphics->IsFsrPluginPresent())
	{
		UpscalerLabel = TEXT("超分：关(TSR)");
	}
	SetButtonLabel(UpscalerButton, FText::FromString(UpscalerLabel), Mode != ESlimeUpscaler::Off, 20.f);

	if (DLSSQualityButton)
	{
		const bool bDlssOn = Mode == ESlimeUpscaler::DLSS;
		const FString QualityLabel = bDlssOn && Graphics
			? FString::Printf(TEXT("超分档位：%s"), *Graphics->GetDLSSQualityDisplayName().ToString())
			: TEXT("超分档位：—");
		SetButtonLabel(DLSSQualityButton, FText::FromString(QualityLabel), bDlssOn, 20.f);
		DLSSQualityButton->SetIsEnabled(bDlssOn);
	}

	if (FrameGenButton)
	{
		const bool bFgOn = Graphics && Graphics->IsFrameGenEnabled();
		const bool bFgOk = Graphics && Graphics->IsFrameGenSupported() && Mode == ESlimeUpscaler::DLSS;
		FString FgLabel = TEXT("帧生成：关");
		if (bFgOn)
		{
			FgLabel = TEXT("帧生成：开");
		}
		else if (Graphics && !Graphics->IsFrameGenSupported())
		{
			FgLabel = TEXT("帧生成：需要 RTX 40/50");
		}
		SetButtonLabel(FrameGenButton, FText::FromString(FgLabel), bFgOn, 20.f);
		FrameGenButton->SetIsEnabled(bFgOk || bFgOn);
	}

	if (PixelStreamingButton)
	{
		const bool bPsOn = Graphics && Graphics->IsPixelStreamingEnabled();
		const bool bHasUrl = Graphics && !Graphics->GetPixelStreamingUrl().IsEmpty();
		FString PsLabel = TEXT("像素流送：关");
		if (bPsOn)
		{
			PsLabel = TEXT("像素流送：开");
		}
		else if (!bHasUrl)
		{
			PsLabel = TEXT("像素流送：未配置地址");
		}
		SetButtonLabel(PixelStreamingButton, FText::FromString(PsLabel), bPsOn, 20.f);
	}
}

void UGraphicsSettingsWidget::ApplyQuality(int32 Level)
{
	if (USlimeGraphicsSettings* Graphics = GetGraphicsSettings())
	{
		Graphics->SetQualityLevel(Level);
	}
	RefreshSelection();
}

void UGraphicsSettingsWidget::OnQuality0Clicked() { ApplyQuality(0); }
void UGraphicsSettingsWidget::OnQuality1Clicked() { ApplyQuality(1); }
void UGraphicsSettingsWidget::OnQuality2Clicked() { ApplyQuality(2); }
void UGraphicsSettingsWidget::OnQuality3Clicked() { ApplyQuality(3); }

void UGraphicsSettingsWidget::OnUpscalerClicked()
{
	if (USlimeGraphicsSettings* Graphics = GetGraphicsSettings())
	{
		Graphics->CycleUpscaler();
	}
	RefreshSelection();
}

void UGraphicsSettingsWidget::OnDLSSQualityClicked()
{
	if (USlimeGraphicsSettings* Graphics = GetGraphicsSettings())
	{
		Graphics->CycleDLSSQuality();
	}
	RefreshSelection();
}

void UGraphicsSettingsWidget::OnFrameGenClicked()
{
	if (USlimeGraphicsSettings* Graphics = GetGraphicsSettings())
	{
		Graphics->ToggleFrameGen();
	}
	RefreshSelection();
}

void UGraphicsSettingsWidget::OnAutoDetectClicked()
{
	if (USlimeGraphicsSettings* Graphics = GetGraphicsSettings())
	{
		Graphics->AutoDetectQuality();
	}
	RefreshSelection();
}

void UGraphicsSettingsWidget::OnPixelStreamingClicked()
{
	if (USlimeGraphicsSettings* Graphics = GetGraphicsSettings())
	{
		Graphics->TogglePixelStreaming();
	}
	RefreshSelection();
}

void UGraphicsSettingsWidget::OnBackClicked()
{
	RemoveFromParent();
	if (UUserWidget* Target = ReturnTarget.Get())
	{
		Target->SetVisibility(ESlateVisibility::Visible);
	}
}
