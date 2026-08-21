// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeCombatHUDWidget.generated.h"

class AActor;
class USlimeCombatComponent;
class UProgressBar;
class UTextBlock;
class UImage;
class UCanvasPanel;
class UButton;
class UBorder;
class UOverlay;
class UMaterialInstanceDynamic;

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
	void SetDeathVisible(bool bVisible);

protected:
	void BuildLayoutIfNeeded();
	void Refresh();
	void RefreshLockOnBar(float DeltaTime);
	void ApplyProgressBarFill(UProgressBar* Bar, const FLinearColor& Fill);
	FLinearColor GetSlimeHudTint() const;

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
	TArray<TObjectPtr<UTextBlock>> SlotCdTexts;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PlayerHealthBar;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PlayerHealthBarMID;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DeathText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WeekText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> SlotBackgrounds;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ResonanceBar;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> UltimateBar;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> LaunchChargeBar;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> LaunchChargeTrack;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> DevourHoldBar;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> DevourHoldTrack;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> DigestBar;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> DigestTrack;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PhantomCountText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Skill1ChargeBar;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Skill1ChargeTrack;

	UPROPERTY(Transient)
	TObjectPtr<UButton> UnstuckButton;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> HotbarLabels;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InteractPrompt;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> LockOnPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LockOnName;

	UPROPERTY(Transient)
	TObjectPtr<UImage> LockOnBar;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LockOnBarMID;

	TWeakObjectPtr<AActor> LastLockTarget;
	float LockOnHealthPercent = 1.f;
	float LockOnGhostPercent = 1.f;
	float LockOnGhostDelay = 0.f;
	float LockOnFlash = 0.f;

	bool bBuiltInCode = false;
};
