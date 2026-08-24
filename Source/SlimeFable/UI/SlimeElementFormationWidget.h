// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeElementTypes.h"
#include "SlimeElementFormationWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UVerticalBox;
class UBorder;
class USlimeElementProgressSubsystem;

UCLASS()
class SLIMEFABLE_API USlimeElementFormationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

protected:
	void BuildLayoutIfNeeded();
	void ApplyLook();
	void RefreshRows();
	USlimeElementProgressSubsystem* GetProgress() const;
	void CloseSelf();
	int32 HitTestRow(const FVector2D& ScreenPos) const;

	UFUNCTION() void OnCloseClicked();

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> DimOverlay;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> HintText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UVerticalBox> RowBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CloseButton;

	UPROPERTY() TArray<TObjectPtr<UBorder>> RowBorders;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> RowLabels;

	int32 DragFromIndex = INDEX_NONE;
	bool bDragging = false;
	bool bBuiltInCode = false;
};
