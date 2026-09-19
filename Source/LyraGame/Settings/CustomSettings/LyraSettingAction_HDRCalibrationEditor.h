// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameSettingAction.h"
#include "GameSettingValueScalarDynamic.h"

#include "LyraSettingAction_HDRCalibrationEditor.generated.h"

class UGameSetting;

UCLASS()
class ULyraSettingAction_HDRCalibrationEditor : public UGameSettingAction
{
	GENERATED_BODY()
	
public:
	ULyraSettingAction_HDRCalibrationEditor();
	virtual TArray<UGameSetting*> GetChildSettings() override;

private:
	UPROPERTY()
	TObjectPtr<UGameSettingValueScalarDynamic> HDRCalibrationValueSetting;
};
