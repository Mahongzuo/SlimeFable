// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DayLevel/DayLevelTypes.h"
#include "DaySlotWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UBorder;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDaySlotClicked, FName, DayId);

UCLASS()
class SLIMEFABLE_API UDaySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void SetDay(const FDayLevelEntry& Entry, bool bIsToday);

	UPROPERTY(BlueprintAssignable, Category = "Day Level")
	FOnDaySlotClicked OnDayClicked;

protected:
	void BuildLayoutIfNeeded();
	void ApplyVisuals();
	void ApplyHoverVisuals(bool bHovered);

	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SlotButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DayLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> SlotBackground;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SlotBorder;

	UPROPERTY()
	FName BoundDayId;

	int32 DayNumber = 0;
	bool bIsTodayCached = false;
	bool bHovered = false;
	bool bBuiltInCode = false;

public:
	static constexpr float SlotSize = 72.f;
};
