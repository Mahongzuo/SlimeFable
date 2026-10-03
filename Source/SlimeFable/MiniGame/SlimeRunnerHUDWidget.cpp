#include "MiniGame/SlimeRunnerHUDWidget.h"
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
#include "Styling/SlateTypes.h"

USlimeRunnerHUDWidget::USlimeRunnerHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

TSharedRef<SWidget> USlimeRunnerHUDWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void USlimeRunnerHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Apply();
}

void USlimeRunnerHUDWidget::BuildLayoutIfNeeded()
{
	if (FlagText && RouteBar) return;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RunnerHudRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RunnerPanel"));
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

	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RunnerWidth"));
	Width->SetWidthOverride(320.f);
	Panel->SetContent(Width);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RunnerColumn"));
	Width->SetContent(Column);

	FlagText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FlagText"));
	FMenuUIStyle::ApplyMixedMenuFont(FlagText, 24.f, FMenuUIStyle::WarmTitleColor());
	Column->AddChildToVerticalBox(FlagText);

	RouteBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("RouteBar"));
	{
		FProgressBarStyle Style = RouteBar->GetWidgetStyle();
		FSlateBrush Back;
		Back.DrawAs = ESlateBrushDrawType::RoundedBox;
		Back.TintColor = FSlateColor(FLinearColor(0.16f, 0.13f, 0.09f, 0.9f));
		Back.OutlineSettings.CornerRadii = FVector4(4.f, 4.f, 4.f, 4.f);
		Back.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		FSlateBrush Fill = Back;
		Fill.TintColor = FSlateColor(FLinearColor(0.72f, 0.16f, 0.1f, 1.f));
		Style.SetBackgroundImage(Back);
		Style.SetFillImage(Fill);
		RouteBar->SetWidgetStyle(Style);
	}
	if (UVerticalBoxSlot* BarSlot = Column->AddChildToVerticalBox(RouteBar))
	{
		BarSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 4.f));
	}
	UHorizontalBox* Ends = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RunnerEnds"));
	Column->AddChildToVerticalBox(Ends);
	StartText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StartText"));
	FMenuUIStyle::ApplyBrushCJKFont(StartText, 14.f, FMenuUIStyle::WarmMutedTextColor());
	if (UHorizontalBoxSlot* StartSlot = Ends->AddChildToHorizontalBox(StartText))
	{
		StartSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	EndText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EndText"));
	FMenuUIStyle::ApplyBrushCJKFont(EndText, 14.f, FMenuUIStyle::WarmMutedTextColor());
	Ends->AddChildToHorizontalBox(EndText);

	TimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TimerText"));
	FMenuUIStyle::ApplyMixedMenuFont(TimerText, 18.f, FMenuUIStyle::WarmTextColor());
	Column->AddChildToVerticalBox(TimerText);
}

void USlimeRunnerHUDWidget::SetLabels(const FText& Start, const FText& End)
{
	StartLabel = Start;
	EndLabel = End;
	Apply();
}

void USlimeRunnerHUDWidget::SetStats(int32 Flags, int32 Total, float Progress, float TimeLeft)
{
	CachedFlags = Flags;
	CachedTotal = Total;
	CachedProgress = Progress;
	CachedTimeLeft = TimeLeft;
	Apply();
}

void USlimeRunnerHUDWidget::Apply()
{
	if (!FlagText || !RouteBar) return;
	FlagText->SetText(FText::FromString(FString::Printf(TEXT("红旗 %d / %d"), CachedFlags, CachedTotal)));
	RouteBar->SetPercent(CachedProgress);
	StartText->SetText(StartLabel);
	EndText->SetText(EndLabel);
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
}
