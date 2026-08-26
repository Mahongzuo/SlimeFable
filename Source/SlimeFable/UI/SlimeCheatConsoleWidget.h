// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeCheatConsoleWidget.generated.h"

class UEditableText;
class UTextBlock;
class UBorder;
class USizeBox;
class USlimeCheatComponent;

UCLASS()
class SLIMEFABLE_API USlimeCheatConsoleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void Setup(USlimeCheatComponent* InOwner);
	void ShowInputBar(bool bShow);
	void ShowToast(const FString& Message, float Duration = 2.f);
	bool IsToastVisible() const { return ToastRemaining > 0.f; }
	void FocusInput();
	void SetIgnoreCommitUntil(float WorldTimeSeconds);
	/** Keyboard Enter fallback when EditableText focus is flaky. */
	void TrySubmitFromEnterKey();

protected:
	void BuildLayoutIfNeeded();
	void ApplyLook();
	void SubmitCommand();
	bool ShouldIgnoreCommit() const;

	UFUNCTION()
	void OnCommandCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UPROPERTY()
	TObjectPtr<USizeBox> InputSize = nullptr;

	UPROPERTY()
	TObjectPtr<UBorder> InputPanel = nullptr;

	UPROPERTY()
	TObjectPtr<UEditableText> CommandInput = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> PromptText = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> ToastText = nullptr;

	UPROPERTY()
	TObjectPtr<UBorder> ToastPanel = nullptr;

	UPROPERTY()
	TWeakObjectPtr<USlimeCheatComponent> OwnerCheat;

	float ToastRemaining = 0.f;
	float IgnoreCommitUntil = 0.f;
	bool bWantFocus = false;
};
