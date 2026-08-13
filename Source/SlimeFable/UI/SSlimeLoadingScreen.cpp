// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SSlimeLoadingScreen.h"
#include "UI/MenuUIStyle.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "MoviePlayer.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"

namespace SlimeLoadingScreenPrivate
{
	static const FName DynamicBgName(TEXT("SlimeFable.LoadingBackground"));

	static TSharedPtr<FSlateBrush> MakeSolidBackground()
	{
		TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		Brush->DrawAs = ESlateBrushDrawType::Image;
		Brush->TintColor = FSlateColor(FLinearColor(0.14f, 0.11f, 0.08f, 1.f));
		Brush->Tiling = ESlateBrushTileType::NoTile;
		return Brush;
	}

	/**
	 * UTexture::Source is editor-only. Packaged targets always return false
	 * and the caller falls back to a solid slate brush (safe on SlateLoadingThread).
	 */
	static bool TryRegisterTexturePixels(UTexture2D* Texture, FVector2D& OutSize)
	{
		OutSize = FVector2D(1920.f, 1080.f);

#if WITH_EDITORONLY_DATA
		if (!Texture || !FSlateApplication::IsInitialized())
		{
			return false;
		}

		FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer();
		if (!Renderer)
		{
			return false;
		}

		if (!Texture->Source.IsValid() || Texture->Source.GetNumMips() <= 0)
		{
			return false;
		}

		const int32 Width = Texture->Source.GetSizeX();
		const int32 Height = Texture->Source.GetSizeY();
		const ETextureSourceFormat SourceFormat = Texture->Source.GetFormat();

		TArray64<uint8> MipData;
		if (!Texture->Source.GetMipData(MipData, 0) || MipData.Num() == 0 || Width <= 0 || Height <= 0)
		{
			return false;
		}

		TArray<uint8> BGRA;
		BGRA.SetNumUninitialized(Width * Height * 4);

		if (SourceFormat == TSF_BGRA8 && MipData.Num() >= BGRA.Num())
		{
			FMemory::Memcpy(BGRA.GetData(), MipData.GetData(), BGRA.Num());
		}
		else
		{
			return false;
		}

		if (!Renderer->GenerateDynamicImageResource(DynamicBgName, Width, Height, BGRA))
		{
			return false;
		}

		OutSize = FVector2D(static_cast<float>(Width), static_cast<float>(Height));
		return true;
#else
		(void)Texture;
		return false;
#endif
	}
}

TSharedPtr<FSlateBrush> SSlimeLoadingScreen::CreateBackgroundBrushOnGameThread()
{
	check(IsInGameThread());

	FVector2D Size(1920.f, 1080.f);
	UTexture2D* Texture = FMenuUIStyle::LoadMenuBackgroundTexture();
	if (Texture && SlimeLoadingScreenPrivate::TryRegisterTexturePixels(Texture, Size))
	{
		TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>(
			FSlateImageBrush(SlimeLoadingScreenPrivate::DynamicBgName, Size));
		Brush->TintColor = FSlateColor(FLinearColor(0.62f, 0.58f, 0.52f, 1.f));
		return Brush;
	}

	return SlimeLoadingScreenPrivate::MakeSolidBackground();
}

void SSlimeLoadingScreen::Construct(const FArguments& InArgs)
{
	BackgroundBrush = InArgs._BackgroundBrush.IsValid()
		? InArgs._BackgroundBrush
		: SlimeLoadingScreenPrivate::MakeSolidBackground();

	TrackBrush = *FCoreStyle::Get().GetBrush("WhiteBrush");
	TrackBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	TrackBrush.TintColor = FSlateColor(FLinearColor(0.12f, 0.09f, 0.06f, 0.82f));
	TrackBrush.OutlineSettings.CornerRadii = FVector4(8.f, 8.f, 8.f, 8.f);
	TrackBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	TrackBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.55f, 0.42f, 0.22f, 0.55f));
	TrackBrush.OutlineSettings.Width = 1.5f;
	TrackBrush.ImageSize = FVector2D(64.f, 16.f);

	FillBrush = *FCoreStyle::Get().GetBrush("WhiteBrush");
	FillBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	FillBrush.TintColor = FSlateColor(FLinearColor(0.82f, 0.66f, 0.34f, 0.95f));
	FillBrush.OutlineSettings.CornerRadii = FVector4(6.f, 6.f, 6.f, 6.f);
	FillBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	FillBrush.ImageSize = FVector2D(64.f, 14.f);

	ProgressStyle = FProgressBarStyle()
		.SetBackgroundImage(TrackBrush)
		.SetFillImage(FillBrush)
		.SetEnableFillAnimation(false);

	const FSlateFontInfo StatusFont = FMenuUIStyle::MakeBrushCJKFont(22.f);

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(BackgroundBrush.Get())
		]
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.04f, 0.03f, 0.02f, 0.28f))
			.Padding(0.f)
			[
				SNullWidget::NullWidget
			]
		]
		+ SOverlay::Slot()
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Fill)
		.Padding(FMargin(96.f, 0.f, 96.f, 72.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.f, 0.f, 0.f, 14.f))
			[
				SAssignNew(StatusText, STextBlock)
				.Text(FText::FromString(TEXT("加载中…")))
				.Font(StatusFont)
				.ColorAndOpacity(FSlateColor(FMenuUIStyle::WarmTextColor()))
				.Justification(ETextJustify::Center)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(16.f)
				[
					SAssignNew(ProgressBar, SProgressBar)
					.Style(&ProgressStyle)
					.Percent(this, &SSlimeLoadingScreen::GetProgressPercent)
					.FillColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.74f, 0.4f, 1.f)))
				]
			]
		]
	];
}

TOptional<float> SSlimeLoadingScreen::GetProgressPercent() const
{
	return DisplayedProgress;
}

void SSlimeLoadingScreen::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	bool bFinished = false;
	if (IGameMoviePlayer* MoviePlayer = GetMoviePlayer())
	{
		bFinished = MoviePlayer->IsLoadingFinished();
	}

	const float Target = bFinished
		? 1.f
		: FMath::Min(0.9f, DisplayedProgress + InDeltaTime * 0.18f);
	DisplayedProgress = FMath::FInterpTo(DisplayedProgress, Target, InDeltaTime, bFinished ? 14.f : 5.f);
	if (bFinished && DisplayedProgress > 0.995f)
	{
		DisplayedProgress = 1.f;
	}

	if (StatusText.IsValid())
	{
		const int32 Pct = FMath::RoundToInt(DisplayedProgress * 100.f);
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("加载中… %d%%"), Pct)));
	}
}
