// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

class SProgressBar;
class STextBlock;

/**
 * MoviePlayer loading UI. Must NOT touch UObjects (texture/font assets) during
 * Construct/Tick/Paint — those run on SlateLoadingThread.
 */
class SSlimeLoadingScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSlimeLoadingScreen)
		: _BackgroundBrush()
	{}
		/** Prebuilt on the game thread (dynamic slate resource or solid color). */
		SLATE_ARGUMENT(TSharedPtr<FSlateBrush>, BackgroundBrush)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Game-thread only: build a brush safe for the loading slate thread. */
	static TSharedPtr<FSlateBrush> CreateBackgroundBrushOnGameThread();

private:
	TOptional<float> GetProgressPercent() const;

	TSharedPtr<FSlateBrush> BackgroundBrush;
	FSlateBrush TrackBrush;
	FSlateBrush FillBrush;
	FProgressBarStyle ProgressStyle;
	TSharedPtr<SProgressBar> ProgressBar;
	TSharedPtr<STextBlock> StatusText;

	float DisplayedProgress = 0.f;
};
