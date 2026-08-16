#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "QuestHUDWidget.generated.h"

class UCanvasPanel;
class UBorder;
class UTextBlock;
class UProgressBar;
class UImage;
class UQuestSubsystem;

UCLASS()
class SLIMEFABLE_API UQuestHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UQuestHUDWidget(const FObjectInitializer& ObjectInitializer);

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

protected:
	void BuildLayoutIfNeeded();
	void Refresh();
	void UpdateWaypoint(UQuestSubsystem* Quests, APlayerController* PC);
	bool IsLocalCombatActive(APlayerController* PC) const;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> TrackerPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BranchText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> BranchProgress;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ToastPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BannerKicker;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ToastText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WaypointMark;

	bool bBuiltInCode = false;
};
