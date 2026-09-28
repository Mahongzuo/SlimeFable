// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hub/SlimeHubSkyDirector.h"

#include "Farm/SlimeFarmPlot.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "SlimeFable.h"

ASlimeHubSkyDirector::ASlimeHubSkyDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	EnsureDefaultPool();
}

void ASlimeHubSkyDirector::EnsureDefaultPool()
{
	if (WeatherPool.Num() > 0)
	{
		return;
	}

	auto Add = [this](const TCHAR* Path, float Weight, const TCHAR* Tag)
	{
		FSlimeWeatherPoolEntry Entry;
		Entry.Preset = TSoftObjectPtr<UObject>(FSoftObjectPath(Path));
		Entry.Weight = Weight;
		Entry.Tag = FName(Tag);
		WeatherPool.Add(Entry);
	};

	Add(TEXT("/Game/UltraDynamicSky/Blueprints/Weather_Effects/Weather_Presets/Clear_Skies.Clear_Skies"), 3.f, TEXT("Clear"));
	Add(TEXT("/Game/UltraDynamicSky/Blueprints/Weather_Effects/Weather_Presets/Partly_Cloudy.Partly_Cloudy"), 2.f, TEXT("Cloud"));
	Add(TEXT("/Game/UltraDynamicSky/Blueprints/Weather_Effects/Weather_Presets/Cloudy.Cloudy"), 2.f, TEXT("Cloud"));
	Add(TEXT("/Game/UltraDynamicSky/Blueprints/Weather_Effects/Weather_Presets/Rain.Rain"), 2.f, TEXT("Rain"));
	Add(TEXT("/Game/UltraDynamicSky/Blueprints/Weather_Effects/Weather_Presets/Rain_Light.Rain_Light"), 1.f, TEXT("Rain"));
	Add(TEXT("/Game/UltraDynamicSky/Blueprints/Weather_Effects/Weather_Presets/Rain_Thunderstorm.Rain_Thunderstorm"), 1.f, TEXT("Thunder"));
	Add(TEXT("/Game/UltraDynamicSky/Blueprints/Weather_Effects/Weather_Presets/Foggy.Foggy"), 1.f, TEXT("Fog"));
	Add(TEXT("/Game/UltraDynamicSky/Blueprints/Weather_Effects/Weather_Presets/Snow_Light.Snow_Light"), 0.5f, TEXT("Snow"));
}

void ASlimeHubSkyDirector::BeginPlay()
{
	Super::BeginPlay();
	DayCycleSeconds = 1200.f;
	EnsureDefaultPool();
	TryBindSky();
}

void ASlimeHubSkyDirector::TryBindSky()
{
	if (Bridge.HasSky() || !GetWorld())
	{
		return;
	}
	Bridge.Bind(GetWorld());
	if (Bridge.HasSky())
	{
		ApplyTimeSettings();
		Bridge.DisableRandomWeather();
		RollWeather();
		if (!bLoggedSkyBind)
		{
			bLoggedSkyBind = true;
			UE_LOG(LogSlimeFable, Log, TEXT("Hub sky: bound Ultra Dynamic Sky"));
		}
	}
	else if (!bLoggedMissingSky)
	{
		bLoggedMissingSky = true;
		UE_LOG(LogSlimeFable, Warning, TEXT("Hub sky: Ultra Dynamic Sky actor not found; HUD uses a local clock"));
	}
}

void ASlimeHubSkyDirector::ApplyTimeSettings()
{
	const float HalfMinutes = FMath::Max(DayCycleSeconds, 30.f) / 120.f;
	Bridge.ConfigureDayCycle(HalfMinutes, HalfMinutes);
	if (Bridge.HasSky())
	{
		LocalTimeOfDay = Bridge.GetTimeOfDay();
		DawnTime = Bridge.GetDawnTime();
		DuskTime = Bridge.GetDuskTime();
	}
}

void ASlimeHubSkyDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!GetWorld() || GetWorld()->IsPaused())
	{
		return;
	}

	if (!Bridge.HasSky())
	{
		SkyBindRetry += DeltaSeconds;
		if (SkyBindRetry >= 1.f)
		{
			SkyBindRetry = 0.f;
			TryBindSky();
		}
	}

	const double PreviousTime = LocalTimeOfDay;
	if (Bridge.HasSky())
	{
		LocalTimeOfDay = Bridge.GetTimeOfDay();
	}
	else
	{
		LocalTimeOfDay += (2400.0 / FMath::Max(DayCycleSeconds, 30.f)) * DeltaSeconds;
		if (LocalTimeOfDay >= 2400.0)
		{
			LocalTimeOfDay -= 2400.0;
		}
	}
	const bool bDayWrapped = bPhaseReady && (LocalTimeOfDay + 200.0 < PreviousTime);

	const bool bDay = ComputeIsDaytime(LocalTimeOfDay);
	if (!bPhaseReady)
	{
		bWasDaytime = bDay;
		bPhaseReady = true;
	}
	else if (bDay != bWasDaytime)
	{
		bWasDaytime = bDay;
		OnDayPhaseChanged.Broadcast(bDay);
	}

	if (bDayWrapped)
	{
		RollWeather();
	}

	TryLightningStrike();
}

