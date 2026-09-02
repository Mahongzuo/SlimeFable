// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeLoadingGateWidget.generated.h"

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

	UPROPERTY(BlueprintAssignable, Category = "Loading")
	FOnSlimeLoadingGateFinished OnGateFinished;

protected:
	void BuildLayoutIfNeeded();
	void ApplyLook();
	void FinishGate();
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
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar;

	float DisplayedProgress = 0.f;
	float ZeroJobStableSeconds = 0.f;
	int32 ExtraFramesAfterReady = 0;
	bool bFinishing = false;
	bool bFinished = false;
	bool bBuiltInCode = false;

	/** Restored when the gate closes. */
	bool bPrevScreenMessagesEnabled = true;

	double LastPollTimeSeconds = 0.0;
};
