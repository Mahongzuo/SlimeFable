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

#include "Modules/ModuleManager.h"

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "RenderGraphResources.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Misc/EngineVersionComparison.h"
#include "RHIAccess.h"
#include "StreamlineNGXRHI.h"

#define UE_API STREAMLINERHI_API

namespace sl
{
	struct AdapterInfo;
	struct FrameToken;
	struct APIError;
	struct FeatureRequirements;
	using Feature = uint32_t;
	enum class FeatureRequirementFlags : uint32_t;
}

class FSLFrameTokenProvider;

enum class EStreamlineSupport : uint8
{
	Supported,
	NotSupported,
	NotSupportedIncompatibleRHI,
	NumValues
};


enum class EStreamlineResource
{
	Depth,
	MotionVectors,
	HUDLessColor,
	UIColorAndAlpha,
	Backbuffer,
	ScalingOutputColor,

	// we use this to size some arrays statically somewhere, but we also don't wanto have a real new enum value so we don't have to handle switch statements ...
	Last = ScalingOutputColor
};

class FRHIStreamlineResource
{
public:
	FRHITexture* Texture = nullptr;
	FIntRect ViewRect = FIntRect(FIntPoint::ZeroValue, FIntPoint::ZeroValue);
	EStreamlineResource StreamlineTag;
	ERHIAccess ResourceRHIAccess = ERHIAccess::Unknown;

	// The other option would be to have those be passed in as an extra argument for TagTextures,
	// but that's inconvenient and also that code eventually will get aged out as we drop support for 
	// UE 5.5 older, whenever that might happen
#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
	FRHITexture* DebugLayerCompatibilityHelperSource = nullptr;
	FRHITexture* DebugLayerCompatibilityHelperDest = nullptr;
#endif 

	static FRHIStreamlineResource FromRDGTextureAccess(FRDGTextureAccess InRDGResource, EStreamlineResource InTag)
	{
		return FromRDGTexture(InRDGResource.GetTexture(), InRDGResource.GetAccess(), InTag);
	}

	static FRHIStreamlineResource FromRDGTextureAccess(FRDGTextureAccess InRDGResource, FIntRect InRect, EStreamlineResource InTag)
	{
		return FromRDGTexture(InRDGResource.GetTexture(), InRDGResource.GetAccess(), InRect, InTag);
	}
		 
	static FRHIStreamlineResource NullResource(EStreamlineResource InTag)
	{
		return { nullptr, FIntRect(FIntPoint::ZeroValue, FIntPoint::ZeroValue), InTag , ERHIAccess::Unknown};
	}

private:
	static FRHIStreamlineResource FromRDGTexture(FRDGTexture* InRDGResource, ERHIAccess InResourceState, EStreamlineResource InTag)
	{
		return { InRDGResource ? InRDGResource->GetRHI() : nullptr,
			FIntRect(FIntPoint::ZeroValue,InRDGResource ? InRDGResource->Desc.Extent :FIntPoint::ZeroValue),
			InTag,
			   InRDGResource ? InResourceState : ERHIAccess::Unknown,
		};
	}

	static FRHIStreamlineResource FromRDGTexture(FRDGTexture* InRDGResource, ERHIAccess InResourceState, FIntRect InRect, EStreamlineResource InTag)
	{
		return { InRDGResource ? InRDGResource->GetRHI() : nullptr,	InRect,	InTag, InRDGResource ? InResourceState : ERHIAccess::Unknown };
	}

};

// TODO STREAMLINE rename variables
struct FRHIStreamlineArguments
{

#if ENGINE_MAJOR_VERSION < 5
	// New type names with dims and precision, required in SHADER_PARAMETER declarations and useful everywhere
	using FMatrix44f = FMatrix;
	using FVector2f = FVector2D;
	using FVector3f = FVector;
	using FVector4f = FVector4;
#else
	using FMatrix44f = ::FMatrix44f;
	using FVector3f = ::FVector3f;
	using FVector2f = ::FVector2f;
	using FVector4f = ::FVector4f;
#endif
	
	// View ID across all active views
	uint32 ViewId;

	uint32 FrameId;

	//! Specifies if previous frame has no connection to the current one (motion vectors are invalid)
	bool bReset;

	//! Specifies if depth values are inverted (value closer to the camera is higher) or not.
	bool bIsDepthInverted;

	//! Specifies clip space jitter offset
	FVector2f JitterOffset;

