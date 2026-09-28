// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hub/SlimeUDSBridge.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "SlimeFable.h"
#include "UObject/UnrealType.h"

namespace SlimeUDSPrivate
{
	FProperty* FindProperty(const UObject* Object, const TCHAR* PropertyName)
	{
		if (!Object || !PropertyName)
		{
			return nullptr;
		}
		return FindFProperty<FProperty>(Object->GetClass(), FName(PropertyName));
	}
}

void FSlimeUDSBridge::Bind(UWorld* World)
{
	Sky = FindActorByClassName(World, TEXT("Ultra_Dynamic_Sky_C"));
	Weather = FindActorByClassName(World, TEXT("Ultra_Dynamic_Weather_C"));
	UE_LOG(LogSlimeFable, Log, TEXT("UDS bridge: sky=%s weather=%s"),
		Sky.IsValid() ? *Sky->GetName() : TEXT("none"),
		Weather.IsValid() ? *Weather->GetName() : TEXT("none"));
}

AActor* FSlimeUDSBridge::FindActorByClassName(UWorld* World, const TCHAR* ClassName)
{
	if (!World || !ClassName)
	{
		return nullptr;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetClass() && It->GetClass()->GetName() == ClassName)
		{
			return *It;
		}
	}
	return nullptr;
}

bool FSlimeUDSBridge::SetNumeric(UObject* Object, const TCHAR* PropertyName, double Value)
{
	FProperty* Prop = SlimeUDSPrivate::FindProperty(Object, PropertyName);
	if (FDoubleProperty* AsDouble = CastField<FDoubleProperty>(Prop))
	{
		AsDouble->SetFloatingPointPropertyValue(AsDouble->ContainerPtrToValuePtr<void>(Object), Value);
		return true;
	}
	if (FFloatProperty* AsFloat = CastField<FFloatProperty>(Prop))
	{
		AsFloat->SetFloatingPointPropertyValue(AsFloat->ContainerPtrToValuePtr<void>(Object), Value);
		return true;
	}
	return false;
}

bool FSlimeUDSBridge::SetBool(UObject* Object, const TCHAR* PropertyName, bool bValue)
{
	FProperty* Prop = SlimeUDSPrivate::FindProperty(Object, PropertyName);
	if (FBoolProperty* AsBool = CastField<FBoolProperty>(Prop))
	{
		AsBool->SetPropertyValue_InContainer(Object, bValue);
		return true;
	}
	return false;
}

bool FSlimeUDSBridge::SetEnumByte(UObject* Object, const TCHAR* PropertyName, uint8 Value)
{
	FProperty* Prop = SlimeUDSPrivate::FindProperty(Object, PropertyName);
	if (FEnumProperty* AsEnum = CastField<FEnumProperty>(Prop))
	{
		AsEnum->GetUnderlyingProperty()->SetIntPropertyValue(AsEnum->ContainerPtrToValuePtr<void>(Object), static_cast<int64>(Value));
		return true;
	}
	if (FByteProperty* AsByte = CastField<FByteProperty>(Prop))
	{
		AsByte->SetPropertyValue_InContainer(Object, Value);
		return true;
	}
	return false;
}

bool FSlimeUDSBridge::ReadNumeric(const UObject* Object, const TCHAR* PropertyName, double& OutValue)
{
	FProperty* Prop = SlimeUDSPrivate::FindProperty(Object, PropertyName);
	if (FDoubleProperty* AsDouble = CastField<FDoubleProperty>(Prop))
	{
		OutValue = AsDouble->GetFloatingPointPropertyValue(AsDouble->ContainerPtrToValuePtr<void>(Object));
		return true;
	}
	if (FFloatProperty* AsFloat = CastField<FFloatProperty>(Prop))
	{
		OutValue = AsFloat->GetFloatingPointPropertyValue(AsFloat->ContainerPtrToValuePtr<void>(Object));
		return true;
	}
	return false;
}

void FSlimeUDSBridge::ConfigureDayCycle(float DayLengthMinutes, float NightLengthMinutes)
{
	AActor* SkyActor = Sky.Get();
	if (!SkyActor)
	{
		return;
	}

	// Simulate Real Sun ignores Day Length. Turn it off so the 10 minute cycle is exact.
	SetBool(SkyActor, TEXT("Simulate Real Sun"), false);
	SetBool(SkyActor, TEXT("Animate Time of Day"), true);
	const bool bDay = SetNumeric(SkyActor, TEXT("Day Length"), DayLengthMinutes);
	const bool bNight = SetNumeric(SkyActor, TEXT("Night Length"), NightLengthMinutes);
	SetNumeric(SkyActor, TEXT("Time Speed"), 1.0);
	UE_LOG(LogSlimeFable, Log, TEXT("UDS day cycle: Day Length %.2f min (%s), Night Length %.2f min (%s)"),
		DayLengthMinutes, bDay ? TEXT("ok") : TEXT("missing"),
		NightLengthMinutes, bNight ? TEXT("ok") : TEXT("missing"));
}

