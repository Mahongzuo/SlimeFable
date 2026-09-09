// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/SlimeGraphicsSettings.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RHI.h"
#include "RHIStats.h"
#include "DynamicRHI.h"
#include "Containers/Ticker.h"
#include "IPixelStreaming2Module.h"
#include "IPixelStreaming2Streamer.h"
#include "DLSSLibrary.h"
#include "Settings/SlimeInputSettings.h"
#include "SlimeFable.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

namespace SlimeGraphicsPrivate
{
	static const TCHAR* ConfigSection = TEXT("SlimeGraphics");
	static const TCHAR* KeyUpscaler = TEXT("Upscaler");
	static const TCHAR* KeyDLSSQuality = TEXT("DLSSQuality");
	static const TCHAR* KeyFrameGen = TEXT("FrameGen");
	static const TCHAR* KeyHasQuality = TEXT("bHasUserOrAutoQuality");
	static const TCHAR* KeyPixelStreaming = TEXT("bPixelStreaming");
	static const TCHAR* KeyPixelStreamingUrl = TEXT("PixelStreamingUrl");
	static const TCHAR* KeyPixelStreamTarget = TEXT("PixelStreamTarget");
	static const TCHAR* KeyPixelStreamingPlayToken = TEXT("PixelStreamingPlayToken");
	static const TCHAR* KeyBodySkin = TEXT("BodySkin");
	static const TCHAR* LanStreamerUrl = TEXT("ws://127.0.0.1:18888");
	static const TCHAR* DefaultCloudStreamerUrl = TEXT("wss://been.chat/ps/streamer");
	static const TCHAR* CloudPlayPage = TEXT("https://been.chat/play");
	static constexpr int32 LanPlayerPort = 18880;
	static constexpr int32 LanStreamerPort = 18888;

	static bool IsUsableStreamingUrl(const FString& Url)
	{
		return Url.StartsWith(TEXT("ws://"), ESearchCase::IgnoreCase)
			|| Url.StartsWith(TEXT("wss://"), ESearchCase::IgnoreCase)
			|| Url.StartsWith(TEXT("wss+insecure://"), ESearchCase::IgnoreCase);
	}

	static FString Unquote(FString Value)
	{
		Value.TrimStartAndEndInline();
		if (Value.Len() >= 2 && Value.StartsWith(TEXT("\"")) && Value.EndsWith(TEXT("\"")))
		{
			return Value.Mid(1, Value.Len() - 2);
		}
		return Value;
	}

	static const TCHAR* QualityNames[] = { TEXT("流畅"), TEXT("均衡"), TEXT("高清"), TEXT("极致") };

	static const TCHAR* DLSSEnableCVars[] = {
		TEXT("r.NGX.DLSS.Enable")
	};
	static const TCHAR* FrameGenCVars[] = {
		TEXT("r.streamline.dlssg.enable"),
		TEXT("r.Streamline.DLSSG.Enable"),
		TEXT("r.NGX.DLSSG.Enable")
	};
	static const TCHAR* ReflexCVars[] = {
		TEXT("r.streamline.reflex.enable"),
		TEXT("r.Streamline.Reflex.Enable"),
		TEXT("t.Streamline.Reflex.Enable")
	};
	static const TCHAR* FSREnableCVars[] = {
		TEXT("r.FidelityFX.FSR.Enabled"),
		TEXT("r.FidelityFX.FSR3.Enabled")
	};

	static const TCHAR* DlssModuleNames[] = {
		TEXT("DLSS"),
		TEXT("DLSSBlueprint"),
		TEXT("NGX"),
		TEXT("StreamlineCore"),
		TEXT("Streamline")
	};

	static UDLSSMode ToUDLSSMode(ESlimeDLSSQuality Quality)
	{
		switch (Quality)
		{
		case ESlimeDLSSQuality::UltraPerformance: return UDLSSMode::UltraPerformance;
		case ESlimeDLSSQuality::Performance: return UDLSSMode::Performance;
		case ESlimeDLSSQuality::Balanced: return UDLSSMode::Balanced;
		case ESlimeDLSSQuality::Quality: return UDLSSMode::Quality;
		case ESlimeDLSSQuality::DLAA: return UDLSSMode::DLAA;
		default: return UDLSSMode::Quality;
		}
	}

	static FVector2D QueryViewportSize()
	{
		if (GEngine && GEngine->GameViewport)
		{
			FVector2D Size(0.0, 0.0);
			GEngine->GameViewport->GetViewportSize(Size);
			if (Size.X >= 32.0 && Size.Y >= 32.0)
			{
				return Size;
			}
		}
		if (GSystemResolution.ResX > 0 && GSystemResolution.ResY > 0)
		{
			return FVector2D(GSystemResolution.ResX, GSystemResolution.ResY);
		}
		return FVector2D(1920.0, 1080.0);
	}
}

void USlimeGraphicsSettings::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (IsRunningCommandlet() || IsRunningDedicatedServer())
	{
		return;
	}
	ProbeGpu();
	WriteGpuLogFile();
	Load();
	if (!GIsEditor && !PixelStreamingUrl.IsEmpty())
	{
		bPixelStreaming = true;
	}
	ApplyFirstRunQualityIfNeeded();
	ApplyUpscaler();
	ApplyPixelStreaming();
}

void USlimeGraphicsSettings::ProbeGpu()
{
	AdapterName = GRHIAdapterName;
	DriverVersion = GRHIGlobals.GpuInfo.AdapterUserDriverVersion;
	const uint32 VendorId = GRHIVendorId;
	bNvidia = VendorId == 0x10DE;
	bAmd = VendorId == 0x1002;
	if (bNvidia)
	{
		VendorName = TEXT("NVIDIA");
	}
	else if (bAmd)
	{
		VendorName = TEXT("AMD");
	}
	else if (VendorId == 0x8086)
	{
		VendorName = TEXT("Intel");
	}
	else
	{
		VendorName = FString::Printf(TEXT("0x%04X"), VendorId);
	}

	DedicatedVramMB = static_cast<int32>(GRHIGlobals.GpuInfo.DedicatedVideoMemory / (1024ull * 1024ull));
	if (DedicatedVramMB <= 0 && GDynamicRHI)
	{
		FTextureMemoryStats TexStats;
		RHIGetTextureMemoryStats(TexStats);
		if (TexStats.DedicatedVideoMemory > 0)
		{
			DedicatedVramMB = static_cast<int32>(TexStats.DedicatedVideoMemory / (1024ll * 1024ll));
		}
	}

	bHardwareRayTracing = GRHISupportsRayTracing;

	const FString NameLower = AdapterName.ToLower();
	const bool bIntelIgpu = VendorId == 0x8086 && !NameLower.Contains(TEXT("arc"));
	const bool bAmdIgpu = bAmd && (NameLower.Contains(TEXT("radeon(tm) graphics"))
		|| NameLower.Contains(TEXT("radeon graphics"))
		|| (NameLower.Contains(TEXT("graphics")) && !NameLower.Contains(TEXT("rx"))));
	bIntegratedGpu = GRHIDeviceIsIntegrated || bIntelIgpu || bAmdIgpu
		|| NameLower.Contains(TEXT("uhd"))
		|| NameLower.Contains(TEXT("iris"))
		|| (DedicatedVramMB > 0 && DedicatedVramMB < 2048);

	UE_LOG(LogSlimeFable, Log,
		TEXT("SlimeGraphics: GPU='%s' vendor=%s vram=%dMB RT=%d iGPU=%d"),
		*AdapterName, *VendorName, DedicatedVramMB,
		bHardwareRayTracing ? 1 : 0, bIntegratedGpu ? 1 : 0);
}