	//! Specifies scale factors used to normalize motion vectors (so the values are in [-1,1] range)
	FVector2f MotionVectorScale;
	//! Specifies if motion vectors are already dilated or not.
	bool bAreMotionVectorsDilated;

	//! Specifies if orthographic projection is used or not.
	bool bIsOrthographicProjection;
	//! Specifies matrix transformation from the camera view to the clip space.
	FMatrix44f CameraViewToClip;
	//! Specifies matrix transformation from the clip space to the camera view space.
	FMatrix44f ClipToCameraView;
	//! Specifies matrix transformation describing lens distortion in clip space.
	FMatrix44f ClipToLenseClip;
	//! Specifies matrix transformation from the current clip to the previous clip space.
	FMatrix44f ClipToPrevClip;
	//! Specifies matrix transformation from the previous clip to the current clip space.
	FMatrix44f PrevClipToClip;

	//! Specifies camera position in world space.
	FVector3f CameraOrigin;
	//! Specifies camera up vector in world space.
	FVector3f CameraUp;
	//! Specifies camera right vector in world space.
	FVector3f CameraRight;
	//! Specifies camera forward vector in world space.
	FVector3f CameraForward;

	//! Specifies camera near view plane distance.
	float CameraNear;
	//! Specifies camera far view plane distance.
	float CameraFar;
	//! Specifies camera field of view in radians.
	float CameraFOV;
	//! Specifies camera aspect ratio defined as view space width divided by height.
	float CameraAspectRatio;
	//! Specifies camera pinhole offset.
	FVector2f CameraPinholeOffset;
	
	
};

struct FStreamlineRHICreateArguments
{
	FString PluginBaseDir;
	FDynamicRHI* DynamicRHI = nullptr;
};

class FSLFrameTokenProvider
{
public:
	FSLFrameTokenProvider();

	sl::FrameToken* GetTokenForFrame(uint64 FrameCounter);

private:
	FCriticalSection Section;
	sl::FrameToken* FrameToken;
	uint32_t LastFrameCounter;
};


class  FStreamlineRHIModule;


class FStreamlineRHI
{

	friend class FStreamlineRHIModule;
public:
	UE_API virtual ~FStreamlineRHI();

	UE_API virtual void SetStreamlineData(FRHICommandList& CmdList, const FRHIStreamlineArguments& InArguments);
	UE_API void StreamlineEvaluateDeepDVC(FRHICommandList& CmdList, const FRHIStreamlineResource& InputOutput, sl::FrameToken* FrameToken, uint32 ViewID);
	
	void TagTextures(FRHICommandList& CmdList, uint32 InViewID, const sl::FrameToken& FrameToken, std::initializer_list< FRHIStreamlineResource> InResources, bool bIsValidUntilEval = false)
	{
		TagTextures(CmdList, InViewID, FrameToken, MakeArrayView(InResources), bIsValidUntilEval);
	}

	void TagTexture(FRHICommandList& CmdList, uint32 InViewID, const sl::FrameToken& FrameToken, const FRHIStreamlineResource& InResource, bool bIsValidUntilEval = false)
	{
		TagTextures(CmdList, InViewID, FrameToken, MakeArrayView<const FRHIStreamlineResource>(&InResource, 1), bIsValidUntilEval);
	}

	// Implemented by API specific  subclasses
	//	
public: 
	virtual void TagTextures(FRHICommandList& CmdList, uint32 InViewID, const sl::FrameToken& FrameToken, const TArrayView<const FRHIStreamlineResource> InResources, bool bIsValidUntilEval = false) = 0;
	virtual const sl::AdapterInfo* GetAdapterInfo() = 0;
	virtual void APIErrorHandler(const sl::APIError& LastError) const = 0;
	
	virtual bool IsPluginSideSwapchainProxyEnabled() const = 0;

protected:

	virtual void* GetCommandBuffer(FRHICommandList& CmdList, FRHITexture* Texture) = 0;
	virtual void PostStreamlineFeatureEvaluation(FRHICommandList& CmdList, FRHITexture* Texture) = 0;


	UE_API TTuple<bool, FString> IsSwapChainProviderRequired(const sl::AdapterInfo& AdapterInfo) const;
public:
	virtual bool IsDLSSGSupportedByRHI() const
	{
		return false;
	}

	virtual bool IsDeepDVCSupportedByRHI() const
	{
		return false;
	}

