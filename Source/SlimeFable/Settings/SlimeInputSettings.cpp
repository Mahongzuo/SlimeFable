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
	static const TCHAR* SchemeVersionKey = TEXT("BindSchemeVersion");
	static constexpr int32 CurrentBindSchemeVersion = 6;

	/** ThirdPerson template move/jump context — removed when move keys are customized. */
	static const TCHAR* DefaultMoveContextPath =
		TEXT("/Game/ThirdPerson/Input/IMC_Default.IMC_Default");

	static FKey LegacyDefaultKey(ESlimeInputAction Action)
	{
		switch (Action)
		{
		case ESlimeInputAction::Flatten: return EKeys::Z;
		case ESlimeInputAction::Launch: return EKeys::Q;
		case ESlimeInputAction::Skill1: return EKeys::One;
		case ESlimeInputAction::Skill2: return EKeys::Two;
		case ESlimeInputAction::Skill3: return EKeys::Three;
		default: return EKeys::Invalid;
		}
	}
}

void USlimeInputSettings::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FillDefaults();
	Load();
	MigrateBindSchemeIfNeeded();
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
	case ESlimeInputAction::Flatten: return EKeys::C;
	case ESlimeInputAction::Absorb: return EKeys::X;
	case ESlimeInputAction::ResetBody: return EKeys::T;
	case ESlimeInputAction::Launch: return EKeys::G;
	case ESlimeInputAction::ElementWheel: return EKeys::Tab;
	case ESlimeInputAction::Attack: return EKeys::LeftMouseButton;
	case ESlimeInputAction::Skill1: return EKeys::Q;
	case ESlimeInputAction::Skill2: return EKeys::E;
	case ESlimeInputAction::Skill3: return EKeys::R;
	case ESlimeInputAction::LockOn: return EKeys::MiddleMouseButton;
	case ESlimeInputAction::Inventory: return EKeys::B;
	case ESlimeInputAction::QuestLog: return EKeys::J;
	case ESlimeInputAction::Interact: return EKeys::F;
	// Direct hotbar keys unbound by default — use Tab wheel (or rebind).
	case ESlimeInputAction::Hotbar1: return EKeys::Invalid;
	case ESlimeInputAction::Hotbar2: return EKeys::Invalid;
	case ESlimeInputAction::Hotbar3: return EKeys::Invalid;
	case ESlimeInputAction::Hotbar4: return EKeys::Invalid;
	case ESlimeInputAction::Hotbar5: return EKeys::Invalid;
	case ESlimeInputAction::Hotbar6: return EKeys::Invalid;
	case ESlimeInputAction::Morph: return EKeys::Z;
	case ESlimeInputAction::Dodge: return EKeys::RightMouseButton;
	case ESlimeInputAction::ShowCursor: return EKeys::LeftAlt;
	case ESlimeInputAction::Element1: return EKeys::One;
	case ESlimeInputAction::Element2: return EKeys::Two;
	case ESlimeInputAction::Element3: return EKeys::Three;
	case ESlimeInputAction::Element4: return EKeys::Four;
	case ESlimeInputAction::Element5: return EKeys::Five;
	case ESlimeInputAction::Element6: return EKeys::Six;
	case ESlimeInputAction::ElementFormation: return EKeys::L;
	case ESlimeInputAction::CheatConsole: return EKeys::Enter;
	case ESlimeInputAction::Sprint: return EKeys::LeftShift;
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
	case ESlimeInputAction::ElementWheel: return FText::FromString(TEXT("快捷栏轮盘"));
	case ESlimeInputAction::Attack: return FText::FromString(TEXT("攻击"));
	case ESlimeInputAction::Skill1: return FText::FromString(TEXT("技能1"));
	case ESlimeInputAction::Skill2: return FText::FromString(TEXT("技能2"));
	case ESlimeInputAction::Skill3: return FText::FromString(TEXT("技能3"));
	case ESlimeInputAction::LockOn: return FText::FromString(TEXT("锁定"));
	case ESlimeInputAction::Inventory: return FText::FromString(TEXT("背包"));
	case ESlimeInputAction::QuestLog: return FText::FromString(TEXT("史书"));
	case ESlimeInputAction::Interact: return FText::FromString(TEXT("拾取/交互"));
	case ESlimeInputAction::Hotbar1: return FText::FromString(TEXT("消耗品快捷1"));
	case ESlimeInputAction::Hotbar2: return FText::FromString(TEXT("消耗品快捷2"));
	case ESlimeInputAction::Hotbar3: return FText::FromString(TEXT("消耗品快捷3"));
	case ESlimeInputAction::Hotbar4: return FText::FromString(TEXT("放置品快捷1"));
	case ESlimeInputAction::Hotbar5: return FText::FromString(TEXT("放置品快捷2"));
	case ESlimeInputAction::Hotbar6: return FText::FromString(TEXT("放置品快捷3"));
	case ESlimeInputAction::Morph: return FText::FromString(TEXT("幻形"));
	case ESlimeInputAction::Dodge: return FText::FromString(TEXT("闪避"));
	case ESlimeInputAction::ShowCursor: return FText::FromString(TEXT("显示鼠标"));
	case ESlimeInputAction::Element1: return FText::FromString(TEXT("属性1（编队）"));
	case ESlimeInputAction::Element2: return FText::FromString(TEXT("属性2（编队）"));
	case ESlimeInputAction::Element3: return FText::FromString(TEXT("属性3（编队）"));
	case ESlimeInputAction::Element4: return FText::FromString(TEXT("属性4（编队）"));
	case ESlimeInputAction::Element5: return FText::FromString(TEXT("属性5（编队）"));
	case ESlimeInputAction::Element6: return FText::FromString(TEXT("属性6（编队）"));
	case ESlimeInputAction::ElementFormation: return FText::FromString(TEXT("属性编队"));
	case ESlimeInputAction::CheatConsole: return FText::FromString(TEXT("作弊台"));
	case ESlimeInputAction::Sprint: return FText::FromString(TEXT("冲刺"));
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

	GConfig->SetInt(
		SlimeInputPrivate::ConfigSection,
		SlimeInputPrivate::SchemeVersionKey,
		SlimeInputPrivate::CurrentBindSchemeVersion,
		GGameUserSettingsIni);

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

