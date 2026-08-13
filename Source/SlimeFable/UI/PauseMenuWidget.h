// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UKeybindSettingsWidget;
class UGraphicsSettingsWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPauseMenuContinue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPauseMenuLevelSelect);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPauseMenuMainMenu);

UCLASS()
class SLIMEFABLE_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPauseMenuWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Close settings overlays first; returns true if Esc was consumed. */
	bool TryHandleEscape();

	UPROPERTY(BlueprintAssignable, Category = "UI")
	FOnPauseMenuContinue OnContinueRequested;

	UPROPERTY(BlueprintAssignable, Category = "UI")
	FOnPauseMenuLevelSelect OnLevelSelectRequested;

	UPROPERTY(BlueprintAssignable, Category = "UI")
	FOnPauseMenuMainMenu OnMainMenuRequested;

protected:
	void BuildLayoutIfNeeded();
	void ApplyLook();
	void ResolveSettingsClasses();
	void OpenKeybindSettings();
	void OpenGraphicsSettings();

	UFUNCTION()
	void OnContinueClicked();

	UFUNCTION()
	void OnLevelSelectClicked();

	UFUNCTION()
	void OnKeybindClicked();

	UFUNCTION()
	void OnGraphicsClicked();

	UFUNCTION()
	void OnMainMenuClicked();

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UKeybindSettingsWidget> KeybindSettingsClass;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSoftClassPtr<UKeybindSettingsWidget> KeybindSettingsClassPath;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UGraphicsSettingsWidget> GraphicsSettingsClass;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSoftClassPtr<UGraphicsSettingsWidget> GraphicsSettingsClassPath;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DimOverlay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ContinueButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LevelSelectButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> KeybindButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> GraphicsButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MainMenuButton;

	UPROPERTY()
	TObjectPtr<UKeybindSettingsWidget> KeybindSettingsWidget;

	UPROPERTY()
	TObjectPtr<UGraphicsSettingsWidget> GraphicsSettingsWidget;

	bool bBuiltInCode = false;
};
