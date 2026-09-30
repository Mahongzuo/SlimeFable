// Copyright Epic Games, Inc. All Rights Reserved.

#include "Farm/SlimePlotStatusWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/MenuUIStyle.h"

TSharedRef<SWidget> USlimePlotStatusWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatusColumn"));
		StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
		StatusText->SetJustification(ETextJustify::Center);
		StatusText->SetText(FText::FromString(TEXT("空地")));
		FMenuUIStyle::ApplyBrushCJKFont(StatusText, 16.f, FMenuUIStyle::WarmTextColor());
		if (UVerticalBoxSlot* TextSlot = Column->AddChildToVerticalBox(StatusText))
		{
			TextSlot->SetHorizontalAlignment(HAlign_Center);
			TextSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
		}

		BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarBox"));
		BarBox->SetHeightOverride(10.f);
		BarBox->SetWidthOverride(160.f);
		Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("GrowthBar"));
		Bar->SetPercent(0.f);
		Bar->SetFillColorAndOpacity(FLinearColor(0.22f, 0.72f, 0.28f, 1.f));
		BarBox->AddChild(Bar);
		if (UVerticalBoxSlot* BarSlot = Column->AddChildToVerticalBox(BarBox))
		{
			BarSlot->SetHorizontalAlignment(HAlign_Center);
		}
		WidgetTree->RootWidget = Column;
	}
	return Super::RebuildWidget();
}

void USlimePlotStatusWidget::SetStatus(const FText& InText, float Progress, float BarWidth, bool bShowBar)
{
	if (StatusText && !StatusText->GetText().EqualTo(InText))
	{
		StatusText->SetText(InText);
	}
	if (BarBox)
	{
		BarBox->SetVisibility(bShowBar ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bShowBar)
		{
			BarBox->SetWidthOverride(FMath::Clamp(BarWidth, 96.f, 280.f));
		}
	}
	if (Bar && bShowBar)
	{
		Bar->SetPercent(FMath::Clamp(Progress, 0.f, 1.f));
	}
}
