// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameSettingValueDiscrete.h"

#include "LyraSettingValueDiscrete_Display.generated.h"

class ULyraSetting;
class UObject;

UCLASS()
class ULyraSettingValueDiscrete_Display : public UGameSettingValueDiscrete
{
	GENERATED_BODY()
	
public:

	ULyraSettingValueDiscrete_Display();

	virtual void BeginDestroy() override;

	/** UGameSettingValue */
	virtual void StoreInitial() override;
	virtual void ResetToDefault() override;
	virtual void RestoreToInitial() override;

	/** UGameSettingValueDiscrete */
	virtual void SetDiscreteOptionByIndex(int32 Index) override;
	virtual int32 GetDiscreteOptionIndex() const override;
	virtual int32 GetDiscreteOptionDefaultIndex() const override;
	virtual TArray<FText> GetDiscreteOptions() const override;

protected:
	/** UGameSettingValue */
	virtual void OnInitialized() override;
	virtual void OnDependencyChanged() override;

	void OnDisplayMetricsChanged(const FDisplayMetrics& NewDisplayMetrics);

	FString InitialMonitorID;
	int32 InitialMonitorIndex = -1;

	FDisplayMetrics CurrentDisplayMetrics;
	FDelegateHandle DisplayMetricsChangedHandle;
};
