#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "WeekSelectWidget.generated.h"

class UImage;
class UBorder;
class UTextBlock;
class UButton;

UCLASS()
class SLIMEFABLE_API UWeekSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWeekSelectWidget(const FObjectInitializer& ObjectInitializer);

	void Setup(FName InDayId, FName InChapterId);

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

protected:
	void BuildLayoutIfNeeded();
	void ApplyVisuals();
	void RefreshButtons();
	void HandleWeekClicked(int32 Week);
	UButton* GetWeekButton(int32 Week) const;
	UTextBlock* GetWeekLabel(int32 Week) const;

	UFUNCTION()
	void HandleWeek1();

	UFUNCTION()
	void HandleWeek2();

	UFUNCTION()
	void HandleWeek3();

	UFUNCTION()
	void HandleCancel();

	UPROPERTY(Transient)
	TObjectPtr<UImage> DimOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Panel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> WeekButton1;

	UPROPERTY(Transient)
	TObjectPtr<UButton> WeekButton2;

	UPROPERTY(Transient)
	TObjectPtr<UButton> WeekButton3;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WeekLabel1;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WeekLabel2;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WeekLabel3;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CancelButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CancelLabel;

	FName DayId;
	FName ChapterId;
	bool bBuiltInCode = false;
};
