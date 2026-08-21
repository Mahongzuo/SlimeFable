/*
* Copyright (c) 2022 - 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
* NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
* property and proprietary rights in and to this material, related
* documentation and any modifications thereto. Any use, reproduction,
* disclosure or distribution of this material and related documentation
* without an express license agreement from NVIDIA CORPORATION or
* its affiliates is strictly prohibited.
*/

#include "StreamlineRHI.h"
#include "StreamlineAPI.h"
#include "StreamlineConversions.h"
#include "StreamlineExtension.h"
#include "StreamlineRHIPrivate.h"
#include "StreamlineSettings.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Settings/LevelEditorPlaySettings.h"
#endif

#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/EngineVersion.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"


#include "Internationalization/Regex.h"
#include "Templates/Function.h"

#include "sl_deepdvc.h"
#include "sl_dlss_g.h"
#include "sl.h"

static TAutoConsoleVariable<int32> CVarStreamlineMaxNumSwapchainProxies(
	TEXT("r.Streamline.MaxNumSwapchainProxies"),
	-1,
	TEXT("Determines how many Streamline swapchain proxies can be created. This impacts compatibility with some Streamline features that have restrictions on that\n")
	TEXT(" -1: automatic, depending on enabled Streamline features (default)\n")
	TEXT(" 0: no swap chain proxy. Likely means features needing one won't work")
	TEXT(" 1..n: only create a Streamline swapchain proxy for that many swapchains/windows"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<bool> CVarStreamlineFilterRedundantSetOptionsCalls(
	TEXT("r.Streamline.FilterRedundantSetOptionsCalls"),
	true,
	TEXT("Determines whether the UE plugin filters redundant calls into\n")
	TEXT(" 0: call every streamline sl{Feature}SetOptions function, regardless of whether UE plugin side changed or not. Helpful for debugging. Can also be override with -sl{no}filter command line option  \n")
	TEXT(" 1: only call sl{Feature}SetOptions when the UE plugin side changed(default)"),
	ECVF_RenderThreadSafe);

#ifndef STREAMLINE_PLATFORM_DIR
#error "You are not supposed to get to this point, check your build configuration."
#endif

static const FString PlatformDir = STREAMLINE_PLATFORM_DIR;

// Epic requested a CVar to control whether the plugin will perform initialization or not.
// This allows the plugin to be included in a project and active but allows for it to not do anything
// at runtime.
static TAutoConsoleVariable<bool> CVarStreamlineInitializePlugin(
	TEXT("r.Streamline.InitializePlugin"),
	true,
	TEXT("Enable/disable initializing the Streamline plugin (default = true)"),
	ECVF_ReadOnly);

DEFINE_LOG_CATEGORY(LogStreamlineRHI);
DEFINE_LOG_CATEGORY_STATIC(LogStreamlineAPI, Log, All);

#define LOCTEXT_NAMESPACE "StreamlineRHI"

/*UE log verbosity can be adjusted in non-shipping builds in various ways as described in 
https://dev.epicgames.com/community/snippets/3GoB/unreal-engine-how-to-set-log-verbosity-via-command-line-with-logcmds?locale=en-en

using -LogCmds:
-LogCmds="LogStreamlineAPI VeryVerbose, LogStreamlineRHI Verbose" 
with CVar syntax:
-ini:Engine:[Core.Log]:LogStreamlineAPI=VeryVerbose
Using a config file:
e.g. in BaseEngine/DefaultEngine.ini

[Core.Log] 
; This can be used to change the default log level for engine logs to help with debugging
LogStreamlineAPI=VeryVerbose
LogStreamlineRHI=Verbose

*/

static void StreamlineLogSink(sl::LogType InSLVerbosity, const char* InSLMessage)
{
#if !NO_LOGGING
	FString Message(FString(UTF8_TO_TCHAR(InSLMessage)).TrimEnd());

	static_assert(uint32_t(sl::LogType::eCount)  == 3U, "sl::LogType enum value mismatch. Dear NVIDIA Streamline plugin developer, please update this code!" ) ;
	static_assert(uint32_t(ELogVerbosity::Type::NumVerbosity) == 8U, "(ELogVerbosity::Type enum value mismatch. Dear NVIDIA Streamline plugin developer, please update this code!");

	ELogVerbosity::Type UEVerbosity;

	switch (InSLVerbosity)
	{
	default:
	case sl::LogType::eInfo:
		UEVerbosity = ELogVerbosity::Log;
		break;
	case sl::LogType::eWarn:
		UEVerbosity = ELogVerbosity::Warning;
		break;
	case sl::LogType::eError:
		UEVerbosity = ELogVerbosity::Error;
		break;
	}

	// the SL log messages have their SL SDK file & function name embedded but we are not matching those to insulate us from any shuffling around on the SDK side
	// e.g. for [15-20-52][streamline][warn][tid:26560][0s:582ms:945us]commonEntry.cpp:814[getNGXFeatureRequirements] ngxResult not implemented
	// we only filter for "ngxResult not implemented"

	auto MatchesAnyFilter = [](const FString& Message, const TArray<const TCHAR*>& Filters) -> bool
	{
		for (const TCHAR* Filter : Filters)
		{
			if (FRegexMatcher(FRegexPattern(Filter), Message).FindNext())
			{
				return true;
			}
		}
		return false;
	};

	// SL thinks those are "warnings" but we think they are "info"/Log
	const TArray<const TCHAR*> LogFilters = 
	{
		// SL reports this as sl::LogType::eWarn but we are downgrading here to log so automation testing doesn't fail
		// That is expected to only happen once during startup
		TEXT("Repeated slDLSSGSetOptions() call for the frame (\\d+). A redundant call or a race condition with Present()"),
	};

	// Those
	const TArray<const TCHAR*> VerboseFilters = 
	{
		TEXT("ngxResult not implemented")
		, TEXT("Keyboard manager disabled in production")
		, TEXT("Frame rate over (\\d+), reseting frame timer") // no need to brag
		, TEXT("Couldn't lock the mutex on sync present - will skip the present.")
		, TEXT("FC feedback: (\\d+)")
		, TEXT(" Achieved (.*) FC feedback state")
		, TEXT("Invalid no warp resource extent, IF optionally specified by the client!Either extent not provided or one of the extent dimensions(0 x 0) is incorrectly zero.Resetting extent to full no warp resource size(0 x 0)")
	};

	const TArray<const TCHAR*> VeryVerboseFilters = 
	{
		// This is just spam
		TEXT("error: failed to load NGXCore")
		 
		// We are not using DLSS-SR/RR in Streamline so we have no need for those
		, TEXT("DLSSD feature is not supported.Please check if you have a valid nvngx_dlssd.dll or your driver is supporting DLSSD.")
		, TEXT("Ignoring plugin 'sl.dlss_d' since it is was not requested by the host")
		, TEXT("Feature 'kFeatureDLSS' is not sharing required data")

		// With SL 2.8, SL 2.9 and DLSS-FG off we get this EVERY frame when we are not using the legacy slSetTag plugin setting.
		, TEXT("SL resource tags for frame (\\d+) not set yet!")
	};

	if (MatchesAnyFilter(Message, LogFilters))
	{
		UEVerbosity = ELogVerbosity::Log;
	}
	else if (MatchesAnyFilter(Message, VerboseFilters))
	{
		UEVerbosity = ELogVerbosity::Verbose;
	}
	else if (MatchesAnyFilter(Message, VeryVerboseFilters))
	{
		UEVerbosity = ELogVerbosity::VeryVerbose;
	}

	//Switch it up since UE_LOG needs the verbosity as a compile time constant
	switch (UEVerbosity)
	{
		case ELogVerbosity::Fatal:
			UE_LOG(LogStreamlineAPI, Fatal, TEXT("%s"), *Message);
			break;

		case ELogVerbosity::Error:
			UE_LOG(LogStreamlineAPI, Error, TEXT("%s"), *Message);
			break;
		case ELogVerbosity::Warning:
			UE_LOG(LogStreamlineAPI, Warning, TEXT("%s"), *Message);
			break;

		case ELogVerbosity::Display:
			UE_LOG(LogStreamlineAPI, Display, TEXT("%s"), *Message);
			break;

		default: /* fall through*/
		case ELogVerbosity::Log:
			UE_LOG(LogStreamlineAPI, Log, TEXT("%s"), *Message);
			break;
		
		case ELogVerbosity::Verbose:
			UE_LOG(LogStreamlineAPI, Verbose, TEXT("%s"), *Message);
			break;
		case ELogVerbosity::VeryVerbose:
			UE_LOG(LogStreamlineAPI, VeryVerbose, TEXT("%s"), *Message);
			break;
	}
#endif
}

static bool bIsStreamlineInitialized = false;

static int32 GetNGXAppID(bool bIsDLSSPluginEnabled)
{
	check(GConfig);

	// Streamline plugin NGX app ID
	int32 SLNGXAppID = 0;
	GConfig->GetInt(TEXT("/Script/StreamlineRHI.StreamlineSettings"), TEXT("NVIDIANGXApplicationId"), SLNGXAppID, GEngineIni);

	if (!bIsDLSSPluginEnabled)
	{
		return SLNGXAppID;
	}

	// DLSS-SR plugin NGX app ID
	int32 DLSSSRNGXAppID = 0;
	GConfig->GetInt(TEXT("/Script/DLSS.DLSSSettings"), TEXT("NVIDIANGXApplicationId"), DLSSSRNGXAppID, GEngineIni);

	int32 NGXAppID = 0;
	if (DLSSSRNGXAppID == SLNGXAppID)
	{
		NGXAppID = SLNGXAppID;
	}
	else if (DLSSSRNGXAppID == 0)
	{
		NGXAppID = SLNGXAppID;
		UE_LOG(LogStreamlineRHI, Warning, TEXT("Using NGX app ID %d from Streamline plugin, may affect DLSS-SR even though NGX app ID is not set in DLSS-SR plugin"), NGXAppID);
	}
	else if (SLNGXAppID == 0)
	{
		NGXAppID = DLSSSRNGXAppID;
		UE_LOG(LogStreamlineRHI, Warning, TEXT("Using NGX app ID %d from DLSS-SR plugin, may affect DLSS-FG even though NGX app ID is not set in Streamline plugin"), NGXAppID);
	}
	else
	{
		NGXAppID = SLNGXAppID;
		UE_LOG(LogStreamlineRHI, Error, TEXT("NGX app ID mismatch! %d in DLSS-SR plugin, %d in Streamline plugin, using %d"), DLSSSRNGXAppID, SLNGXAppID, NGXAppID);
	}

	return NGXAppID;
}

// TODO: the derived RHIs will set this to true during their initialization
bool FStreamlineRHI::bIsIncompatibleAPICaptureToolActive = false;
TArray<sl::Feature> FStreamlineRHI::FeaturesRequestedAtSLInitTime;
FSLFrameTokenProvider::FSLFrameTokenProvider() : Section()
{
	// truncated to 32 bits because that's all SL stores
	LastFrameCounter = static_cast<uint32_t>(GFrameCounter);
	SLgetNewFrameToken(FrameToken, &LastFrameCounter);
}

sl::FrameToken* FSLFrameTokenProvider::GetTokenForFrame(uint64 FrameCounter)
{
	uint32_t FrameCounter32 = static_cast<uint32_t>(FrameCounter);
	FScopeLock Lock(&Section);
	if (FrameCounter32 == LastFrameCounter)
	{
		return FrameToken;
	}

	// this should be safe, we can create multiple tokens to track the same frame
	LastFrameCounter = FrameCounter32;
	SLgetNewFrameToken(FrameToken, &LastFrameCounter);

	return FrameToken;
}


FStreamlineRHI::FStreamlineRHI(const FStreamlineRHICreateArguments& Arguments)
	: DynamicRHI(Arguments.DynamicRHI), FrameTokenProvider(MakeUnique<FSLFrameTokenProvider>())
{
	UE_LOG(LogStreamlineRHI, Log, TEXT("%s Enter"), ANSI_TO_TCHAR(__FUNCTION__));

#if WITH_EDITOR
	BeginPIEHandle = FEditorDelegates::BeginPIE.AddLambda([this](bool bIsSimulating) { OnBeginPIE(bIsSimulating); });
	EndPIEHandle = FEditorDelegates::EndPIE.AddLambda([this](bool bIsSimulating) { OnEndPIE(bIsSimulating); });
#endif
	
	UE_LOG(LogStreamlineRHI, Log, TEXT("%s Leave"), ANSI_TO_TCHAR(__FUNCTION__));
}

#if WITH_EDITOR
void FStreamlineRHI::OnBeginPIE(const bool bIsSimulating)
{
	// ULevelEditorPlaySettings::LastExecutedPlayModeType gets set in SetLastExecutedPlayMode in\Engine\Source\Editor\UnrealEd\Private\Kismet2\DebuggerCommands.cpp as part of PIE startup sequence
	const EPlayModeType PlayMode = GetDefault<ULevelEditorPlaySettings>()->LastExecutedPlayModeType;
	int32 PlayNumberOfClients; 
	GetDefault<ULevelEditorPlaySettings>()->GetPlayNumberOfClients(PlayNumberOfClients);
	const bool bIsMultiplayer = 1 != PlayNumberOfClients;

	// PlayMode_InEditorFloating gets its own swap chain in this process, so hooking works.
	// PlayMode_InNewProcess launches a separate process that runs in non-editor mode,
	// where swap chain proxy & DLSS-FG are enabled by default. We must NOT install a
	// swap chain proxy in the current editor process for that mode, or the next
	// notification/menu window could accidentally get DLSS-FG enabled.
	
	const bool bIsSupportedMode = !bIsMultiplayer && !bIsSimulating && (PlayMode == EPlayModeType::PlayMode_InEditorFloating);
	// elsewhere we check this for creation of swapchain proxies
	bIsSupportedPIEActive = bIsSupportedMode; 
	// elsewhere we show on screen warnings based on this. 
	bIsUnsupportedPIEActive = !bIsSupportedMode && (PlayMode != EPlayModeType::PlayMode_InNewProcess);

	// For Standalone we don't want to create a swapchain proxy, but we also don't want to show the warning
	if (PlayMode == EPlayModeType::PlayMode_InNewProcess)
	{
		check(bIsSupportedPIEActive == true && bIsUnsupportedPIEActive == false);
	}

	if (bIsUnsupportedPIEActive)
	{
		UE_LOG(LogStreamlineRHI, Warning, TEXT("PIE mode (%s) is not supported for Streamline features that require swap chain hooking in the editor process. Please use %s single player instead, or %s."),
			*UEnum::GetValueAsString(PlayMode),
			*UEnum::GetValueAsString(EPlayModeType::PlayMode_InEditorFloating),
			*UEnum::GetValueAsString(EPlayModeType::PlayMode_InNewProcess));
	}
}

void FStreamlineRHI::OnEndPIE(const bool bIsSimulating)
{
	const ULevelEditorPlaySettings* PlaySettings = GetDefault<ULevelEditorPlaySettings>();
	const EPlayModeType PlayMode = PlaySettings->LastExecutedPlayModeType;
	const UEnum* Enum = StaticEnum<EPlayModeType>();

	bIsSupportedPIEActive = false;
	bIsUnsupportedPIEActive = false;

	UE_LOG(LogStreamlineRHI, Verbose, TEXT("%s %s PlayMode = %s (%u) bIsSupportedPIEActive=%u bIsUnsupportedPIEActive=%u"), ANSI_TO_TCHAR(__FUNCTION__), *CurrentThreadName(), *Enum->GetDisplayNameTextByValue(int64(PlayMode)).ToString(), PlayMode, bIsSupportedPIEActive, bIsUnsupportedPIEActive);
}
#endif

bool FStreamlineRHI::IsSwapchainHookingAllowed() const
{
	if (!IsDLSSGSupportedByRHI())
	{
		return false;
	}
	// We either have a maximum or are set to -1 (auto) depending on which feature is enabled
	if (const int32 MaxNumSwapchainProxies = GetMaxNumSwapchainProxies())
	{
		if (NumActiveSwapchainProxies >= MaxNumSwapchainProxies)
		{
			return false;
		}
	}
	else // or we have a maximum of 0
	{
		return false;
	}

#if WITH_EDITOR
	if (GIsEditor)
	{
#if ENGINE_MAJOR_VERSION == 4
		return false;
#endif
		if (bIsSupportedPIEActive)
		{
			EStreamlineSettingOverride PIEOverride = GetDefault<UStreamlineOverrideSettings>()->EnableDLSSFGInPlayInEditorViewportsOverride;
			if (PIEOverride == EStreamlineSettingOverride::UseProjectSettings)
			{
				return GetDefault<UStreamlineSettings>()->bEnableDLSSFGInPlayInEditorViewports;
			}
			else
			{
				return PIEOverride == EStreamlineSettingOverride::Enabled;
			}
		}
		return false;
	}
#endif
	return true;
}

int32 FStreamlineRHI::GetMaxNumSwapchainProxies() const
{
	// TODO figure out the actual thread used, and/or try to get all of that stuff run on one thread
	const int32 MaxNumSwapchainProxies = CVarStreamlineMaxNumSwapchainProxies.GetValueOnAnyThread();

	// automatic 
	if (MaxNumSwapchainProxies == -1)
	{
		// TODO make this depend on the required features and their limitations.
		return 1;
	}
	else
	{
		return MaxNumSwapchainProxies;
	}
}

void  FStreamlineRHI::ValidateNumSwapchainProxies(const char* CallSite) const
{
	if (NumActiveSwapchainProxies < 0 || NumActiveSwapchainProxies > GetMaxNumSwapchainProxies())
	{
		UE_LOG(LogStreamlineRHI, Error, TEXT("%s NumActiveSwapchainProxies=%d is outside of the valid range of [0, %d]. This can cause instability, particularly in the editor when multiple windows are created and destroyed. NVIDIA would appreciate a report to dlss-support@nvidia.com"), ANSI_TO_TCHAR(CallSite), NumActiveSwapchainProxies, /*GetMaxNumSwapchainProxies()*/ 1);
	}
}

bool FStreamlineRHI::IsSwapchainProviderInstalled() const
{
	return bIsSwapchainProviderInstalled;
}

void FStreamlineRHI::ReleaseStreamlineResourcesForAllFeatures(uint32 ViewID)
{
	for (sl::Feature Feature : LoadedFeatures)
	{
		//UE_LOG(LogStreamlineRHI, Log, TEXT("%s %u (skipped)"), ANSI_TO_TCHAR(__FUNCTION__), ViewID);
		SLFreeResources(Feature, ViewID);
	}
}

void FStreamlineRHI::PostPlatformRHICreateInit()
{
	UE_LOG(LogStreamlineRHI, Verbose, TEXT("%s Enter"), ANSI_TO_TCHAR(__FUNCTION__));
	
	UE_LOG(LogStreamlineRHI, Log, TEXT("RequestedFeatures = %s)"),
		*FString::JoinBy(FeaturesRequestedAtSLInitTime, TEXT(", "), [](const sl::Feature& Feature) { return FString::Printf(TEXT("%s (%u)"), ANSI_TO_TCHAR(sl::getFeatureAsStr(Feature)), Feature); }));

	SupportedFeatures = FeaturesRequestedAtSLInitTime.FilterByPredicate([this](sl::Feature Feature)
		{ return SLisFeatureSupported(Feature, *GetAdapterInfo()) == sl::Result::eOk; });

	UE_LOG(LogStreamlineRHI, Log, TEXT("SupportedFeatures = %s"),
		*FString::JoinBy(SupportedFeatures, TEXT(", "), [](const sl::Feature& Feature)
			{ return FString::Printf(TEXT("%s (%u)"), ANSI_TO_TCHAR(sl::getFeatureAsStr(Feature)), Feature); }));

	LoadedFeatures = SupportedFeatures.FilterByPredicate([](sl::Feature Feature)
		{
		bool bIsLoaded = false;
		SLisFeatureLoaded(Feature, bIsLoaded);
		return bIsLoaded;
		});

	UE_LOG(LogStreamlineRHI, Log, TEXT("LoadedFeatures = %s"),
		*FString::JoinBy(LoadedFeatures, TEXT(", "), [](const sl::Feature& Feature) { return FString::Printf(TEXT("%s (%u)"), ANSI_TO_TCHAR(sl::getFeatureAsStr(Feature)), Feature); }));

	UE_LOG(LogStreamlineRHI, Verbose, TEXT("%s Leave"), ANSI_TO_TCHAR(__FUNCTION__));
}

void FStreamlineRHI::OnSwapchainCreated(void* InNativeSwapchain, bool bIsKnownStreamlineProxy) const
{
	UE_LOG(LogStreamlineRHI, Verbose, TEXT("%s Enter %s NumActiveSwapchainProxies=%u"), ANSI_TO_TCHAR(__FUNCTION__), *CurrentThreadName(), NumActiveSwapchainProxies);
	ValidateNumSwapchainProxies(__FUNCTION__);
	const bool bIsSwapchainProxy = IsPluginSideSwapchainProxyEnabled() ? bIsKnownStreamlineProxy : IsStreamlineSwapchainProxy(InNativeSwapchain);

	if (bIsSwapchainProxy)
	{
		++NumActiveSwapchainProxies;
	}
	UE_LOG(LogStreamlineRHI, Verbose, TEXT("NativeSwapChain=%p IsSwapChainProxy=%u , NumActiveSwapchainProxies=%d"), InNativeSwapchain, bIsSwapchainProxy, NumActiveSwapchainProxies);
	ValidateNumSwapchainProxies(__FUNCTION__);
	UE_LOG(LogStreamlineRHI, Verbose, TEXT("%s Leave %u"), ANSI_TO_TCHAR(__FUNCTION__), NumActiveSwapchainProxies);
}

void FStreamlineRHI::OnSwapchainDestroyed(void* InNativeSwapchain, bool bIsKnownStreamlineProxy) const
{
	UE_LOG(LogStreamlineRHI, Verbose, TEXT("%s Enter %s NumActiveSwapchainProxies=%u"), ANSI_TO_TCHAR(__FUNCTION__), *CurrentThreadName(), NumActiveSwapchainProxies);
	ValidateNumSwapchainProxies(__FUNCTION__);
	const bool bIsSwapchainProxy = IsPluginSideSwapchainProxyEnabled() ? bIsKnownStreamlineProxy  : IsStreamlineSwapchainProxy(InNativeSwapchain);

	if (bIsSwapchainProxy)
	{
		--NumActiveSwapchainProxies;
	}

	UE_LOG(LogStreamlineRHI, Verbose, TEXT("NativeSwapchain=%p IsSwapChainProxy=%u, NumActiveSwapchainProxies=%d "), InNativeSwapchain, bIsSwapchainProxy, NumActiveSwapchainProxies);
	ValidateNumSwapchainProxies(__FUNCTION__);
	UE_LOG(LogStreamlineRHI, Verbose, TEXT("%s Leave %u"), ANSI_TO_TCHAR(__FUNCTION__), NumActiveSwapchainProxies);
}

#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
bool FStreamlineRHI::NeedExtraPassesForDebugLayerCompatibility()
{
	return false;
}
#endif


bool FStreamlineRHI::IsStreamlineAvailable() const
{
	return IsStreamlineSupported();
}

void FStreamlineRHI::SetStreamlineData(FRHICommandList& CmdList, const FRHIStreamlineArguments& InArguments)
{
	check(!IsRunningRHIInSeparateThread() || IsInRHIThread());
	sl::Constants StreamlineConstants = {};

	StreamlineConstants.reset = ToSL(InArguments.bReset);
	StreamlineConstants.jitterOffset = ToSL(InArguments.JitterOffset);

	StreamlineConstants.depthInverted = ToSL(InArguments.bIsDepthInverted);

	StreamlineConstants.mvecScale = ToSL(InArguments.MotionVectorScale);
	StreamlineConstants.motionVectorsDilated = ToSL(InArguments.bAreMotionVectorsDilated);
	StreamlineConstants.cameraMotionIncluded = sl::eTrue;
	StreamlineConstants.motionVectors3D = sl::eFalse;

	StreamlineConstants.orthographicProjection = ToSL(InArguments.bIsOrthographicProjection);
	StreamlineConstants.cameraViewToClip = ToSL(InArguments.CameraViewToClip, InArguments.bIsOrthographicProjection);
	StreamlineConstants.clipToCameraView = ToSL(InArguments.ClipToCameraView);
	StreamlineConstants.clipToLensClip = ToSL(InArguments.ClipToLenseClip);
	StreamlineConstants.clipToPrevClip = ToSL(InArguments.ClipToPrevClip);
	StreamlineConstants.prevClipToClip = ToSL(InArguments.PrevClipToClip);
	
	StreamlineConstants.cameraPos = ToSL(InArguments.CameraOrigin);
	StreamlineConstants.cameraUp = ToSL(InArguments.CameraUp);
	StreamlineConstants.cameraRight = ToSL(InArguments.CameraRight);
	StreamlineConstants.cameraFwd = ToSL(InArguments.CameraForward);
	
	StreamlineConstants.cameraNear = InArguments.CameraNear;
	StreamlineConstants.cameraFar = InArguments.CameraFar;
	StreamlineConstants.cameraFOV = FMath::DegreesToRadians(InArguments.CameraFOV);
	StreamlineConstants.cameraAspectRatio = InArguments.CameraAspectRatio;

	StreamlineConstants.cameraPinholeOffset = ToSL(InArguments.CameraPinholeOffset);

	SLsetConstants(StreamlineConstants, *GetFrameToken(InArguments.FrameId), sl::ViewportHandle(InArguments.ViewId));

}

sl::FrameToken* FStreamlineRHI::GetFrameToken(uint64 FrameCounter)
{
	if (!FrameTokenProvider.IsValid())
	{
		return nullptr;
	}
	return FrameTokenProvider->GetTokenForFrame(FrameCounter);
}

void FStreamlineRHI::StreamlineEvaluateDeepDVC(FRHICommandList& CmdList, const FRHIStreamlineResource& InputOutput, sl::FrameToken* FrameToken, uint32 ViewID)
{
	check(InputOutput.StreamlineTag == EStreamlineResource::ScalingOutputColor);
	TagTexture(CmdList, ViewID, *FrameToken, InputOutput);
	sl::Feature SLFeature = sl::kFeatureDeepDVC;


	sl::CommandBuffer* NativeCommandBuffer = GetCommandBuffer(CmdList, InputOutput.Texture);
	sl::ViewportHandle SLView(ViewID);

	const sl::BaseStructure* SLInputs[] = { &SLView };
	SLevaluateFeature(SLFeature, *FrameToken, SLInputs, UE_ARRAY_COUNT(SLInputs), NativeCommandBuffer);
	PostStreamlineFeatureEvaluation(CmdList, InputOutput.Texture);
}

FStreamlineRHI::~FStreamlineRHI()
{
	UE_LOG(LogStreamlineRHI, Log, TEXT("%s Enter"), ANSI_TO_TCHAR(__FUNCTION__));
#if WITH_EDITOR
	if (BeginPIEHandle.IsValid())
	{
		FEditorDelegates::BeginPIE.Remove(BeginPIEHandle);
	}
	if (EndPIEHandle.IsValid())
	{
		FEditorDelegates::EndPIE.Remove(EndPIEHandle);
	}
#endif
	UE_LOG(LogStreamlineRHI, Log, TEXT("%s Leave"), ANSI_TO_TCHAR(__FUNCTION__));
}

#if PLATFORM_WINDOWS
THIRD_PARTY_INCLUDES_START
#include <winerror.h>
THIRD_PARTY_INCLUDES_END
bool FStreamlineRHI::IsDXGIStatus(const HRESULT HR) const
{
	switch (HR)
	{
	default: return false;

	case DXGI_STATUS_OCCLUDED: return true;
	case DXGI_STATUS_CLIPPED: return true;
	case DXGI_STATUS_NO_REDIRECTION: return true;
	case DXGI_STATUS_NO_DESKTOP_ACCESS: return true;
	case DXGI_STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE: return true;
	case DXGI_STATUS_MODE_CHANGED: return true;
	case DXGI_STATUS_MODE_CHANGE_IN_PROGRESS: return true;
	}
}
#endif


TTuple<bool, FString> FStreamlineRHI::IsSwapChainProviderRequired(const sl::AdapterInfo& AdapterInfo) const
{
	TTuple <bool, FString> Result(false, TEXT(""));

	// TODO query SL for which of all features implemented in UE need a swapchain proxy
	TArray<sl::Feature> FeaturesThatNeedSwapchainProvider = { sl::kFeatureImGUI, sl::kFeatureDLSS_G
		/*	, sl::kFeatureDeepDVC, sl::kFeatureReflex, sl::kFeaturePCL */
	};

	TArray<FString> SLResultStrings;

	TSet<sl::Result> UniqueResults;
	for (sl::Feature Feature : FeaturesThatNeedSwapchainProvider)
	{
		sl::Result SLResult  = SLisFeatureSupported(Feature, AdapterInfo);

		UniqueResults.Add(SLResult);
		
		// put the supported features at the begin of what eventually will be logged
		SLResultStrings.Insert(
			FString::Printf(TEXT("(%s, %s)"), ANSI_TO_TCHAR(sl::getFeatureAsStr(Feature)), ANSI_TO_TCHAR(sl::getResultAsStr(SLResult))), 
			(SLResult == sl::Result::eOk) || (SLResultStrings.Num() == 0) ? 0 : SLResultStrings.Num() - 1
		);

	}
	const FString CombinedResultString = FString::Join(SLResultStrings, TEXT(","));
	
	if (UniqueResults.Contains(sl::Result::eOk))
	{
		Result = MakeTuple(true, FString::Printf(TEXT("a supported feature needing a swap chain provider: %s. This can be overriden with -sl{no}swapchainprovider"), *CombinedResultString));
	}
	else
	{
		Result = MakeTuple(false, FString::Printf(TEXT("no supported feature needing a swap chain provider: %s. This can be overriden with -sl{no}swapchainprovider"), *CombinedResultString));
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("slswapchainprovider")))
	{
		Result = MakeTuple(true, TEXT("-slswapchainprovider command line"));
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("slnoswapchainprovider")))
	{
		Result = MakeTuple(false, TEXT("-slnoswapchainprovider command line"));
	}
	return Result;
}


