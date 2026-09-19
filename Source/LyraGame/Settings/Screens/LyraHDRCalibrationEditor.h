// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "Widgets/IGameSettingActionInterface.h"

#include "LyraHDRCalibrationEditor.generated.h"

enum class ECommonInputType : uint8;

class UCommonButtonBase;
class UCommonRichTextBlock;
class UGameSettingValueScalar;
class UImage;
class UWidgetSwitcher;

UCLASS(Abstract)
class ULyraHDRCalibrationEditor : public UCommonActivatableWidget, public IGameSettingActionInterface
{
	GENERATED_BODY()
	
public:
	ULyraHDRCalibrationEditor(const FObjectInitializer& Initializer);

	// Begin IGameSettingActionInterface
	virtual bool ExecuteActionForSetting_Implementation(FGameplayTag ActionTag, UGameSetting* InSetting) override;
	// End IGameSettingActionInterface

	UFUNCTION(BlueprintImplementableEvent)
	void OnMaxLuminanceChange(float InMaxLuminance);

protected:

	UPROPERTY(EditAnywhere, Category = "Restrictions")
	bool bCanCancel = true;

	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual FReply NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleDoneClicked();

	float bStartingUILevel;
	float bStartingUILuminance;
	bool bStartingUILuminanceSeparate;
	float MaxLuminancePQ;

	TWeakObjectPtr<UGameSettingValueScalar> ValueSetting;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UImage> Checkerboard;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UCommonButtonBase> Button_Back;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UCommonButtonBase> Button_Done;
};