void USlimeGraphicsSettings::WriteGpuLogFile() const
{
	const FString Path = FPaths::ProjectSavedDir() / TEXT("SlimeFable_GPU.txt");
	const int32 Recommended = RecommendQualityLevel();
	const FString Body = FString::Printf(
		TEXT("Adapter=%s\nVendor=%s\nDriver=%s\nDedicatedVramMB=%d\nHardwareRT=%s\nIntegrated=%s\n")
		TEXT("DLSSSupported=%s\nFrameGenSupported=%s\nFSRPlugin=%s\nRecommendedQuality=%s (%d)\n"),
		*AdapterName,
		*VendorName,
		*DriverVersion,
		DedicatedVramMB,
		bHardwareRayTracing ? TEXT("yes") : TEXT("no"),
		bIntegratedGpu ? TEXT("yes") : TEXT("no"),
		IsDlssSupported() ? TEXT("yes") : TEXT("no"),
		IsFrameGenSupported() ? TEXT("yes") : TEXT("no"),
		IsFsrPluginPresent() ? TEXT("yes") : TEXT("no"),
		SlimeGraphicsPrivate::QualityNames[FMath::Clamp(Recommended, 0, 3)],
		Recommended);
	FFileHelper::SaveStringToFile(Body, *Path);
}

bool USlimeGraphicsSettings::HasCVar(const TCHAR* Name) const
{
	return IConsoleManager::Get().FindConsoleVariable(Name) != nullptr;
}

void USlimeGraphicsSettings::SetCVarInt(const TCHAR* Name, int32 Value) const
{
	if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
	{
		Var->Set(Value, ECVF_SetByGameSetting);
	}
}

void USlimeGraphicsSettings::SetCVarFloat(const TCHAR* Name, float Value) const
{
	if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
	{
		Var->Set(Value, ECVF_SetByGameSetting);
	}
}

void USlimeGraphicsSettings::SetCVarString(const TCHAR* Name, const FString& Value) const
{
	if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
	{
		Var->Set(*Value, ECVF_SetByGameSetting);
	}
}

bool USlimeGraphicsSettings::IsDlssModuleAvailable() const
{
	FModuleManager& Manager = FModuleManager::Get();
	for (const TCHAR* ModuleName : SlimeGraphicsPrivate::DlssModuleNames)
	{
		if (Manager.ModuleExists(ModuleName) || Manager.IsModuleLoaded(ModuleName))
		{
			return true;
		}
	}
	for (const TCHAR* CVarName : SlimeGraphicsPrivate::DLSSEnableCVars)
	{
		if (HasCVar(CVarName))
		{
			return true;
		}
	}
	return false;
}

bool USlimeGraphicsSettings::IsDlssSupported() const
{
	if (!bNvidia)
	{
		return false;
	}
	const bool bRtxName = AdapterName.Contains(TEXT("RTX"), ESearchCase::IgnoreCase);
	return (bRtxName || bHardwareRayTracing) && IsDlssModuleAvailable();
}

