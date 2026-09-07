// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/SlimeInputSettings.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Misc/ConfigCacheIni.h"
#include "Settings/SlimeGraphicsSettings.h"
#include "SlimeFable.h"
#include "UObject/SoftObjectPath.h"

namespace SlimeInputPrivate
{
	static const TCHAR* ConfigSection = TEXT("SlimeInput");
	static const TCHAR* SchemeVersionKey = TEXT("BindSchemeVersion");
	static const TCHAR* PlayModeKey = TEXT("PlayInputMode");
	static const TCHAR* HandednessKey = TEXT("TouchHandedness");
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
	LoadDevicePrefs();
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
	SaveDevicePrefs();
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
	const bool bIgnoreMouse = Action == ESlimeInputAction::Attack && Key.IsMouseButton() && IsTouchPointerBusy();
	if (Key.IsValid() && !bIgnoreMouse && PC->IsInputKeyDown(Key))
	{
		return true;
	}
	if (IsVirtualActionDown(Action))
	{
		return true;
	}
	if (ShouldReadGamepadAbilityKeys())
	{
		const FKey Pad = GetDefaultGamepadKey(Action);
		if (Pad.IsValid() && PC->IsInputKeyDown(Pad))
		{
			return true;
		}
	}
	return false;
}

bool USlimeInputSettings::WasKeyPressed(const APlayerController* PC, ESlimeInputAction Action) const
{
	if (!PC)
	{
		return false;
	}
	const FKey Key = GetKey(Action);
	const bool bIgnoreMouse = Action == ESlimeInputAction::Attack && Key.IsMouseButton() && IsTouchPointerBusy();
	if (Key.IsValid() && !bIgnoreMouse && PC->WasInputKeyJustPressed(Key))
	{
		return true;
	}
	if (WasVirtualActionPressed(Action))
	{
		return true;
	}
	if (ShouldReadGamepadAbilityKeys())
	{
		const FKey Pad = GetDefaultGamepadKey(Action);
		if (Pad.IsValid() && PC->WasInputKeyJustPressed(Pad))
		{
			return true;
		}
	}
	return false;
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

void USlimeInputSettings::SaveDevicePrefs()
{
	if (!GConfig)
	{
		return;
	}
	GConfig->SetInt(
		SlimeInputPrivate::ConfigSection,
		SlimeInputPrivate::PlayModeKey,
		static_cast<int32>(PlayInputMode),
		GGameUserSettingsIni);
	GConfig->SetInt(
		SlimeInputPrivate::ConfigSection,
		SlimeInputPrivate::HandednessKey,
		static_cast<int32>(TouchHandedness),
		GGameUserSettingsIni);
}

void USlimeInputSettings::LoadDevicePrefs()
{
	if (!GConfig)
	{
		return;
	}
	int32 ModeValue = static_cast<int32>(ESlimePlayInputMode::KeyboardMouse);
	if (GConfig->GetInt(
		SlimeInputPrivate::ConfigSection,
		SlimeInputPrivate::PlayModeKey,
		ModeValue,
		GGameUserSettingsIni))
	{
		PlayInputMode = static_cast<ESlimePlayInputMode>(
			FMath::Clamp(ModeValue, 0, static_cast<int32>(ESlimePlayInputMode::Touch)));
	}
	int32 HandValue = static_cast<int32>(ESlimeTouchHandedness::Right);
	if (GConfig->GetInt(
		SlimeInputPrivate::ConfigSection,
		SlimeInputPrivate::HandednessKey,
		HandValue,
		GGameUserSettingsIni))
	{
		TouchHandedness = static_cast<ESlimeTouchHandedness>(
			FMath::Clamp(HandValue, 0, static_cast<int32>(ESlimeTouchHandedness::Left)));
	}
}

void USlimeInputSettings::SetPlayInputMode(ESlimePlayInputMode Mode)
{
	if (PlayInputMode == Mode)
	{
		return;
	}
	PlayInputMode = Mode;
	if (PlayInputMode != ESlimePlayInputMode::Touch)
	{
		ClearVirtualActions();
	}
	Save();
	OnPlayInputModeChanged.Broadcast();
}

void USlimeInputSettings::SetTouchHandedness(ESlimeTouchHandedness Hand)
{
	if (TouchHandedness == Hand)
	{
		return;
	}
	TouchHandedness = Hand;
	Save();
	OnPlayInputModeChanged.Broadcast();
}

ESlimeResolvedInputMode USlimeInputSettings::ResolvePlayInputMode() const
{
	switch (PlayInputMode)
	{
	case ESlimePlayInputMode::KeyboardMouse:
		return ESlimeResolvedInputMode::KeyboardMouse;
	case ESlimePlayInputMode::Gamepad:
		return ESlimeResolvedInputMode::Gamepad;
	case ESlimePlayInputMode::Touch:
		return ESlimeResolvedInputMode::Touch;
	default:
		break;
	}

#if PLATFORM_ANDROID || PLATFORM_IOS
	return ESlimeResolvedInputMode::Touch;
#else
	if (LastInputDevice == ESlimeLastInputDevice::Gamepad)
	{
		return ESlimeResolvedInputMode::Gamepad;
	}
	return ESlimeResolvedInputMode::KeyboardMouse;
#endif
}

FText USlimeInputSettings::GetPlayInputModeDisplayName() const
{
	switch (PlayInputMode)
	{
	case ESlimePlayInputMode::KeyboardMouse:
		return FText::FromString(TEXT("游玩方式：键鼠"));
	case ESlimePlayInputMode::Gamepad:
		return FText::FromString(TEXT("游玩方式：手柄"));
	case ESlimePlayInputMode::Touch:
		return FText::FromString(TEXT("游玩方式：触屏"));
	default:
		return FText::FromString(TEXT("游玩方式：自动"));
	}
}

bool USlimeInputSettings::ShouldReadGamepadAbilityKeys() const
{
	return ResolvePlayInputMode() == ESlimeResolvedInputMode::Gamepad;
}

bool USlimeInputSettings::ShouldUseTouchHud() const
{
	// Explicit 键鼠 never shows the phone overlay, even while Pixel Streaming.
	if (PlayInputMode == ESlimePlayInputMode::KeyboardMouse)
	{
		return false;
	}
	if (ResolvePlayInputMode() == ESlimeResolvedInputMode::Touch)
	{
		return true;
	}
	if (PlayInputMode == ESlimePlayInputMode::Auto && ShouldShowPixelStreamPlayHint())
	{
		return true;
	}
	return false;
}

bool USlimeInputSettings::ShouldShowPixelStreamPlayHint() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const USlimeGraphicsSettings* Graphics = GI->GetSubsystem<USlimeGraphicsSettings>())
		{
			return Graphics->IsPixelStreamingEnabled();
		}
	}
	return false;
}