float ASlimeHubSkyDirector::GetTimeOfDay01() const
{
	return FMath::Fmod(static_cast<float>(LocalTimeOfDay), 2400.f) / 2400.f;
}

bool ASlimeHubSkyDirector::ComputeIsDaytime(double TimeOfDay) const
{
	const double Time = FMath::Fmod(TimeOfDay, 2400.0);
	if (DawnTime <= DuskTime)
	{
		return Time >= DawnTime && Time < DuskTime;
	}
	return Time >= DawnTime || Time < DuskTime;
}

bool ASlimeHubSkyDirector::IsDaytime() const
{
	return ComputeIsDaytime(LocalTimeOfDay);
}

float ASlimeHubSkyDirector::GetRain01() const
{
	float FromTag = 0.f;
	if (CurrentWeatherTag == TEXT("Rain"))
	{
		FromTag = 0.75f;
	}
	else if (CurrentWeatherTag == TEXT("Thunder"))
	{
		FromTag = 0.95f;
	}
	return FMath::Clamp(FMath::Max(static_cast<float>(Bridge.GetRain()), FromTag), 0.f, 1.f);
}

float ASlimeHubSkyDirector::GetSnow01() const
{
	const float FromTag = CurrentWeatherTag == TEXT("Snow") ? 0.8f : 0.f;
	return FMath::Clamp(FMath::Max(static_cast<float>(Bridge.GetSnow()), FromTag), 0.f, 1.f);
}

float ASlimeHubSkyDirector::GetThunder01() const
{
	const float FromTag = CurrentWeatherTag == TEXT("Thunder") ? 1.f : 0.f;
	return FMath::Clamp(FMath::Max(static_cast<float>(Bridge.GetThunder()), FromTag), 0.f, 1.f);
}

void ASlimeHubSkyDirector::RollWeather()
{
	float Total = 0.f;
	for (int32 Index = 0; Index < WeatherPool.Num(); ++Index)
	{
		if (bAvoidRepeat && Index == LastPresetIndex)
		{
			continue;
		}
		Total += FMath::Max(WeatherPool[Index].Weight, 0.f);
	}
	if (Total <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	float Pick = FMath::FRandRange(0.f, Total);
	int32 Chosen = INDEX_NONE;
	for (int32 Index = 0; Index < WeatherPool.Num(); ++Index)
	{
		if (bAvoidRepeat && Index == LastPresetIndex)
		{
			continue;
		}
		Pick -= FMath::Max(WeatherPool[Index].Weight, 0.f);
		if (Pick <= 0.f)
		{
			Chosen = Index;
			break;
		}
	}
	if (Chosen == INDEX_NONE)
	{
		return;
	}

	UObject* Preset = WeatherPool[Chosen].Preset.LoadSynchronous();
	if (!Preset)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("Hub sky: weather preset failed to load (%s)"),
			*WeatherPool[Chosen].Preset.ToSoftObjectPath().ToString());
		return;
	}

	Bridge.ChangeWeather(Preset, WeatherTransitionSeconds);
	LastPresetIndex = Chosen;
	CurrentWeatherTag = WeatherPool[Chosen].Tag.IsNone() ? TEXT("Cloud") : WeatherPool[Chosen].Tag;
	OnWeatherChanged.Broadcast(CurrentWeatherTag);
	UE_LOG(LogSlimeFable, Log, TEXT("Hub sky: weather -> %s (%s)"), *CurrentWeatherTag.ToString(), *Preset->GetName());
}

void ASlimeHubSkyDirector::TryLightningStrike()
{
	if (GetThunder01() < 0.4f)
	{
		LightningAccumulator = 0.f;
		return;
	}

	LightningAccumulator += GetWorld()->GetDeltaSeconds();
	if (LightningAccumulator < 8.f)
	{
		return;
	}
	LightningAccumulator = 0.f;

	TArray<ASlimeFarmPlot*> Plots;
	for (TActorIterator<ASlimeFarmPlot> It(GetWorld()); It; ++It)
	{
		if (It->CanReceiveLightning())
		{
			Plots.Add(*It);
		}
	}
	if (Plots.Num() == 0)
	{
		return;
	}
	ASlimeFarmPlot* Plot = Plots[FMath::RandRange(0, Plots.Num() - 1)];
	SlimeElementDelivery::NotifyActor(Plot, ESlimeElement::Lightning, this, 1.f);
}