bool USlimeGraphicsSettings::IsRtx40Or50() const
{
	if (!bNvidia)
	{
		return false;
	}
	static const TCHAR* Models[] = {
		TEXT("RTX 4050"), TEXT("RTX 4060"), TEXT("RTX 4070"), TEXT("RTX 4080"), TEXT("RTX 4090"),
		TEXT("RTX 5050"), TEXT("RTX 5060"), TEXT("RTX 5070"), TEXT("RTX 5080"), TEXT("RTX 5090")
	};
	for (const TCHAR* Model : Models)
	{
		if (AdapterName.Contains(Model, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	const bool bLaptop40 = AdapterName.Contains(TEXT("RTX 40"), ESearchCase::IgnoreCase)
		&& AdapterName.Contains(TEXT("Laptop"), ESearchCase::IgnoreCase);
	const bool bLaptop50 = AdapterName.Contains(TEXT("RTX 50"), ESearchCase::IgnoreCase)
		&& AdapterName.Contains(TEXT("Laptop"), ESearchCase::IgnoreCase);
	const bool bAda = AdapterName.Contains(TEXT("Ada"), ESearchCase::IgnoreCase)
		&& AdapterName.Contains(TEXT("RTX"), ESearchCase::IgnoreCase);
	const bool bBlackwell = AdapterName.Contains(TEXT("Blackwell"), ESearchCase::IgnoreCase);
	return bLaptop40 || bLaptop50 || bAda || bBlackwell;
}

bool USlimeGraphicsSettings::IsFrameGenSupported() const
{
	if (!IsDlssSupported() || !IsRtx40Or50())
	{
		return false;
	}
	for (const TCHAR* Name : SlimeGraphicsPrivate::FrameGenCVars)
	{
		if (HasCVar(Name))
		{
			return true;
		}
	}
	return FModuleManager::Get().ModuleExists(TEXT("StreamlineDLSSG"))
		|| FModuleManager::Get().IsModuleLoaded(TEXT("StreamlineDLSSG"));
}

bool USlimeGraphicsSettings::IsFsrPluginPresent() const
{
	if (FModuleManager::Get().ModuleExists(TEXT("FSR")) || FModuleManager::Get().IsModuleLoaded(TEXT("FSR")))
	{
		return true;
	}
	for (const TCHAR* Name : SlimeGraphicsPrivate::FSREnableCVars)
	{
		if (HasCVar(Name))
		{
			return true;
		}
	}
	return false;
}

int32 USlimeGraphicsSettings::RecommendQualityLevel() const
{
	if (bIntegratedGpu || !bHardwareRayTracing || (DedicatedVramMB > 0 && DedicatedVramMB < 6144))
	{
		return 0;
	}
	if (DedicatedVramMB <= 0)
	{
		return 2;
	}
	if (DedicatedVramMB < 8192)
	{
		return 1;
	}
	if (DedicatedVramMB < 12288)
	{
		return 2;
	}
	return 3;
}

int32 USlimeGraphicsSettings::GetQualityLevel() const
{
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	const int32 Level = Settings ? Settings->GetOverallScalabilityLevel() : 1;
	return FMath::Clamp(Level, 0, 3);
}

bool USlimeGraphicsSettings::HasExistingScalabilitySave() const
{
	if (!GConfig)
	{
		return false;
	}
	int32 Dummy = 0;
	return GConfig->GetInt(TEXT("ScalabilityGroups"), TEXT("sg.ViewDistanceQuality"), Dummy, GGameUserSettingsIni);
}

void USlimeGraphicsSettings::ApplyFirstRunQualityIfNeeded()
{
	if (bHasUserOrAutoQuality)
	{
		return;
	}
	if (HasExistingScalabilitySave())
	{
		bHasUserOrAutoQuality = true;
		Save();
		UE_LOG(LogSlimeFable, Log, TEXT("SlimeGraphics: keep existing scalability; skip first-run auto."));
		return;
	}
	AutoDetectQuality();
}

void USlimeGraphicsSettings::SetQualityLevel(int32 Level)
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetOverallScalabilityLevel(FMath::Clamp(Level, 0, 3));
		Settings->ApplySettings(false);
		Settings->SaveSettings();
		SetCVarInt(TEXT("r.RayTracing.ForceAllRayTracingEffects"), 0);
	}
	bHasUserOrAutoQuality = true;
	bHasCachedTsrScreenPercentage = false;
	CacheTsrScreenPercentage();
	if (Upscaler == ESlimeUpscaler::DLSS)
	{
		ApplyUpscaler();
	}
	Save();
}

void USlimeGraphicsSettings::AutoDetectQuality()
{
	const int32 Level = RecommendQualityLevel();
	UE_LOG(LogSlimeFable, Log,
		TEXT("SlimeGraphics: auto quality -> %s (%d) vram=%dMB RT=%d iGPU=%d"),
		SlimeGraphicsPrivate::QualityNames[Level], Level, DedicatedVramMB,
		bHardwareRayTracing ? 1 : 0, bIntegratedGpu ? 1 : 0);
	SetQualityLevel(Level);
}

float USlimeGraphicsSettings::ReadScreenPercentage() const
{
	if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
	{
		return Var->GetFloat();
	}
	return 100.f;
}

void USlimeGraphicsSettings::CacheTsrScreenPercentage() const
{
	CachedTsrScreenPercentage = FMath::Clamp(ReadScreenPercentage(), 25.f, 200.f);
	bHasCachedTsrScreenPercentage = true;
}

void USlimeGraphicsSettings::RestoreTsrScreenPercentage() const
{
	const float Restore = bHasCachedTsrScreenPercentage ? CachedTsrScreenPercentage : 100.f;
	SetCVarFloat(TEXT("r.ScreenPercentage"), Restore);
	UE_LOG(LogSlimeFable, Log, TEXT("SlimeGraphics: restore TSR screen percentage=%.1f"), Restore);
}

void USlimeGraphicsSettings::ApplyDlssMode() const
{
	FModuleManager& Manager = FModuleManager::Get();
	if (Manager.ModuleExists(TEXT("DLSSBlueprint")) && !Manager.IsModuleLoaded(TEXT("DLSSBlueprint")))
	{
		Manager.LoadModule(TEXT("DLSSBlueprint"));
	}
	if (Manager.ModuleExists(TEXT("DLSS")) && !Manager.IsModuleLoaded(TEXT("DLSS")))
	{
		Manager.LoadModule(TEXT("DLSS"));
	}

	if (!UDLSSLibrary::IsDLSSSupported())
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeGraphics: DLSS library not ready or GPU unsupported (Query=%d)."),
			static_cast<int32>(UDLSSLibrary::QueryDLSSSupport()));
		return;
	}

	if (!bHasCachedTsrScreenPercentage)
	{
		CacheTsrScreenPercentage();
	}

	UDLSSMode Mode = SlimeGraphicsPrivate::ToUDLSSMode(DLSSQuality);
	if (!UDLSSLibrary::IsDLSSModeSupported(Mode))
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeGraphics: DLSS mode %d unsupported, falling back to Quality."),
			static_cast<int32>(Mode));
		Mode = UDLSSMode::Quality;
		if (!UDLSSLibrary::IsDLSSModeSupported(Mode))
		{
			return;
		}
	}

	bool bModeSupported = false;
	float OptimalPercentage = 0.f;
	bool bFixed = false;
	float MinPercentage = 0.f;
	float MaxPercentage = 0.f;
	float UnusedSharpness = 0.f;
	UDLSSLibrary::GetDLSSModeInformation(
		Mode,
		SlimeGraphicsPrivate::QueryViewportSize(),
		bModeSupported,
		OptimalPercentage,
		bFixed,
		MinPercentage,
		MaxPercentage,
		UnusedSharpness);

	UDLSSLibrary::EnableDLSS(true);
	SetCVarInt(TEXT("r.NGX.DLSS.Enable"), 1);
	SetCVarInt(TEXT("r.TemporalAA.Upscaler"), 1);

	if (bModeSupported && OptimalPercentage > 1.f)
	{
		SetCVarFloat(TEXT("r.ScreenPercentage"), OptimalPercentage);
	}
	else if (Mode == UDLSSMode::DLAA)
	{
		SetCVarFloat(TEXT("r.ScreenPercentage"), 100.f);
	}

	UE_LOG(LogSlimeFable, Log,
		TEXT("SlimeGraphics: DLSS mode=%d screenPercentage=%.1f (optimal=%.1f supported=%d)"),
		static_cast<int32>(Mode), ReadScreenPercentage(), OptimalPercentage, bModeSupported ? 1 : 0);
}

