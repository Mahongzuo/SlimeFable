#include "MiniGame/SlimeTrackHUDWidget.h"
#include "UI/MenuUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	const TCHAR* StationNames[] = {
		TEXT("苹果园"), TEXT("五棵松"), TEXT("军事博物馆"),
		TEXT("复兴门"), TEXT("前门"), TEXT("北京站"),
	};
}

USlimeTrackHUDWidget::USlimeTrackHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

TSharedRef<SWidget> USlimeTrackHUDWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void USlimeTrackHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Apply();
}

void USlimeTrackHUDWidget::BuildLayoutIfNeeded()
{
	if (RouteText && RouteBar) return;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TrackHudRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TrackPanel"));
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor(0.05f, 0.045f, 0.035f, 0.72f));
		Brush.OutlineSettings.CornerRadii = FVector4(10.f, 10.f, 10.f, 10.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Color = FSlateColor(FLinearColor(0.72f, 0.64f, 0.46f, 0.4f));
		Brush.OutlineSettings.Width = 1.4f;
		Panel->SetBrush(Brush);
		Panel->SetPadding(FMargin(16.f, 10.f, 16.f, 12.f));
	}
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(1.f, 0.f));
		PanelSlot->SetAlignment(FVector2D(1.f, 0.f));
		PanelSlot->SetPosition(FVector2D(-28.f, 28.f));
		PanelSlot->SetAutoSize(true);
	}

	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TrackWidth"));
	Width->SetWidthOverride(460.f);
	Panel->SetContent(Width);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TrackColumn"));
	Width->SetContent(Column);

	RouteText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RouteText"));
	FMenuUIStyle::ApplyBrushCJKFont(RouteText, 22.f, FMenuUIStyle::WarmTitleColor());
	Column->AddChildToVerticalBox(RouteText);

	StepText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StepText"));
	FMenuUIStyle::ApplyMixedMenuFont(StepText, 18.f, FMenuUIStyle::WarmTextColor());
	if (UVerticalBoxSlot* StepSlot = Column->AddChildToVerticalBox(StepText))
	{
		StepSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}

	RouteBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("RouteBar"));
	{
		FProgressBarStyle Style = RouteBar->GetWidgetStyle();
		FSlateBrush Back;
		Back.DrawAs = ESlateBrushDrawType::RoundedBox;
		Back.TintColor = FSlateColor(FLinearColor(0.16f, 0.13f, 0.09f, 0.9f));
		Back.OutlineSettings.CornerRadii = FVector4(4.f, 4.f, 4.f, 4.f);
		Back.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		FSlateBrush Fill = Back;
		Fill.TintColor = FSlateColor(FLinearColor(0.55f, 0.12f, 0.1f, 1.f));
		Style.SetBackgroundImage(Back);
		Style.SetFillImage(Fill);
		RouteBar->SetWidgetStyle(Style);
	}
	if (UVerticalBoxSlot* BarSlot = Column->AddChildToVerticalBox(RouteBar))
	{
		BarSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 4.f));
	}

	UHorizontalBox* Stations = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Stations"));
	Column->AddChildToVerticalBox(Stations);
	StationTexts.Reset();
	for (int32 Index = 0; Index < 6; ++Index)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("Station%d"), Index));
		FMenuUIStyle::ApplyBrushCJKFont(Label, 11.f, FMenuUIStyle::WarmMutedTextColor());
		Label->SetText(FText::FromString(StationNames[Index]));
		if (UHorizontalBoxSlot* StationSlot = Stations->AddChildToHorizontalBox(Label))
		{
			StationSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			StationSlot->SetHorizontalAlignment(HAlign_Center);
		}
		StationTexts.Add(Label);
	}

	TimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TimerText"));
	FMenuUIStyle::ApplyMixedMenuFont(TimerText, 18.f, FMenuUIStyle::WarmTextColor());
	Column->AddChildToVerticalBox(TimerText);

	UBorder* KeyPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("KeyPanel"));
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor(0.05f, 0.045f, 0.035f, 0.66f));
		Brush.OutlineSettings.CornerRadii = FVector4(8.f, 8.f, 8.f, 8.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Color = FSlateColor(FLinearColor(0.72f, 0.64f, 0.46f, 0.28f));
		Brush.OutlineSettings.Width = 1.f;
		KeyPanel->SetBrush(Brush);
		KeyPanel->SetPadding(FMargin(16.f, 8.f));
	}
	if (UVerticalBoxSlot* KeySlot = Column->AddChildToVerticalBox(KeyPanel))
	{
		KeySlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}
	KeyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("KeyText"));
	FMenuUIStyle::ApplyBrushCJKFont(KeyText, 14.f, FMenuUIStyle::WarmTextColor());
	KeyText->SetAutoWrapText(true);
	KeyText->SetWrapTextAt(428.f);
	KeyPanel->SetContent(KeyText);
}

void USlimeTrackHUDWidget::SetStats(const FText& Route, int32 Steps, int32 Limit, int32 StationsLit, float TimeLeft, const FText& Keys)
{
	CachedRoute = Route;
	CachedSteps = Steps;
	CachedLimit = Limit;
	CachedLit = StationsLit;
	CachedTimeLeft = TimeLeft;
	CachedKeys = Keys;
	Apply();
}

void USlimeTrackHUDWidget::Apply()
{
	if (!RouteText || !RouteBar) return;
	RouteText->SetText(CachedRoute);
	StepText->SetText(FText::FromString(FString::Printf(TEXT("步数 %d / %d"), CachedSteps, CachedLimit)));
	RouteBar->SetPercent(FMath::Clamp((CachedLit - 1) / 5.f, 0.f, 1.f));
	for (int32 Index = 0; Index < StationTexts.Num(); ++Index)
	{
		if (!StationTexts[Index]) continue;
		const bool bLit = Index < CachedLit;
		StationTexts[Index]->SetColorAndOpacity(bLit ? FMenuUIStyle::WarmTitleColor() : FMenuUIStyle::WarmMutedTextColor());
	}
	if (CachedTimeLeft >= 0.f)
	{
		TimerText->SetVisibility(ESlateVisibility::HitTestInvisible);
		const int32 Seconds = FMath::CeilToInt(CachedTimeLeft);
		TimerText->SetText(FText::FromString(FString::Printf(TEXT("剩余 %d:%02d"), Seconds / 60, Seconds % 60)));
	}
	else
	{
		TimerText->SetVisibility(ESlateVisibility::Collapsed);
	}
	KeyText->SetText(CachedKeys);
}
