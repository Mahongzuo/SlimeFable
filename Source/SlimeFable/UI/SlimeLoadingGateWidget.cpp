// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeLoadingGateWidget.h"
#include "UI/MenuUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreaming.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "ShaderCompiler.h"
#include "ShaderPipelineCache.h"
#include "ContentStreaming.h"
#include "Engine/GameInstance.h"
#include "UObject/UObjectGlobals.h"
#include "Styling/SlateTypes.h"
#include "Misc/App.h"
#include "SlimeSkillVfxSubsystem.h"

void USlimeLoadingGateWidget::SetStory(const FText& Title, const FText& Body)
{
	PendingTitle = Title;
	PendingBody = Body;
	ApplyStoryTexts();
}

void USlimeLoadingGateWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyLook();
	SetIsFocusable(true);

	bPrevScreenMessagesEnabled = GAreScreenMessagesEnabled;
	GAreScreenMessagesEnabled = false;

	DisplayedProgress = 0.02f;
	ShownSeconds = 0.f;
	ZeroJobStableSeconds = 0.f;
	ExtraFramesAfterReady = 0;
	bFinishing = false;
	bFinished = false;
	LastPollTimeSeconds = FApp::GetCurrentTime();
}

void USlimeLoadingGateWidget::NativeDestruct()
{
	GAreScreenMessagesEnabled = bPrevScreenMessagesEnabled;
	Super::NativeDestruct();
}

void USlimeLoadingGateWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Poll with wall-clock delta so progress still advances while the world is paused.
	PollGate();
}

TSharedRef<SWidget> USlimeLoadingGateWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void USlimeLoadingGateWidget::BuildLayoutIfNeeded()
{
	if (BackgroundImage && StatusText && ProgressBar)
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
		DimSlot->SetAnchors(FAnchors(0.f, 0.86f, 1.f, 1.f));
		DimSlot->SetOffsets(FMargin(0.f));
	}

	StoryPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StoryPlate"));
	StoryPlate->SetPadding(FMargin(28.f, 18.f, 36.f, 18.f));
	StoryPlate->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* PlateSlot = Root->AddChildToCanvas(StoryPlate))
	{
		PlateSlot->SetAnchors(FAnchors(0.f, 1.f));
		PlateSlot->SetAlignment(FVector2D(0.f, 1.f));
		PlateSlot->SetPosition(FVector2D(64.f, -78.f));
		PlateSlot->SetAutoSize(true);
	}

	UVerticalBox* StoryCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StoryCol"));
	StoryPlate->SetContent(StoryCol);
	StoryTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StoryTitle"));
	StoryTitle->SetJustification(ETextJustify::Left);
	StoryTitle->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* TitleSlot = StoryCol->AddChildToVerticalBox(StoryTitle))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Left);
	}
	StoryBody = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StoryBody"));
	StoryBody->SetJustification(ETextJustify::Left);
	StoryBody->SetAutoWrapText(true);
	StoryBody->SetWrapTextAt(720.f);
	StoryBody->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* BodySlot = StoryCol->AddChildToVerticalBox(StoryBody))
	{
		BodySlot->SetHorizontalAlignment(HAlign_Left);
	}

	UHorizontalBox* LineRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LineRow"));
	if (UCanvasPanelSlot* LineSlot = Root->AddChildToCanvas(LineRow))
	{
		LineSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
		LineSlot->SetAlignment(FVector2D(0.f, 1.f));
		LineSlot->SetOffsets(FMargin(64.f, 0.f, 48.f, 36.f));
		LineSlot->SetAutoSize(true);
	}

	USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarSize"));
	BarSize->SetHeightOverride(3.f);
	if (UHorizontalBoxSlot* BarSlot = LineRow->AddChildToHorizontalBox(BarSize))
	{
		BarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		BarSlot->SetVerticalAlignment(VAlign_Center);
		BarSlot->SetPadding(FMargin(0.f, 0.f, 28.f, 0.f));
	}
	ProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ProgressBar"));
	ProgressBar->SetPercent(0.02f);
	BarSize->AddChild(ProgressBar);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetText(FText::FromString(TEXT("2%")));
	StatusText->SetJustification(ETextJustify::Right);
	if (UHorizontalBoxSlot* PercentSlot = LineRow->AddChildToHorizontalBox(StatusText))
	{
		PercentSlot->SetVerticalAlignment(VAlign_Center);
		PercentSlot->SetHorizontalAlignment(HAlign_Right);
	}

	ApplyStoryTexts();
}

