// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/AudioSettingsWidget.h"
#include "UI/MenuUIStyle.h"
#include "Settings/SlimeAudioSettings.h"
#include "SlimeFablePlayerController.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"

TSharedRef<SWidget> UAudioSettingsWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void UAudioSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	ApplyLook();

	if (MasterSlider)
	{
		MasterSlider->OnValueChanged.AddUniqueDynamic(this, &UAudioSettingsWidget::OnMasterChanged);
	}
	if (MusicSlider)
	{
		MusicSlider->OnValueChanged.AddUniqueDynamic(this, &UAudioSettingsWidget::OnMusicChanged);
	}
	if (SfxSlider)
	{
		SfxSlider->OnValueChanged.AddUniqueDynamic(this, &UAudioSettingsWidget::OnSfxChanged);
	}
	if (BackButton)
	{
		BackButton->OnClicked.AddUniqueDynamic(this, &UAudioSettingsWidget::OnBackClicked);
	}

	RefreshFromSettings();
}

void UAudioSettingsWidget::SetReturnTarget(UUserWidget* InTarget)
{
	ReturnTarget = InTarget;
}

USlimeAudioSettings* UAudioSettingsWidget::GetAudioSettings() const
{
	if (UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<USlimeAudioSettings>();
	}
	return nullptr;
}

void UAudioSettingsWidget::BuildLayoutIfNeeded()
{
	if (TitleText && MasterSlider && MusicSlider && SfxSlider && BackButton)
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

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelSize"));
	PanelSize->SetWidthOverride(420.f);
	if (UCanvasPanelSlot* SizeSlot = Root->AddChildToCanvas(PanelSize))
	{
		SizeSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		SizeSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		SizeSlot->SetAutoSize(true);
	}

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuBox"));
	PanelSize->AddChild(VBox);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("音乐与音效")));
	if (UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
	}

	auto AddSliderRow = [this, VBox](const FName& LabelName, const FName& SliderName, const FText& Label) -> TPair<UTextBlock*, USlider*>
	{
		UTextBlock* LabelBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), LabelName);
		LabelBlock->SetText(Label);
		if (UVerticalBoxSlot* LabelSlot = VBox->AddChildToVerticalBox(LabelBlock))
		{
			LabelSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 4.f));
			LabelSlot->SetHorizontalAlignment(HAlign_Left);
		}

		USizeBox* SliderBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), *FString::Printf(TEXT("%s_Size"), *SliderName.ToString()));
		SliderBox->SetWidthOverride(380.f);
		SliderBox->SetHeightOverride(28.f);
		if (UVerticalBoxSlot* BoxSlot = VBox->AddChildToVerticalBox(SliderBox))
		{
			BoxSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
			BoxSlot->SetHorizontalAlignment(HAlign_Center);
		}

		USlider* Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), SliderName);
		Slider->SetMinValue(0.f);
		Slider->SetMaxValue(1.f);
		Slider->SetStepSize(0.01f);
		SliderBox->AddChild(Slider);
		return {LabelBlock, Slider};
	};

	{
		const TPair<UTextBlock*, USlider*> Row = AddSliderRow(
			TEXT("MasterLabel"), TEXT("MasterSlider"), FText::FromString(TEXT("主音量 100%")));
		MasterLabel = Row.Key;
		MasterSlider = Row.Value;
	}
	{
		const TPair<UTextBlock*, USlider*> Row = AddSliderRow(
			TEXT("MusicLabel"), TEXT("MusicSlider"), FText::FromString(TEXT("音乐 85%")));
		MusicLabel = Row.Key;
		MusicSlider = Row.Value;
	}
	{
		const TPair<UTextBlock*, USlider*> Row = AddSliderRow(
			TEXT("SfxLabel"), TEXT("SfxSlider"), FText::FromString(TEXT("音效 100%")));
		SfxLabel = Row.Key;
		SfxSlider = Row.Value;
	}

	USizeBox* BackSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BackButton_Size"));
	BackSize->SetWidthOverride(360.f);
	BackSize->SetHeightOverride(52.f);
	if (UVerticalBoxSlot* BackSlot = VBox->AddChildToVerticalBox(BackSize))
	{
		BackSlot->SetPadding(FMargin(0.f, 24.f, 0.f, 0.f));
		BackSlot->SetHorizontalAlignment(HAlign_Center);
	}
	BackButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BackButton"));
	BackSize->AddChild(BackButton);
	UTextBlock* BackLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BackButton_Label"));
	BackLabel->SetText(FText::FromString(TEXT("返回")));
	BackLabel->SetJustification(ETextJustify::Center);
	BackButton->AddChild(BackLabel);
}