void USlimeInputSettings::NoteLastInputDevice(ESlimeLastInputDevice Device)
{
	if (Device == ESlimeLastInputDevice::None || LastInputDevice == Device)
	{
		return;
	}
	const ESlimeResolvedInputMode Before = ResolvePlayInputMode();
	LastInputDevice = Device;
	if (PlayInputMode == ESlimePlayInputMode::Auto && ResolvePlayInputMode() != Before)
	{
		OnPlayInputModeChanged.Broadcast();
	}
}

FKey USlimeInputSettings::GetDefaultGamepadKey(ESlimeInputAction Action)
{
	switch (Action)
	{
	case ESlimeInputAction::Flatten: return EKeys::Gamepad_LeftTrigger;
	case ESlimeInputAction::Absorb: return EKeys::Gamepad_LeftShoulder;
	case ESlimeInputAction::ResetBody: return EKeys::Gamepad_DPad_Down;
	case ESlimeInputAction::Attack: return EKeys::Gamepad_RightTrigger;
	case ESlimeInputAction::Skill1: return EKeys::Gamepad_RightShoulder;
	case ESlimeInputAction::LockOn: return EKeys::Gamepad_RightThumbstick;
	case ESlimeInputAction::Inventory: return EKeys::Gamepad_DPad_Up;
	case ESlimeInputAction::QuestLog: return EKeys::Gamepad_Special_Left;
	case ESlimeInputAction::Interact: return EKeys::Gamepad_FaceButton_Left;
	case ESlimeInputAction::Morph: return EKeys::Gamepad_FaceButton_Top;
	case ESlimeInputAction::Dodge: return EKeys::Gamepad_FaceButton_Right;
	case ESlimeInputAction::Sprint: return EKeys::Gamepad_LeftThumbstick;
	default: return EKeys::Invalid;
	}
}

FText USlimeInputSettings::GetGamepadGuideText()
{
	return FText::FromString(
		TEXT("手柄默认（不可重绑）\n")
		TEXT("左摇杆 移动　L3 冲刺　右摇杆 视角　R3 锁定\n")
		TEXT("A 跳 / 长按发射　B 闪避　X 交互/吞噬　Y 幻形\n")
		TEXT("LT 压扁　LB 吸收　RT 攻击　RB 技能1 / 长按幻影轮\n")
		TEXT("上 背包　下 重置　左右 切换属性　View 史书　Menu 暂停"));
}

bool USlimeInputSettings::IsGamepadDismissKey(const FKey& Key)
{
	return Key == EKeys::Gamepad_FaceButton_Right
		|| Key == EKeys::Gamepad_Special_Right;
}

void USlimeInputSettings::SetVirtualActionDown(ESlimeInputAction Action, bool bDown)
{
	if (bDown)
	{
		if (!VirtualDown.Contains(Action))
		{
			VirtualPressFrame.Add(Action, GFrameCounter);
		}
		VirtualDown.Add(Action);
	}
	else
	{
		VirtualDown.Remove(Action);
	}
}

void USlimeInputSettings::ClearVirtualActions()
{
	VirtualDown.Reset();
	VirtualPressFrame.Reset();
	VirtualMoveAxis = FVector2D::ZeroVector;
	VirtualLookPending = FVector2D::ZeroVector;
	bTouchPointerBusy = false;
}

void USlimeInputSettings::SetTouchPointerBusy(bool bBusy)
{
	bTouchPointerBusy = bBusy;
}

void USlimeInputSettings::SetVirtualMoveAxis(FVector2D Axis)
{
	if (Axis.SizeSquared() > 1.f)
	{
		Axis.Normalize();
	}
	VirtualMoveAxis = Axis;
}

void USlimeInputSettings::AddVirtualLookDelta(FVector2D Delta)
{
	VirtualLookPending += Delta;
}

FVector2D USlimeInputSettings::ConsumeVirtualLookDelta()
{
	const FVector2D Out = VirtualLookPending;
	VirtualLookPending = FVector2D::ZeroVector;
	return Out;
}

void USlimeInputSettings::NotifyPixelStreamingChanged()
{
	OnPlayInputModeChanged.Broadcast();
}

bool USlimeInputSettings::IsVirtualActionDown(ESlimeInputAction Action) const
{
	return VirtualDown.Contains(Action);
}

bool USlimeInputSettings::WasVirtualActionPressed(ESlimeInputAction Action) const
{
	if (const uint64* Frame = VirtualPressFrame.Find(Action))
	{
		return *Frame == GFrameCounter;
	}
	return false;
}
