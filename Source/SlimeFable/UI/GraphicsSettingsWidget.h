// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GraphicsSettingsWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
UCLASS()
class SLIMEFABLE_API UGraphicsSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void SetReturnTarget(UUserWidget* InTarget);
	void RefreshSelection();

protected:
	void BuildLayoutIfNeeded();
	void ApplyLook();
	void ApplyQuality(int32 Level);

	UFUNCTION()
	void OnBackClicked();

	UFUNCTION()
	void OnQuality0Clicked();

	UFUNCTION()
	void OnQuality1Clicked();

	UFUNCTION()
	void OnQuality2Clicked();

	UFUNCTION()
	void OnQuality3Clicked();

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
	TObjectPtr<UButton> Quality0Button;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Quality1Button;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Quality2Button;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Quality3Button;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackButton;

	bool bBuiltInCode = false;
};