void USlimeLoadingGateWidget::ApplyLook()
{
	ApplyRandomPoster();
	if (DimOverlay)
	{
		FSlateBrush DimBrush;
		DimBrush.DrawAs = ESlateBrushDrawType::Box;
		DimBrush.TintColor = FSlateColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.28f));
		DimBrush.Margin = FMargin(0.f);
		DimBrush.ImageSize = FVector2D(32.f, 32.f);
		DimOverlay->SetBrush(DimBrush);
	}
	if (StoryPlate)
	{
		FSlateBrush Plate;
		Plate.DrawAs = ESlateBrushDrawType::RoundedBox;
		Plate.TintColor = FSlateColor(FLinearColor(0.03f, 0.03f, 0.04f, 0.58f));
		Plate.OutlineSettings.CornerRadii = FVector4(10.f, 10.f, 10.f, 10.f);
		Plate.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Plate.OutlineSettings.Color = FSlateColor(FLinearColor(0.92f, 0.88f, 0.78f, 0.28f));
		Plate.OutlineSettings.Width = 1.f;
		Plate.ImageSize = FVector2D(32.f, 32.f);
		StoryPlate->SetBrush(Plate);
	}

	FMenuUIStyle::ApplyMarkerFont(StatusText, 42.f, FLinearColor(0.96f, 0.93f, 0.86f, 1.f));
	ApplyStoryTexts();

	if (ProgressBar)
	{
		FProgressBarStyle Style = ProgressBar->GetWidgetStyle();
		FSlateBrush Track = *FCoreStyle::Get().GetBrush("WhiteBrush");
		Track.DrawAs = ESlateBrushDrawType::Image;
		Track.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.22f));
		Track.ImageSize = FVector2D(64.f, 3.f);

		FSlateBrush Fill = *FCoreStyle::Get().GetBrush("WhiteBrush");
		Fill.DrawAs = ESlateBrushDrawType::Image;
		Fill.TintColor = FSlateColor(FLinearColor(0.96f, 0.93f, 0.86f, 0.95f));
		Fill.ImageSize = FVector2D(64.f, 3.f);

		Style.SetBackgroundImage(Track);
		Style.SetFillImage(Fill);
		Style.SetMarqueeImage(Fill);
		ProgressBar->SetWidgetStyle(Style);
		ProgressBar->SetFillColorAndOpacity(FLinearColor(0.96f, 0.93f, 0.86f, 1.f));
	}
}

void USlimeLoadingGateWidget::ApplyRandomPoster()
{
	if (!BackgroundImage)
	{
		return;
	}
	const int32 Index = FMath::RandRange(1, 6);
	const FString Path = FString::Printf(
		TEXT("/Game/UI/Loading/T_LoadPoster_%02d.T_LoadPoster_%02d"), Index, Index);
	if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Path))
	{
		BackgroundImage->SetBrush(FMenuUIStyle::MakeTextureBrush(Texture, FVector2D(1920.f, 1080.f)));
		BackgroundImage->SetColorAndOpacity(FLinearColor::White);
		return;
	}
	FMenuUIStyle::ApplyMenuBackground(BackgroundImage);
}