static TUniquePtr<FStreamlineRHI> GStreamlineRHI;
static EStreamlineSupport GStreamlineSupport = EStreamlineSupport::NotSupported;


static const sl::FeatureRequirementFlags GImplementedStreamlineRHIs =
#if PLATFORM_WINDOWS
sl::FeatureRequirementFlags(uint32_t(sl::FeatureRequirementFlags::eD3D11Supported) | uint32_t(sl::FeatureRequirementFlags::eD3D12Supported));
#else 
sl::FeatureRequirementFlags(0);
#endif

STREAMLINERHI_API sl::FeatureRequirementFlags PlatformGetAllImplementedStreamlineRHIs()
{
	return  GImplementedStreamlineRHIs;
}

STREAMLINERHI_API void PlatformCreateStreamlineRHI()
{
	UE_LOG(LogStreamlineRHI, Log, TEXT("%s Enter"), ANSI_TO_TCHAR(__FUNCTION__));

	// TODO catch init order issues
	check(!GStreamlineRHI);

	const FString RHIName = GDynamicRHI->GetName();

	UE_LOG(LogStreamlineRHI, Log, TEXT("GDynamicRHIName %s %s"), RHIVendorIdToString(), *RHIName);

	{

		// make sure that GImplementedStreamlineRHIs matches what we actually have implemented
		static_assert(sl::FeatureRequirementFlags::eD3D11Supported  == SLBitwiseAnd(sl::FeatureRequirementFlags::eD3D11Supported, GImplementedStreamlineRHIs), "Streamline API/RHI support mismatch");
		static_assert(sl::FeatureRequirementFlags::eD3D12Supported  == SLBitwiseAnd(sl::FeatureRequirementFlags::eD3D12Supported, GImplementedStreamlineRHIs), "Streamline API/RHI support mismatch");
		static_assert(sl::FeatureRequirementFlags::eVulkanSupported != SLBitwiseAnd(sl::FeatureRequirementFlags::eVulkanSupported, GImplementedStreamlineRHIs), "Streamline API/RHI support mismatch");
		
		const ERHIInterfaceType RHIType = RHIGetInterfaceType();
		const bool bIsDX12 = RHIType == ERHIInterfaceType::D3D12;
		const bool bIsDX11 = RHIType == ERHIInterfaceType::D3D11;

		const TCHAR* StreamlineRHIModuleName = nullptr;

		GStreamlineSupport = (bIsDX11 || bIsDX12) ? EStreamlineSupport::Supported : EStreamlineSupport::NotSupportedIncompatibleRHI;

		if (GStreamlineSupport == EStreamlineSupport::Supported)
		{
			if (bIsDX11)
			{
				StreamlineRHIModuleName = TEXT("StreamlineD3D11RHI");
			}
			else if (bIsDX12)
			{
				StreamlineRHIModuleName = TEXT("StreamlineD3D12RHI");
			}

			IStreamlineRHIModule* StreamlineRHIModule = &FModuleManager::LoadModuleChecked<IStreamlineRHIModule>(StreamlineRHIModuleName);

			// now that the RHI-specific SL module has been loaded, we have enough information to determine if SL is supported
			// TODO: better practice might be to make it a method on the module instead of a bare function
			if (IsStreamlineSupported())
			{
				// Get the base directory of this plugin
				const FString PluginBaseDir = IPluginManager::Get().FindPlugin(TEXT("StreamlineCore"))->GetBaseDir();
				const FString SLBinariesDir = FPaths::Combine(*PluginBaseDir, TEXT("Binaries/ThirdParty/"), PlatformDir, TEXT("/"));
				UE_LOG(LogStreamlineRHI, Log, TEXT("PluginBaseDir %s"), *PluginBaseDir);
				UE_LOG(LogStreamlineRHI, Log, TEXT("SLBinariesDir %s"), *SLBinariesDir);

				FStreamlineRHICreateArguments Arguments;
				Arguments.PluginBaseDir = PluginBaseDir;
				Arguments.DynamicRHI = GDynamicRHI;
				GStreamlineRHI = StreamlineRHIModule->CreateStreamlineRHI(Arguments);

				// TODO: handle renderdoc
				const bool bRenderDocPluginFound = FModuleManager::Get().ModuleExists(TEXT("RenderDocPlugin"));

				if (GStreamlineRHI && GStreamlineRHI->IsStreamlineAvailable())
				{
					GStreamlineSupport = EStreamlineSupport::Supported;
					UE_LOG(LogStreamlineRHI, Log, TEXT("Streamline supported by the %s %s RHI in the %s module at runtime"), RHIVendorIdToString(), *RHIName, StreamlineRHIModuleName);
					
					GStreamlineRHI->PostPlatformRHICreateInit();

				}
				else
				{
					UE_LOG(LogStreamlineRHI, Log, TEXT("Could not load %s module"), StreamlineRHIModuleName);
					GStreamlineSupport = EStreamlineSupport::NotSupported;
				}
			}
			else
			{
				UE_LOG(LogStreamlineRHI, Log, TEXT("Streamline not supported for the %s RHI"), *RHIName);
				GStreamlineSupport = EStreamlineSupport::NotSupported;
			}
		}
		else
		{
			UE_LOG(LogStreamlineRHI, Log, TEXT("Streamline not implemented for the %s RHI"), *RHIName);
		}
	}
	UE_LOG(LogStreamlineRHI, Log, TEXT("%s Leave"), ANSI_TO_TCHAR(__FUNCTION__));
}