void USlimeGraphicsSettings::ApplyUpscaler() const
{
	// UE 5.8 official DLSS may ghost emissives if NGXRHI assigns DepthInverted
	// instead of OR-ing it. Run Plugins/NVIDIA/enable_nvidia_dlss.py after dropping
	// the plugin to apply: DLSSFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_DepthInverted.
	SetCVarInt(TEXT("r.AntiAliasingMethod"), 4);
	SetCVarInt(TEXT("r.TemporalAA.Upsampling"), 1);

	const bool bWantDlss = Upscaler == ESlimeUpscaler::DLSS && IsDlssSupported();
	const bool bWantFsr = Upscaler == ESlimeUpscaler::FSR && IsFsrPluginPresent();

	for (const TCHAR* Name : SlimeGraphicsPrivate::FSREnableCVars)
	{
		SetCVarInt(Name, bWantFsr ? 1 : 0);
	}

	if (bWantDlss)
	{
		ApplyDlssMode();
		TWeakObjectPtr<const USlimeGraphicsSettings> WeakThis(this);
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([WeakThis](float)
			{
				const USlimeGraphicsSettings* Settings = WeakThis.Get();
				if (!Settings || Settings->GetUpscaler() != ESlimeUpscaler::DLSS)
				{
					return false;
				}
				Settings->ApplyDlssMode();
				return false;
			}),
			0.5f);
	}
	else
	{
		if (FModuleManager::Get().IsModuleLoaded(TEXT("DLSSBlueprint")))
		{
			UDLSSLibrary::EnableDLSS(false);
		}
		SetCVarInt(TEXT("r.NGX.DLSS.Enable"), 0);
		if (bHasCachedTsrScreenPercentage)
		{
			RestoreTsrScreenPercentage();
		}
	}

	const bool bWantFg = bWantDlss && bFrameGen && IsFrameGenSupported();
	for (const TCHAR* Name : SlimeGraphicsPrivate::FrameGenCVars)
	{
		SetCVarInt(Name, bWantFg ? 1 : 0);
	}
	for (const TCHAR* Name : SlimeGraphicsPrivate::ReflexCVars)
	{
		SetCVarInt(Name, bWantFg ? 1 : 0);
	}

	UE_LOG(LogSlimeFable, Log,
		TEXT("SlimeGraphics: apply upscaler=%d dlss=%d fsr=%d fg=%d sp=%.1f"),
		static_cast<int32>(Upscaler), bWantDlss ? 1 : 0, bWantFsr ? 1 : 0, bWantFg ? 1 : 0,
		ReadScreenPercentage());
}

bool USlimeGraphicsSettings::TrySetUpscaler(ESlimeUpscaler NewMode, FText& OutError)
{
	OutError = FText::GetEmpty();
	if (NewMode == ESlimeUpscaler::DLSS && !IsDlssSupported())
	{
		OutError = IsDlssModuleAvailable()
			? FText::FromString(TEXT("当前显卡不支持 DLSS"))
			: FText::FromString(TEXT("未安装 DLSS 插件"));
		return false;
	}
	if (NewMode == ESlimeUpscaler::FSR && !IsFsrPluginPresent())
	{
		OutError = FText::FromString(TEXT("FSR 待官方 UE 5.8 插件"));
		return false;
	}

	Upscaler = NewMode;
	if (Upscaler != ESlimeUpscaler::DLSS)
	{
		bFrameGen = false;
	}
	ApplyUpscaler();
	Save();
	return true;
}

void USlimeGraphicsSettings::CycleUpscaler()
{
	FText Error;
	if (Upscaler == ESlimeUpscaler::Off)
	{
		if (TrySetUpscaler(ESlimeUpscaler::DLSS, Error))
		{
			return;
		}
		if (TrySetUpscaler(ESlimeUpscaler::FSR, Error))
		{
			return;
		}
		return;
	}
	if (Upscaler == ESlimeUpscaler::DLSS)
	{
		if (TrySetUpscaler(ESlimeUpscaler::FSR, Error))
		{
			return;
		}
		TrySetUpscaler(ESlimeUpscaler::Off, Error);
		return;
	}
	TrySetUpscaler(ESlimeUpscaler::Off, Error);
}

void USlimeGraphicsSettings::CycleDLSSQuality()
{
	if (Upscaler != ESlimeUpscaler::DLSS)
	{
		return;
	}
	const uint8 Count = static_cast<uint8>(ESlimeDLSSQuality::DLAA) + 1;
	for (uint8 Step = 1; Step <= Count; ++Step)
	{
		const uint8 Next = (static_cast<uint8>(DLSSQuality) + Step) % Count;
		const UDLSSMode Mode = SlimeGraphicsPrivate::ToUDLSSMode(static_cast<ESlimeDLSSQuality>(Next));
		if (!FModuleManager::Get().IsModuleLoaded(TEXT("DLSSBlueprint")) || UDLSSLibrary::IsDLSSModeSupported(Mode))
		{
			DLSSQuality = static_cast<ESlimeDLSSQuality>(Next);
			break;
		}
	}
	ApplyUpscaler();
	Save();
}

bool USlimeGraphicsSettings::TrySetFrameGen(bool bEnable, FText& OutError)
{
	OutError = FText::GetEmpty();
	if (bEnable && !IsFrameGenSupported())
	{
		OutError = FText::FromString(TEXT("帧生成需要 RTX 40/50 与 Streamline 插件"));
		return false;
	}
	if (bEnable && Upscaler != ESlimeUpscaler::DLSS)
	{
		OutError = FText::FromString(TEXT("请先打开 DLSS 再开帧生成"));
		return false;
	}
	bFrameGen = bEnable;
	ApplyUpscaler();
	Save();
	return true;
}

void USlimeGraphicsSettings::ToggleFrameGen()
{
	FText Error;
	TrySetFrameGen(!bFrameGen, Error);
}

