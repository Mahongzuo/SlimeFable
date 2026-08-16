#include "Quest/QuestLogWidget.h"
#include "Quest/QuestSubsystem.h"
#include "Settings/SlimeInputSettings.h"
#include "UI/MenuUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"

namespace
{
	FSlateBrush MakeRockPanelBrush()
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor(0.08f, 0.06f, 0.04f, 0.88f));
		Brush.OutlineSettings.CornerRadii = FVector4(12.f, 12.f, 12.f, 12.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Color = FSlateColor(FLinearColor(FMenuUIStyle::TodayEdgeColor().R, FMenuUIStyle::TodayEdgeColor().G, FMenuUIStyle::TodayEdgeColor().B, 0.45f));
		Brush.OutlineSettings.Width = 1.6f;
		Brush.ImageSize = FVector2D(64.f, 64.f);
		return Brush;
	}
}

void UQuestLogRowWidget::Setup(FName InChapterId, FName InQuestId, FName InBranchId, bool bInSide, const FText& Label, bool bInCompleted, bool bInCurrent, bool bInLocked, bool bInTracked)
{
	ChapterId = InChapterId;
	QuestId = InQuestId;
	BranchId = InBranchId;
	bSide = bInSide;
	CachedLabel = Label;
	bCompleted = bInCompleted;
	bCurrent = bInCurrent;
	bLocked = bInLocked;
	bTracked = bInTracked;
	if (LabelText)
	{
		LabelText->SetText(CachedLabel);
		const FLinearColor Color = bLocked
			? FMenuUIStyle::WarmMutedTextColor()
			: (bCompleted ? FMenuUIStyle::WarmTextColor() : FMenuUIStyle::WarmTitleColor());
		FMenuUIStyle::ApplyBrushCJKFont(LabelText, 16.f, Color);
	}
	if (Bookmark)
	{
		Bookmark->SetVisibility(bTracked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

TSharedRef<SWidget> UQuestLogRowWidget::RebuildWidget()
{
	if (!bBuiltInCode)
	{
		bBuiltInCode = true;
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
		WidgetTree->RootWidget = Row;

		Bookmark = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Bookmark"));
		{
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.TintColor = FSlateColor(FMenuUIStyle::TodayEdgeColor());
			Brush.ImageSize = FVector2D(4.f, 28.f);
			Bookmark->SetBrush(Brush);
		}
		USizeBox* MarkBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MarkBox"));
		MarkBox->SetWidthOverride(4.f);
		MarkBox->SetMinDesiredHeight(28.f);
		MarkBox->AddChild(Bookmark);
		if (UHorizontalBoxSlot* MarkSlot = Row->AddChildToHorizontalBox(MarkBox))
		{
			MarkSlot->SetPadding(FMargin(0.f, 2.f, 8.f, 2.f));
			MarkSlot->SetVerticalAlignment(VAlign_Center);
		}

		RowButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RowButton"));
		LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LabelText"));
		LabelText->SetText(CachedLabel);
		LabelText->SetAutoWrapText(false);
		RowButton->AddChild(LabelText);
		if (UHorizontalBoxSlot* BtnSlot = Row->AddChildToHorizontalBox(RowButton))
		{
			BtnSlot->SetSize(ESlateSizeRule::Fill);
		}
	}
	return Super::RebuildWidget();
}

void UQuestLogRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (RowButton)
	{
		FMenuUIStyle::ApplyFlatButtonStyle(RowButton, FLinearColor(0.12f, 0.09f, 0.06f, 0.55f), FVector2D(280.f, 34.f), FMargin(8.f, 4.f));
		RowButton->OnClicked.AddUniqueDynamic(this, &UQuestLogRowWidget::HandleClicked);
	}
	if (LabelText)
	{
		LabelText->SetText(CachedLabel);
		LabelText->SetAutoWrapText(false);
		FMenuUIStyle::ApplyBrushCJKFont(LabelText, 16.f, bLocked ? FMenuUIStyle::WarmMutedTextColor() : FMenuUIStyle::WarmTitleColor());
	}
	if (Bookmark)
	{
		Bookmark->SetVisibility(bTracked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UQuestLogRowWidget::HandleClicked()
{
	if (UQuestLogWidget* Log = GetTypedOuter<UQuestLogWidget>())
	{
		Log->HandleRowClicked(ChapterId, QuestId, BranchId, bSide);
	}
}

UQuestLogWidget::UQuestLogWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

TSharedRef<SWidget> UQuestLogWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void UQuestLogWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Visible);
	if (TrackButton)
	{
		TrackButton->OnClicked.AddUniqueDynamic(this, &UQuestLogWidget::HandleTrackClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UQuestLogWidget::HandleCloseClicked);
	}
	Refresh();
}

FReply UQuestLogWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	FKey CloseKey = EKeys::J;
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const USlimeInputSettings* InputSettings = GI->GetSubsystem<USlimeInputSettings>())
		{
			CloseKey = InputSettings->GetKey(ESlimeInputAction::QuestLog);
		}
	}
	if (InKeyEvent.GetKey() == CloseKey)
	{
		HandleCloseClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UQuestLogWidget::BuildLayoutIfNeeded()
{
	if (bBuiltInCode && Panel && MainList && SideList)
	{
		return;
	}
	bBuiltInCode = true;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("QuestLogRoot"));
	WidgetTree->RootWidget = Root;

	DimOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DimOverlay"));
	if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(DimOverlay))
	{
		DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimSlot->SetOffsets(FMargin(0.f));
		DimSlot->SetZOrder(0);
	}

	Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrush(MakeRockPanelBrush());
	Panel->SetPadding(FMargin(28.f, 22.f));
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
		PanelSlot->SetZOrder(1);
	}

	UVerticalBox* RootCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootCol"));
	Panel->SetContent(RootCol);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("史书")));
	if (UVerticalBoxSlot* TitleSlot = RootCol->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	}

	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Body"));
	RootCol->AddChildToVerticalBox(Body);

	UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Left"));
	if (UHorizontalBoxSlot* LeftSlot = Body->AddChildToHorizontalBox(Left))
	{
		LeftSlot->SetPadding(FMargin(0.f, 0.f, 20.f, 0.f));
	}

	auto MakeSection = [this](UVerticalBox* Parent, const TCHAR* Title, const TCHAR* ListName) -> UVerticalBox*
	{
		UTextBlock* Head = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(FString(ListName) + TEXT("_Head")));
		Head->SetText(FText::FromString(Title));
		if (UVerticalBoxSlot* HeadSlot = Parent->AddChildToVerticalBox(Head))
		{
			HeadSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 6.f));
		}
		FMenuUIStyle::ApplyBrushCJKFont(Head, 18.f, FMenuUIStyle::TodayEdgeColor());

		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), *(FString(ListName) + TEXT("_Scroll")));
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *(FString(ListName) + TEXT("_Box")));
		Box->SetWidthOverride(320.f);
		Box->SetHeightOverride(160.f);
		Box->AddChild(Scroll);
		Parent->AddChildToVerticalBox(Box);

		UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), ListName);
		Scroll->AddChild(List);
		return List;
	};

	MainList = MakeSection(Left, TEXT("主线"), TEXT("MainList"));
	SideList = MakeSection(Left, TEXT("支线"), TEXT("SideList"));

	UBorder* Detail = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Detail"));
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor(0.06f, 0.05f, 0.035f, 0.7f));
		Brush.OutlineSettings.CornerRadii = FVector4(8.f, 8.f, 8.f, 8.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Color = FSlateColor(FLinearColor(0.72f, 0.64f, 0.46f, 0.28f));
		Brush.OutlineSettings.Width = 1.2f;
		Detail->SetBrush(Brush);
		Detail->SetPadding(FMargin(16.f, 14.f));
	}
	USizeBox* DetailBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DetailBox"));
	DetailBox->SetWidthOverride(280.f);
	DetailBox->SetMinDesiredHeight(220.f);
	DetailBox->AddChild(Detail);
	if (UHorizontalBoxSlot* DetailSlot = Body->AddChildToHorizontalBox(DetailBox))
	{
		DetailSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* DetailCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DetailCol"));
	Detail->SetContent(DetailCol);

	DetailTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DetailTitle"));
	DetailTitle->SetAutoWrapText(true);
	DetailTitle->SetWrapTextAt(248.f);
	DetailCol->AddChildToVerticalBox(DetailTitle);
	DetailProgress = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DetailProgress"));
	DetailProgress->SetAutoWrapText(true);
	DetailProgress->SetWrapTextAt(248.f);
	if (UVerticalBoxSlot* ProgSlot = DetailCol->AddChildToVerticalBox(DetailProgress))
	{
		ProgSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 16.f));
	}

	TrackButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TrackButton"));
	TrackButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TrackButtonLabel"));
	TrackButtonLabel->SetText(FText::FromString(TEXT("追踪")));
	TrackButton->AddChild(TrackButtonLabel);
	DetailCol->AddChildToVerticalBox(TrackButton);

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	UTextBlock* CloseLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseLabel"));
	CloseLabel->SetText(FText::FromString(TEXT("关闭")));
	CloseButton->AddChild(CloseLabel);
	if (UVerticalBoxSlot* CloseSlot = RootCol->AddChildToVerticalBox(CloseButton))
	{
		CloseSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 8.f));
	}

	FooterText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FooterText"));
	FooterText->SetText(FText::FromString(TEXT("J 史书 · 点击切换追踪 · 主线仍挡过关")));
	FooterText->SetAutoWrapText(true);
	FooterText->SetWrapTextAt(600.f);
	USizeBox* FooterBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("FooterBox"));
	FooterBox->SetWidthOverride(620.f);
	FooterBox->AddChild(FooterText);
	RootCol->AddChildToVerticalBox(FooterBox);
}

