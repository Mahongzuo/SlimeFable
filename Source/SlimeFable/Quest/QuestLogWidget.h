#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuestLogWidget.generated.h"

class UCanvasPanel;
class UImage;
class UBorder;
class UTextBlock;
class UButton;
class UVerticalBox;
class UScrollBox;
class UQuestSubsystem;

UCLASS()
class SLIMEFABLE_API UQuestLogRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(FName InChapterId, FName InQuestId, FName InBranchId, bool bInSide, const FText& Label, bool bCompleted, bool bCurrent, bool bLocked, bool bTracked);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	TObjectPtr<UButton> RowButton;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Bookmark;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LabelText;

	FName ChapterId;
	FName QuestId;
	FName BranchId;
	bool bSide = false;
	bool bCompleted = false;
	bool bCurrent = false;
	bool bLocked = false;
	bool bTracked = false;
	FText CachedLabel;
	bool bBuiltInCode = false;
};

UCLASS()
class SLIMEFABLE_API UQuestLogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UQuestLogWidget(const FObjectInitializer& ObjectInitializer);

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	void HandleRowClicked(FName ChapterId, FName QuestId, FName BranchId, bool bSide);

protected:
	void BuildLayoutIfNeeded();
	void Refresh();
	void RebuildLists();
	void RefreshDetail();

	UFUNCTION()
	void HandleTrackClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TObjectPtr<UImage> DimOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Panel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> MainList;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> SideList;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailTitle;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailProgress;

	UPROPERTY(Transient)
	TObjectPtr<UButton> TrackButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TrackButtonLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FooterText;

	FName SelectedChapterId;
	FName SelectedQuestId;
	FName SelectedBranchId;
	bool bSelectedSide = false;
	bool bBuiltInCode = false;
};
