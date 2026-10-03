#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeRunnerHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;

/** Top-right runner panel: flag count, route progress, optional countdown. Built in code. */
UCLASS()
class SLIMEFABLE_API USlimeRunnerHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USlimeRunnerHUDWidget(const FObjectInitializer& ObjectInitializer);

	void SetLabels(const FText& Start, const FText& End);
	/** TimeLeft < 0 hides the timer. */
	void SetStats(int32 Flags, int32 Total, float Progress, float TimeLeft);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void BuildLayoutIfNeeded();
	void Apply();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FlagText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StartText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EndText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TimerText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> RouteBar;

	FText StartLabel;
	FText EndLabel;
	int32 CachedFlags = 0;
	int32 CachedTotal = 0;
	float CachedProgress = 0.f;
	float CachedTimeLeft = -1.f;
};
