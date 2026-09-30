// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/StreamableManager.h"
#include "Farm/SlimeCropTypes.h"
#include "SlimeSeedPickerWidget.generated.h"

class ASlimeFarmPlot;
class UBorder;
class UButton;
class UHorizontalBox;
class UImage;
class UScrollBox;
class UTextBlock;
class UUniformGridPanel;

UCLASS()
class SLIMEFABLE_API USlimeSeedCardProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FName CropId;

	UPROPERTY()
	TObjectPtr<USlimeSeedPickerWidget> Owner;

	UFUNCTION()
	void HandleClick();
};

UCLASS()
class SLIMEFABLE_API USlimeSeedPickerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USlimeSeedPickerWidget(const FObjectInitializer& ObjectInitializer);

	static void OpenForPlot(ASlimeFarmPlot* Plot);
	static bool IsOpen();
	static bool CloseOpen();

	void SelectCrop(FName CropId, bool bPlantIfRepeat);

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void BuildLayout();
	void ApplyLook();
	void RebuildCards();
	void RefreshSelection();
	void RefreshDetail();
	void ApplyLoadedIcons();
	void Close();
	void Confirm();

	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void OnPlantClicked();
	UFUNCTION() void OnTabAll();
	UFUNCTION() void OnTabVegetable();
	UFUNCTION() void OnTabFruit();
	UFUNCTION() void OnTabGrain();
	UFUNCTION() void OnTabFlower();
	UFUNCTION() void OnTabCash();

	void SelectCategory(int32 CategoryIndex);

	UPROPERTY()
	TWeakObjectPtr<ASlimeFarmPlot> Plot;

	UPROPERTY()
	TArray<TObjectPtr<USlimeSeedCardProxy>> CardProxies;

	UPROPERTY()
	TObjectPtr<UImage> DimOverlay;

	UPROPERTY()
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> TabRow;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> TabButtons;

	UPROPERTY()
	TObjectPtr<UScrollBox> Scroll;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> CardGrid;

	UPROPERTY()
	TObjectPtr<UTextBlock> DetailText;

	UPROPERTY()
	TObjectPtr<UButton> PlantButton;

	UPROPERTY()
	TObjectPtr<UButton> CloseButton;

	UPROPERTY()
	TArray<FName> CardCropIds;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> CardNames;

	UPROPERTY()
	TArray<TObjectPtr<UImage>> CardIcons;

	TSharedPtr<FStreamableHandle> IconHandle;

	FName SelectedCropId;
	int32 CategoryFilter = INDEX_NONE;
	double LastClickSeconds = 0.0;
	FName LastClickCrop;

	static TWeakObjectPtr<USlimeSeedPickerWidget> OpenWidget;
};