void USlimeGraphicsSettings::ApplyPixelStreaming() const
{
	SetCVarString(TEXT("PixelStreaming2.InputController"), TEXT("Any"));
	if (!bPixelStreaming)
	{
		SetCVarInt(TEXT("PixelStreaming2.AutoStartStream"), 0);
		if (IPixelStreaming2Module::IsAvailable())
		{
			IPixelStreaming2Module::Get().StopStreaming();
		}
		return;
	}
	if (PixelStreamingUrl.IsEmpty())
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeGraphics: pixel streaming on but URL empty."));
		return;
	}
	SetCVarInt(TEXT("PixelStreaming2.AutoStartStream"), 1);
	if (IConsoleVariable* UrlVar = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming2.ConnectionURL")))
	{
		UrlVar->Set(*PixelStreamingUrl, ECVF_SetByCode);
	}
	FString CmdStreamerId;
	if (!FParse::Value(FCommandLine::Get(), TEXT("PixelStreaming2.ID="), CmdStreamerId) || CmdStreamerId.IsEmpty())
	{
		SetCVarString(TEXT("PixelStreaming2.ID"), TEXT("slime-0"));
	}
	UE_LOG(LogSlimeFable, Log, TEXT("SlimeGraphics: pixel streaming URL=%s"), *PixelStreamingUrl);
	if (!IPixelStreaming2Module::IsAvailable())
	{
		FModuleManager::LoadModulePtr<IPixelStreaming2Module>(TEXT("PixelStreaming2"));
	}
	if (!IPixelStreaming2Module::IsAvailable())
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeGraphics: PixelStreaming2 module missing."));
		return;
	}
	IPixelStreaming2Module& Streaming = IPixelStreaming2Module::Get();
	if (Streaming.IsReady())
	{
		StartPixelStreamingNow();
	}
	else
	{
		UE_LOG(LogSlimeFable, Log, TEXT("SlimeGraphics: PixelStreaming2 not ready yet; will retry after factory init."));
	}
	TWeakObjectPtr<const USlimeGraphicsSettings> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([WeakThis](float)
		{
			const USlimeGraphicsSettings* Settings = WeakThis.Get();
			if (!Settings || !Settings->IsPixelStreamingEnabled())
			{
				return false;
			}
			if (!IPixelStreaming2Module::IsAvailable() || !IPixelStreaming2Module::Get().IsReady()
				|| IPixelStreaming2Module::Get().GetStreamerIds().IsEmpty())
			{
				return true;
			}
			Settings->StartPixelStreamingNow();
			return false;
		}),
		0.25f);
}

void USlimeGraphicsSettings::StartPixelStreamingNow() const
{
	if (!bPixelStreaming || PixelStreamingUrl.IsEmpty() || !IPixelStreaming2Module::IsAvailable())
	{
		return;
	}
	IPixelStreaming2Module& Streaming = IPixelStreaming2Module::Get();
	if (!Streaming.IsReady())
	{
		return;
	}
	TArray<FString> StreamerIds = Streaming.GetStreamerIds();
	if (StreamerIds.IsEmpty())
	{
		UE_LOG(LogSlimeFable, Log, TEXT("SlimeGraphics: default streamer not created yet."));
		return;
	}
	for (const FString& StreamerId : StreamerIds)
	{
		if (TSharedPtr<IPixelStreaming2Streamer> Streamer = Streaming.FindStreamer(StreamerId))
		{
			Streamer->StopStreaming();
			Streamer->SetConnectionURL(PixelStreamingUrl);
			UE_LOG(LogSlimeFable, Log, TEXT("SlimeGraphics: streamer '%s' -> %s"), *StreamerId, *PixelStreamingUrl);
		}
	}
	Streaming.StartStreaming();
}

bool USlimeGraphicsSettings::TrySetPixelStreaming(bool bEnable, FText& OutError)
{
	OutError = FText::GetEmpty();
	ApplyPixelStreamTargetUrl();
	if (bEnable && PixelStreamingUrl.IsEmpty())
	{
		OutError = FText::FromString(TEXT("未配置推流地址（PixelStreamingUrl）"));
		return false;
	}
	if (bEnable && !FModuleManager::Get().ModuleExists(TEXT("PixelStreaming2"))
		&& !IPixelStreaming2Module::IsAvailable())
	{
		OutError = FText::FromString(TEXT("未启用 PixelStreaming2 插件"));
		return false;
	}
	if (bEnable && PixelStreamTarget == ESlimePixelStreamTarget::Lan)
	{
		FText SignallingStatus;
		EnsureLocalSignalling(SignallingStatus);
	}
	bPixelStreaming = bEnable;
	ApplyPixelStreaming();
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USlimeInputSettings* Input = GI->GetSubsystem<USlimeInputSettings>())
		{
			Input->NotifyPixelStreamingChanged();
		}
	}
	Save();
	return true;
}

void USlimeGraphicsSettings::TogglePixelStreaming()
{
	FText Error;
	if (!TrySetPixelStreaming(!bPixelStreaming, Error) && !Error.IsEmpty())
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeGraphics: %s"), *Error.ToString());
	}
}

void USlimeGraphicsSettings::ApplyPixelStreamTargetUrl()
{
	if (PixelStreamTarget == ESlimePixelStreamTarget::Lan)
	{
		PixelStreamingUrl = SlimeGraphicsPrivate::LanStreamerUrl;
		return;
	}
	if (!SlimeGraphicsPrivate::IsUsableStreamingUrl(CloudPixelStreamingUrl))
	{
		CloudPixelStreamingUrl = SlimeGraphicsPrivate::DefaultCloudStreamerUrl;
	}
	PixelStreamingUrl = CloudPixelStreamingUrl;
}

FText USlimeGraphicsSettings::GetPixelStreamTargetDisplayName() const
{
	return PixelStreamTarget == ESlimePixelStreamTarget::Lan
		? FText::FromString(TEXT("推流目标：局域网"))
		: FText::FromString(TEXT("推流目标：云端"));
}

FText USlimeGraphicsSettings::GetBodySkinDisplayName() const
{
	switch (BodySkin)
	{
	case ESlimeBodySkin::Spectral: return FText::FromString(TEXT("史莱姆皮肤：光谱折射"));
	case ESlimeBodySkin::Volumetric: return FText::FromString(TEXT("史莱姆皮肤：体积折射"));
	default: return FText::FromString(TEXT("史莱姆皮肤：经典果冻"));
	}
}

void USlimeGraphicsSettings::SetBodySkin(ESlimeBodySkin NewSkin)
{
	if (NewSkin >= ESlimeBodySkin::COUNT)
	{
		NewSkin = ESlimeBodySkin::Spectral;
	}
	if (BodySkin == NewSkin)
	{
		return;
	}
	BodySkin = NewSkin;
	Save();
	OnBodySkinChanged.Broadcast(BodySkin);
	UE_LOG(LogSlimeFable, Log, TEXT("Body skin -> %s"), *GetBodySkinDisplayName().ToString());
}

void USlimeGraphicsSettings::CycleBodySkin()
{
	// Spectral (default) -> Volumetric -> Classic -> Spectral.
	switch (BodySkin)
	{
	case ESlimeBodySkin::Spectral: SetBodySkin(ESlimeBodySkin::Volumetric); break;
	case ESlimeBodySkin::Volumetric: SetBodySkin(ESlimeBodySkin::Classic); break;
	default: SetBodySkin(ESlimeBodySkin::Spectral); break;
	}
}

