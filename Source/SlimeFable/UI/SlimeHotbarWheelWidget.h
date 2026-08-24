// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeHotbarWheelWidget.generated.h"

class UTextBlock;
class UImage;
class UWidget;

/**
 * Hold Tab: pick a hotbar slot with the mouse wheel; release to activate.
 * Never takes focus (same as the old element wheel).
 */
UCLASS()
class SLIMEFABLE_API USlimeHotbarWheelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USlimeHotbarWheelWidget(const FObjectInitializer& ObjectInitializer);

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "Slime|Hotbar")
	void SetHighlightedSlot(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category = "Slime|Hotbar")
	int32 GetHighlightedSlot() const { return HighlightedSlot; }

	void RefreshSlots();

protected:
	UPROPERTY(EditAnywhere, Category = "Slime|Wheel", meta = (ClampMin = "60.0"))
	float WheelRadius = 150.f;

	UPROPERTY(EditAnywhere, Category = "Slime|Wheel", meta = (ClampMin = "40.0"))
	float SectorSize = 92.f;

private:
	void BuildLayoutIfNeeded();

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

	int32 HighlightedSlot = 0;
	bool bBuiltInCode = false;
};
