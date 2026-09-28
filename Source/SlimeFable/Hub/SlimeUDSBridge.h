// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UObject;
class UWorld;

/**
 * Reflection access to Ultra Dynamic Sky / Weather blueprint actors.
 * Property and function names match this project's UDS assets:
 * Time of Day, Animate Time of Day, Day Length, Night Length,
 * Change Weather, Random Weather Variation (UDS_RandomWeatherTiming::Disabled = 3),
 * Current Rain, Current Snow, Thunder/Lightning.
 */
class SLIMEFABLE_API FSlimeUDSBridge
{
public:
	void Bind(UWorld* World);

	bool HasSky() const { return Sky.IsValid(); }
	bool HasWeather() const { return Weather.IsValid(); }

	/** Day Length / Night Length are real minutes from sunrise to sunset and back. */
	void ConfigureDayCycle(float DayLengthMinutes, float NightLengthMinutes);
	void DisableRandomWeather();
	bool ChangeWeather(UObject* Preset, float TransitionSeconds);

	double GetTimeOfDay() const;
	double GetDawnTime() const;
	double GetDuskTime() const;
	double GetRain() const;
	double GetSnow() const;
	double GetThunder() const;

private:
	static AActor* FindActorByClassName(UWorld* World, const TCHAR* ClassName);
	static bool SetNumeric(UObject* Object, const TCHAR* PropertyName, double Value);
	static bool SetBool(UObject* Object, const TCHAR* PropertyName, bool bValue);
	static bool SetEnumByte(UObject* Object, const TCHAR* PropertyName, uint8 Value);
	static bool ReadNumeric(const UObject* Object, const TCHAR* PropertyName, double& OutValue);

	TWeakObjectPtr<AActor> Sky;
	TWeakObjectPtr<AActor> Weather;
	bool bLoggedChangeWeather = false;
};
