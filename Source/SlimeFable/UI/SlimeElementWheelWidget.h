// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeElementTypes.h"
#include "SlimeElementWheelWidget.generated.h"

class UCanvasPanel;
class UImage;
class UTextBlock;
class USlimeElementComponent;

/**
 *  Six sector element wheel, held open with Tab and stepped with the mouse wheel.
 *
 *  Deliberately never takes focus and never shows a cursor: selection comes from discrete
 *  scroll steps rather than hover, so the camera keeps working and no keys get eaten by the UI.
 *
 *  Builds its own layout when no Blueprint shell binds the widgets, matching UMainMenuWidget.
 */
UCLASS()
class SLIMEFABLE_API USlimeElementWheelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USlimeElementWheelWidget(const FObjectInitializer& ObjectInitializer);

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	/** Source of the element palette. Safe to call before the widget tree exists. */
	void SetElementComponent(USlimeElementComponent* InElementComponent);

	/** Moves the highlight. Cached until construction if the tree is not built yet. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Wheel")
	void SetHighlightedElement(ESlimeElement Element);

	UFUNCTION(BlueprintPure, Category = "Slime|Wheel")
	ESlimeElement GetHighlightedElement() const { return HighlightedElement; }

protected:
	/** Radius of the sector ring in slate units. */
	UPROPERTY(EditAnywhere, Category = "Slime|Wheel", meta = (ClampMin = "60.0"))
	float WheelRadius = 150.f;

	UPROPERTY(EditAnywhere, Category = "Slime|Wheel", meta = (ClampMin = "40.0"))
	float SectorSize = 92.f;

private:
	void BuildLayoutIfNeeded();
	void RefreshSectors();
	FSlimeElementProfile GetProfile(ESlimeElement Element) const;

	UPROPERTY(Transient)
	TObjectPtr<USlimeElementComponent> ElementComponent;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CenterLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HintLabel;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> SectorRoots;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> SectorImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> SectorNames;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> SectorTags;

	ESlimeElement HighlightedElement = ESlimeElement::Water;
	bool bBuiltInCode = false;
};
