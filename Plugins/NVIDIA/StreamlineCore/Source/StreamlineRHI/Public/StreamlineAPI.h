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
#pragma once

#include "CoreMinimal.h"

#include "sl.h"
#include "sl_helpers.h"
#include "sl_dlss_g.h"
#include "sl_deepdvc.h"

// Those are the actual Streamline API calls
extern STREAMLINERHI_API sl::Result SLinit(const sl::Preferences& pref, uint64_t sdkVersion = sl::kSDKVersion);
extern STREAMLINERHI_API sl::Result SLshutdown();
extern STREAMLINERHI_API sl::Result SLisFeatureSupported(sl::Feature feature, const sl::AdapterInfo& adapterInfo);
extern STREAMLINERHI_API sl::Result SLisFeatureLoaded(sl::Feature feature, bool& loaded);
extern STREAMLINERHI_API sl::Result SLsetFeatureLoaded(sl::Feature feature, bool loaded);
extern STREAMLINERHI_API sl::Result SLevaluateFeature(sl::Feature feature, const sl::FrameToken& frame, const sl::BaseStructure** inputs, uint32_t numInputs, sl::CommandBuffer* cmdBuffer);
extern STREAMLINERHI_API sl::Result SLAllocateResources(sl::CommandBuffer* cmdBuffer, sl::Feature feature, const sl::ViewportHandle& viewport);
extern STREAMLINERHI_API sl::Result SLFreeResources(sl::Feature feature, const sl::ViewportHandle& viewport);
extern STREAMLINERHI_API sl::Result SLsetTag(const sl::ViewportHandle& viewport, const sl::ResourceTag* tags, uint32_t numTags, sl::CommandBuffer* cmdBuffer);
extern STREAMLINERHI_API sl::Result SLsetTagForFrame(const sl::FrameToken& frame, const sl::ViewportHandle& viewport, const sl::ResourceTag* tags, uint32_t numTags, sl::CommandBuffer* cmdBuffer);
extern STREAMLINERHI_API sl::Result SLgetFeatureRequirements(sl::Feature feature, sl::FeatureRequirements& requirements);
extern STREAMLINERHI_API sl::Result SLgetFeatureVersion(sl::Feature feature, sl::FeatureVersion& version);
extern STREAMLINERHI_API sl::Result SLUpgradeInterface(void** baseInterface);
extern STREAMLINERHI_API sl::Result SLsetConstants(const sl::Constants& values, const sl::FrameToken& frame, const sl::ViewportHandle& viewport);
extern STREAMLINERHI_API sl::Result SLgetNativeInterface(void* proxyInterface, void** baseInterface);
extern STREAMLINERHI_API sl::Result SLgetFeatureFunction(sl::Feature feature, const char* functionName, void*& function);
extern STREAMLINERHI_API sl::Result SLgetNewFrameToken(sl::FrameToken*& token, uint32_t* frameIndex = nullptr);
extern STREAMLINERHI_API sl::Result SLsetD3DDevice(void* d3dDevice);


extern STREAMLINERHI_API void LogStreamlineFunctionCall(sl::Feature Feature, const FString& Function, const FString& Arguments);

extern STREAMLINERHI_API bool LogStreamlineFunctions();


// workaround for bad operator& definition that returns bool in sl_consts.h macro, until Streamline header is fixed
// if you get a compile error for SLBitwiseAnd specialization, it's likely that Streamline has been fixed, so you can use & and remove this function template
using SLBitwiseAndOperatorReturnType = decltype(sl::operator&(sl::FeatureRequirementFlags::eD3D11Supported, sl::FeatureRequirementFlags::eD3D11Supported));
template<typename E, std::enable_if_t<!std::is_same_v<SLBitwiseAndOperatorReturnType, sl::FeatureRequirementFlags>, bool> = true>
constexpr E SLBitwiseAnd(E x1, E x2)
{
	return static_cast<E>(static_cast<std::underlying_type<E>::type>(x1) & static_cast<std::underlying_type<E>::type>(x2));
}


struct StringifySLArgument
{
	TArray<FString> ArgStrings;

	FString GetJoinedArgString() const
	{
		return FString::Join(ArgStrings, TEXT(", "));
	}

	
	template <typename Whatever>
	void operator()(const Whatever& In)
	{
		ArgStrings.Add(FString::Printf(TEXT("arg%d"), ArgStrings.Num()));
	}

	void operator()(const sl::PCLMarker& In)
	{
		ArgStrings.Add(FString(ANSI_TO_TCHAR(sl::getPCLMarkerAsStr(In))));
	}

	void operator()(const sl::FrameToken& In)
	{
		ArgStrings.Add(FString::Printf(TEXT("frame=%u"), In.operator unsigned int()));
	}

	void operator()(const sl::ViewportHandle& In)
	{
		ArgStrings.Add(FString::Printf(TEXT("viewport=%u"), In.operator unsigned int()));
	}



	inline const char* getDLSSGFlagsSingleBitFlagsAsStr(sl::DLSSGFlags f)
	{
		using namespace sl;
		static_assert(uint64_t(DLSSGFlags::eAll) == 31U, "dear NVIDIA plugin developer, please update this code to handle the new sl::DLSSGFlags enum values");
		switch (f)
		{
			SL_CASE_STR(DLSSGFlags::eShowOnlyInterpolatedFrame);
			SL_CASE_STR(DLSSGFlags::eDynamicResolutionEnabled);
			SL_CASE_STR(DLSSGFlags::eRequestVRAMEstimate);
			SL_CASE_STR(DLSSGFlags::eRetainResourcesWhenOff);
			SL_CASE_STR(DLSSGFlags::eEnableFullscreenMenuDetection);
		}
		return "Unknown";
	}

