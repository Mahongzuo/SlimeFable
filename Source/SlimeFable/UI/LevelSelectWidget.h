// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LevelSelectWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UBorder;
class UUniformGridPanel;
class UMainMenuWidget;
class UDayLevelSubsystem;
class UDaySlotWidget;

UCLASS()
class SLIMEFABLE_API ULevelSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	ULevelSelectWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void SetParentMenu(UMainMenuWidget* InParent);
	void JumpToTodayMonth();
	void RefreshForCurrentMonth();

	UFUNCTION()
	void HandleDaySlotClicked(FName DayId);

protected:
	void BuildLayoutIfNeeded();
	void ApplyMaterialLabLook();
	void ApplyCalendarChrome();
	void RebuildDayButtons();
	void ResolveDaySlotClass();
	UDayLevelSubsystem* GetDayLevelSubsystem() const;

	UFUNCTION()
	void OnPrevMonthClicked();

	UFUNCTION()
	void OnNextMonthClicked();

	UFUNCTION()
	void OnBackClicked();

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UDaySlotWidget> DaySlotClass;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSoftClassPtr<UDaySlotWidget> DaySlotClassPath;

	UPROPERTY()
	TObjectPtr<UMainMenuWidget> ParentMenu;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DimOverlay;

	/** Outer warm frame around month + day grid. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> CalendarFrame;

	/** Larger semi-transparent panel behind the day cells. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> DayPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MonthText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PrevMonthButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> NextMonthButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> DayGrid;

	UPROPERTY()
	TArray<TObjectPtr<UDaySlotWidget>> DaySlots;

	/** 0 = uninitialized; JumpToTodayMonth / NativeConstruct set real month. */
	int32 CurrentMonth = 0;
	bool bBuiltInCode = false;
};
