// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/SlimeInputSettings.h"

#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Misc/ConfigCacheIni.h"
#include "SlimeFable.h"
#include "UObject/SoftObjectPath.h"

namespace SlimeInputPrivate
{
	static const TCHAR* ConfigSection = TEXT("SlimeInput");

	/** ThirdPerson template move/jump context — removed when move keys are customized. */
	static const TCHAR* DefaultMoveContextPath =
		TEXT("/Game/ThirdPerson/Input/IMC_Default.IMC_Default");
}

void USlimeInputSettings::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FillDefaults();
	Load();
	AppliedMovementKeys = Keys;
}

void USlimeInputSettings::FillDefaults()
{
	Keys.Reset();
	for (ESlimeInputAction Action : GetAllActions())
	{
		Keys.Add(Action, GetDefaultKey(Action));
	}
}

FKey USlimeInputSettings::GetDefaultKey(ESlimeInputAction Action)
{
	switch (Action)
	{
	case ESlimeInputAction::MoveForward: return EKeys::W;
	case ESlimeInputAction::MoveBack: return EKeys::S;
	case ESlimeInputAction::MoveLeft: return EKeys::A;
	case ESlimeInputAction::MoveRight: return EKeys::D;
	case ESlimeInputAction::Jump: return EKeys::SpaceBar;
	case ESlimeInputAction::Flatten: return EKeys::Z;
	case ESlimeInputAction::Absorb: return EKeys::X;
	case ESlimeInputAction::ResetBody: return EKeys::T;
	case ESlimeInputAction::Launch: return EKeys::Q;
	case ESlimeInputAction::ElementWheel: return EKeys::Tab;
	case ESlimeInputAction::Attack: return EKeys::LeftMouseButton;
	case ESlimeInputAction::Skill1: return EKeys::One;
	case ESlimeInputAction::Skill2: return EKeys::Two;
	case ESlimeInputAction::Skill3: return EKeys::Three;
	case ESlimeInputAction::LockOn: return EKeys::MiddleMouseButton;
	default: return EKeys::Invalid;
	}
}

TArray<ESlimeInputAction> USlimeInputSettings::GetAllActions()
{
	TArray<ESlimeInputAction> Actions;
	for (uint8 Index = 0; Index < static_cast<uint8>(ESlimeInputAction::COUNT); ++Index)
	{
		Actions.Add(static_cast<ESlimeInputAction>(Index));
	}
	return Actions;
}

FKey USlimeInputSettings::GetKey(ESlimeInputAction Action) const
{
	if (const FKey* Found = Keys.Find(Action))
	{
		return *Found;
	}
	return GetDefaultKey(Action);
}

FText USlimeInputSettings::GetActionDisplayName(ESlimeInputAction Action) const
{
	switch (Action)
	{
	case ESlimeInputAction::MoveForward: return FText::FromString(TEXT("前进"));
	case ESlimeInputAction::MoveBack: return FText::FromString(TEXT("后退"));
	case ESlimeInputAction::MoveLeft: return FText::FromString(TEXT("左移"));
	case ESlimeInputAction::MoveRight: return FText::FromString(TEXT("右移"));
	case ESlimeInputAction::Jump: return FText::FromString(TEXT("跳跃"));
	case ESlimeInputAction::Flatten: return FText::FromString(TEXT("压扁"));
	case ESlimeInputAction::Absorb: return FText::FromString(TEXT("吸收/召回"));
	case ESlimeInputAction::ResetBody: return FText::FromString(TEXT("重置身体"));
	case ESlimeInputAction::Launch: return FText::FromString(TEXT("发射"));
	case ESlimeInputAction::ElementWheel: return FText::FromString(TEXT("元素轮盘"));
	case ESlimeInputAction::Attack: return FText::FromString(TEXT("攻击"));
	case ESlimeInputAction::Skill1: return FText::FromString(TEXT("技能1"));
	case ESlimeInputAction::Skill2: return FText::FromString(TEXT("技能2"));
	case ESlimeInputAction::Skill3: return FText::FromString(TEXT("技能3"));
	case ESlimeInputAction::LockOn: return FText::FromString(TEXT("锁定"));
	default: return FText::GetEmpty();
	}
}

