// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/SlimeGraphicsSettings.h"

#include "Engine/Engine.h"
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
#include "SlimeFable.h"

namespace SlimeGraphicsPrivate
{
	static const TCHAR* ConfigSection = TEXT("SlimeGraphics");
	static const TCHAR* KeyUpscaler = TEXT("Upscaler");
	static const TCHAR* KeyDLSSQuality = TEXT("DLSSQuality");
	static const TCHAR* KeyFrameGen = TEXT("FrameGen");
	static const TCHAR* KeyHasQuality = TEXT("bHasUserOrAutoQuality");
	static const TCHAR* KeyPixelStreaming = TEXT("bPixelStreaming");
	static const TCHAR* KeyPixelStreamingUrl = TEXT("PixelStreamingUrl");

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
	SetCVarString(TEXT("PixelStreaming2.InputController"), TEXT("Host"));
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
	bPixelStreaming = bEnable;
	ApplyPixelStreaming();
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
	return FText::FromString(FString::Printf(
		TEXT("当前：%s · 超分：%s · 帧生成%s · 像素流送%s"),
		QualityName,
		*UpscalerPart,
		bFrameGen ? TEXT("开") : TEXT("关"),
		bPixelStreaming ? TEXT("开") : TEXT("关")));
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
	GConfig->SetString(SlimeGraphicsPrivate::ConfigSection, SlimeGraphicsPrivate::KeyPixelStreamingUrl, *PixelStreamingUrl, GGameUserSettingsIni);
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
	GConfig->GetString(TEXT("SlimeGraphics"), SlimeGraphicsPrivate::KeyPixelStreamingUrl, PixelStreamingUrl, GGameIni);
	PixelStreamingUrl = SlimeGraphicsPrivate::Unquote(PixelStreamingUrl);
	FString SavedUrl;
	if (GConfig->GetString(SlimeGraphicsPrivate::ConfigSection, SlimeGraphicsPrivate::KeyPixelStreamingUrl, SavedUrl, GGameUserSettingsIni)
		&& SlimeGraphicsPrivate::IsUsableStreamingUrl(SlimeGraphicsPrivate::Unquote(SavedUrl)))
	{
		PixelStreamingUrl = SlimeGraphicsPrivate::Unquote(SavedUrl);
	}
	FString CmdUrl;
	const bool bParsedCmdUrl =
		FParse::Value(FCommandLine::Get(), TEXT("PixelStreamingConnectionURL="), CmdUrl)
		|| FParse::Value(FCommandLine::Get(), TEXT("PixelStreaming2.ConnectionURL="), CmdUrl);
	CmdUrl = SlimeGraphicsPrivate::Unquote(CmdUrl);
	if (bParsedCmdUrl && SlimeGraphicsPrivate::IsUsableStreamingUrl(CmdUrl))
	{
		PixelStreamingUrl = CmdUrl;
	}
	else if (IConsoleVariable* UrlCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming2.ConnectionURL")))
	{
		const FString CVarUrl = SlimeGraphicsPrivate::Unquote(UrlCVar->GetString());
		if (SlimeGraphicsPrivate::IsUsableStreamingUrl(CVarUrl))
		{
			PixelStreamingUrl = CVarUrl;
		}
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
