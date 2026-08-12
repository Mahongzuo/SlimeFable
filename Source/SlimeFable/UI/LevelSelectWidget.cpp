// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/LevelSelectWidget.h"
#include "UI/MainMenuWidget.h"
#include "UI/DaySlotWidget.h"
#include "UI/MenuUIStyle.h"
#include "DayLevel/DayLevelSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "Misc/DateTime.h"

namespace LevelSelectPrivate
{
	static int32 MondayBasedColumnForMonth(int32 Year, int32 Month)
	{
		const FDateTime FirstOfMonth(Year, Month, 1);
		return static_cast<int32>(FirstOfMonth.GetDayOfWeek());
	}

	static void StylePanelBorder(UBorder* Border, const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth, float CornerRadius, const FMargin& Padding)
	{
		if (!Border)
		{
			return;
		}

		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings.CornerRadii = FVector4(CornerRadius, CornerRadius, CornerRadius, CornerRadius);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Color = FSlateColor(Outline);
		Brush.OutlineSettings.Width = OutlineWidth;
		Brush.ImageSize = FVector2D(64.f, 64.f);
		Border->SetBrush(Brush);
		Border->SetPadding(Padding);
	}
}

ULevelSelectWidget::ULevelSelectWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DaySlotClassPath = TSoftClassPtr<UDaySlotWidget>(
		FSoftObjectPath(TEXT("/Game/UI/WBP_DaySlot.WBP_DaySlot_C")));
}

TSharedRef<SWidget> ULevelSelectWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void ULevelSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveDaySlotClass();

	if (CurrentMonth < 1 || CurrentMonth > 12)
	{
		CurrentMonth = FDateTime::Now().GetMonth();
	}

	ApplyMaterialLabLook();

	if (PrevMonthButton)
	{
		PrevMonthButton->OnClicked.AddUniqueDynamic(this, &ULevelSelectWidget::OnPrevMonthClicked);
	}
	if (NextMonthButton)
	{
		NextMonthButton->OnClicked.AddUniqueDynamic(this, &ULevelSelectWidget::OnNextMonthClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked.AddUniqueDynamic(this, &ULevelSelectWidget::OnBackClicked);
	}

	RefreshForCurrentMonth();
}

void ULevelSelectWidget::SetParentMenu(UMainMenuWidget* InParent)
{
	ParentMenu = InParent;
}

void ULevelSelectWidget::ResolveDaySlotClass()
{
	if (!DaySlotClass && !DaySlotClassPath.IsNull())
	{
		DaySlotClass = DaySlotClassPath.LoadSynchronous();
	}
	if (!DaySlotClass)
	{
		DaySlotClass = UDaySlotWidget::StaticClass();
	}
}

void ULevelSelectWidget::BuildLayoutIfNeeded()
{
	if (MonthText && PrevMonthButton && NextMonthButton && BackButton && DayGrid && DayPanel)
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

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SelectBox"));
	if (UCanvasPanelSlot* VBoxSlot = Root->AddChildToCanvas(VBox))
	{
		VBoxSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		VBoxSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		VBoxSlot->SetAutoSize(true);
	}

	auto MakeLabeledButton = [this](const FName& Name, const FText& Label, float Width, USizeBox*& OutSizeBox) -> UButton*
	{
		OutSizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			*FString::Printf(TEXT("%s_Size"), *Name.ToString()));
		OutSizeBox->SetWidthOverride(Width);
		OutSizeBox->SetHeightOverride(48.f);

		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		OutSizeBox->AddChild(Button);
		UTextBlock* LabelBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			*FString::Printf(TEXT("%s_Label"), *Name.ToString()));
		LabelBlock->SetText(Label);
		LabelBlock->SetJustification(ETextJustify::Center);
		Button->AddChild(LabelBlock);
		return Button;
	};

	USizeBox* PrevSize = nullptr;
	USizeBox* NextSize = nullptr;
	USizeBox* BackSize = nullptr;
	// Compact nav buttons so they sit next to the month label.
	PrevMonthButton = MakeLabeledButton(TEXT("PrevMonthButton"), FText::FromString(TEXT("<")), 48.f, PrevSize);
	NextMonthButton = MakeLabeledButton(TEXT("NextMonthButton"), FText::FromString(TEXT(">")), 48.f, NextSize);
	MonthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MonthText"));
	MonthText->SetJustification(ETextJustify::Center);

	UHorizontalBox* MonthRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MonthRow"));
	UVerticalBoxSlot* MonthRowSlot = VBox->AddChildToVerticalBox(MonthRow);
	MonthRowSlot->SetHorizontalAlignment(HAlign_Center);
	MonthRowSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 10.f));

	MonthRow->AddChildToHorizontalBox(PrevSize)->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
	UHorizontalBoxSlot* MonthSlot = MonthRow->AddChildToHorizontalBox(MonthText);
	MonthSlot->SetPadding(FMargin(4.f, 0.f));
	MonthSlot->SetVerticalAlignment(VAlign_Center);
	MonthRow->AddChildToHorizontalBox(NextSize)->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));

	// Only the day grid keeps a bordered translucent panel.
	DayPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DayPanel"));
	UVerticalBoxSlot* DayPanelSlot = VBox->AddChildToVerticalBox(DayPanel);
	DayPanelSlot->SetHorizontalAlignment(HAlign_Center);
	DayPanelSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 0.f));

	DayGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("DayGrid"));
	DayGrid->SetSlotPadding(FMargin(6.f));
	DayPanel->SetContent(DayGrid);

	BackButton = MakeLabeledButton(TEXT("BackButton"), FText::FromString(TEXT("返回")), 220.f, BackSize);
	UVerticalBoxSlot* BackSlot = VBox->AddChildToVerticalBox(BackSize);
	BackSlot->SetPadding(FMargin(0.f, 18.f));
	BackSlot->SetHorizontalAlignment(HAlign_Center);

	CalendarFrame = nullptr;
}