void USlimeGraphicsSettings::CyclePixelStreamTarget()
{
	PixelStreamTarget = PixelStreamTarget == ESlimePixelStreamTarget::Cloud
		? ESlimePixelStreamTarget::Lan
		: ESlimePixelStreamTarget::Cloud;
	ApplyPixelStreamTargetUrl();
	if (bPixelStreaming && PixelStreamTarget == ESlimePixelStreamTarget::Lan)
	{
		FText SignallingStatus;
		EnsureLocalSignalling(SignallingStatus);
	}
	if (bPixelStreaming)
	{
		ApplyPixelStreaming();
	}
	Save();
}

void USlimeGraphicsSettings::LoadPlayToken()
{
	FString EnvToken = FPlatformMisc::GetEnvironmentVariable(TEXT("SLIME_PLAY_TOKEN"));
	EnvToken.TrimStartAndEndInline();
	if (!EnvToken.IsEmpty())
	{
		PixelStreamingPlayToken = EnvToken;
		return;
	}

	FString SavedToken;
	if (GConfig && GConfig->GetString(
		SlimeGraphicsPrivate::ConfigSection,
		SlimeGraphicsPrivate::KeyPixelStreamingPlayToken,
		SavedToken,
		GGameUserSettingsIni))
	{
		SavedToken.TrimStartAndEndInline();
		if (!SavedToken.IsEmpty())
		{
			PixelStreamingPlayToken = SavedToken;
			return;
		}
	}

	const FString ConfigPath = FPaths::ProjectDir() / TEXT("Tools/PixelStreaming/worker.config.json");
	FString Json;
	if (FFileHelper::LoadFileToString(Json, *ConfigPath))
	{
		const FString Key = TEXT("\"token\"");
		int32 KeyIndex = Json.Find(Key, ESearchCase::IgnoreCase);
		if (KeyIndex != INDEX_NONE)
		{
			const int32 Colon = Json.Find(TEXT(":"), ESearchCase::IgnoreCase, ESearchDir::FromStart, KeyIndex);
			const int32 FirstQuote = Json.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Colon + 1);
			const int32 SecondQuote = FirstQuote != INDEX_NONE
				? Json.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, FirstQuote + 1)
				: INDEX_NONE;
			if (FirstQuote != INDEX_NONE && SecondQuote != INDEX_NONE && SecondQuote > FirstQuote + 1)
			{
				PixelStreamingPlayToken = Json.Mid(FirstQuote + 1, SecondQuote - FirstQuote - 1);
			}
		}
	}
}

FString USlimeGraphicsSettings::DetectLanIPv4()
{
	ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Sockets)
	{
		return FString();
	}

	TArray<TSharedPtr<FInternetAddr>> Addresses;
	if (Sockets->GetLocalAdapterAddresses(Addresses))
	{
		for (const TSharedPtr<FInternetAddr>& Addr : Addresses)
		{
			if (!Addr.IsValid() || !Addr->IsValid())
			{
				continue;
			}
			const FString Text = Addr->ToString(false);
			if (Text.StartsWith(TEXT("127.")) || Text.StartsWith(TEXT("169.254.")) || !Text.Contains(TEXT(".")))
			{
				continue;
			}
			return Text;
		}
	}

	bool bCanBindAll = false;
	const TSharedRef<FInternetAddr> Local = Sockets->GetLocalHostAddr(*GLog, bCanBindAll);
	if (Local->IsValid())
	{
		const FString Text = Local->ToString(false);
		if (!Text.StartsWith(TEXT("127.")))
		{
			return Text;
		}
	}
	return FString();
}

bool USlimeGraphicsSettings::IsLocalTcpOpen(int32 Port)
{
	ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Sockets)
	{
		return false;
	}
	TSharedRef<FInternetAddr> Addr = Sockets->CreateInternetAddr();
	bool bIsValid = false;
	Addr->SetIp(TEXT("127.0.0.1"), bIsValid);
	if (!bIsValid)
	{
		return false;
	}
	Addr->SetPort(Port);
	FSocket* Socket = Sockets->CreateSocket(NAME_Stream, TEXT("SlimeLanProbe"), false);
	if (!Socket)
	{
		return false;
	}
	Socket->SetNonBlocking(false);
	const bool bOk = Socket->Connect(*Addr);
	Socket->Close();
	Sockets->DestroySocket(Socket);
	return bOk;
}

bool USlimeGraphicsSettings::LaunchLocalSignalling()
{
	const FString Script = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectDir() / TEXT("Tools/PixelStreaming/start_local.py"));
	if (!FPaths::FileExists(Script))
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeGraphics: missing %s"), *Script);
		return false;
	}

	const FString Args = FString::Printf(TEXT("\"%s\" --ensure"), *Script);
	const FString WorkDir = FPaths::GetPath(Script);
	const TCHAR* Launchers[] = {
		TEXT("C:\\Windows\\py.exe"),
		TEXT("py"),
		TEXT("python"),
		TEXT("python3")
	};
	for (const TCHAR* Launcher : Launchers)
	{
		FProcHandle Proc = FPlatformProcess::CreateProc(
			Launcher,
			*Args,
			true,
			false,
			false,
			nullptr,
			0,
			*WorkDir,
			nullptr);
		if (Proc.IsValid())
		{
			FPlatformProcess::CloseProc(Proc);
			UE_LOG(LogSlimeFable, Log, TEXT("SlimeGraphics: launched local signalling via %s"), Launcher);
			return true;
		}
	}
	UE_LOG(LogSlimeFable, Warning, TEXT("SlimeGraphics: could not launch start_local.py (py/python missing)."));
	return false;
}

bool USlimeGraphicsSettings::EnsureLocalSignalling(FText& OutStatus)
{
	if (IsLocalTcpOpen(SlimeGraphicsPrivate::LanPlayerPort)
		&& IsLocalTcpOpen(SlimeGraphicsPrivate::LanStreamerPort))
	{
		OutStatus = FText::FromString(TEXT("本机播放页已就绪"));
		return true;
	}

	const double Now = FPlatformTime::Seconds();
	if (Now - LastLocalSignallingLaunchSeconds > 12.0)
	{
		if (!LaunchLocalSignalling())
		{
			OutStatus = FText::FromString(TEXT("无法启动本机播放页（缺少 py 或 start_local.py）"));
			return false;
		}
		LastLocalSignallingLaunchSeconds = Now;
	}

	for (int32 Attempt = 0; Attempt < 40; ++Attempt)
	{
		if (IsLocalTcpOpen(SlimeGraphicsPrivate::LanPlayerPort)
			&& IsLocalTcpOpen(SlimeGraphicsPrivate::LanStreamerPort))
		{
			OutStatus = FText::FromString(TEXT("本机播放页已启动"));
			return true;
		}
		FPlatformProcess::Sleep(0.25f);
	}

	OutStatus = FText::FromString(TEXT("本机播放页还在启动，稍后再打开复制的链接"));
	return false;
}

