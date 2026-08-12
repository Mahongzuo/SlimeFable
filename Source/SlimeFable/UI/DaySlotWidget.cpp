// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/DaySlotWidget.h"
#include "UI/MenuUIStyle.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"

TSharedRef<SWidget> UDaySlotWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	ApplyVisuals();
	return Super::RebuildWidget();
}

void UDaySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (SlotButton)
	{
		SlotButton->OnClicked.AddUniqueDynamic(this, &UDaySlotWidget::HandleClicked);
	}
	ApplyVisuals();
}

void UDaySlotWidget::BuildLayoutIfNeeded()
{
	if (SlotButton && DayLabel)
	{
		bBuiltInCode = false;
		return;
	}

	bBuiltInCode = true;
	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RootSize"));
	SizeBox->SetWidthOverride(SlotSize);
	SizeBox->SetHeightOverride(SlotSize);
	WidgetTree->RootWidget = SizeBox;

	SlotButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SlotButton"));
	SizeBox->AddChild(SlotButton);

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay"));
	SlotButton->AddChild(Overlay);

	SlotBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotBorder"));
	if (UOverlaySlot* BorderSlot = Overlay->AddChildToOverlay(SlotBorder))
	{
		BorderSlot->SetHorizontalAlignment(HAlign_Fill);
		BorderSlot->SetVerticalAlignment(VAlign_Fill);
	}

	SlotBackground = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SlotBackground"));
	if (UOverlaySlot* BgSlot = Overlay->AddChildToOverlay(SlotBackground))
	{
		BgSlot->SetHorizontalAlignment(HAlign_Fill);
		BgSlot->SetVerticalAlignment(VAlign_Fill);
		BgSlot->SetPadding(FMargin(3.f));
	}

	DayLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DayLabel"));
	DayLabel->SetJustification(ETextJustify::Center);
	if (UOverlaySlot* LabelSlot = Overlay->AddChildToOverlay(DayLabel))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UDaySlotWidget::SetDay(const FDayLevelEntry& Entry, bool bIsToday)
{
	BoundDayId = Entry.DayId;
	DayNumber = Entry.Day;
	bIsTodayCached = bIsToday;
	ApplyVisuals();
}

void UDaySlotWidget::ApplyVisuals()
{
	if (!DayLabel && !SlotButton)
	{
		return;
	}

	const FLinearColor CellFill = bIsTodayCached
		? FLinearColor(0.42f, 0.32f, 0.18f, 0.88f)
		: FLinearColor(0.08f, 0.07f, 0.06f, 0.72f);
	const FLinearColor CellBorder = bIsTodayCached
		? FLinearColor(0.92f, 0.72f, 0.32f, 0.95f)
		: FLinearColor(0.55f, 0.48f, 0.38f, 0.55f);
	const FLinearColor TextColor = bIsTodayCached
		? FLinearColor(1.f, 0.9f, 0.55f, 1.f)
		: FMenuUIStyle::WarmTextColor();

	if (DayLabel && DayNumber > 0)
	{
		DayLabel->SetText(FText::AsNumber(DayNumber));
		FMenuUIStyle::ApplyMarkerFont(DayLabel, bIsTodayCached ? 26.f : 22.f, TextColor);
	}

	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(CellBorder);
		SlotBorder->SetPadding(FMargin(2.f));
	}

	if (SlotBackground)
	{
		FSlateBrush FillBrush;
		FillBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		FillBrush.TintColor = FSlateColor(CellFill);
		FillBrush.OutlineSettings.CornerRadii = FVector4(10.f, 10.f, 10.f, 10.f);
		FillBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		FillBrush.OutlineSettings.Color = FSlateColor(CellBorder);
		FillBrush.OutlineSettings.Width = bIsTodayCached ? 2.5f : 1.25f;
		FillBrush.ImageSize = FVector2D(SlotSize, SlotSize);
		SlotBackground->SetBrush(FillBrush);
	}

	if (SlotButton)
	{
		FMenuUIStyle::ApplyFlatButtonStyle(
			SlotButton,
			FLinearColor(0.f, 0.f, 0.f, 0.f),
			FVector2D(SlotSize, SlotSize),
			FMargin(0.f));
	}
}

void UDaySlotWidget::HandleClicked()
{
	if (!BoundDayId.IsNone())
	{
		OnDayClicked.Broadcast(BoundDayId);
	}
}
