#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeTrackHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;

/** 区间名、步数、六站进度，以及底部按键提示。 */
UCLASS()
class SLIMEFABLE_API USlimeTrackHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USlimeTrackHUDWidget(const FObjectInitializer& ObjectInitializer);

	/** TimeLeft < 0 hides the countdown. StationsLit is 1–6. */
	void SetStats(const FText& Route, int32 Steps, int32 Limit, int32 StationsLit, float TimeLeft, const FText& Keys);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void BuildLayoutIfNeeded();
	void Apply();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RouteText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StepText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TimerText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> KeyText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> RouteBar;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> StationTexts;

	FText CachedRoute;
	FText CachedKeys;
	int32 CachedSteps = 0;
	int32 CachedLimit = 0;
	int32 CachedLit = 1;
	float CachedTimeLeft = -1.f;
};
