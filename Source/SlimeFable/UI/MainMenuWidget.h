// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class ULevelSelectWidget;
class UDayLevelSubsystem;

UCLASS()
class SLIMEFABLE_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMainMenuWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Show the level-select calendar overlay (also used when returning from a day level). */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OpenLevelSelect();

protected:
	void BuildLayoutIfNeeded();
	void ApplyMaterialLabLook();
	void RefreshTodayInfo();
	void ResolveLevelSelectClass();
	UDayLevelSubsystem* GetDayLevelSubsystem() const;

	UFUNCTION()
	void OnPlayTodayClicked();

	UFUNCTION()
	void OnSelectLevelClicked();

	UFUNCTION()
	void OnQuitClicked();

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<ULevelSelectWidget> LevelSelectClass;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSoftClassPtr<ULevelSelectWidget> LevelSelectClassPath;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DimOverlay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TodayText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PlayTodayButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SelectLevelButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuitButton;

	UPROPERTY()
	TObjectPtr<ULevelSelectWidget> LevelSelectWidget;

	bool bBuiltInCode = false;
};
