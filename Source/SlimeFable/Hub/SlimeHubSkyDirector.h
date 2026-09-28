// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Hub/SlimeUDSBridge.h"
#include "SlimeHubSkyDirector.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlimeHubWeatherChanged, FName, WeatherTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlimeHubDayPhaseChanged, bool, bIsDaytime);

USTRUCT(BlueprintType)
struct FSlimeWeatherPoolEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Weather",
		meta = (ToolTip = "Ultra Dynamic Weather 的天气预设（Weather_Presets 里的数据资产）。"))
	TSoftObjectPtr<UObject> Preset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Weather",
		meta = (ClampMin = "0.0", ToolTip = "抽中权重。0 表示不参与。晴默认 3，雪默认 0.5。"))
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Weather",
		meta = (ToolTip = "给种田用的标签：Clear / Cloud / Rain / Thunder / Fog / Snow。"))
	FName Tag = NAME_None;
};

/**
 * Drives the museum's Ultra Dynamic Sky: 20 minute day/night, weather once per cycle.
 * Crops read this actor so the sky and the farm share one clock.
 */
UCLASS(Blueprintable, meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeHubSkyDirector : public AActor
{
	GENERATED_BODY()

public:
	ASlimeHubSkyDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Hub|Sky")
	float GetTimeOfDay01() const;

	UFUNCTION(BlueprintPure, Category = "Hub|Sky")
	bool IsDaytime() const;

	UFUNCTION(BlueprintPure, Category = "Hub|Sky")
	float GetRain01() const;

	UFUNCTION(BlueprintPure, Category = "Hub|Sky")
	float GetSnow01() const;

	UFUNCTION(BlueprintPure, Category = "Hub|Sky")
	float GetThunder01() const;

	UFUNCTION(BlueprintPure, Category = "Hub|Sky")
	FName GetCurrentWeatherTag() const { return CurrentWeatherTag; }

	UPROPERTY(BlueprintAssignable, Category = "Hub|Sky")
	FOnSlimeHubWeatherChanged OnWeatherChanged;

	UPROPERTY(BlueprintAssignable, Category = "Hub|Sky")
	FOnSlimeHubDayPhaseChanged OnDayPhaseChanged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Sky",
		meta = (ClampMin = "30.0", Units = "s",
			ToolTip = "一个完整昼夜的真实秒数。开局会写成 1200（20 分钟）。昼夜各占一半，写入 UDS 的 Day Length 和 Night Length。"))
	float DayCycleSeconds = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Weather",
		meta = (ClampMin = "10.0", Units = "s",
			ToolTip = "不再按秒抽天气。天气在绑定天空时抽一次，之后每个昼夜走完再抽一次。"))
	float WeatherIntervalSeconds = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Weather",
		meta = (ClampMin = "0.0", Units = "s",
			ToolTip = "UDS Change Weather 的过渡秒数。默认 15。"))
	float WeatherTransitionSeconds = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Weather",
		meta = (ToolTip = "勾选后不会连续两次抽到同一个预设。"))
	bool bAvoidRepeat = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Weather",
		meta = (ToolTip = "天气池。空着就用内置的晴/云/雨/雷/雾/小雪。"))
	TArray<FSlimeWeatherPoolEntry> WeatherPool;

protected:
	void EnsureDefaultPool();
	void ApplyTimeSettings();
	void TryBindSky();
	void RollWeather();
	void TryLightningStrike();
	bool ComputeIsDaytime(double TimeOfDay) const;

	FSlimeUDSBridge Bridge;
	FName CurrentWeatherTag = TEXT("Cloud");
	int32 LastPresetIndex = INDEX_NONE;
	float WeatherAccumulator = 0.f;
	float LightningAccumulator = 0.f;
	float SkyBindRetry = 0.f;
	bool bLoggedSkyBind = false;
	bool bLoggedMissingSky = false;
	double LocalTimeOfDay = 900.0;
	double DawnTime = 600.0;
	double DuskTime = 1800.0;
	bool bPhaseReady = false;
	bool bWasDaytime = true;
};
