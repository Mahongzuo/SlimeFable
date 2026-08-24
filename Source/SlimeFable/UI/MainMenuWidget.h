// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class ULevelSelectWidget;
class UKeybindSettingsWidget;
class UGraphicsSettingsWidget;
class UAudioSettingsWidget;
class UTutorialMenuWidget;
class UDayLevelSubsystem;
class USoundBase;
class UAudioComponent;

UCLASS()
class SLIMEFABLE_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMainMenuWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Show the level-select calendar overlay (also used when returning from a day level). */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OpenLevelSelect();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OpenKeybindSettings();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OpenGraphicsSettings();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OpenAudioSettings();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OpenTutorialMenu();

protected:
	void BuildLayoutIfNeeded();
	void ApplyMaterialLabLook();
	void RefreshTodayInfo();
	void ResolveLevelSelectClass();
	void ResolveKeybindClass();
	void ResolveGraphicsClass();
	void ResolveAudioClass();
	void ResolveTutorialClass();
	void StartMenuMusic();
	void StopMenuMusic();
	void RefreshMenuMusicVolume();
	UFUNCTION()
	void HandleAudioVolumesChanged();
	UDayLevelSubsystem* GetDayLevelSubsystem() const;

	UFUNCTION()
	void OnPlayTodayClicked();

	UFUNCTION()
	void OnSelectLevelClicked();

	UFUNCTION()
	void OnKeybindClicked();

	UFUNCTION()
	void OnGraphicsClicked();

	UFUNCTION()
	void OnAudioClicked();

	UFUNCTION()
	void OnTutorialClicked();

	UFUNCTION()
	void OnQuitClicked();

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<ULevelSelectWidget> LevelSelectClass;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSoftClassPtr<ULevelSelectWidget> LevelSelectClassPath;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UKeybindSettingsWidget> KeybindSettingsClass;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSoftClassPtr<UKeybindSettingsWidget> KeybindSettingsClassPath;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UGraphicsSettingsWidget> GraphicsSettingsClass;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSoftClassPtr<UGraphicsSettingsWidget> GraphicsSettingsClassPath;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UAudioSettingsWidget> AudioSettingsClass;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSoftClassPtr<UAudioSettingsWidget> AudioSettingsClassPath;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UTutorialMenuWidget> TutorialMenuClass;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSoftClassPtr<UTutorialMenuWidget> TutorialMenuClassPath;

	UPROPERTY(EditAnywhere, Category = "0_Config|Audio",
		meta = (ToolTip = "主菜单循环 BGM。空则 /Game/Audio/BGM/bgm_global_menu。"))
	TSoftObjectPtr<USoundBase> MenuMusic;

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
	TObjectPtr<UButton> KeybindButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> GraphicsButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> AudioButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TutorialButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuitButton;

	UPROPERTY()
	TObjectPtr<ULevelSelectWidget> LevelSelectWidget;

	UPROPERTY()
	TObjectPtr<UKeybindSettingsWidget> KeybindSettingsWidget;

	UPROPERTY()
	TObjectPtr<UGraphicsSettingsWidget> GraphicsSettingsWidget;

	UPROPERTY()
	TObjectPtr<UAudioSettingsWidget> AudioSettingsWidget;

	UPROPERTY()
	TObjectPtr<UTutorialMenuWidget> TutorialMenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MenuMusicComponent;

	bool bBuiltInCode = false;
};
