// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeDevourComponent.h"
#include "SlimePhantomWheelWidget.generated.h"

class UCanvasPanel;
class UImage;
class UTextBlock;

UCLASS()
class SLIMEFABLE_API USlimePhantomWheelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USlimePhantomWheelWidget(const FObjectInitializer& ObjectInitializer);

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	void SetSlots(const TArray<FSlimeDevourCapture>& Slots, int32 SelectedIndex, int32 Capacity);

protected:
	UPROPERTY(EditAnywhere, Category = "Slime|Wheel", meta = (ClampMin = "60.0"))
	float WheelRadius = 150.f;

	UPROPERTY(EditAnywhere, Category = "Slime|Wheel", meta = (ClampMin = "40.0"))
	float SectorSize = 92.f;

private:
	void BuildLayoutIfNeeded();
	void RefreshSectors();

	TArray<FSlimeDevourCapture> CachedSlots;
	int32 CachedSelected = 0;
	int32 CachedCapacity = 6;

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

	bool bBuiltInCode = false;
};
