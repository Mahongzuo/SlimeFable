// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hub/SlimeHubStatusWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "UI/MenuUIStyle.h"

TSharedRef<SWidget> USlimeHubStatusWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
		Panel->SetBrushColor(FLinearColor(0.16f, 0.11f, 0.07f, 0.78f));
		Panel->SetPadding(FMargin(14.f, 8.f));
		if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
		{
			PanelSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
			PanelSlot->SetAlignment(FVector2D(1.f, 0.f));
			PanelSlot->SetAutoSize(true);
			PanelSlot->SetPosition(FVector2D(-28.f, 24.f));
		}

		StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
		StatusText->SetText(FText::FromString(TEXT("时光博物馆")));
		FMenuUIStyle::ApplyBrushCJKFont(StatusText, 22.f, FMenuUIStyle::WarmTitleColor());
		Panel->AddChild(StatusText);
	}
	return Super::RebuildWidget();
}

void USlimeHubStatusWidget::SetStatusText(const FText& InText)
{
	if (StatusText)
	{
		StatusText->SetText(InText);
	}
}
