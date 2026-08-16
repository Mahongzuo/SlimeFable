#include "Quest/WeekSelectWidget.h"
#include "Quest/QuestSubsystem.h"
#include "UI/MenuUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

namespace
{
	FSlateBrush MakeWeekPanelBrush()
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor(0.08f, 0.06f, 0.04f, 0.9f));
		Brush.OutlineSettings.CornerRadii = FVector4(12.f, 12.f, 12.f, 12.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Color = FSlateColor(FLinearColor(
			FMenuUIStyle::TodayEdgeColor().R,
			FMenuUIStyle::TodayEdgeColor().G,
			FMenuUIStyle::TodayEdgeColor().B,
			0.45f));
		Brush.OutlineSettings.Width = 1.6f;
		Brush.ImageSize = FVector2D(64.f, 64.f);
		return Brush;
	}
}

UWeekSelectWidget::UWeekSelectWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UWeekSelectWidget::Setup(FName InDayId, FName InChapterId)
{
	DayId = InDayId;
	ChapterId = InChapterId;
	if (TitleText)
	{
		ApplyVisuals();
		RefreshButtons();
	}
}

TSharedRef<SWidget> UWeekSelectWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void UWeekSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Visible);
	if (WeekButton1)
	{
		WeekButton1->OnClicked.AddUniqueDynamic(this, &UWeekSelectWidget::HandleWeek1);
	}
	if (WeekButton2)
	{
		WeekButton2->OnClicked.AddUniqueDynamic(this, &UWeekSelectWidget::HandleWeek2);
	}
	if (WeekButton3)
	{
		WeekButton3->OnClicked.AddUniqueDynamic(this, &UWeekSelectWidget::HandleWeek3);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddUniqueDynamic(this, &UWeekSelectWidget::HandleCancel);
	}
	ApplyVisuals();
	RefreshButtons();
}

void UWeekSelectWidget::BuildLayoutIfNeeded()
{
	if (bBuiltInCode && Panel && WeekButton1 && CancelButton)
	{
		return;
	}
	bBuiltInCode = true;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WeekSelectRoot"));
	WidgetTree->RootWidget = Root;

	DimOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DimOverlay"));
	if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(DimOverlay))
	{
		DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimSlot->SetOffsets(FMargin(0.f));
		DimSlot->SetZOrder(0);
	}

	Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrush(MakeWeekPanelBrush());
	Panel->SetPadding(FMargin(32.f, 24.f));
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
		PanelSlot->SetZOrder(1);
	}

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Col"));
	Panel->SetContent(Col);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* TitleSlot = Col->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
	}

	auto MakeWeekButton = [this, Col](int32 Week, TObjectPtr<UButton>& OutButton, TObjectPtr<UTextBlock>& OutLabel)
	{
		OutButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("WeekButton%d"), Week));
		OutLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("WeekLabel%d"), Week));
		OutLabel->SetJustification(ETextJustify::Center);
		OutButton->AddChild(OutLabel);
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("WeekSize%d"), Week));
		Size->SetWidthOverride(280.f);
		Size->SetHeightOverride(52.f);
		Size->AddChild(OutButton);
		if (UVerticalBoxSlot* Slot = Col->AddChildToVerticalBox(Size))
		{
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
			Slot->SetHorizontalAlignment(HAlign_Center);
		}
	};

	MakeWeekButton(1, WeekButton1, WeekLabel1);
	MakeWeekButton(2, WeekButton2, WeekLabel2);
	MakeWeekButton(3, WeekButton3, WeekLabel3);

	CancelButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CancelButton"));
	CancelLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CancelLabel"));
	CancelLabel->SetText(FText::FromString(TEXT("取消")));
	CancelLabel->SetJustification(ETextJustify::Center);
	CancelButton->AddChild(CancelLabel);
	USizeBox* CancelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CancelSize"));
	CancelSize->SetWidthOverride(280.f);
	CancelSize->SetHeightOverride(48.f);
	CancelSize->AddChild(CancelButton);
	if (UVerticalBoxSlot* CancelSlot = Col->AddChildToVerticalBox(CancelSize))
	{
		CancelSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		CancelSlot->SetHorizontalAlignment(HAlign_Center);
	}
}

