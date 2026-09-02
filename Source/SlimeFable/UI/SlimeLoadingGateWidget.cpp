// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeLoadingGateWidget.h"
#include "UI/MenuUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "ShaderCompiler.h"
#include "ShaderPipelineCache.h"
#include "ContentStreaming.h"
#include "Engine/GameInstance.h"
#include "UObject/UObjectGlobals.h"
#include "Styling/SlateTypes.h"
#include "Misc/App.h"
#include "SlimeSkillVfxSubsystem.h"

void USlimeLoadingGateWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyLook();
	SetIsFocusable(true);

	bPrevScreenMessagesEnabled = GAreScreenMessagesEnabled;
	GAreScreenMessagesEnabled = false;

	DisplayedProgress = 0.02f;
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
		DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimSlot->SetOffsets(FMargin(0.f));
	}

	UVerticalBox* Bottom = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BottomBox"));
	if (UCanvasPanelSlot* BottomSlot = Root->AddChildToCanvas(Bottom))
	{
		BottomSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
		BottomSlot->SetAlignment(FVector2D(0.f, 1.f));
		BottomSlot->SetOffsets(FMargin(96.f, 0.f, 96.f, 72.f));
		BottomSlot->SetAutoSize(true);
	}

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetText(FText::FromString(TEXT("加载中…")));
	StatusText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* StatusSlot = Bottom->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
		StatusSlot->SetHorizontalAlignment(HAlign_Center);
	}

	USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarSize"));
	BarSize->SetHeightOverride(16.f);
	Bottom->AddChildToVerticalBox(BarSize);

	ProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ProgressBar"));
	ProgressBar->SetPercent(0.02f);
	BarSize->AddChild(ProgressBar);
}

void USlimeLoadingGateWidget::ApplyLook()
{
	FMenuUIStyle::ApplyMenuBackground(BackgroundImage);
	if (DimOverlay)
	{
		FSlateBrush DimBrush;
		DimBrush.DrawAs = ESlateBrushDrawType::Box;
		DimBrush.TintColor = FSlateColor(FLinearColor(0.04f, 0.03f, 0.02f, 0.35f));
		DimBrush.Margin = FMargin(0.f);
		DimBrush.ImageSize = FVector2D(32.f, 32.f);
		DimOverlay->SetBrush(DimBrush);
	}

	FMenuUIStyle::ApplyBrushCJKFont(StatusText, 22.f, FMenuUIStyle::WarmTextColor());

	if (ProgressBar)
	{
		FProgressBarStyle Style = ProgressBar->GetWidgetStyle();
		FSlateBrush Track = *FCoreStyle::Get().GetBrush("WhiteBrush");
		Track.DrawAs = ESlateBrushDrawType::RoundedBox;
		Track.TintColor = FSlateColor(FLinearColor(0.12f, 0.09f, 0.06f, 0.82f));
		Track.OutlineSettings.CornerRadii = FVector4(8.f, 8.f, 8.f, 8.f);
		Track.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Track.OutlineSettings.Color = FSlateColor(FLinearColor(0.55f, 0.42f, 0.22f, 0.55f));
		Track.OutlineSettings.Width = 1.5f;
		Track.ImageSize = FVector2D(64.f, 16.f);

		FSlateBrush Fill = *FCoreStyle::Get().GetBrush("WhiteBrush");
		Fill.DrawAs = ESlateBrushDrawType::RoundedBox;
		Fill.TintColor = FSlateColor(FLinearColor(0.82f, 0.66f, 0.34f, 0.95f));
		Fill.OutlineSettings.CornerRadii = FVector4(6.f, 6.f, 6.f, 6.f);
		Fill.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Fill.ImageSize = FVector2D(64.f, 14.f);

		Style.SetBackgroundImage(Track);
		Style.SetFillImage(Fill);
		ProgressBar->SetWidgetStyle(Style);
		ProgressBar->SetFillColorAndOpacity(FLinearColor(0.9f, 0.74f, 0.4f, 1.f));
	}
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

	const int32 ShaderJobs = GetShaderJobsRemaining();
	const int32 StreamJobs = GetStreamingJobsRemaining();
	const int32 SkillVfxJobs = GetSkillVfxJobsRemaining();
	const int32 PsoJobs = GetPsoJobsRemaining();
	const bool bJobsIdle = IsRenderReady();

	if (bJobsIdle)
	{
		ZeroJobStableSeconds += InDeltaTime;
	}
	else
	{
		ZeroJobStableSeconds = 0.f;
		ExtraFramesAfterReady = 0;
	}

	const bool bReady = bJobsIdle && ZeroJobStableSeconds >= 0.3f;

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
		const int32 Pct = FMath::RoundToInt(DisplayedProgress * 100.f);
		if (SkillVfxJobs > 0)
		{
			StatusText->SetText(FText::FromString(FString::Printf(TEXT("加载中… %d%%（技能特效 %d）"), Pct, SkillVfxJobs)));
		}
		else if (PsoJobs > 0)
		{
			StatusText->SetText(FText::FromString(FString::Printf(TEXT("加载中… %d%%（渲染管线 %d）"), Pct, PsoJobs)));
		}
		else if (ShaderJobs > 0)
		{
			StatusText->SetText(FText::FromString(FString::Printf(TEXT("加载中… %d%%（着色器 %d）"), Pct, ShaderJobs)));
		}
		else if (StreamJobs > 0)
		{
			StatusText->SetText(FText::FromString(FString::Printf(TEXT("加载中… %d%%（资源 %d）"), Pct, StreamJobs)));
		}
		else
		{
			StatusText->SetText(FText::FromString(FString::Printf(TEXT("加载中… %d%%"), Pct)));
		}
		FMenuUIStyle::ApplyBrushCJKFont(StatusText, 22.f, FMenuUIStyle::WarmTextColor());
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