void USlimeLoadingGateWidget::ApplyStoryTexts()
{
	const bool bShow = !PendingTitle.IsEmpty();
	if (StoryPlate)
	{
		StoryPlate->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (StoryTitle)
	{
		StoryTitle->SetText(PendingTitle);
		StoryTitle->SetJustification(ETextJustify::Left);
		StoryTitle->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		FMenuUIStyle::ApplyMixedMenuFont(StoryTitle, 32.f, FMenuUIStyle::WarmTitleColor());
	}
	if (StoryBody)
	{
		StoryBody->SetText(PendingBody);
		StoryBody->SetJustification(ETextJustify::Left);
		StoryBody->SetAutoWrapText(true);
		StoryBody->SetWrapTextAt(720.f);
		StoryBody->SetVisibility(bShow && !PendingBody.IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		FMenuUIStyle::ApplyBrushCJKFont(StoryBody, 20.f, FMenuUIStyle::WarmTextColor());
	}
}

bool USlimeLoadingGateWidget::HasPlayerPawn() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC && PC->GetPawn();
}

bool USlimeLoadingGateWidget::NeedsPlayerPawn() const
{
	const UWorld* World = GetWorld();
	const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	return GameMode && GameMode->DefaultPawnClass != nullptr;
}

int32 USlimeLoadingGateWidget::GetShaderJobsRemaining() const
{
	if (GShaderCompilingManager)
	{
		return GShaderCompilingManager->GetNumRemainingJobs();
	}
	return 0;
}

int32 USlimeLoadingGateWidget::GetStreamingJobsRemaining() const
{
	int32 Count = IsAsyncLoading() ? 1 : 0;
	Count += IStreamingManager::Get().GetNumWantingResources();
	return Count;
}

int32 USlimeLoadingGateWidget::GetSkillVfxJobsRemaining() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const USlimeSkillVfxSubsystem* VfxSubsystem = GameInstance
		? GameInstance->GetSubsystem<USlimeSkillVfxSubsystem>()
		: nullptr;
	if (!VfxSubsystem || VfxSubsystem->IsPreloadComplete())
	{
		return 0;
	}
	return FMath::Max(VfxSubsystem->GetRequestedAssetCount() - VfxSubsystem->GetFailedAssetCount(), 1);
}

int32 USlimeLoadingGateWidget::GetPsoJobsRemaining() const
{
	return static_cast<int32>(FShaderPipelineCache::NumPrecompilesRemaining());
}

bool USlimeLoadingGateWidget::IsRenderReady() const
{
	return GetShaderJobsRemaining() <= 0
		&& GetStreamingJobsRemaining() <= 0
		&& GetSkillVfxJobsRemaining() <= 0
		&& GetPsoJobsRemaining() <= 0;
}

void USlimeLoadingGateWidget::PollGate()
{
	if (bFinished)
	{
		return;
	}

	const double Now = FApp::GetCurrentTime();
	const float InDeltaTime = FMath::Clamp(static_cast<float>(Now - LastPollTimeSeconds), 0.01f, 0.25f);
	LastPollTimeSeconds = Now;
	ShownSeconds += InDeltaTime;

	if (!bFlushedStreaming)
	{
		bFlushedStreaming = true;
		if (UWorld* World = GetWorld())
		{
			World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
		}
	}

	const bool bJobsIdle = IsRenderReady();
	const bool bPawnReady = !NeedsPlayerPawn() || HasPlayerPawn();
	if (bJobsIdle && bPawnReady)
	{
		ZeroJobStableSeconds += InDeltaTime;
	}
	else if (!bFinishing)
	{
		ZeroJobStableSeconds = 0.f;
		ExtraFramesAfterReady = 0;
	}

	const bool bStoryHoldMet = PendingTitle.IsEmpty() || ShownSeconds >= 2.5f;
	const bool bTimedOut = ShownSeconds >= 12.f;
	const bool bReady = bTimedOut || (bJobsIdle && bPawnReady && bStoryHoldMet && ZeroJobStableSeconds >= 1.f);

	float Target = DisplayedProgress;
	if (bReady || bFinishing)
	{
		bFinishing = true;
		Target = 1.f;
	}
	else
	{
		// Fake ease toward 95% while compiling; never stuck at 1–2%.
		const float Climb = FMath::Max(0.08f, 0.22f - DisplayedProgress * 0.12f);
		Target = FMath::Min(0.95f, DisplayedProgress + InDeltaTime * Climb);
	}

	DisplayedProgress = FMath::FInterpTo(DisplayedProgress, Target, InDeltaTime, bFinishing ? 10.f : 3.5f);
	if (bFinishing && DisplayedProgress > 0.995f)
	{
		DisplayedProgress = 1.f;
	}

	if (ProgressBar)
	{
		ProgressBar->SetPercent(DisplayedProgress);
	}
	if (StatusText)
	{
		const int32 Pct = FMath::Clamp(FMath::RoundToInt(DisplayedProgress * 100.f), 0, 100);
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), Pct)));
	}

	if (bFinishing && DisplayedProgress >= 1.f)
	{
		++ExtraFramesAfterReady;
		if (ExtraFramesAfterReady >= 3)
		{
			FinishGate();
		}
	}
}

void USlimeLoadingGateWidget::FinishGate()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	GAreScreenMessagesEnabled = bPrevScreenMessagesEnabled;
	OnGateFinished.Broadcast();
	RemoveFromParent();
}
