// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Settings/SlimeInputTypes.h"
#include "SlimeTouchHUDWidget.generated.h"

class UButton;
class UCanvasPanel;
class UImage;
class UTextBlock;
class USlimeInputSettings;
class USlimeTouchHUDWidget;

UCLASS()
class SLIMEFABLE_API USlimeTouchActionProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	ESlimeInputAction Action = ESlimeInputAction::Attack;

	UPROPERTY()
	TObjectPtr<USlimeTouchHUDWidget> Owner;

	UFUNCTION()
	void HandlePressed();

	UFUNCTION()
	void HandleReleased();
};

UCLASS()
class SLIMEFABLE_API USlimeTouchHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

	void SetVirtualAction(ESlimeInputAction Action, bool bDown);
	void ApplyHandedness();

protected:
	void BuildLayoutIfNeeded();
	void ApplyLook();
	void ApplyClusterLayout();
	void PlaceActionButton(UButton* Button, float AnchorX, FVector2D Pos, FVector2D Size);
	USlimeInputSettings* GetInputSettings() const;
	UButton* AddRoundButton(UCanvasPanel* Root, const FName& Name, const FText& Label,
		FVector2D Position, FVector2D Size, ESlimeInputAction Action, bool bHidden = false);
	void SetButtonVisible(UButton* Button, bool bVisible);
	bool IsOverInteractiveControl(FVector2D ScreenPos) const;
	UButton* FindActionButtonAt(FVector2D ScreenPos) const;
	ESlimeInputAction ActionForButton(const UButton* Button) const;
	bool TryBeginVirtualAction(FVector2D ScreenPos, int32 PointerIndex);
	void PlaceStickVisuals();
	bool HandlePointerDown(const FGeometry& InGeometry, FVector2D ScreenPos, int32 PointerIndex);
	bool HandlePointerMove(const FGeometry& InGeometry, FVector2D ScreenPos, int32 PointerIndex, FVector2D CursorDelta);
	void HandlePointerUp(int32 PointerIndex);
	void RefreshTouchPointerBusy();
	void RefreshContextualButtons();

	UFUNCTION()
	void OnPauseClicked();

	UFUNCTION()
	void OnQuestClicked();

	UFUNCTION()
	void OnInventoryClicked();

	UPROPERTY()
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY()
	TObjectPtr<UImage> StickBase;

	UPROPERTY()
	TObjectPtr<UImage> StickKnob;

	UPROPERTY()
	TObjectPtr<UButton> PauseButton;

	UPROPERTY()
	TObjectPtr<UButton> QuestButton;

	UPROPERTY()
	TObjectPtr<UButton> InventoryButton;

	UPROPERTY()
	TObjectPtr<UButton> AttackButton;

	UPROPERTY()
	TObjectPtr<UButton> JumpButton;

	UPROPERTY()
	TObjectPtr<UButton> DodgeButton;

	UPROPERTY()
	TObjectPtr<UButton> FlattenButton;

	UPROPERTY()
	TObjectPtr<UButton> SprintButton;

	UPROPERTY()
	TObjectPtr<UButton> LaunchButton;

	UPROPERTY()
	TObjectPtr<UButton> InteractButton;

	UPROPERTY()
	TObjectPtr<UButton> AbsorbButton;

	UPROPERTY()
	TObjectPtr<UButton> MorphButton;

	UPROPERTY()
	TObjectPtr<UButton> LockOnButton;

	UPROPERTY()
	TObjectPtr<UButton> Skill1Button;

	UPROPERTY()
	TObjectPtr<UButton> Skill2Button;

	UPROPERTY()
	TObjectPtr<UButton> Skill3Button;

	UPROPERTY()
	TArray<TObjectPtr<USlimeTouchActionProxy>> ActionProxies;

	FVector2D HomeStickLocal = FVector2D(160.f, -200.f);
	FVector2D StickCenterLocal = FVector2D(160.f, -200.f);
	float StickRadius = 90.f;
	int32 StickPointer = INDEX_NONE;
	int32 LookPointer = INDEX_NONE;
	int32 ActionPointer = INDEX_NONE;
	ESlimeInputAction HeldPointerAction = ESlimeInputAction::Jump;
	FVector2D LastLookScreen = FVector2D::ZeroVector;
	bool bBuiltInCode = false;
};