void UAudioSettingsWidget::ApplyLook()
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
	FMenuUIStyle::ApplyBrushCJKFont(MasterLabel, 20.f, FMenuUIStyle::WarmTextColor());
	FMenuUIStyle::ApplyBrushCJKFont(MusicLabel, 20.f, FMenuUIStyle::WarmTextColor());
	FMenuUIStyle::ApplyBrushCJKFont(SfxLabel, 20.f, FMenuUIStyle::WarmTextColor());

	UMaterialInterface* BrushBtn = FMenuUIStyle::LoadButtonMaterial();
	FMenuUIStyle::ApplyMaterialButtonStyle(BackButton, BrushBtn, FVector2D(360.f, 52.f));
	if (BackButton && BackButton->GetChildrenCount() > 0)
	{
		if (UTextBlock* Label = Cast<UTextBlock>(BackButton->GetChildAt(0)))
		{
			FMenuUIStyle::ApplyBrushCJKFont(Label, 22.f, FMenuUIStyle::WarmTextColor());
		}
	}

	auto StyleSlider = [](USlider* Slider)
	{
		if (!Slider)
		{
			return;
		}
		Slider->SetSliderBarColor(FLinearColor(0.55f, 0.42f, 0.28f, 0.9f));
		Slider->SetSliderHandleColor(FLinearColor(0.92f, 0.82f, 0.62f, 1.f));
	};
	StyleSlider(MasterSlider);
	StyleSlider(MusicSlider);
	StyleSlider(SfxSlider);
}

void UAudioSettingsWidget::RefreshFromSettings()
{
	if (const USlimeAudioSettings* Settings = GetAudioSettings())
	{
		if (MasterSlider)
		{
			MasterSlider->SetValue(Settings->GetMasterVolume());
		}
		if (MusicSlider)
		{
			MusicSlider->SetValue(Settings->GetMusicVolume());
		}
		if (SfxSlider)
		{
			SfxSlider->SetValue(Settings->GetSfxVolume());
		}
	}
	SyncLabels();
}

void UAudioSettingsWidget::SyncLabels()
{
	auto Pct = [](float V) { return FMath::RoundToInt(V * 100.f); };
	const float Master = MasterSlider ? MasterSlider->GetValue() : 1.f;
	const float Music = MusicSlider ? MusicSlider->GetValue() : 1.f;
	const float Sfx = SfxSlider ? SfxSlider->GetValue() : 1.f;
	if (MasterLabel)
	{
		MasterLabel->SetText(FText::FromString(FString::Printf(TEXT("主音量 %d%%"), Pct(Master))));
	}
	if (MusicLabel)
	{
		MusicLabel->SetText(FText::FromString(FString::Printf(TEXT("音乐 %d%%"), Pct(Music))));
	}
	if (SfxLabel)
	{
		SfxLabel->SetText(FText::FromString(FString::Printf(TEXT("音效 %d%%"), Pct(Sfx))));
	}
}

void UAudioSettingsWidget::OnMasterChanged(float Value)
{
	if (USlimeAudioSettings* Settings = GetAudioSettings())
	{
		Settings->SetMasterVolume(Value);
	}
	SyncLabels();
}

void UAudioSettingsWidget::OnMusicChanged(float Value)
{
	if (USlimeAudioSettings* Settings = GetAudioSettings())
	{
		Settings->SetMusicVolume(Value);
	}
	SyncLabels();
}

void UAudioSettingsWidget::OnSfxChanged(float Value)
{
	if (USlimeAudioSettings* Settings = GetAudioSettings())
	{
		Settings->SetSfxVolume(Value);
	}
	SyncLabels();
}

void UAudioSettingsWidget::OnBackClicked()
{
	SetVisibility(ESlateVisibility::Collapsed);
	if (UUserWidget* Target = ReturnTarget.Get())
	{
		Target->SetVisibility(ESlateVisibility::Visible);
		if (ASlimeFablePlayerController* PC = Cast<ASlimeFablePlayerController>(GetOwningPlayer()))
		{
			PC->RetargetUIFocus(Target);
		}
	}
}
