// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPauseMenuContinue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPauseMenuLevelSelect);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPauseMenuMainMenu);

UCLASS()
class SLIMEFABLE_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintAssignable, Category = "UI")
	FOnPauseMenuContinue OnContinueRequested;

	UPROPERTY(BlueprintAssignable, Category = "UI")
	FOnPauseMenuLevelSelect OnLevelSelectRequested;

	UPROPERTY(BlueprintAssignable, Category = "UI")
	FOnPauseMenuMainMenu OnMainMenuRequested;

protected:
	void BuildLayoutIfNeeded();
	void ApplyLook();

	UFUNCTION()
	void OnContinueClicked();

	UFUNCTION()
	void OnLevelSelectClicked();

	UFUNCTION()
	void OnMainMenuClicked();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DimOverlay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ContinueButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LevelSelectButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MainMenuButton;

	bool bBuiltInCode = false;
};