STREAMLINERHI_API FStreamlineRHI* GetPlatformStreamlineRHI()
{
	return GStreamlineRHI.Get();
}

STREAMLINERHI_API EStreamlineSupport GetPlatformStreamlineSupport()
{
	return GStreamlineSupport;
}

namespace
{
	const TCHAR* StreamlineIniSection = TEXT("/Script/StreamlineRHI.StreamlineSettings");
	const TCHAR* StreamlineOverrideIniSection = TEXT("/Script/StreamlineRHI.StreamlineOverrideSettings");

	bool LoadConfigSettingWithOverrides(bool bDefaultValue, const TCHAR* SettingName, const TCHAR* OverrideName, const TCHAR* CommandLineWithoutDashSLPrefix)
	{
		bool bResult = bDefaultValue;
		bool bSettingBool = false;
		check(GConfig != nullptr);
		bool bHasConfig = GConfig->GetBool(StreamlineIniSection, SettingName, bSettingBool, GEngineIni);

		if (bHasConfig)
		{
			bResult = bSettingBool;
		}

		// we treat EStreamlineSettingOverride::UseProjectSettings  as project setting, either the C++ default or whatever is in the config file

		FString SettingBoolOverrideString{};
		bool bHasOverrideConfig = GConfig->GetString(StreamlineOverrideIniSection, OverrideName, SettingBoolOverrideString, GEngineIni);
		if (bHasOverrideConfig)
		{
			if (SettingBoolOverrideString == TEXT("Enabled"))
			{
				bResult = true;
			}
			else if (SettingBoolOverrideString == TEXT("Disabled"))
			{
				bResult = false;
			}
			else if(SettingBoolOverrideString == TEXT("UseProjectSettings"))
			{
				bHasOverrideConfig = false;
			}
		}
		else
		{
			// this assumes that UStreamlineOverrideSettings::* are  all set to EStreamlineSettingOverride::UseProjectSettings in C++
		}

		TArray<FString> FeatureEnableDisableCommandlines;

		// That's skipping the leading '-' intentionally
		const FString AllowCMD = FString::Printf(TEXT("sl%s"), CommandLineWithoutDashSLPrefix);
		const FString DisallowCMD = FString::Printf(TEXT("slno%s"), CommandLineWithoutDashSLPrefix);

		bool bHasAllowCommand = false;
		bool bHasDisallowCommand = false;
		if (FParse::Param(FCommandLine::Get(), *AllowCMD))
		{
			bResult = true;
			bHasAllowCommand = true;
		}
		else if (FParse::Param(FCommandLine::Get(), *DisallowCMD))
		{
			bResult = false;
			bHasDisallowCommand = true;
		}

		if (bHasAllowCommand || bHasDisallowCommand)
		{
			UE_LOG(LogStreamlineRHI, Log, TEXT("Setting %-25s to %u due to -%s command line option"), SettingName, bResult,  bHasAllowCommand ? *AllowCMD : *DisallowCMD);
		}
		else if (bHasOverrideConfig)
		{
			UE_LOG(LogStreamlineRHI, Log, TEXT("Setting %-25s to %u due to %s in the local project user config file. See command line -sl{no}%s."), SettingName, bResult, StreamlineOverrideIniSection, CommandLineWithoutDashSLPrefix);
		}
		else if (bHasConfig)
		{
			UE_LOG(LogStreamlineRHI, Log, TEXT("Setting %-25s to %u due to %s in the project config file. See -sl{no}%s command line or project user settings"), SettingName, bResult, StreamlineIniSection, CommandLineWithoutDashSLPrefix);
		}
		else
		{
			UE_LOG(LogStreamlineRHI, Log, TEXT("Setting %-25s to %u default. See -sl{no}%s command line or project and project user settings"), SettingName, bResult, CommandLineWithoutDashSLPrefix);
		}

		return bResult;
	}