bool USlimeGraphicsSettings::CopyTextToClipboard(const FString& Text)
{
	if (Text.IsEmpty())
	{
		return false;
	}
	FPlatformApplicationMisc::ClipboardCopy(*Text);
	return true;
}

FString USlimeGraphicsSettings::GetCloudPlayUrl() const
{
	const FString Token = PixelStreamingPlayToken.IsEmpty() ? TEXT("change-me") : PixelStreamingPlayToken;
	return FString::Printf(TEXT("%s?k=%s"), SlimeGraphicsPrivate::CloudPlayPage, *Token);
}

FString USlimeGraphicsSettings::GetLanPlayUrl() const
{
	FString Ip = DetectLanIPv4();
	if (Ip.IsEmpty())
	{
		Ip = TEXT("127.0.0.1");
	}
	return FString::Printf(
		TEXT("http://%s:18880/player.html?StreamerId=slime-0&FakeMouseWithTouches=true&AutoConnect=true&AutoPlayVideo=true&WaitForStreamer=true&HideUI=true"),
		*Ip);
}

bool USlimeGraphicsSettings::CopyCloudPlayUrl(FText& OutStatus)
{
	const FString Url = GetCloudPlayUrl();
	if (!CopyTextToClipboard(Url))
	{
		OutStatus = FText::FromString(TEXT("复制失败"));
		return false;
	}
	OutStatus = FText::FromString(TEXT("已复制云端观看链接（不要打开 /ps/player）"));
	return true;
}

bool USlimeGraphicsSettings::CopyLanPlayUrl(FText& OutStatus)
{
	PixelStreamTarget = ESlimePixelStreamTarget::Lan;
	ApplyPixelStreamTargetUrl();
	FText SignallingStatus;
	if (!bPixelStreaming)
	{
		FText EnableError;
		TrySetPixelStreaming(true, EnableError);
	}
	const bool bReady = EnsureLocalSignalling(SignallingStatus);
	if (bPixelStreaming)
	{
		ApplyPixelStreaming();
		Save();
	}

	const FString Url = GetLanPlayUrl();
	if (!CopyTextToClipboard(Url))
	{
		OutStatus = FText::FromString(TEXT("复制失败"));
		return false;
	}
	OutStatus = bReady
		? FText::FromString(TEXT("已复制局域网链接，手机同一 WiFi 打开即可"))
		: SignallingStatus;
	return bReady;
}

FText USlimeGraphicsSettings::GetUpscalerDisplayName() const
{
	switch (Upscaler)
	{
	case ESlimeUpscaler::DLSS: return FText::FromString(TEXT("DLSS"));
	case ESlimeUpscaler::FSR: return FText::FromString(TEXT("FSR"));
	default: return FText::FromString(TEXT("关(TSR)"));
	}
}

FText USlimeGraphicsSettings::GetDLSSQualityDisplayName() const
{
	switch (DLSSQuality)
	{
	case ESlimeDLSSQuality::Balanced: return FText::FromString(TEXT("平衡"));
	case ESlimeDLSSQuality::Performance: return FText::FromString(TEXT("性能"));
	case ESlimeDLSSQuality::UltraPerformance: return FText::FromString(TEXT("超级性能"));
	case ESlimeDLSSQuality::DLAA: return FText::FromString(TEXT("DLAA"));
	default: return FText::FromString(TEXT("质量"));
	}
}

FText USlimeGraphicsSettings::GetStatusText() const
{
	const int32 Level = GetQualityLevel();
	const TCHAR* QualityName = SlimeGraphicsPrivate::QualityNames[FMath::Clamp(Level, 0, 3)];
	FString UpscalerPart = GetUpscalerDisplayName().ToString();
	if (Upscaler == ESlimeUpscaler::DLSS)
	{
		UpscalerPart += TEXT(" ");
		UpscalerPart += GetDLSSQualityDisplayName().ToString();
	}
	const TCHAR* TargetHint = PixelStreamTarget == ESlimePixelStreamTarget::Lan
		? TEXT("局域网（先跑 start_local.py）")
		: TEXT("云端（勿开 /ps/player）");
	return FText::FromString(FString::Printf(
		TEXT("当前：%s · 超分：%s · 帧生成%s · 像素流送%s · %s"),
		QualityName,
		*UpscalerPart,
		bFrameGen ? TEXT("开") : TEXT("关"),
		bPixelStreaming ? TEXT("开") : TEXT("关"),
		TargetHint));
}

void USlimeGraphicsSettings::Save()
{
	if (!GConfig)
	{
		return;
	}
	GConfig->SetInt(SlimeGraphicsPrivate::ConfigSection, SlimeGraphicsPrivate::KeyUpscaler, static_cast<int32>(Upscaler), GGameUserSettingsIni);
	GConfig->SetInt(SlimeGraphicsPrivate::ConfigSection, SlimeGraphicsPrivate::KeyDLSSQuality, static_cast<int32>(DLSSQuality), GGameUserSettingsIni);
	GConfig->SetBool(SlimeGraphicsPrivate::ConfigSection, SlimeGraphicsPrivate::KeyFrameGen, bFrameGen, GGameUserSettingsIni);
	GConfig->SetBool(SlimeGraphicsPrivate::ConfigSection, SlimeGraphicsPrivate::KeyHasQuality, bHasUserOrAutoQuality, GGameUserSettingsIni);
	GConfig->SetBool(SlimeGraphicsPrivate::ConfigSection, SlimeGraphicsPrivate::KeyPixelStreaming, bPixelStreaming, GGameUserSettingsIni);
	GConfig->SetString(SlimeGraphicsPrivate::ConfigSection, SlimeGraphicsPrivate::KeyPixelStreamingUrl, *CloudPixelStreamingUrl, GGameUserSettingsIni);
	GConfig->SetInt(SlimeGraphicsPrivate::ConfigSection, SlimeGraphicsPrivate::KeyPixelStreamTarget, static_cast<int32>(PixelStreamTarget), GGameUserSettingsIni);
	GConfig->SetInt(SlimeGraphicsPrivate::ConfigSection, SlimeGraphicsPrivate::KeyBodySkin, static_cast<int32>(BodySkin), GGameUserSettingsIni);
	if (!PixelStreamingPlayToken.IsEmpty())
	{
		GConfig->SetString(SlimeGraphicsPrivate::ConfigSection, SlimeGraphicsPrivate::KeyPixelStreamingPlayToken, *PixelStreamingPlayToken, GGameUserSettingsIni);
	}
	GConfig->Flush(false, GGameUserSettingsIni);
}

