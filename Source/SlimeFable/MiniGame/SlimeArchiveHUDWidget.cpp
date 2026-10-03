#include "MiniGame/SlimeArchiveHUDWidget.h"
#include "UI/MenuUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> USlimeArchiveHUDWidget::RebuildWidget()
{
	if (!StatusText)
	{
		SetVisibility(ESlateVisibility::HitTestInvisible);
		auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Root;
		auto AddPanel = [&](float X, float Y, float Width, float Size, const FLinearColor& Color, float AnchorX = .5f, bool Bottom = false)
		{
			auto* Panel = WidgetTree->ConstructWidget<UBorder>();
			Panel->SetBrushColor(FLinearColor(.05f, .045f, .035f, .65f));
			Panel->SetPadding(FMargin(10.f, 6.f));
			auto* Slot = Root->AddChildToCanvas(Panel);
			Slot->SetAnchors(FAnchors(AnchorX, Bottom ? 1.f : 0.f));
			Slot->SetAlignment(FVector2D(AnchorX, Bottom ? 1.f : 0.f));
			Slot->SetPosition(FVector2D(X, Y)); Slot->SetAutoSize(true);
			auto* Box = WidgetTree->ConstructWidget<USizeBox>();
			Box->SetWidthOverride(Width); Panel->SetContent(Box);
			auto* Text = WidgetTree->ConstructWidget<UTextBlock>();
			FMenuUIStyle::ApplyMixedMenuFont(Text, Size, Color);
			Text->SetAutoWrapText(true); Text->SetWrapTextAt(Width);
			Box->SetContent(Text); return Text;
		};
		AddPanel(0.f, 16.f, 430.f, 14.f, FMenuUIStyle::WarmTitleColor())->SetText(FText::FromString(
			TEXT("1958 · NASA 接收日\n吞下档案与设备，送入同编号柜子，完成接收班次。")));
		TimerText = AddPanel(24.f, 208.f, 145.f, 19.f, FMenuUIStyle::TodayEdgeColor(), 0.f);
		StatusText = AddPanel(-24.f, 72.f, 260.f, 13.f, FMenuUIStyle::WarmTextColor(), 1.f);
		MessageText = AddPanel(0.f, 78.f, 430.f, 12.f, FMenuUIStyle::WarmMutedTextColor());
		AddPanel(0.f, -108.f, 370.f, 12.f, FMenuUIStyle::WarmMutedTextColor(), .5f, true)->SetText(FText::FromString(
			TEXT("F 吞下 · E 投递 · R 重开 · Enter 开始 / 确认")));

	}
	SetDisplay(CachedStatus, CachedCargo, CachedMessage);
	return Super::RebuildWidget();
}

void USlimeArchiveHUDWidget::SetDisplay(const FString& Status, const FString& Cargo, const FString& Message)
{
	CachedStatus = Status; CachedCargo = Cargo; CachedMessage = Message;
	TArray<FString> Lines; Status.ParseIntoArrayLines(Lines);
	if (TimerText && Lines.Num())
	{
		TArray<FString> Parts; Lines[0].ParseIntoArray(Parts, TEXT("·"), true);
		TimerText->SetText(FText::FromString(Parts.Num() > 1 ? FString(TEXT("剩余时间\n")) + Parts[1].TrimStartAndEnd() : Lines[0]));
		if (Parts.Num() > 2) Lines[0] = Parts[0].TrimStartAndEnd() + TEXT("  ·  ") + Parts[2].TrimStartAndEnd();
	}
	if (StatusText) StatusText->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
	if (CargoText) CargoText->SetText(FText::FromString(Cargo));
	if (MessageText) MessageText->SetText(FText::FromString(Message));
}