	bool ShouldLoadDebugOverlay()
	{
#if UE_BUILD_SHIPPING
		return false;
	
#endif
		return LoadConfigSettingWithOverrides(UStreamlineSettings::CppDefaults()->bLoadDebugOverlay, TEXT("bLoadDebugOverlay"), TEXT("LoadDebugOverlayOverride"), TEXT("debugoverlay"));
	}

	bool ShouldOta()
	{
		// intentionally available in shipping builds
		return LoadConfigSettingWithOverrides(UStreamlineSettings::CppDefaults()->bAllowOTAUpdate, TEXT("bAllowOTAUpdate"), TEXT("AllowOTAUpdateOverride"), TEXT("ota"));
	}
}

bool ShouldUseSlSetTag()
{
	// intentionally available in shipping builds
	// caching the result here since we call ShouldUseSetTag both at slInit time but also then every frame
	static bool bUseSlSetTag = LoadConfigSettingWithOverrides(UStreamlineSettings::CppDefaults()->bUseSlSetTag, TEXT("bUseSlSetTag"), TEXT("UseSlSetTagOverride"), TEXT("settag"));
	return bUseSlSetTag;
}

bool ShouldUseSlateCallbacksForSwapchainTracking()
{
#if UE_VERSION_OLDER_THAN(5,8,0)
	static bool bUseSlateCallbacksForSwapchainTracking = LoadConfigSettingWithOverrides(
		UStreamlineSettings::CppDefaults()->bUseSlateCallbacksForSwapchainTracking,
		TEXT("bUseSlateCallbacksForSwapchainTracking"),
		TEXT("UseSlateCallbacksForSwapchainTrackingOverride"),
		TEXT("slatecallbacks"));
	return bUseSlateCallbacksForSwapchainTracking;
#else
	return false;
#endif
}

