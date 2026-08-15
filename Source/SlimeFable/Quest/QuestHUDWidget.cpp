#include "Quest/QuestHUDWidget.h"
#include "Quest/QuestSubsystem.h"
#include "UI/MenuUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Materials/MaterialInterface.h"
#include "Styling/SlateTypes.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"

UQuestHUDWidget::UQuestHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

TSharedRef<SWidget> UQuestHUDWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void UQuestHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Refresh();
}

void UQuestHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Refresh();
}

void UQuestHUDWidget::BuildLayoutIfNeeded()
{
	if (bBuiltInCode && TrackerPanel && TitleText && BranchText && BranchProgress && ToastPanel && ToastText && BannerKicker && WaypointMark)
	{
		return;
	}

	bBuiltInCode = true;
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("QuestHudRoot"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;

	TrackerPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TrackerPanel"));
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor(0.05f, 0.045f, 0.035f, 0.72f));
		Brush.OutlineSettings.CornerRadii = FVector4(10.f, 10.f, 10.f, 10.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Color = FSlateColor(FLinearColor(0.72f, 0.64f, 0.46f, 0.4f));
		Brush.OutlineSettings.Width = 1.4f;
		Brush.ImageSize = FVector2D(64.f, 64.f);
		TrackerPanel->SetBrush(Brush);
		TrackerPanel->SetPadding(FMargin(12.f, 10.f, 16.f, 12.f));
	}
	TrackerPanel->SetVisibility(ESlateVisibility::Visible);
	if (UCanvasPanelSlot* TrackerSlot = Root->AddChildToCanvas(TrackerPanel))
	{
		TrackerSlot->SetAnchors(FAnchors(0.f, 0.f));
		TrackerSlot->SetAlignment(FVector2D(0.f, 0.f));
		TrackerSlot->SetPosition(FVector2D(28.f, 28.f));
		TrackerSlot->SetAutoSize(true);
		TrackerSlot->SetZOrder(10);
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TrackerRow"));
	TrackerPanel->SetContent(Row);

	USizeBox* MarkBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MarkBox"));
	MarkBox->SetWidthOverride(4.f);
	MarkBox->SetMinDesiredHeight(52.f);
	UImage* Bookmark = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Bookmark"));
	Bookmark->SetColorAndOpacity(FMenuUIStyle::TodayEdgeColor());
	MarkBox->AddChild(Bookmark);
	if (UHorizontalBoxSlot* BoxSlot = Row->AddChildToHorizontalBox(MarkBox))
	{
		BoxSlot->SetPadding(FMargin(0.f, 2.f, 10.f, 2.f));
		BoxSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TrackerTexts"));
	if (UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Texts))
	{
		TextSlot->SetSize(ESlateSizeRule::Fill);
		TextSlot->SetVerticalAlignment(VAlign_Center);
	}

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	Texts->AddChildToVerticalBox(TitleText);

	BranchText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BranchText"));
	if (UVerticalBoxSlot* BranchSlot = Texts->AddChildToVerticalBox(BranchText))
	{
		BranchSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 6.f));
	}

	BranchProgress = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("BranchProgress"));
	BranchProgress->SetPercent(0.f);
	BranchProgress->SetFillColorAndOpacity(FMenuUIStyle::TodayEdgeColor());
	{
		FProgressBarStyle Style = BranchProgress->GetWidgetStyle();
		FSlateBrush Fill;
		Fill.DrawAs = ESlateBrushDrawType::RoundedBox;
		Fill.TintColor = FSlateColor(FMenuUIStyle::TodayEdgeColor());
		Fill.OutlineSettings.CornerRadii = FVector4(3.f, 3.f, 3.f, 3.f);
		Fill.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Style.SetFillImage(Fill);
		FSlateBrush Empty;
		Empty.DrawAs = ESlateBrushDrawType::RoundedBox;
		Empty.TintColor = FSlateColor(FLinearColor(0.12f, 0.1f, 0.07f, 0.85f));
		Empty.OutlineSettings.CornerRadii = FVector4(3.f, 3.f, 3.f, 3.f);
		Empty.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Style.SetBackgroundImage(Empty);
		BranchProgress->SetWidgetStyle(Style);
	}
	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarBox"));
	BarBox->SetWidthOverride(260.f);
	BarBox->SetHeightOverride(6.f);
	BarBox->AddChild(BranchProgress);
	Texts->AddChildToVerticalBox(BarBox);

	ToastPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ToastPanel"));
	ToastPanel->SetVisibility(ESlateVisibility::Collapsed);
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor(0.10f, 0.08f, 0.05f, 0.82f));
		Brush.OutlineSettings.CornerRadii = FVector4(10.f, 10.f, 10.f, 10.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Color = FSlateColor(FMenuUIStyle::TodayEdgeColor());
		Brush.OutlineSettings.Width = 2.0f;
		ToastPanel->SetBrush(Brush);
	}
	ToastPanel->SetPadding(FMargin(36.f, 16.f, 36.f, 18.f));
	if (UCanvasPanelSlot* ToastSlot = Root->AddChildToCanvas(ToastPanel))
	{
		ToastSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		ToastSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ToastSlot->SetPosition(FVector2D(0.f, -70.f));
		ToastSlot->SetAutoSize(true);
		ToastSlot->SetZOrder(20);
	}

	UVerticalBox* BannerCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BannerCol"));
	ToastPanel->SetContent(BannerCol);

	BannerKicker = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BannerKicker"));
	BannerKicker->SetText(FText::FromString(TEXT("已完成")));
	BannerKicker->SetJustification(ETextJustify::Center);
	BannerCol->AddChildToVerticalBox(BannerKicker);

	ToastText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ToastText"));
	ToastText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* TitleSlot = BannerCol->AddChildToVerticalBox(ToastText))
	{
		TitleSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}

	WaypointMark = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("WaypointMark"));
	WaypointMark->SetText(FText::FromString(TEXT("◆")));
	WaypointMark->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* WaySlot = Root->AddChildToCanvas(WaypointMark))
	{
		WaySlot->SetAnchors(FAnchors(0.f, 0.f));
		WaySlot->SetAlignment(FVector2D(0.5f, 0.5f));
		WaySlot->SetAutoSize(true);
		WaySlot->SetZOrder(5);
	}
}

