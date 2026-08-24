// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeHotbarConfirmWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class USlimeAbilityComponent;

UCLASS()
class SLIMEFABLE_API USlimeHotbarConfirmWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	void Setup(int32 InSlotIndex, FName InItemId, const FText& InDisplayName);

protected:
	void BuildLayoutIfNeeded();
	void ApplyLook();
	void CloseSelf();

	UFUNCTION() void OnUseClicked();
	UFUNCTION() void OnDiscardClicked();
	UFUNCTION() void OnCancelClicked();

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> DimOverlay;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> UseButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> DiscardButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CancelButton;

	int32 SlotIndex = INDEX_NONE;
	FName ItemId = NAME_None;
	bool bBuiltInCode = false;
};
