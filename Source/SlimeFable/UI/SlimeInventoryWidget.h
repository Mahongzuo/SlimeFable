// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeItemTypes.h"
#include "SlimeInventoryWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UBorder;
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

	void HandleSlotClicked(FName ItemId);

protected:
	void BuildLayoutIfNeeded();
	void ApplyLook();
	void Refresh();
	void SelectCategory(ESlimeItemCategory Category);
	void SelectItem(FName ItemId);
	USlimeInventorySubsystem* GetInventory() const;
	void StyleSlotChrome(UBorder* Border, UImage* SlotBg, bool bSelected) const;
	void ApplyItemIcon(UImage* Image, const USlimeItemDefinition* Def, FVector2D Size) const;
	void RefreshHotbarAssign();
	void BuildHotbarAssignButtonsIfNeeded();

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

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> DimOverlay;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> PanelBorder;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CloseButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> TabConsumable;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> TabPlaceable;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> TabSouvenir;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UUniformGridPanel> ItemGrid;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> DetailIcon;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DetailName;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DetailDesc;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> PrimaryActionButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> DiscardButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> ActionRow;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> HotbarAssignRow;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> HotbarHint;

	ESlimeItemCategory ActiveCategory = ESlimeItemCategory::Consumable;
	FName SelectedItemId = NAME_None;
	bool bBuiltInCode = false;
	/** After a successful assign, highlight this hotbar cell once. */
	int32 FlashHotbarSlot = INDEX_NONE;

	static constexpr int32 GridColumns = 4;
	static constexpr int32 GridRows = 3;
	static constexpr float CellSize = 88.f;
	static constexpr float HotbarCellSize = 52.f;

	UPROPERTY()
	TArray<TObjectPtr<UObject>> SlotProxies;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> HotbarButtons;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> HotbarBorders;

	UPROPERTY()
	TArray<TObjectPtr<UImage>> HotbarIcons;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> HotbarIndexLabels;
};