void UQuestLogWidget::Refresh()
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
		FMenuUIStyle::ApplyMixedMenuFont(TitleText, 28.f, FMenuUIStyle::WarmTitleColor());
	}
	if (FooterText)
	{
		FooterText->SetAutoWrapText(true);
		FooterText->SetWrapTextAt(600.f);
		FMenuUIStyle::ApplyBrushCJKFont(FooterText, 14.f, FMenuUIStyle::WarmMutedTextColor());
	}
	if (DetailTitle)
	{
		DetailTitle->SetAutoWrapText(true);
		DetailTitle->SetWrapTextAt(248.f);
	}
	if (DetailProgress)
	{
		DetailProgress->SetAutoWrapText(true);
		DetailProgress->SetWrapTextAt(248.f);
	}
	if (UMaterialInterface* Ink = FMenuUIStyle::LoadButtonMaterial())
	{
		if (TrackButton)
		{
			FMenuUIStyle::ApplyMaterialButtonStyle(TrackButton, Ink, FVector2D(160.f, 40.f));
			FMenuUIStyle::BindInkButtonHover(TrackButton, TrackButtonLabel);
		}
		if (CloseButton)
		{
			if (UTextBlock* CloseLabel = Cast<UTextBlock>(CloseButton->GetChildAt(0)))
			{
				FMenuUIStyle::ApplyMaterialButtonStyle(CloseButton, Ink, FVector2D(120.f, 36.f));
				FMenuUIStyle::BindInkButtonHover(CloseButton, CloseLabel);
				FMenuUIStyle::ApplyBrushCJKFont(CloseLabel, 16.f, FMenuUIStyle::WarmTitleColor());
			}
		}
	}
	if (TrackButtonLabel)
	{
		FMenuUIStyle::ApplyBrushCJKFont(TrackButtonLabel, 16.f, FMenuUIStyle::WarmTitleColor());
	}

	UQuestSubsystem* Quests = GetGameInstance() ? GetGameInstance()->GetSubsystem<UQuestSubsystem>() : nullptr;
	if (Quests && SelectedQuestId.IsNone())
	{
		SelectedChapterId = Quests->GetTrackedChapterId();
		SelectedQuestId = Quests->GetTrackedQuestId();
		SelectedBranchId = Quests->GetTrackedBranchId();
		bSelectedSide = Quests->IsTrackingSide();
	}

	RebuildLists();
	RefreshDetail();
}