void UQuestHUDWidget::Refresh()
{
	UGameInstance* GI = GetGameInstance();
	UQuestSubsystem* Quests = GI ? GI->GetSubsystem<UQuestSubsystem>() : nullptr;
	APlayerController* PC = GetOwningPlayer();
	if (!Quests)
	{
		if (TrackerPanel)
		{
			TrackerPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (ToastPanel)
		{
			ToastPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (WaypointMark)
		{
			WaypointMark->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	FText Toast;
	if (Quests->GetActiveToast(Toast) && ToastPanel && ToastText)
	{
		ToastPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (BannerKicker)
		{
			BannerKicker->SetText(FText::FromString(TEXT("已完成")));
			FMenuUIStyle::ApplyBrushCJKFont(BannerKicker, 18.f, FMenuUIStyle::WarmTitleColor());
		}
		ToastText->SetText(Toast);
		FMenuUIStyle::ApplyMixedMenuFont(ToastText, 38.f, FMenuUIStyle::TodayEdgeColor());
	}
	else if (ToastPanel)
	{
		ToastPanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (!Quests->HasTrackedQuest())
	{
		if (TrackerPanel)
		{
			TrackerPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (WaypointMark)
		{
			WaypointMark->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (TrackerPanel)
	{
		TrackerPanel->SetVisibility(ESlateVisibility::Visible);
	}

	const FText Title = FText::FromString(FString::Printf(
		TEXT("%s  %s"),
		*Quests->GetTrackedChapterLabel().ToString(),
		*Quests->GetTrackedMainTitle().ToString()));
	if (TitleText)
	{
		TitleText->SetText(Title);
		FMenuUIStyle::ApplyMixedMenuFont(TitleText, 20.f, FMenuUIStyle::WarmTitleColor());
	}

	const int32 Count = Quests->GetTrackedCount();
	const int32 Required = FMath::Max(1, Quests->GetTrackedRequired());
	if (BranchText)
	{
		BranchText->SetText(FText::FromString(FString::Printf(
			TEXT("%s  %d/%d"),
			*Quests->GetTrackedBranchTitle().ToString(),
			Count,
			Required)));
		FMenuUIStyle::ApplyBrushCJKFont(BranchText, 16.f, FMenuUIStyle::WarmTextColor());
	}
	if (BranchProgress)
	{
		BranchProgress->SetPercent(static_cast<float>(Count) / static_cast<float>(Required));
	}

	UpdateWaypoint(Quests, PC);
}

FReply UQuestHUDWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC || !PC->bShowMouseCursor)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	if (TrackerPanel && TrackerPanel->GetVisibility() == ESlateVisibility::Visible && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FGeometry TrackerGeo = TrackerPanel->GetCachedGeometry();
		if (TrackerGeo.IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
		{
			if (UGameInstance* GI = GetGameInstance())
			{
				if (UQuestSubsystem* Quests = GI->GetSubsystem<UQuestSubsystem>())
				{
					Quests->ToggleQuestLog();
					return FReply::Handled();
				}
			}
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UQuestHUDWidget::UpdateWaypoint(UQuestSubsystem* Quests, APlayerController* PC)
{
	if (!WaypointMark || !Quests || !PC)
	{
		return;
	}

	FVector WorldPos;
	if (!Quests->GetActiveWaypointLocation(WorldPos))
	{
		WaypointMark->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	FVector2D Projected;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, WorldPos, Projected, false))
	{
		WaypointMark->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FVector2D Viewport = UWidgetLayoutLibrary::GetViewportSize(this);
	const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
	const FVector2D Size = Viewport / FMath::Max(Scale, 0.01f);
	const float Pad = 36.f;
	const bool bOffscreen =
		Projected.X < Pad || Projected.Y < Pad ||
		Projected.X > Size.X - Pad || Projected.Y > Size.Y - Pad;

	Projected.X = FMath::Clamp(Projected.X, Pad, Size.X - Pad);
	Projected.Y = FMath::Clamp(Projected.Y, Pad, Size.Y - Pad);

	WaypointMark->SetVisibility(ESlateVisibility::HitTestInvisible);
	WaypointMark->SetText(FText::FromString(bOffscreen ? TEXT("▸") : TEXT("◆")));
	FMenuUIStyle::ApplyMarkerFont(WaypointMark, bOffscreen ? 26.f : 22.f, FMenuUIStyle::TodayEdgeColor());

	if (UCanvasPanelSlot* WaySlot = Cast<UCanvasPanelSlot>(WaypointMark->Slot))
	{
		WaySlot->SetPosition(Projected);
	}
}