static void RemoveDuplicateSlashesFromPath(FString& Path)
{
	if (Path.StartsWith(FString("//")))
	{
		// preserve the initial double slash to support network paths
		FPaths::RemoveDuplicateSlashes(Path);
		Path = FString("/") + Path;
	}
	else
	{
		FPaths::RemoveDuplicateSlashes(Path);
	}
}

static TArray<FString> GetSLDLLSearchPaths(const TArray<FString>& StreamlineBinaryDirectories, bool bIsDLSSPluginEnabled, TSharedPtr<IPlugin>& DLSSPlugin)
{
	TArray<FString> StreamlineDLLSearchPaths{ StreamlineBinaryDirectories };

	if (bIsDLSSPluginEnabled)
	{
		// This is based on FDLSSModule::StartupModule()
		auto CVarNGXEnable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.NGX.Enable"));
		bool bLoadLibraries = CVarNGXEnable && CVarNGXEnable->GetBool();
		auto CVarNGXEnableAllowCommandLine = IConsoleManager::Get().FindConsoleVariable(TEXT("r.NGX.Enable.AllowCommandLine"));

		if (CVarNGXEnableAllowCommandLine && CVarNGXEnableAllowCommandLine->GetBool())
		{
			if (FParse::Param(FCommandLine::Get(), TEXT("ngxenable")))
			{
				bLoadLibraries = true;
			}
			else if (FParse::Param(FCommandLine::Get(), TEXT("ngxdisable")))
			{
				bLoadLibraries = false;
			}
		}

		if (bLoadLibraries)
		{
			UE_LOG(LogStreamlineRHI, Log, TEXT("DLSS plugin enabled, adding DLSS plugin binary search paths to Streamline init paths"));

			// TODO STREAMLINE have this respect r.NGX.BinarySearchOrder
			// this is a stripped down variant from the logic  NGXRHI::NGXRHI
			const FString ProjectNGXBinariesDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries/ThirdParty/NVIDIA/NGX/"), PlatformDir);
			const FString LaunchNGXBinariesDir = FPaths::Combine(FPaths::LaunchDir(), TEXT("Binaries/ThirdParty/NVIDIA/NGX/"), PlatformDir);
			const FString DLSSPluginBaseDir = DLSSPlugin->GetBaseDir();
			const FString PluginNGXProductionBinariesDir = FPaths::Combine(*DLSSPluginBaseDir, TEXT("Binaries/ThirdParty/"), PlatformDir);
			StreamlineDLLSearchPaths.Append({ ProjectNGXBinariesDir, LaunchNGXBinariesDir, PluginNGXProductionBinariesDir });
		}
		else
		{
			UE_LOG(LogStreamlineRHI, Log, TEXT("NGX loading has been disabled in the DLSS plugin (var r.NGX.Enable or -ngx{dis,en}able), thus NOT adding those binary search paths to the Streamline init paths"));
		}
	}
	else
	{
		UE_LOG(LogStreamlineRHI, Log, TEXT("DLSS plugin not enabled "));
	}

	return StreamlineDLLSearchPaths;
}