void USlimeInputSettings::MigrateBindSchemeIfNeeded()
{
	if (!GConfig)
	{
		return;
	}

	int32 Version = 0;
	GConfig->GetInt(
		SlimeInputPrivate::ConfigSection,
		SlimeInputPrivate::SchemeVersionKey,
		Version,
		GGameUserSettingsIni);

	if (Version >= SlimeInputPrivate::CurrentBindSchemeVersion)
	{
		return;
	}

	auto RemapIfLegacy = [this](ESlimeInputAction Action)
	{
		const FKey Legacy = SlimeInputPrivate::LegacyDefaultKey(Action);
		if (!Legacy.IsValid())
		{
			return;
		}
		const FKey Current = GetKey(Action);
		if (Current == Legacy)
		{
			Keys.Add(Action, GetDefaultKey(Action));
		}
	};

	// Order matters: free Launch/Flatten first, then assign skills onto Q/E/R.
	RemapIfLegacy(ESlimeInputAction::Launch);
	RemapIfLegacy(ESlimeInputAction::Flatten);
	RemapIfLegacy(ESlimeInputAction::Skill1);
	RemapIfLegacy(ESlimeInputAction::Skill2);
	RemapIfLegacy(ESlimeInputAction::Skill3);

	// Ensure new actions exist with defaults if missing from old configs.
	const ESlimeInputAction NewActions[] = {
		ESlimeInputAction::Inventory,
		ESlimeInputAction::QuestLog,
		ESlimeInputAction::Interact,
		ESlimeInputAction::Hotbar1,
		ESlimeInputAction::Hotbar2,
		ESlimeInputAction::Hotbar3,
		ESlimeInputAction::Hotbar4,
		ESlimeInputAction::Hotbar5,
		ESlimeInputAction::Hotbar6,
		ESlimeInputAction::Dodge,
		ESlimeInputAction::ShowCursor,
		ESlimeInputAction::Morph,
		ESlimeInputAction::Element1,
		ESlimeInputAction::Element2,
		ESlimeInputAction::Element3,
		ESlimeInputAction::Element4,
		ESlimeInputAction::Element5,
		ESlimeInputAction::Element6,
		ESlimeInputAction::ElementFormation,
		ESlimeInputAction::CheatConsole,
		ESlimeInputAction::Sprint
	};
	for (ESlimeInputAction Action : NewActions)
	{
		if (!Keys.Contains(Action))
		{
			Keys.Add(Action, GetDefaultKey(Action));
		}
	}

	// v6: 1–6 = formation elements; Tab = hotbar wheel; release digit keys from hotbar defaults.
	if (Version < 6)
	{
		const ESlimeInputAction Hotbars[] = {
			ESlimeInputAction::Hotbar1, ESlimeInputAction::Hotbar2, ESlimeInputAction::Hotbar3,
			ESlimeInputAction::Hotbar4, ESlimeInputAction::Hotbar5, ESlimeInputAction::Hotbar6
		};
		const FKey Digits[] = {
			EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six
		};
		for (int32 Index = 0; Index < 6; ++Index)
		{
			if (GetKey(Hotbars[Index]) == Digits[Index])
			{
				Keys.Add(Hotbars[Index], EKeys::Invalid);
			}
		}
		Keys.Add(ESlimeInputAction::Element1, EKeys::One);
		Keys.Add(ESlimeInputAction::Element2, EKeys::Two);
		Keys.Add(ESlimeInputAction::Element3, EKeys::Three);
		Keys.Add(ESlimeInputAction::Element4, EKeys::Four);
		Keys.Add(ESlimeInputAction::Element5, EKeys::Five);
		Keys.Add(ESlimeInputAction::Element6, EKeys::Six);
		if (!GetKey(ESlimeInputAction::ElementFormation).IsValid())
		{
			Keys.Add(ESlimeInputAction::ElementFormation, EKeys::L);
		}
	}

	Save();
	UE_LOG(LogSlimeFable, Log, TEXT("SlimeInputSettings: migrated bind scheme to v%d."), SlimeInputPrivate::CurrentBindSchemeVersion);
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
