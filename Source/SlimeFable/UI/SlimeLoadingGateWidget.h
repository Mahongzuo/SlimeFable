// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeLoadingGateWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UProgressBar;
class UCanvasPanel;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSlimeLoadingGateFinished);

/**
 * Game-thread fullscreen gate after map load: fake progress while shaders compile,
 * then release control when jobs stay at zero.
 * Uses NativeTick (not World timers) so it still finishes while the game is paused.
 */
UCLASS()
class SLIMEFABLE_API USlimeLoadingGateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Year travel title and the chapter's one-sentence history. Empty hides both lines. */
	void SetStory(const FText& Title, const FText& Body);

	UPROPERTY(BlueprintAssignable, Category = "Loading")
	FOnSlimeLoadingGateFinished OnGateFinished;

protected:
	void BuildLayoutIfNeeded();
	void ApplyLook();
	void ApplyStoryTexts();
	void ApplyRandomPoster();
	void FinishGate();
	bool HasPlayerPawn() const;
	bool NeedsPlayerPawn() const;
	int32 GetShaderJobsRemaining() const;
	int32 GetStreamingJobsRemaining() const;
	int32 GetSkillVfxJobsRemaining() const;
	int32 GetPsoJobsRemaining() const;
	bool IsRenderReady() const;
	void PollGate();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DimOverlay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> StoryPlate;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StoryTitle;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StoryBody;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar;

	FText PendingTitle;
	FText PendingBody;

	float DisplayedProgress = 0.f;
	float ShownSeconds = 0.f;
	float ZeroJobStableSeconds = 0.f;
	int32 ExtraFramesAfterReady = 0;
	bool bFinishing = false;
	bool bFinished = false;
	bool bBuiltInCode = false;
	bool bFlushedStreaming = false;

	/** Restored when the gate closes. */
	bool bPrevScreenMessagesEnabled = true;

	double LastPollTimeSeconds = 0.0;
};