void FStreamlineRHIModule::InitializeStreamline()
{
	sl::Preferences Preferences;
	FMemory::Memzero(Preferences);

	Preferences.showConsole = false;
	Preferences.logLevel = sl::LogLevel::eDefault;
	// we cannot use cvars since they haven't been loaded yet this early in the module loading order...
	{
		FString LogArgumentString;
		if (FParse::Value(FCommandLine::Get(), TEXT("slloglevel="), LogArgumentString))
		{
			if (LogArgumentString == TEXT("0"))
			{
				Preferences.logLevel = sl::LogLevel::eOff;
			}
			else if (LogArgumentString == TEXT("1"))
			{
				Preferences.logLevel = sl::LogLevel::eDefault;
			}
			else if (LogArgumentString == TEXT("2"))
			{
				Preferences.logLevel = sl::LogLevel::eVerbose;
			}
			else if (LogArgumentString == TEXT("3"))
			{
				Preferences.logLevel = sl::LogLevel::eVerbose;
				SetStreamlineAPILoggingEnabled(true);
			}
		}

		if (FParse::Value(FCommandLine::Get(), TEXT("sllogconsole="), LogArgumentString))
		{
			if (LogArgumentString == TEXT("0"))
			{
				Preferences.showConsole = false;
			}
			else if (LogArgumentString == TEXT("1"))
			{
				Preferences.showConsole = true;
			}
		}
	}

	// TODO: consider filling these in too
	Preferences.pathToLogsAndData = nullptr;
	Preferences.allocateCallback = nullptr;
	Preferences.releaseCallback = nullptr;
#if !NO_LOGGING
	Preferences.logMessageCallback = StreamlineLogSink;
#else
	Preferences.logMessageCallback = nullptr;
#endif
	Preferences.flags = sl::PreferenceFlags::eDisableCLStateTracking | sl::PreferenceFlags::eUseManualHooking;

	Preferences.engine = sl::EngineType::eUnreal;
	FString EngineVersion = FString::Printf(TEXT("%u.%u"), FEngineVersion::Current().GetMajor(), FEngineVersion::Current().GetMinor());
	FTCHARToUTF8 EngineVersionUTF8(*EngineVersion);
	Preferences.engineVersion = EngineVersionUTF8.Get();

	check(GConfig);
	FString ProjectID(TEXT("0"));
	GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectID"), ProjectID, GGameIni);
	FTCHARToUTF8 ProjectIDUTF8(*ProjectID);
	Preferences.projectId = ProjectIDUTF8.Get();

	// NGX will get initialized by Streamline in slInit, long before the DLSS-SR plugin tries to initialize NGX in PostEngineInit.
	// So we need to know if the DLSS-SR plugin is enabled and duplicate what it would do for NGX initialization, to avoid breaking DLSS-SR.
	TSharedPtr<IPlugin> DLSSPlugin = IPluginManager::Get().FindPlugin(TEXT("DLSS"));
	const bool bIsDLSSPluginEnabled = DLSSPlugin && (DLSSPlugin->IsEnabled() || DLSSPlugin->IsEnabledByDefault(false));

	Preferences.applicationId = GetNGXAppID(bIsDLSSPluginEnabled);


	// metat data for the UE plugins and relevant SL plugins, their load cvars and their command lines
	TArray<FSLFeatureDesc> SLFeatureDescs =
	{
		{sl::kFeatureReflex, TEXT("StreamlineReflex"), TEXT("Reflex")},
		{sl::kFeatureDeepDVC, TEXT("StreamlineDeepDVC"), TEXT("DeepDVC")}
	};
	// DLSS-FG feature doesn't follow the typical naming pattern, command line and cvar are "dlssg" not "dlssfg" for historical reasons
	FSLFeatureDesc& FGFeature = SLFeatureDescs.Emplace_GetRef();
	FGFeature.SLFeature = sl::kFeatureDLSS_G;
	FGFeature.UEPluginName = TEXT("StreamlineDLSSG");
	FGFeature.FeatureName = TEXT("DLSS-FG");
	FGFeature.CommandLineSuffix = TEXT("dlssg");
	FGFeature.LoadCVar = TEXT("r.Streamline.Load.DLSSG");

	SLFeatureDescs += FStreamlineExtensionModule::Get().GetRegisteredFeatures();

	// Generate console variables for each feature

	IConsoleManager& CVarManager = IConsoleManager::Get();

	for (const FSLFeatureDesc& FeatureDesc : SLFeatureDescs)
	{
		const FString LoadCVarName = FeatureDesc.LoadCVar;
		const FString Description = FString::Printf(TEXT("Determines whether feature %s is loaded. This can be useful to resolve conflicts where multiple SL features are incompatible with each other."),
			*FeatureDesc.FeatureName);

		CVarManager.RegisterConsoleVariable(
			*LoadCVarName,
			FeatureDesc.bAllowByDefault,
			*Description,
			ECVF_RenderThreadSafe | ECVF_ReadOnly);
	}

	// sl::kFeaturePCL is always loaded by SL and doesn't have to be explicitly requested
	TArray<sl::Feature> Features = {};

	TArray <FString> FeatureEnableDisableCommandlines;
	TArray <FString> FeatureEnableDisableConsoleVariables;
	// The default SL binary directory needs to be the first in the list because sl::Preferences::pathsToPlugins is surprising:
	// - All directories in the list are used for NGX binaries (nvngx_*.dll)
	// - Only the first directory in the list that contains a SL binary is used for SL binaries (sl.*.dll)
	// As a result, plugins that implement SL features may optionally store NGX binaries in their own plugin but their SL
	// binaries need to be stored in StreamlineCore.
	TArray <FString> SLBinaryDirectories{ StreamlineBinaryDirectory };

	// If the UE feature plugin is enabled
	//     then the priority of what controls enabling the SL plugin is: command line -> load cvar ->  FSLFeatureDesc::bAllowByDefault
	// else 
	//     don't load the SL plugin at all.
	auto EnableStreamlineFeature =
		[&Features, &FeatureEnableDisableCommandlines, &FeatureEnableDisableConsoleVariables, &SLBinaryDirectories, &BinaryFlavor = StreamlineBinaryFlavor]
		(const FSLFeatureDesc& FeatureDesc)
	{
		TSharedPtr<IPlugin> RequiredPlugin = IPluginManager::Get().FindPlugin(FeatureDesc.UEPluginName);
		const bool bIsRequiredPluginEnabled = RequiredPlugin && (RequiredPlugin->IsEnabled() || RequiredPlugin->IsEnabledByDefault(false));

		if (!bIsRequiredPluginEnabled)
		{
			UE_LOG(LogStreamlineRHI, Log, TEXT("Skipping loading Streamline %s since the corresponding UE %s plugin is not enabled"), *FeatureDesc.FeatureName, *FeatureDesc.UEPluginName);
			return;
		}

		bool bAllowFeature = FeatureDesc.bAllowByDefault;

		/*re-entrant, thus NON STATIC!*/ const auto CVarLoad = IConsoleManager::Get().FindConsoleVariable(*FeatureDesc.LoadCVar);
		if (CVarLoad != nullptr)
		{
			FeatureEnableDisableConsoleVariables.Add(FeatureDesc.LoadCVar);
			const bool bLoad = CVarLoad->GetBool();
			bAllowFeature = bLoad;

			if (bLoad)
			{
				UE_LOG(LogStreamlineRHI, Log, TEXT("Loading Streamline %s since the corresponding cvar %s is set to true"), *FeatureDesc.FeatureName, *FeatureDesc.LoadCVar);
			}
			else
			{
				UE_LOG(LogStreamlineRHI, Log, TEXT("Not loading Streamline %s since the corresponding cvar %s is set to false"), *FeatureDesc.FeatureName, *FeatureDesc.LoadCVar);
			}
		}
		else
		{
			UE_LOG(LogStreamlineRHI, Warning, TEXT("Cannot find cvar %s that controls whether feature %s is loaded or not, so loading"), *FeatureDesc.LoadCVar, *FeatureDesc.FeatureName);
		}

		// That's skipping the leading '-' intentionally
		const FString AllowCMD = FString::Printf(TEXT("sl%s"), *FeatureDesc.CommandLineSuffix);
		const FString DisallowCMD = FString::Printf(TEXT("slno%s"), *FeatureDesc.CommandLineSuffix);

		// And this one has it intentinally for further logging
		FeatureEnableDisableCommandlines.Add(FString::Printf(TEXT("-sl{no}%s"), *FeatureDesc.CommandLineSuffix));

		if (FParse::Param(FCommandLine::Get(), *AllowCMD))
		{
			UE_LOG(LogStreamlineRHI, Log, TEXT("Loading Streamline %s due to -%s command line option"), *FeatureDesc.FeatureName, *AllowCMD);
			bAllowFeature = true;
		}
		else if (FParse::Param(FCommandLine::Get(), *DisallowCMD))
		{
			UE_LOG(LogStreamlineRHI, Log, TEXT("Not loading Streamline %s due to -%s command line option"), *FeatureDesc.FeatureName, *DisallowCMD);
			bAllowFeature = false;
		}

		if (bAllowFeature)
		{
			Features.Add(FeatureDesc.SLFeature);
			for (const auto& BinaryDirectory : FeatureDesc.BinaryDirectories)
			{
				SLBinaryDirectories.Add(FPaths::Combine(BinaryDirectory, BinaryFlavor));
			}
		}
	};
	
	// enable features based on command line, cvar state etc
	for (const FSLFeatureDesc& FeatureDesc : SLFeatureDescs)
	{
		EnableStreamlineFeature(FeatureDesc);
	}

	// Normalize paths and add NGX search paths if appropriate
	TArray<FString> StreamlineDLLSearchPaths = GetSLDLLSearchPaths(SLBinaryDirectories, bIsDLSSPluginEnabled, DLSSPlugin);
	TArray<const wchar_t*> StreamlineDLLSearchPathRawStrings;

	for (int32 i = 0; i < StreamlineDLLSearchPaths.Num(); ++i)
	{
		StreamlineDLLSearchPaths[i] = FPaths::ConvertRelativePathToFull(StreamlineDLLSearchPaths[i]);
		RemoveDuplicateSlashesFromPath(StreamlineDLLSearchPaths[i]);
		FPaths::MakePlatformFilename(StreamlineDLLSearchPaths[i]);
		FPaths::NormalizeDirectoryName(StreamlineDLLSearchPaths[i]);
		// After this we should not touch StreamlineDLLSearchPaths since that provides the backing store for StreamlineDLLSearchPathRawStrings
		StreamlineDLLSearchPathRawStrings.Add(*StreamlineDLLSearchPaths[i]);
		const bool bHasStreamlineInterposerBinary = IPlatformFile::GetPlatformPhysical().FileExists(*FPaths::Combine(StreamlineDLLSearchPaths[i], STREAMLINE_INTERPOSER_BINARY_NAME));
		UE_LOG(LogStreamlineRHI, Log, TEXT("NVIDIA Streamline interposer plugin %s %s in search path %s"), STREAMLINE_INTERPOSER_BINARY_NAME, bHasStreamlineInterposerBinary ? TEXT("found") : TEXT("not found"), *StreamlineDLLSearchPaths[i]);

		// copied binary name here from the DLSS-SR plugin to avoid creating a dependency on that plugin
#ifndef NGX_DLSS_SR_BINARY_NAME
#define NGX_DLSS_SR_BINARY_NAME (TEXT("nvngx_dlss.dll"))
#endif

#ifdef NGX_DLSS_SR_BINARY_NAME
		if (bIsDLSSPluginEnabled)
		{
			const bool bHasDLSSBinary = IPlatformFile::GetPlatformPhysical().FileExists(*FPaths::Combine(StreamlineDLLSearchPaths[i], NGX_DLSS_SR_BINARY_NAME));
			UE_LOG(LogStreamlineRHI, Log, TEXT("NVIDIA NGX DLSS binary %s %s in search path %s"), NGX_DLSS_SR_BINARY_NAME, bHasDLSSBinary ? TEXT("found") : TEXT("not found"), *StreamlineDLLSearchPaths[i]);
		}
#endif
	}

	Preferences.pathsToPlugins = &StreamlineDLLSearchPathRawStrings[0];
	Preferences.numPathsToPlugins = StreamlineDLLSearchPathRawStrings.Num();

#if !UE_BUILD_SHIPPING
	if ( ShouldLoadDebugOverlay())
	{
		Features.Push(sl::kFeatureImGUI);
	}
#endif
	Preferences.featuresToLoad = Features.GetData();
	Preferences.numFeaturesToLoad = Features.Num();

	bool bEnableStreamlineD3D11 = true;
	bool bEnableStreamlineD3D12 = true;
	GConfig->GetBool(StreamlineIniSection, TEXT("bEnableStreamlineD3D11"), bEnableStreamlineD3D11, GEngineIni);
	GConfig->GetBool(StreamlineIniSection, TEXT("bEnableStreamlineD3D12"), bEnableStreamlineD3D12, GEngineIni);

	const FString RHIName = GDynamicRHI->GetName();
	if (bEnableStreamlineD3D12 && (RHIGetInterfaceType() == ERHIInterfaceType::D3D12))
	{
		Preferences.renderAPI = sl::RenderAPI::eD3D12;
	}
	else if (bEnableStreamlineD3D11 && (RHIGetInterfaceType() == ERHIInterfaceType::D3D11))
	{
		Preferences.renderAPI = sl::RenderAPI::eD3D11;
	}
	else
	{
		UE_LOG(LogStreamlineRHI, Warning, TEXT("Unsupported RHI %s, skipping Streamline init"), *RHIName);
		return;
	}

	if (ShouldOta())
	{
		Preferences.flags |= (sl::PreferenceFlags::eAllowOTA | sl::PreferenceFlags::eLoadDownloadedPlugins);
	}

	if (!ShouldUseSlSetTag())
	{
		Preferences.flags |= sl::PreferenceFlags::eUseFrameBasedResourceTagging;
	}

	UE_LOG(LogStreamlineRHI, Log, TEXT("Initializing Streamline"));
	UE_LOG(LogStreamlineRHI, Log, TEXT("sl::Preferences::logLevel    = %u. Can be overridden via -slloglevel={0,1,2} command line switches"), Preferences.logLevel);
	UE_LOG(LogStreamlineRHI, Log, TEXT("sl::Preferences::showConsole = %u. Can be overridden via -sllogconsole={0,1} command line switches"), Preferences.showConsole);
	UE_LOG(LogStreamlineRHI, Log, TEXT("sl::Preferences::flags       = 0x%x %s"), Preferences.flags, *getPreferenceFlagsAsStr(Preferences.flags));
	UE_LOG(LogStreamlineRHI, Log, TEXT("sl::Preferences::featuresToLoad = {%s}. Feature loading can be overridden on the command line and console variables:"),
		*FString::JoinBy(Features, TEXT(", "), [](const sl::Feature& Feature) { return FString::Printf(TEXT("%s (%u)"), ANSI_TO_TCHAR(sl::getFeatureAsStr(Feature)), Feature); }));
	UE_LOG(LogStreamlineRHI, Log, TEXT("command line %s -sl{no}debugoverlay (non-shipping)"), *FString::Join(FeatureEnableDisableCommandlines, TEXT(", ")));
	UE_LOG(LogStreamlineRHI, Log, TEXT("console/config %s"), *FString::Join(FeatureEnableDisableConsoleVariables, TEXT(", ")));

	FStreamlineRHI::FeaturesRequestedAtSLInitTime = Features;


	sl::Result Result = SLinit(Preferences);
	if (Result == sl::Result::eOk)
	{
		bIsStreamlineInitialized = true;
		FStreamlineExtensionModule::Get().SetSLInitialized();
	}
	else
	{
		UE_LOG(LogStreamlineRHI, Error, TEXT("Failed to initialize Streamline (%d, %s)"), Result, ANSI_TO_TCHAR(sl::getResultAsStr(Result)));
		bIsStreamlineInitialized = false;
	}
}