void ULevelSelectWidget::ApplyCalendarChrome()
{
	if (CalendarFrame)
	{
		CalendarFrame->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Day panel only: soft dark fill + muted gold edge.
	LevelSelectPrivate::StylePanelBorder(
		DayPanel,
		FLinearColor(0.05f, 0.045f, 0.035f, 0.62f),
		FLinearColor(0.72f, 0.64f, 0.46f, 0.35f),
		1.25f,
		14.f,
		FMargin(18.f, 16.f));
}

void ULevelSelectWidget::ApplyMaterialLabLook()
{
	FMenuUIStyle::ApplyMenuBackground(BackgroundImage);

	if (DimOverlay)
	{
		FSlateBrush DimBrush;
		DimBrush.DrawAs = ESlateBrushDrawType::Box;
		DimBrush.TintColor = FSlateColor(FLinearColor(0.05f, 0.04f, 0.03f, 0.48f));
		DimBrush.Margin = FMargin(0.f);
		DimBrush.ImageSize = FVector2D(32.f, 32.f);
		DimOverlay->SetBrush(DimBrush);
		DimOverlay->SetColorAndOpacity(FLinearColor::White);
	}

	ApplyCalendarChrome();

	FMenuUIStyle::ApplyMixedMenuFont(MonthText, 36.f, FMenuUIStyle::WarmTitleColor());

	UMaterialInterface* BrushBtn = FMenuUIStyle::LoadButtonMaterial();
	FMenuUIStyle::ApplyMaterialButtonStyle(PrevMonthButton, BrushBtn, FVector2D(48.f, 48.f));
	FMenuUIStyle::ApplyMaterialButtonStyle(NextMonthButton, BrushBtn, FVector2D(48.f, 48.f));
	FMenuUIStyle::ApplyMaterialButtonStyle(BackButton, BrushBtn, FVector2D(220.f, 52.f));

	auto StyleChildLabel = [](UButton* Button, float Size, bool bMarkerDigits)
	{
		if (Button)
		{
			if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
			{
				if (bMarkerDigits)
				{
					FMenuUIStyle::ApplyMarkerFont(Label, Size, FMenuUIStyle::WarmTextColor());
				}
				else
				{
					FMenuUIStyle::ApplyBrushCJKFont(Label, Size, FMenuUIStyle::WarmTextColor());
				}
			}
		}
	};
	StyleChildLabel(PrevMonthButton, 22.f, true);
	StyleChildLabel(NextMonthButton, 22.f, true);
	StyleChildLabel(BackButton, 22.f, false);
}

void ULevelSelectWidget::RefreshForCurrentMonth()
{
	CurrentMonth = FMath::Clamp(CurrentMonth, 1, 12);
	if (MonthText)
	{
		MonthText->SetText(FText::FromString(FString::Printf(TEXT("%d 月"), CurrentMonth)));
	}
	RebuildDayButtons();
}

void ULevelSelectWidget::RebuildDayButtons()
{
	if (!DayGrid)
	{
		return;
	}

	DayGrid->ClearChildren();
	DaySlots.Reset();
	ResolveDaySlotClass();

	UDayLevelSubsystem* DayLevels = GetDayLevelSubsystem();
	if (!DayLevels)
	{
		return;
	}

	constexpr int32 Columns = 7;

	TArray<FDayLevelEntry> Entries;
	DayLevels->GetEntriesForMonth(CurrentMonth, Entries);
	const FName TodayId = DayLevels->GetTodayDayId().Id;

	const int32 Year = FDateTime::Now().GetYear();
	const int32 StartCol = LevelSelectPrivate::MondayBasedColumnForMonth(Year, CurrentMonth);

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FDayLevelEntry& Entry = Entries[Index];
		UDaySlotWidget* DaySlot = CreateWidget<UDaySlotWidget>(this, DaySlotClass);
		if (!DaySlot)
		{
			continue;
		}

		DaySlot->SetDay(Entry, Entry.DayId == TodayId);
		DaySlot->OnDayClicked.AddUniqueDynamic(this, &ULevelSelectWidget::HandleDaySlotClicked);

		const int32 CellIndex = StartCol + (Entry.Day - 1);
		const int32 Row = CellIndex / Columns;
		const int32 Col = CellIndex % Columns;
		if (UUniformGridSlot* GridSlot = DayGrid->AddChildToUniformGrid(DaySlot, Row, Col))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}
		DaySlots.Add(DaySlot);
	}
}

UDayLevelSubsystem* ULevelSelectWidget::GetDayLevelSubsystem() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<UDayLevelSubsystem>();
	}
	return nullptr;
}

void ULevelSelectWidget::OnPrevMonthClicked()
{
	CurrentMonth = CurrentMonth <= 1 ? 12 : CurrentMonth - 1;
	RefreshForCurrentMonth();
}

void ULevelSelectWidget::OnNextMonthClicked()
{
	CurrentMonth = CurrentMonth >= 12 ? 1 : CurrentMonth + 1;
	RefreshForCurrentMonth();
}

void ULevelSelectWidget::OnBackClicked()
{
	SetVisibility(ESlateVisibility::Collapsed);
	if (ParentMenu)
	{
		ParentMenu->SetVisibility(ESlateVisibility::Visible);
	}
}

void ULevelSelectWidget::HandleDaySlotClicked(FName DayId)
{
	if (UDayLevelSubsystem* DayLevels = GetDayLevelSubsystem())
	{
		DayLevels->TravelToDayId(this, DayId);
	}
}