void UQuestLogWidget::RebuildLists()
{
	if (!MainList || !SideList)
	{
		return;
	}
	MainList->ClearChildren();
	SideList->ClearChildren();

	UQuestSubsystem* Quests = GetGameInstance() ? GetGameInstance()->GetSubsystem<UQuestSubsystem>() : nullptr;
	const UDayQuestBook* Book = Quests ? Quests->GetBook() : nullptr;
	if (!Quests || !Book)
	{
		return;
	}

	const FQuestChapter* Chapter = Book->FindChapter(Quests->GetActiveChapterId());
	if (!Chapter)
	{
		Chapter = Book->FindChapter(Quests->GetTrackedChapterId());
	}
	if (!Chapter)
	{
		for (const FQuestChapter& Candidate : Book->Chapters)
		{
			if (!Quests->IsChapterComplete(Candidate.ChapterId))
			{
				Chapter = &Candidate;
				break;
			}
		}
	}
	if (!Chapter && Book->Chapters.Num() > 0)
	{
		Chapter = &Book->Chapters[0];
	}
	if (!Chapter)
	{
		return;
	}

	bool bReachedCurrent = false;
	for (const FQuestMain& Main : Chapter->MainQuests)
	{
		for (int32 Index = 0; Index < Main.Branches.Num(); ++Index)
		{
			const FQuestBranch& Branch = Main.Branches[Index];
			const bool bIsCurrent = Quests->GetActiveQuestId() == Main.QuestId
				&& Quests->GetActiveBranchId() == Branch.BranchId;
			const bool bIsTracked = !Quests->IsTrackingSide()
				&& Quests->GetTrackedQuestId() == Main.QuestId
				&& Quests->GetTrackedBranchId() == Branch.BranchId;
			bool bLocked = false;
			bool bCompletedStep = false;
			if (Quests->IsChapterComplete(Chapter->ChapterId))
			{
				bCompletedStep = true;
			}
			else if (bIsCurrent)
			{
				bReachedCurrent = true;
			}
			else if (!bReachedCurrent)
			{
				bCompletedStep = true;
			}
			else
			{
				bLocked = true;
			}

			const FString Mark = bCompletedStep ? TEXT("✓ ") : (bIsCurrent ? TEXT("▸ ") : TEXT("  "));
			UQuestLogRowWidget* Row = CreateWidget<UQuestLogRowWidget>(this, UQuestLogRowWidget::StaticClass());
			if (!Row)
			{
				continue;
			}
			Row->Setup(
				Chapter->ChapterId,
				Main.QuestId,
				Branch.BranchId,
				false,
				FText::FromString(Mark + Branch.Title.ToString()),
				bCompletedStep,
				bIsCurrent,
				bLocked,
				bIsTracked);
			MainList->AddChildToVerticalBox(Row);
		}
	}

	for (const FQuestMain& Side : Chapter->SideQuests)
	{
		const bool bDone = Quests->IsSideComplete(Chapter->ChapterId, Side.QuestId);
		const FName BranchId = Side.Branches.Num() > 0 ? Side.Branches[0].BranchId : NAME_None;
		const bool bTracked = Quests->IsTrackingSide()
			&& Quests->GetTrackedQuestId() == Side.QuestId;
		const FString Mark = bDone ? TEXT("完成  ") : TEXT("");
		UQuestLogRowWidget* Row = CreateWidget<UQuestLogRowWidget>(this, UQuestLogRowWidget::StaticClass());
		if (!Row)
		{
			continue;
		}
		Row->Setup(
			Chapter->ChapterId,
			Side.QuestId,
			BranchId,
			true,
			FText::FromString(Mark + Side.Title.ToString()),
			bDone,
			false,
			false,
			bTracked);
		SideList->AddChildToVerticalBox(Row);
	}
}

