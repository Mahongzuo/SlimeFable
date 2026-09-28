// Copyright Epic Games, Inc. All Rights Reserved.

#include "Farm/SlimePlotStatusWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "UI/MenuUIStyle.h"

TSharedRef<SWidget> USlimePlotStatusWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
		StatusText->SetJustification(ETextJustify::Center);
		StatusText->SetText(FText::FromString(TEXT("空地")));
		FMenuUIStyle::ApplyBrushCJKFont(StatusText, 16.f, FMenuUIStyle::WarmTextColor());
		WidgetTree->RootWidget = StatusText;
	}
	return Super::RebuildWidget();
}

void USlimePlotStatusWidget::SetStatusText(const FText& InText)
{
	if (StatusText && !StatusText->GetText().EqualTo(InText))
	{
		StatusText->SetText(InText);
	}
}
