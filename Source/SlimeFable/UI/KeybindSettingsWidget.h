// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Settings/SlimeInputTypes.h"
#include "KeybindSettingsWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UScrollBox;
class UVerticalBox;
class USlimeInputSettings;
class UKeybindSettingsWidget;

UCLASS()
class SLIMEFABLE_API USlimeKeybindRowProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	ESlimeInputAction Action = ESlimeInputAction::Jump;

	UPROPERTY()
	TObjectPtr<UKeybindSettingsWidget> Owner;

	UFUNCTION()
	void HandleClicked();
};

UCLASS()
class SLIMEFABLE_API UKeybindSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	void SetReturnTarget(UUserWidget* InTarget);
	void RefreshList();
	void BeginRebind(ESlimeInputAction Action);

protected:
	void BuildLayoutIfNeeded();
	void ApplyLook();
	USlimeInputSettings* GetInputSettings() const;
	bool CaptureKey(FKey Key);

	UFUNCTION()
	void OnBackClicked();

	UFUNCTION()
	void OnResetClicked();

	UPROPERTY()
	TWeakObjectPtr<UUserWidget> ReturnTarget;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DimOverlay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> BindScroll;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> BindList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ResetButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackButton;

	UPROPERTY()
	TArray<TObjectPtr<USlimeKeybindRowProxy>> RowProxies;

	UPROPERTY()
	TMap<ESlimeInputAction, TObjectPtr<UTextBlock>> ActionKeyLabels;

	TOptional<ESlimeInputAction> ListeningAction;
	bool bBuiltInCode = false;
};