FText USlimeInputSettings::GetKeyDisplayName(ESlimeInputAction Action) const
{
	const FKey Key = GetKey(Action);
	return Key.IsValid() ? Key.GetDisplayName() : FText::FromString(TEXT("—"));
}

FString USlimeInputSettings::ActionConfigName(ESlimeInputAction Action) const
{
	return StaticEnum<ESlimeInputAction>()->GetNameStringByValue(static_cast<int64>(Action));
}

bool USlimeInputSettings::TrySetKey(ESlimeInputAction Action, FKey NewKey, FText& OutError)
{
	OutError = FText::GetEmpty();
	if (!NewKey.IsValid() || NewKey == EKeys::Escape)
	{
		OutError = FText::FromString(TEXT("无效按键"));
		return false;
	}

	for (const TPair<ESlimeInputAction, FKey>& Pair : Keys)
	{
		if (Pair.Key != Action && Pair.Value == NewKey)
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("按键已被「%s」占用"),
				*GetActionDisplayName(Pair.Key).ToString()));
			return false;
		}
	}

	Keys.Add(Action, NewKey);
	Save();
	return true;
}

void USlimeInputSettings::ResetToDefaults()
{
	FillDefaults();
	Save();
}

void USlimeInputSettings::Save()
{
	if (!GConfig)
	{
		return;
	}

	for (ESlimeInputAction Action : GetAllActions())
	{
		GConfig->SetString(
			SlimeInputPrivate::ConfigSection,
			*ActionConfigName(Action),
			*GetKey(Action).ToString(),
			GGameUserSettingsIni);
	}
	GConfig->Flush(false, GGameUserSettingsIni);
}

void USlimeInputSettings::Load()
{
	if (!GConfig)
	{
		return;
	}

	for (ESlimeInputAction Action : GetAllActions())
	{
		FString KeyName;
		if (GConfig->GetString(
			SlimeInputPrivate::ConfigSection,
			*ActionConfigName(Action),
			KeyName,
			GGameUserSettingsIni))
		{
			const FKey Loaded(*KeyName);
			if (Loaded.IsValid())
			{
				Keys.Add(Action, Loaded);
			}
		}
	}
}

bool USlimeInputSettings::IsKeyDown(const APlayerController* PC, ESlimeInputAction Action) const
{
	if (!PC)
	{
		return false;
	}
	const FKey Key = GetKey(Action);
	return Key.IsValid() && PC->IsInputKeyDown(Key);
}

bool USlimeInputSettings::WasKeyPressed(const APlayerController* PC, ESlimeInputAction Action) const
{
	if (!PC)
	{
		return false;
	}
	const FKey Key = GetKey(Action);
	return Key.IsValid() && PC->WasInputKeyJustPressed(Key);
}

bool USlimeInputSettings::UsesCustomMovementKeys() const
{
	const ESlimeInputAction MoveActions[] = {
		ESlimeInputAction::MoveForward,
		ESlimeInputAction::MoveBack,
		ESlimeInputAction::MoveLeft,
		ESlimeInputAction::MoveRight,
		ESlimeInputAction::Jump
	};
	for (ESlimeInputAction Action : MoveActions)
	{
		if (GetKey(Action) != GetDefaultKey(Action))
		{
			return true;
		}
	}
	return false;
}

void USlimeInputSettings::ApplyEnhancedInputRemaps(APlayerController* PC)
{
	if (!PC)
	{
		return;
	}

	const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	UInputMappingContext* DefaultMoveContext = Cast<UInputMappingContext>(
		FSoftObjectPath(SlimeInputPrivate::DefaultMoveContextPath).TryLoad());
	if (!DefaultMoveContext)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeInputSettings: IMC_Default missing; custom move keys will poll only."));
		return;
	}

	if (UsesCustomMovementKeys())
	{
		Subsystem->RemoveMappingContext(DefaultMoveContext);
		UE_LOG(LogSlimeFable, Log, TEXT("SlimeInputSettings: removed IMC_Default for custom move/jump keys."));
	}
	else
	{
		Subsystem->AddMappingContext(DefaultMoveContext, 0);
	}

	AppliedMovementKeys = Keys;
}