void FSlimeUDSBridge::DisableRandomWeather()
{
	// UDS_RandomWeatherTiming: 0 Random Interval, 1 Daily, 2 Hourly, 3 Disabled.
	if (!SetEnumByte(Weather.Get(), TEXT("Random Weather Variation"), 3))
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("UDS: could not disable Random Weather Variation"));
	}
}

bool FSlimeUDSBridge::ChangeWeather(UObject* Preset, float TransitionSeconds)
{
	AActor* WeatherActor = Weather.Get();
	if (!WeatherActor || !Preset)
	{
		return false;
	}

	UFunction* Function = WeatherActor->FindFunction(FName(TEXT("Change Weather")));
	if (!Function)
	{
		if (!bLoggedChangeWeather)
		{
			bLoggedChangeWeather = true;
			UE_LOG(LogSlimeFable, Warning, TEXT("UDS: Change Weather function not found"));
		}
		return false;
	}

	uint8* Params = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
	FMemory::Memzero(Params, Function->ParmsSize);

	bool bSetPreset = false;
	bool bSetTime = false;
	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop->HasAnyPropertyFlags(CPF_Parm) || Prop->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			continue;
		}
		Prop->InitializeValue(Prop->ContainerPtrToValuePtr<void>(Params));
		if (Prop->HasAnyPropertyFlags(CPF_OutParm) && !Prop->HasAnyPropertyFlags(CPF_ReferenceParm))
		{
			continue;
		}

		if (FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Prop))
		{
			ObjectProp->SetObjectPropertyValue(ObjectProp->ContainerPtrToValuePtr<void>(Params), Preset);
			bSetPreset = true;
		}
		else if (FDoubleProperty* AsDouble = CastField<FDoubleProperty>(Prop))
		{
			AsDouble->SetFloatingPointPropertyValue(AsDouble->ContainerPtrToValuePtr<void>(Params), TransitionSeconds);
			bSetTime = true;
		}
		else if (FFloatProperty* AsFloat = CastField<FFloatProperty>(Prop))
		{
			AsFloat->SetFloatingPointPropertyValue(AsFloat->ContainerPtrToValuePtr<void>(Params), TransitionSeconds);
			bSetTime = true;
		}
	}

	if (!bLoggedChangeWeather)
	{
		bLoggedChangeWeather = true;
		UE_LOG(LogSlimeFable, Log, TEXT("UDS Change Weather params: preset=%s time=%s"),
			bSetPreset ? TEXT("yes") : TEXT("no"),
			bSetTime ? TEXT("yes") : TEXT("no"));
	}

	WeatherActor->ProcessEvent(Function, Params);

	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		FProperty* Prop = *It;
		if (Prop->HasAnyPropertyFlags(CPF_Parm))
		{
			Prop->DestroyValue(Prop->ContainerPtrToValuePtr<void>(Params));
		}
	}
	return bSetPreset;
}

double FSlimeUDSBridge::GetTimeOfDay() const
{
	double Value = 1200.0;
	ReadNumeric(Sky.Get(), TEXT("Time of Day"), Value);
	return Value;
}

double FSlimeUDSBridge::GetDawnTime() const
{
	double Value = 600.0;
	ReadNumeric(Sky.Get(), TEXT("Dawn Time"), Value);
	return Value;
}

double FSlimeUDSBridge::GetDuskTime() const
{
	double Value = 1800.0;
	ReadNumeric(Sky.Get(), TEXT("Dusk Time"), Value);
	return Value;
}

double FSlimeUDSBridge::GetRain() const
{
	double Value = 0.0;
	if (!ReadNumeric(Sky.Get(), TEXT("Current Rain"), Value))
	{
		ReadNumeric(Weather.Get(), TEXT("Rain"), Value);
	}
	return Value > 1.5 ? Value / 10.0 : Value;
}

double FSlimeUDSBridge::GetSnow() const
{
	double Value = 0.0;
	if (!ReadNumeric(Sky.Get(), TEXT("Current Snow"), Value))
	{
		ReadNumeric(Weather.Get(), TEXT("Snow"), Value);
	}
	return Value > 1.5 ? Value / 10.0 : Value;
}

double FSlimeUDSBridge::GetThunder() const
{
	double Value = 0.0;
	if (!ReadNumeric(Sky.Get(), TEXT("Thunder/Lightning"), Value))
	{
		ReadNumeric(Weather.Get(), TEXT("Thunder/Lightning"), Value);
	}
	return Value > 1.5 ? Value / 10.0 : Value;
}