void UQuestLogWidget::RefreshDetail()
{
	UQuestSubsystem* Quests = GetGameInstance() ? GetGameInstance()->GetSubsystem<UQuestSubsystem>() : nullptr;
	const UDayQuestBook* Book = Quests ? Quests->GetBook() : nullptr;
	if (!Quests || !Book || !DetailTitle || !DetailProgress)
	{
		return;
	}

	const FQuestMain* Main = Book->FindMain(SelectedChapterId, SelectedQuestId);
	const FQuestBranch* Branch = Book->FindBranch(SelectedChapterId, SelectedQuestId, SelectedBranchId);
	if (!Main)
	{
		DetailTitle->SetText(FText::FromString(TEXT("选择一条任务")));
		FMenuUIStyle::ApplyBrushCJKFont(DetailTitle, 18.f, FMenuUIStyle::WarmMutedTextColor());
		DetailProgress->SetText(FText::GetEmpty());
		if (TrackButton)
		{
			TrackButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	DetailTitle->SetText(Main->Title);
	FMenuUIStyle::ApplyMixedMenuFont(DetailTitle, 20.f, FMenuUIStyle::WarmTitleColor());

	int32 Count = 0;
	int32 Required = 1;
	if (Branch)
	{
		Required = FMath::Max(1, Branch->RequiredCount);
		Count = bSelectedSide
			? Quests->GetSideCount(SelectedChapterId, SelectedQuestId, SelectedBranchId)
			: (Quests->IsTrackingSide() ? 0 : Quests->GetActiveCount());
		if (!bSelectedSide && Quests->GetTrackedQuestId() == SelectedQuestId && Quests->GetTrackedBranchId() == SelectedBranchId)
		{
			Count = Quests->GetTrackedCount();
			Required = Quests->GetTrackedRequired();
		}
		DetailProgress->SetText(FText::FromString(FString::Printf(
			TEXT("%s  %d/%d"),
			*Branch->Title.ToString(),
			Count,
			Required)));
	}
	else
	{
		DetailProgress->SetText(FText::GetEmpty());
	}
	FMenuUIStyle::ApplyBrushCJKFont(DetailProgress, 16.f, FMenuUIStyle::WarmTextColor());

	const bool bCanTrack = bSelectedSide && !Quests->IsSideComplete(SelectedChapterId, SelectedQuestId);
	if (TrackButton)
	{
		TrackButton->SetVisibility(bCanTrack ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (TrackButtonLabel)
	{
		const bool bAlready = Quests->IsTrackingSide() && Quests->GetTrackedQuestId() == SelectedQuestId;
		TrackButtonLabel->SetText(FText::FromString(bAlready ? TEXT("追踪中") : TEXT("追踪")));
	}
}

void UQuestLogWidget::HandleRowClicked(FName ChapterId, FName QuestId, FName BranchId, bool bSide)
{
	SelectedChapterId = ChapterId;
	SelectedQuestId = QuestId;
	SelectedBranchId = BranchId;
	bSelectedSide = bSide;

	UQuestSubsystem* Quests = GetGameInstance() ? GetGameInstance()->GetSubsystem<UQuestSubsystem>() : nullptr;
	if (Quests && bSide && !Quests->IsSideComplete(ChapterId, QuestId))
	{
		Quests->SetTracked(ChapterId, QuestId, BranchId, true);
	}
	Refresh();
}

void UQuestLogWidget::HandleTrackClicked()
{
	UQuestSubsystem* Quests = GetGameInstance() ? GetGameInstance()->GetSubsystem<UQuestSubsystem>() : nullptr;
	if (!Quests || !bSelectedSide)
	{
		return;
	}
	Quests->SetTracked(SelectedChapterId, SelectedQuestId, SelectedBranchId, true);
	Refresh();
}

void UQuestLogWidget::HandleCloseClicked()
{
	if (UQuestSubsystem* Quests = GetGameInstance() ? GetGameInstance()->GetSubsystem<UQuestSubsystem>() : nullptr)
	{
		Quests->CloseQuestLog();
	}
}
