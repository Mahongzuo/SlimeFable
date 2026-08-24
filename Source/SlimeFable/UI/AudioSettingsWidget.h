// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AudioSettingsWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class USlider;
class USlimeAudioSettings;

UCLASS()
class SLIMEFABLE_API UAudioSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void SetReturnTarget(UUserWidget* InTarget);
	void RefreshFromSettings();

protected:
	void BuildLayoutIfNeeded();
	void ApplyLook();
	void SyncLabels();
	USlimeAudioSettings* GetAudioSettings() const;

	UFUNCTION()
	void OnBackClicked();

	UFUNCTION()
	void OnMasterChanged(float Value);

	UFUNCTION()
	void OnMusicChanged(float Value);

	UFUNCTION()
	void OnSfxChanged(float Value);

	UPROPERTY()
	TWeakObjectPtr<UUserWidget> ReturnTarget;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DimOverlay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MasterLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> MasterSlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MusicLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> MusicSlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SfxLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> SfxSlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackButton;

	bool bBuiltInCode = false;
};
