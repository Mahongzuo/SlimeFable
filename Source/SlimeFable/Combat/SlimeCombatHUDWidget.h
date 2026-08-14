// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeCombatHUDWidget.generated.h"

class USlimeCombatComponent;
class UProgressBar;
class UTextBlock;
class UImage;
class UCanvasPanel;
class UButton;

UCLASS()
class SLIMEFABLE_API USlimeCombatHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USlimeCombatHUDWidget(const FObjectInitializer& ObjectInitializer);

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void SetCombat(USlimeCombatComponent* InCombat);

protected:
	void BuildLayoutIfNeeded();
	void Refresh();

	UFUNCTION()
	void HandleUnstuckClicked();

	UPROPERTY(Transient)
	TObjectPtr<USlimeCombatComponent> Combat;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ComboText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> SlotKeys;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> SlotNames;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> SlotCds;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> SlotBackgrounds;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ResonanceBar;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> UltimateBar;

	UPROPERTY(Transient)
	TObjectPtr<UButton> UnstuckButton;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> HotbarLabels;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InteractPrompt;

	bool bBuiltInCode = false;
};
