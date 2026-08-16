// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeItemTypes.h"
#include "SlimeInventoryWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UScrollBox;
class UVerticalBox;
class UHorizontalBox;
class UUniformGridPanel;
class USlimeInventorySubsystem;
class USlimeItemDefinition;

UCLASS()
class SLIMEFABLE_API USlimeInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USlimeInventoryWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

protected:
	void BuildLayoutIfNeeded();
	void ApplyLook();
	void Refresh();
	void SelectCategory(ESlimeItemCategory Category);
	void SelectItem(FName ItemId);
	USlimeInventorySubsystem* GetInventory() const;

	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void OnTabConsumable();
	UFUNCTION() void OnTabPlaceable();
	UFUNCTION() void OnTabSouvenir();
	UFUNCTION() void OnPrimaryActionClicked();
	UFUNCTION() void OnDiscardClicked();
	void EnsureDiscardButton();
	UFUNCTION() void OnHotbar1();
	UFUNCTION() void OnHotbar2();
	UFUNCTION() void OnHotbar3();
	UFUNCTION() void OnHotbar4();
	UFUNCTION() void OnHotbar5();
	UFUNCTION() void OnHotbar6();

	void AssignSelectedToHotbar(int32 SlotIndex);

public:
	void HandleSlotClicked(FName ItemId);

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> DimOverlay;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CloseButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> TabConsumable;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> TabPlaceable;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> TabSouvenir;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUniformGridPanel> ItemGrid;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DetailName;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DetailDesc;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> PrimaryActionButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> DiscardButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> ActionRow;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> HotbarAssignRow;

	ESlimeItemCategory ActiveCategory = ESlimeItemCategory::Consumable;
	FName SelectedItemId = NAME_None;
	bool bBuiltInCode = false;

	UPROPERTY()
	TArray<TObjectPtr<UObject>> SlotProxies;
};