STREAMLINERHI_API bool IsStreamlineSupported()
{
	return IsEngineExecutionModeSupported().Get<0>() && bIsStreamlineInitialized && AreStreamlineFunctionsLoaded();
}

STREAMLINERHI_API bool StreamlineFilterRedundantSetOptionsCalls()
{
#if (UE_BUILD_TEST || UE_BUILD_SHIPPING)
	return true;
#else
	if (FParse::Param(FCommandLine::Get(), TEXT("slfilter")))
	{
		return true;
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("slnofilter")))
	{
		return false;
	}
	else
	{
		return CVarStreamlineFilterRedundantSetOptionsCalls.GetValueOnAnyThread();
	}
#endif
}

STREAMLINERHI_API void FStreamlineRHIModule::ShutdownStreamline()
{
	UE_LOG(LogStreamlineRHI, Log, TEXT("Shutting down Streamline"));
	sl::Result Result = SLshutdown();
	if (Result != sl::Result::eOk)
	{
		UE_LOG(LogStreamlineRHI, Error, TEXT("Failed to shut down Streamline (%s)"), ANSI_TO_TCHAR(sl::getResultAsStr(Result)));
	}
	bIsStreamlineInitialized = false;
}


/** IModuleInterface implementation */

void FStreamlineRHIModule::StartupModule()
{
	auto CVarInitializePlugin = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.InitializePlugin"));
	if (CVarInitializePlugin && !CVarInitializePlugin->GetBool() || (FParse::Param(FCommandLine::Get(), TEXT("slno"))))
	{
		UE_LOG(LogStreamlineRHI, Log, TEXT("Initialization of StreamlineRHI is disabled."));
		return;
	}

	UE_LOG(LogStreamlineRHI, Log, TEXT("%s Enter"), ANSI_TO_TCHAR(__FUNCTION__));
	if (FApp::CanEverRender())
	{
#if !(UE_BUILD_SHIPPING)
		{
			// debug overlay requires development binaries
			FString BinaryFlavorArgument = ShouldLoadDebugOverlay() ? TEXT("Development") : TEXT("");

			// optional command line override
			FParse::Value(FCommandLine::Get(), TEXT("slbinaries="), BinaryFlavorArgument);

			if (!BinaryFlavorArgument.IsEmpty())
			{
				if (FPaths::DirectoryExists(BinaryFlavorArgument))
				{
					// absolute/relative path to a directory containing SL binaries
					StreamlineBinaryDirectory = BinaryFlavorArgument;
				}
				else
				{
					for (auto Argument : { TEXT("Development"), TEXT("Debug") })
					{
						if (BinaryFlavorArgument.Compare(Argument, ESearchCase::IgnoreCase) == 0)
						{
							StreamlineBinaryFlavor = Argument;
							break;
						}
					}
					if (BinaryFlavorArgument.Compare(TEXT("Production"), ESearchCase::IgnoreCase) == 0)
					{
						// production binaries are not in a subdirectory
						StreamlineBinaryFlavor.Empty();
					}
				}
			}
		}
#endif
		if (StreamlineBinaryDirectory.IsEmpty())
		{
			const FString StreamlinePluginBaseDir = IPluginManager::Get().FindPlugin(TEXT("StreamlineCore"))->GetBaseDir();
			StreamlineBinaryDirectory = FPaths::Combine(*StreamlinePluginBaseDir, TEXT("Binaries/ThirdParty/"), PlatformDir, *(StreamlineBinaryFlavor));
		}
		UE_LOG(LogStreamlineRHI, Log, TEXT("Using Streamline %s binaries from %s. Can be overridden via -slbinaries={production,development,debug,<path>} command line switches for non-shipping builds")
			, StreamlineBinaryFlavor.IsEmpty() ? TEXT("production") : *StreamlineBinaryFlavor
			, *StreamlineBinaryDirectory
		);

		const FString StreamlineInterposerBinaryPath = FPaths::Combine(*StreamlineBinaryDirectory, STREAMLINE_INTERPOSER_BINARY_NAME);
		LoadStreamlineFunctionPointers(StreamlineInterposerBinaryPath);
	}
	else
	{
		UE_LOG(LogStreamlineRHI, Log, TEXT("This UE instance does not render, skipping loading of core Streamline functions"));
		StreamlineBinaryDirectory = TEXT("");
	}

	PlatformCreateStreamlineRHI();
	UE_LOG(LogStreamlineRHI, Log, TEXT("%s Leave"), ANSI_TO_TCHAR(__FUNCTION__));
}

void FStreamlineRHIModule::ShutdownModule()
{
	auto CVarInitializePlugin = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.InitializePlugin"));
	if (CVarInitializePlugin && !CVarInitializePlugin->GetBool())
	{
		return;
	}

	UE_LOG(LogStreamlineRHI, Log, TEXT("%s Enter"), ANSI_TO_TCHAR(__FUNCTION__));
	GStreamlineRHI.Reset();
	// TODO STREAMLINE sort out proper shutdown order between the SL interposer and the RHIs
	// don't shut down streamline so the D3D12RHI destructors don't crash
	//ShutdownStreamline();
	//FPlatformProcess::FreeDllHandle(SLInterPoserDLL);
	//SLInterPoserDLL = nullptr;
	UE_LOG(LogStreamlineRHI, Log, TEXT("%s Leave"), ANSI_TO_TCHAR(__FUNCTION__));
}


IMPLEMENT_MODULE(FStreamlineRHIModule, StreamlineRHI )
#undef LOCTEXT_NAMESPACE