	virtual bool IsReflexSupportedByRHI() const
	{
		return false;
	}

	UE_API bool IsStreamlineAvailable() const;

	static bool IsIncompatibleAPICaptureToolActive()
	{
		return bIsIncompatibleAPICaptureToolActive;
	}

	UE_API sl::FrameToken* GetFrameToken(uint64 FrameCounter);
	UE_API bool IsSwapchainHookingAllowed() const;
	bool IsSwapchainProviderInstalled() const;
	UE_API void ReleaseStreamlineResourcesForAllFeatures(uint32 ViewID);

	// that needs to call some virtual methods that we can't call in the ctor. Just C++ things
	void PostPlatformRHICreateInit();

	UE_API void OnSwapchainDestroyed(void* InNativeSwapchain, bool bIsKnownProxy = false) const;
	UE_API void OnSwapchainCreated(void* InNativeSwapchain, bool bIsKnownProxy = false) const;

#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
	UE_API virtual bool NeedExtraPassesForDebugLayerCompatibility();
#endif

#if WITH_EDITOR
	bool IsUnsupportedPIEActive() const { return bIsUnsupportedPIEActive; }
#endif

protected:


	UE_API FStreamlineRHI(const FStreamlineRHICreateArguments& Arguments);

#if WITH_EDITOR

	void OnBeginPIE(const bool bIsSimulating);
	void OnEndPIE(const bool bIsSimulating);
	bool bIsSupportedPIEActive = false;
	bool bIsUnsupportedPIEActive = false;
	FDelegateHandle BeginPIEHandle;
	FDelegateHandle EndPIEHandle;
#endif


	mutable int32 NumActiveSwapchainProxies = 0;
	virtual bool IsStreamlineSwapchainProxy(void* NativeSwapchain) const = 0;
	
	int32 GetMaxNumSwapchainProxies() const;
	void ValidateNumSwapchainProxies(const char* CallSite) const;
#if PLATFORM_WINDOWS
	// whether an HRESULT is a DXGI_STATUS_*
	UE_API bool IsDXGIStatus(const HRESULT HR) const ;
#endif

	

	FDynamicRHI* DynamicRHI = nullptr;
	TUniquePtr<FSLFrameTokenProvider> FrameTokenProvider = nullptr;

	static bool bIsIncompatibleAPICaptureToolActive;


	bool bIsSwapchainProviderInstalled = false;
	static TArray<sl::Feature> FeaturesRequestedAtSLInitTime;
	

	TArray<sl::Feature> LoadedFeatures;
	TArray<sl::Feature> SupportedFeatures;

};

class IStreamlineRHIModule : public IModuleInterface
{
public:

	virtual TUniquePtr<FStreamlineRHI> CreateStreamlineRHI(const FStreamlineRHICreateArguments& Arguments) = 0;
};

class FStreamlineRHIModule final : public IModuleInterface
{
public:
	UE_API void InitializeStreamline();
	UE_API void ShutdownStreamline();

	/** IModuleInterface implementation */ 
	virtual void StartupModule();
	virtual void ShutdownModule();

private:
	FString StreamlineBinaryDirectory;
	FString StreamlineBinaryFlavor;
};

STREAMLINERHI_API void PlatformCreateStreamlineRHI();
STREAMLINERHI_API FStreamlineRHI* GetPlatformStreamlineRHI();
STREAMLINERHI_API EStreamlineSupport GetPlatformStreamlineSupport();
STREAMLINERHI_API bool IsStreamlineSupported();
STREAMLINERHI_API bool AreStreamlineFunctionsLoaded();

STREAMLINERHI_API sl::FeatureRequirementFlags PlatformGetAllImplementedStreamlineRHIs();


namespace sl
{

};

STREAMLINERHI_API bool StreamlineFilterRedundantSetOptionsCalls();
STREAMLINERHI_API void LogStreamlineFeatureSupport(sl::Feature Feature, const sl::AdapterInfo& Adapter);
STREAMLINERHI_API void LogStreamlineFeatureRequirements(sl::Feature Feature, const sl::FeatureRequirements& Requirements);
STREAMLINERHI_API FString CurrentThreadName();


STREAMLINERHI_API bool ShouldUseSlSetTag();
STREAMLINERHI_API bool ShouldUseSlateCallbacksForSwapchainTracking();

#undef UE_API
