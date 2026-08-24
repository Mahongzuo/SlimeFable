// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TutorialMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UScrollBox;

UCLASS()
class SLIMEFABLE_API UTutorialMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void SetReturnTarget(UUserWidget* InTarget);

protected:
	void BuildLayoutIfNeeded();
	void ApplyLook();

	UFUNCTION()
	void OnBackClicked();

	UPROPERTY()
	TWeakObjectPtr<UUserWidget> ReturnTarget;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DimOverlay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> BodyScroll;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BodyText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackButton;

	bool bBuiltInCode = false;
};