void UWeekSelectWidget::ApplyVisuals()
{
	if (DimOverlay)
	{
		FSlateBrush DimBrush;
		DimBrush.DrawAs = ESlateBrushDrawType::Box;
		DimBrush.TintColor = FSlateColor(FLinearColor(0.04f, 0.03f, 0.02f, 0.42f));
		DimBrush.ImageSize = FVector2D(32.f, 32.f);
		DimOverlay->SetBrush(DimBrush);
	}
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(FString::Printf(TEXT("进入 %s"), *ChapterId.ToString())));
		FMenuUIStyle::ApplyMixedMenuFont(TitleText, 28.f, FMenuUIStyle::WarmTitleColor());
	}

	UMaterialInterface* BrushBtn = FMenuUIStyle::LoadButtonMaterial();
	const FVector2D Size(280.f, 52.f);
	auto StyleInk = [BrushBtn, Size](UButton* Button, UTextBlock* Label, const FString& Text, bool bLocked)
	{
		if (!Button || !Label)
		{
			return;
		}
		Label->SetText(FText::FromString(Text));
		if (bLocked)
		{
			FMenuUIStyle::ApplyFlatButtonStyle(Button, FLinearColor(0.12f, 0.09f, 0.06f, 0.55f), Size, FMargin(12.f, 8.f));
			FMenuUIStyle::ApplyBrushCJKFont(Label, 20.f, FMenuUIStyle::WarmMutedTextColor());
			Button->SetIsEnabled(false);
		}
		else
		{
			FMenuUIStyle::ApplyMaterialButtonStyle(Button, BrushBtn, Size);
			FMenuUIStyle::ApplyMixedMenuFont(Label, 20.f, FMenuUIStyle::WarmTextColor());
			FMenuUIStyle::BindInkButtonHover(Button, Label);
			Button->SetIsEnabled(true);
		}
	};

	const UGameInstance* GI = GetGameInstance();
	const UQuestSubsystem* Quests = GI ? GI->GetSubsystem<UQuestSubsystem>() : nullptr;
	for (int32 Week = 1; Week <= 3; ++Week)
	{
		const bool bLocked = !(Quests && Quests->CanSelectWeek(ChapterId, Week));
		const FString Label = bLocked
			? FString::Printf(TEXT("第 %d 周目（未解锁）"), Week)
			: FString::Printf(TEXT("第 %d 周目"), Week);
		StyleInk(GetWeekButton(Week), GetWeekLabel(Week), Label, bLocked);
	}

	if (CancelButton && CancelLabel)
	{
		FMenuUIStyle::ApplyMaterialButtonStyle(CancelButton, BrushBtn, FVector2D(280.f, 48.f));
		FMenuUIStyle::ApplyBrushCJKFont(CancelLabel, 18.f, FMenuUIStyle::WarmTextColor());
		FMenuUIStyle::BindInkButtonHover(CancelButton, CancelLabel);
	}
}

void UWeekSelectWidget::RefreshButtons()
{
	ApplyVisuals();
}

UButton* UWeekSelectWidget::GetWeekButton(int32 Week) const
{
	switch (Week)
	{
	case 1: return WeekButton1;
	case 2: return WeekButton2;
	case 3: return WeekButton3;
	default: return nullptr;
	}
}

UTextBlock* UWeekSelectWidget::GetWeekLabel(int32 Week) const
{
	switch (Week)
	{
	case 1: return WeekLabel1;
	case 2: return WeekLabel2;
	case 3: return WeekLabel3;
	default: return nullptr;
	}
}

void UWeekSelectWidget::HandleWeekClicked(int32 Week)
{
	UGameInstance* GI = GetGameInstance();
	UQuestSubsystem* Quests = GI ? GI->GetSubsystem<UQuestSubsystem>() : nullptr;
	if (!Quests || !Quests->CanSelectWeek(ChapterId, Week))
	{
		return;
	}
	Quests->TravelToChapter(DayId, ChapterId, Week);
}

void UWeekSelectWidget::HandleWeek1()
{
	HandleWeekClicked(1);
}

void UWeekSelectWidget::HandleWeek2()
{
	HandleWeekClicked(2);
}

void UWeekSelectWidget::HandleWeek3()
{
	HandleWeekClicked(3);
}

void UWeekSelectWidget::HandleCancel()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UQuestSubsystem* Quests = GI->GetSubsystem<UQuestSubsystem>())
		{
			Quests->CloseWeekSelect();
		}
	}
}