	inline FString getDLSSGFlagsAsStr(sl::DLSSGFlags f)
	{
		FString Result;

		for (uint32 SingleBit = uint32(sl::DLSSGFlags::eEnableFullscreenMenuDetection); SingleBit != 0; SingleBit >>= 1)
		{
			sl::DLSSGFlags Flag = sl::DLSSGFlags(SingleBit);

			if (Flag == SLBitwiseAnd(f, Flag))
			{
				Result.Append(getDLSSGFlagsSingleBitFlagsAsStr(Flag));
				Result.AppendChar(TCHAR('|'));
			}
		}

		Result.RemoveFromEnd(TEXT("|"));
		return Result;
	}

	void operator()(const sl::DLSSGOptions& In)
	{
		ArgStrings.Add(FString::Printf(TEXT("%s numFramesToGenerate=%u"), ANSI_TO_TCHAR(getDLSSGModeAsStr(In.mode)), In.numFramesToGenerate));
	}

	void operator()(const sl::DeepDVCOptions& In)
	{
		ArgStrings.Add(FString::Printf(TEXT("%s intensity=%.3f saturationBoost=%.3f"), ANSI_TO_TCHAR(getDeepDVCModeAsStr(In.mode)), In.intensity, In.saturationBoost));
	}
	void operator()(const sl::ReflexOptions& In)
	{
		ArgStrings.Add(FString::Printf(TEXT("mode=%s"), ANSI_TO_TCHAR(getReflexModeAsStr(In.mode))));
	}

	void operator()(const sl::ReflexCameraData& In)
	{
		ArgStrings.Add(FString::Printf(TEXT("worldToViewMatrix=... viewToClipMatrix=... prevRenderedWorldToViewMatrix=..., prevRenderedViewToClipMatrix=...")));
	}
	
};


inline const char* getPreferenceFlagsSingleBitFlagsAsStr(sl::PreferenceFlags f)
{
	using namespace sl;
	static_assert(uint64_t(PreferenceFlags::eAll) == 255U, "dear NVIDIA plugin developer, please update this code to handle the new sl::PreferenceFlags enum values");
	switch (f)
	{
		SL_CASE_STR(PreferenceFlags::eDisableCLStateTracking);
		SL_CASE_STR(PreferenceFlags::eDisableDebugText);
		SL_CASE_STR(PreferenceFlags::eUseManualHooking);
		SL_CASE_STR(PreferenceFlags::eAllowOTA);
		SL_CASE_STR(PreferenceFlags::eBypassOSVersionCheck);
		SL_CASE_STR(PreferenceFlags::eUseDXGIFactoryProxy);
		SL_CASE_STR(PreferenceFlags::eLoadDownloadedPlugins);
		SL_CASE_STR(PreferenceFlags::eUseFrameBasedResourceTagging);
	}
	return "Unknown";
}

inline FString getPreferenceFlagsAsStr(sl::PreferenceFlags f)
{
	FString Result;

	for (uint64 SingleBit = uint64(sl::PreferenceFlags::eUseFrameBasedResourceTagging); SingleBit != 0; SingleBit >>= 1)
	{
		sl::PreferenceFlags Flag = sl::PreferenceFlags(SingleBit);

		if (Flag == SLBitwiseAnd(f, Flag))
		{
			Result.Append(getPreferenceFlagsSingleBitFlagsAsStr(Flag));
			Result.AppendChar(TCHAR('|'));
		}
	}

	Result.RemoveFromEnd(TEXT("|"));
	return Result;
}

// Convenience function template and macro for using SLgetFeatureFunction
//
// Example use:
//	sl::Result Result = CALL_SL_FEATURE_FN(sl::kFeatureReflex, slReflexSleep, *FrameToken);
template<typename F, typename... Ts> static sl::Result CallSLFeatureFn(sl::Feature Feature, const char* FunctionName, Ts&&... args)
{
	static F PtrFn = nullptr;
	if (PtrFn == nullptr)
	{
		sl::Result Result = SLgetFeatureFunction(Feature, FunctionName,  reinterpret_cast<void*&>(PtrFn));
		checkf(Result == sl::Result::eOk, TEXT("%s: unable to map function %s (%s)"), ANSI_TO_TCHAR(__FUNCTION__), ANSI_TO_TCHAR(FunctionName), ANSI_TO_TCHAR(sl::getResultAsStr(Result)));
	}

	FString FunctionArgs;

	const TTuple<Ts...> Quarrel(args...);
	StringifySLArgument Stringifier;
	VisitTupleElements(Stringifier, Quarrel);


	LogStreamlineFunctionCall(Feature,FString::Printf(TEXT("%s"), ANSI_TO_TCHAR(FunctionName)), Stringifier.GetJoinedArgString());
	

	return PtrFn(std::forward<Ts>(args)...);
}
#define CALL_SL_FEATURE_FN(Feature, FunctionName, ...) CallSLFeatureFn<decltype(&FunctionName)>(Feature, #FunctionName, __VA_ARGS__)