void USlimeGraphicsSettings::Load()
{
	if (!GConfig)
	{
		return;
	}
	int32 UpscalerInt = 0;
	if (GConfig->GetInt(SlimeGraphicsPrivate::ConfigSection, SlimeGraphicsPrivate::KeyUpscaler, UpscalerInt, GGameUserSettingsIni))
	{
		Upscaler = static_cast<ESlimeUpscaler>(FMath::Clamp(UpscalerInt, 0, static_cast<int32>(ESlimeUpscaler::FSR)));
	}
	int32 QualityInt = 0;
	if (GConfig->GetInt(SlimeGraphicsPrivate::ConfigSection, SlimeGraphicsPrivate::KeyDLSSQuality, QualityInt, GGameUserSettingsIni))
	{
		DLSSQuality = static_cast<ESlimeDLSSQuality>(FMath::Clamp(QualityInt, 0, static_cast<int32>(ESlimeDLSSQuality::DLAA)));
	}
	GConfig->GetBool(SlimeGraphicsPrivate::ConfigSection, SlimeGraphicsPrivate::KeyFrameGen, bFrameGen, GGameUserSettingsIni);
	GConfig->GetBool(SlimeGraphicsPrivate::ConfigSection, SlimeGraphicsPrivate::KeyHasQuality, bHasUserOrAutoQuality, GGameUserSettingsIni);
	GConfig->GetString(TEXT("SlimeGraphics"), SlimeGraphicsPrivate::KeyPixelStreamingUrl, CloudPixelStreamingUrl, GGameIni);
	CloudPixelStreamingUrl = SlimeGraphicsPrivate::Unquote(CloudPixelStreamingUrl);
	FString SavedUrl;
	if (GConfig->GetString(SlimeGraphicsPrivate::ConfigSection, SlimeGraphicsPrivate::KeyPixelStreamingUrl, SavedUrl, GGameUserSettingsIni)
		&& SlimeGraphicsPrivate::IsUsableStreamingUrl(SlimeGraphicsPrivate::Unquote(SavedUrl)))
	{
		SavedUrl = SlimeGraphicsPrivate::Unquote(SavedUrl);
		if (SavedUrl.Equals(SlimeGraphicsPrivate::LanStreamerUrl, ESearchCase::IgnoreCase))
		{
			PixelStreamTarget = ESlimePixelStreamTarget::Lan;
		}
		else
		{
			CloudPixelStreamingUrl = SavedUrl;
		}
	}
	if (!SlimeGraphicsPrivate::IsUsableStreamingUrl(CloudPixelStreamingUrl))
	{
		CloudPixelStreamingUrl = SlimeGraphicsPrivate::DefaultCloudStreamerUrl;
	}
	int32 TargetInt = static_cast<int32>(ESlimePixelStreamTarget::Cloud);
	if (GConfig->GetInt(SlimeGraphicsPrivate::ConfigSection, SlimeGraphicsPrivate::KeyPixelStreamTarget, TargetInt, GGameUserSettingsIni))
	{
		PixelStreamTarget = static_cast<ESlimePixelStreamTarget>(
			FMath::Clamp(TargetInt, 0, static_cast<int32>(ESlimePixelStreamTarget::Lan)));
	}
	int32 SkinInt = static_cast<int32>(ESlimeBodySkin::Spectral);
	if (GConfig->GetInt(SlimeGraphicsPrivate::ConfigSection, SlimeGraphicsPrivate::KeyBodySkin, SkinInt, GGameUserSettingsIni))
	{
		BodySkin = static_cast<ESlimeBodySkin>(
			FMath::Clamp(SkinInt, 0, static_cast<int32>(ESlimeBodySkin::COUNT) - 1));
	}
	LoadPlayToken();
	FString CmdUrl;
	const bool bParsedCmdUrl =
		FParse::Value(FCommandLine::Get(), TEXT("PixelStreamingConnectionURL="), CmdUrl)
		|| FParse::Value(FCommandLine::Get(), TEXT("PixelStreaming2.ConnectionURL="), CmdUrl);
	CmdUrl = SlimeGraphicsPrivate::Unquote(CmdUrl);
	if (bParsedCmdUrl && SlimeGraphicsPrivate::IsUsableStreamingUrl(CmdUrl))
	{
		PixelStreamingUrl = CmdUrl;
		PixelStreamTarget = CmdUrl.Equals(SlimeGraphicsPrivate::LanStreamerUrl, ESearchCase::IgnoreCase)
			? ESlimePixelStreamTarget::Lan
			: ESlimePixelStreamTarget::Cloud;
		if (PixelStreamTarget == ESlimePixelStreamTarget::Cloud)
		{
			CloudPixelStreamingUrl = CmdUrl;
		}
	}
	else
	{
		ApplyPixelStreamTargetUrl();
	}
	GConfig->GetBool(SlimeGraphicsPrivate::ConfigSection, SlimeGraphicsPrivate::KeyPixelStreaming, bPixelStreaming, GGameUserSettingsIni);
	int32 CmdAutoStart = 0;
	if ((FParse::Value(FCommandLine::Get(), TEXT("PixelStreamingAutoStartStream="), CmdAutoStart)
			|| FParse::Value(FCommandLine::Get(), TEXT("PixelStreaming2.AutoStartStream="), CmdAutoStart))
		&& CmdAutoStart != 0
		&& SlimeGraphicsPrivate::IsUsableStreamingUrl(PixelStreamingUrl))
	{
		bPixelStreaming = true;
	}

	if (Upscaler == ESlimeUpscaler::DLSS && !IsDlssSupported())
	{
		Upscaler = ESlimeUpscaler::Off;
		bFrameGen = false;
	}
	if (Upscaler == ESlimeUpscaler::FSR && !IsFsrPluginPresent())
	{
		Upscaler = ESlimeUpscaler::Off;
	}
	if (bFrameGen && !IsFrameGenSupported())
	{
		bFrameGen = false;
	}
}
