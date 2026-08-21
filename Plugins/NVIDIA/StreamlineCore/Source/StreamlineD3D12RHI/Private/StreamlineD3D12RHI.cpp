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

#include "StreamlineD3D12RHI.h"

#include "Features/IModularFeatures.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformMisc.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Misc/EngineVersionComparison.h"
#include "ID3D12DynamicRHI.h"
#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 3)
#include "Windows/WindowsD3D12ThirdParty.h" // for dxgi1_6.h
#else
#include "Windows/D3D12ThirdParty.h" // for dxgi1_6.h
#endif
#include "HAL/IConsoleManager.h"


class FD3D12Device;
#ifndef DX_MAX_MSAA_COUNT
#define DX_MAX_MSAA_COUNT	8
#endif
#if !defined(D3D12_RHI_RAYTRACING)
#define D3D12_RHI_RAYTRACING (RHI_RAYTRACING)
#endif

struct FShaderCodePackedResourceCounts;

#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION < 3
#include "D3D12Util.h"
#endif

#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Windows/IDXGISwapchainProvider.h"

#include "StreamlineAPI.h"
#include "StreamlineConversions.h"
#include "StreamlineDXGISwapchainProxy.h"
#include "StreamlineRHI.h"
#include "StreamlineNGXRHI.h"
#include "sl.h"
#include "sl_dlss_g.h"


// The UE module
DEFINE_LOG_CATEGORY_STATIC(LogStreamlineD3D12RHI, Log, All);


#define LOCTEXT_NAMESPACE "StreamlineD3D12RHI"


class FStreamlineD3D12DXGISwapchainProvider : public IDXGISwapchainProvider
{
public:
	FStreamlineD3D12DXGISwapchainProvider(const FStreamlineRHI* InRHI) : StreamlineRHI(InRHI) {}

	virtual ~FStreamlineD3D12DXGISwapchainProvider() = default;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
	bool SupportsRHI(ERHIInterfaceType RHIType) const override final { return RHIType == ERHIInterfaceType::D3D12; }
#else
	bool SupportsRHI(const TCHAR* RHIName) const override final { return FString(RHIName) == FString("D3D12"); }
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	const TCHAR* GetProviderName() const override final { return TEXT("FStreamlineD3D12DXGISwapchainProvider"); }
#else
	TCHAR* GetName() const override final
	{
		static TCHAR Name[] = TEXT("FStreamlineD3D12DXGISwapchainProvider");
		return Name;
	}
#endif

	HRESULT CreateSwapChainForHwnd(IDXGIFactory2* pFactory, IUnknown* pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1* pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullScreenDesc, IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain) override final
	{
		HRESULT DXGIResult = E_FAIL;
		const bool bUseStreamlineProxy = StreamlineRHI->IsSwapchainHookingAllowed();
		if (!bUseStreamlineProxy)
		{
			DXGIResult = pFactory->CreateSwapChainForHwnd(pDevice, hWnd, pDesc, pFullScreenDesc, pRestrictToOutput, ppSwapChain);
		}
		else
		{
			// TODO: what happens if a second swapchain is created while PIE is active?
			IDXGIFactory2* SLFactory = pFactory;
			sl::Result SLResult = SLUpgradeInterface(reinterpret_cast<void**>(&SLFactory));
			checkf(SLResult == sl::Result::eOk, TEXT("%s: error upgrading IDXGIFactory (%s)"), ANSI_TO_TCHAR(__FUNCTION__), ANSI_TO_TCHAR(sl::getResultAsStr(SLResult)));
			DXGIResult = SLFactory->CreateSwapChainForHwnd(pDevice, hWnd, pDesc, pFullScreenDesc, pRestrictToOutput, ppSwapChain);
		}

		if (FStreamlineDXGISwapChainProxy::IsEnabled())
		{
			ensure(FStreamlineDXGISwapChainProxy::WrapSwapChain(ppSwapChain, StreamlineRHI, bUseStreamlineProxy));
		}

		StreamlineRHI->OnSwapchainCreated(*ppSwapChain, bUseStreamlineProxy);
		return DXGIResult;
	}

	HRESULT CreateSwapChain(IDXGIFactory* pFactory, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain) override final
	{
		HRESULT DXGIResult = E_FAIL;
		const bool bUseStreamlineProxy = StreamlineRHI->IsSwapchainHookingAllowed();
		if (!bUseStreamlineProxy)
		{
			DXGIResult = pFactory->CreateSwapChain(pDevice, pDesc, ppSwapChain);
		}
		else
		{
			// TODO: what happens if a second swapchain is created while PIE is active?
			IDXGIFactory* SLFactory = pFactory;
			sl::Result SLResult = SLUpgradeInterface(reinterpret_cast<void**>(&SLFactory));
			checkf(SLResult == sl::Result::eOk, TEXT("%s: error upgrading IDXGIFactory (%s)"), ANSI_TO_TCHAR(__FUNCTION__), ANSI_TO_TCHAR(sl::getResultAsStr(SLResult)));
			DXGIResult = SLFactory->CreateSwapChain(pDevice, pDesc, ppSwapChain);
		}


		if (FStreamlineDXGISwapChainProxy::IsEnabled())
		{
			ensure(FStreamlineDXGISwapChainProxy::WrapSwapChain(ppSwapChain, StreamlineRHI, bUseStreamlineProxy));
		}

		StreamlineRHI->OnSwapchainCreated(*ppSwapChain, bUseStreamlineProxy);
		return DXGIResult;
	}
private:
	const FStreamlineRHI* StreamlineRHI;
};


namespace
{
	void UpdateResidency(ID3D12DynamicRHI* D3D12RHI, FRHICommandList& CmdList, FRHITexture* InTexture)
	{
		if (InTexture)
		{
		// we are not checking for ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS since for 5.5 and older we still need to add resource state transitions
		// since in 5.5 or older the RDG transitions are not reliable
		// via ID3D12DynamicRHI::RHITransitionResource which also calls UpdateResidency. So we can skip that redudant call

#if UE_VERSION_AT_LEAST(5,6,0)
			D3D12RHI->RHIUpdateResourceResidency(CmdList, D3D12RHI->RHIGetResourceDeviceIndex(InTexture), InTexture);
#endif
		}
	}

	void UpdateResidencyByTransitionBarrier(ID3D12DynamicRHI* D3D12RHI, FRHICommandList& CmdList, FRHITexture* InTexture, D3D12_RESOURCE_STATES ResourceState, uint32 SubResourceIndex)
	{
		if (InTexture)
		{
			// we are not checking for ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS since for 5.5 and older we still need to add resource state transitions
			// since in 5.5 or older the RDG transitions are not reliable
			// via ID3D12DynamicRHI::RHITransitionResource which also calls UpdateResidency. So we can skip that redudant call

#if UE_VERSION_OLDER_THAN(5,6,0)

		// This is a workaround for a GPU memory residency issues that can happen in 5.5 when the resources are not resident
		// when under memory pressure
		// 5.6 has a method to allow plugins to make textures resident, see above
		// for older UE versions we are using a side-effect of IFD3D12DynamicRHI::RHITransitionResource, which makes the resource resident
		// at the begin before actually going into the transition logic so we use that to make all DLSS input and output resources resident
		// Note: This also adds a pending resource state transitions to the D3D12RHI state tracker that then need to get flushed explicitely 
		//       at the calls site. That's done since only when the d3d debug layer is active since the underlying implementation is not too picky
		//       about resource states. But having d3d debug layer compatiblity is useful for developers

			D3D12RHI->RHITransitionResource(CmdList, InTexture, ResourceState, SubResourceIndex);
#endif
		}
	}
}


class STREAMLINED3D12RHI_API FStreamlineD3D12RHI : public FStreamlineRHI
{
public:

	FStreamlineD3D12RHI(const FStreamlineRHICreateArguments& Arguments)
	:	FStreamlineRHI(Arguments)
		, D3D12RHI(CastDynamicRHI<ID3D12DynamicRHI>(Arguments.DynamicRHI))
	{
		UE_LOG(LogStreamlineD3D12RHI, Log, TEXT("%s Enter"), ANSI_TO_TCHAR(__FUNCTION__));

		check(D3D12RHI != nullptr);
		TArray<FD3D12MinimalAdapterDesc> AdapterDescs = D3D12RHI->RHIGetAdapterDescs();
		check(AdapterDescs.Num() > 0);
		if (AdapterDescs.Num() > 1)
		{
			UE_LOG(LogStreamlineD3D12RHI, Warning, TEXT("%s: found %d adapters, using first one found to query feature availability"), ANSI_TO_TCHAR(__FUNCTION__), AdapterDescs.Num());
		}

		const DXGI_ADAPTER_DESC& DXGIAdapterDesc = AdapterDescs[0].Desc;
		AdapterLuid = DXGIAdapterDesc.AdapterLuid;
		SLAdapterInfo.deviceLUID = reinterpret_cast<uint8_t*>(&AdapterLuid);
		SLAdapterInfo.deviceLUIDSizeInBytes = sizeof(AdapterLuid);
		SLAdapterInfo.vkPhysicalDevice = nullptr;

		if (IsStreamlineSupported())
		{
			TTuple<bool, FString> bSwapchainProvider = IsSwapChainProviderRequired(SLAdapterInfo);
			if (bSwapchainProvider.Get<0>())
			{
				UE_LOG(LogStreamlineD3D12RHI, Log, TEXT("Registering FStreamlineD3D12DXGISwapchainProvider as IDXGISwapchainProvider, due to %s"), *bSwapchainProvider.Get<1>());
				CustomSwapchainProvider = MakeUnique<FStreamlineD3D12DXGISwapchainProvider>(this);
				IModularFeatures::Get().RegisterModularFeature(IDXGISwapchainProvider::GetModularFeatureName(), CustomSwapchainProvider.Get());
				bIsSwapchainProviderInstalled = true;
			}
			else
			{
				UE_LOG(LogStreamlineD3D12RHI, Log, TEXT("Skip registering IDXGISwapchainProvider, due to %s"), *bSwapchainProvider.Get<1>());
				bIsSwapchainProviderInstalled = false;
			}

#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
			UE_CLOG(NeedExtraPassesForDebugLayerCompatibility(), LogStreamlineD3D12RHI, Warning, TEXT("Adding extra renderpasses for Streamline D3D debug layer compatibility. See StreamlineRHI.h for alternatives"));
#endif 
		}

		UE_LOG(LogStreamlineD3D12RHI, Log, TEXT("%s Leave"), ANSI_TO_TCHAR(__FUNCTION__));
	}

#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
	bool NeedExtraPassesForDebugLayerCompatibility()
	{
#if UE_VERSION_AT_LEAST(5,6,0)
		return false;
#elif UE_VERSION_AT_LEAST(5,3,0)
		return GRHIIsDebugLayerEnabled;
#else
		return D3D12RHI->IsD3DDebugEnabled();
#endif
	}
#endif


	virtual ~FStreamlineD3D12RHI()
	{
		UE_LOG(LogStreamlineD3D12RHI, Log, TEXT("%s Enter"), ANSI_TO_TCHAR(__FUNCTION__));
		if (CustomSwapchainProvider.IsValid())
		{
			UE_LOG(LogStreamlineD3D12RHI, Log, TEXT("Unregistering FStreamlineD3D12DXGISwapchainProvider as IDXGISwapchainProvider"));
			IModularFeatures::Get().UnregisterModularFeature(IDXGISwapchainProvider::GetModularFeatureName(), CustomSwapchainProvider.Get());
			CustomSwapchainProvider.Reset();
		}
		UE_LOG(LogStreamlineD3D12RHI, Log, TEXT("%s Leave"), ANSI_TO_TCHAR(__FUNCTION__));
	}

	static D3D12_RESOURCE_STATES GetD3D12ResourceStateFromRHIAccess(ERHIAccess RHIAccess)
	{
		D3D12_RESOURCE_STATES D3D12ResourceState = D3D12_RESOURCE_STATE_COMMON;

		if (EnumHasAnyFlags(RHIAccess, ERHIAccess::CopySrc))
		{
			D3D12ResourceState |= D3D12_RESOURCE_STATE_COPY_SOURCE;
		}

		if (EnumHasAnyFlags(RHIAccess, ERHIAccess::CopyDest))
		{
			D3D12ResourceState |= D3D12_RESOURCE_STATE_COPY_DEST;
		}

		if (EnumHasAnyFlags(RHIAccess, ERHIAccess::DSVRead))
		{
			D3D12ResourceState |= D3D12_RESOURCE_STATE_DEPTH_READ;
		}

		if (EnumHasAnyFlags(RHIAccess, ERHIAccess::DSVWrite))
		{
			D3D12ResourceState |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}

		if (EnumHasAnyFlags(RHIAccess, ERHIAccess::SRVCompute))
		{
			D3D12ResourceState |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}

		if (EnumHasAnyFlags(RHIAccess, ERHIAccess::SRVGraphics))
		{
			D3D12ResourceState |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}

		if (EnumHasAnyFlags(RHIAccess, ERHIAccess::UAVMask))
		{
			D3D12ResourceState |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		if (EnumHasAnyFlags(RHIAccess, ERHIAccess::RTV))
		{
			D3D12ResourceState |= D3D12_RESOURCE_STATE_RENDER_TARGET;
		}

		if (EnumHasAnyFlags(RHIAccess, ERHIAccess::Present))
		{
			D3D12ResourceState |= D3D12_RESOURCE_STATE_PRESENT;
		}

		if (EnumHasAnyFlags(RHIAccess, ERHIAccess::IndirectArgs))
		{
			D3D12ResourceState |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
		}

		return D3D12ResourceState;
	}


	ID3D12GraphicsCommandList* GetNativeCommandList(FRHICommandList& CmdList, const TArrayView<const FRHIStreamlineResource> InResources)
	{
		ID3D12GraphicsCommandList* NativeCmdList = nullptr;

		for (const FRHIStreamlineResource& Resource : InResources)
		{
			if (Resource.Texture)
			{
				// that's inconsistent with below, but...
				check(Resource.Texture->IsValid());
				NativeCmdList = D3D12RHI->RHIGetGraphicsCommandList(RHICMDLIST_ARG_PASSTHROUGH D3D12RHI->RHIGetResourceDeviceIndex(Resource.Texture));
				// TODO check that all resources have the same device index. So if that ever changes we might need to split the calls into slTag into per command list/per device index calls.
				// for now we take any commandlist
				break;
			}
		}
		return NativeCmdList;
	}

	struct FStreamlineD3D12Transition
	{
		FRHITexture* Texture;
		D3D12_RESOURCE_STATES AfterState;
		uint32 SubresourceIndex;
		
		FStreamlineD3D12Transition(FRHITexture* InTexture,	D3D12_RESOURCE_STATES InAfterState,	uint32 InSubresourceIndex)
			: Texture(InTexture)
			, AfterState(InAfterState)
			, SubresourceIndex(InSubresourceIndex)
		{

		}

	};


	virtual void TagTextures(FRHICommandList& CmdList, uint32 InViewID, const sl::FrameToken& FrameToken, const TArrayView<const FRHIStreamlineResource> InResources, bool isValidUntilEval = false) final
	{
		RHI_SCOPED_DRAW_EVENT(CmdList, StreamlineTagTextures);

		if (InResources.IsEmpty())
		{
			return;
		}

#if ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
		for (const FRHIStreamlineResource& Resource : InResources)
		{
			UpdateResidency(D3D12RHI, CmdList, Resource.Texture);
		}
#endif 
		// adding + 1 to get to the count
		constexpr uint32 AllocatorNum = uint32(EStreamlineResource::Last) + 1;

		// those get filled in also for null input resource so we can "Streamline nulltag" them
		TArray<sl::Resource, TInlineAllocator<AllocatorNum>> SLResources;
		TArray<sl::ResourceTag, TInlineAllocator<AllocatorNum>> SLTags;

		// if all input resources are nullptr, those arrays stay empty below
		TArray<FStreamlineD3D12Transition, TInlineAllocator<AllocatorNum>> PreTagTransitions;

#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
		FRHITexture* DebugLayerCompatibilityHelperSource = nullptr;
		FRHITexture* DebugLayerCompatibilityHelperDest = nullptr;
#endif 

		for(const FRHIStreamlineResource&  Resource : InResources)
		{
			sl::Resource SLResource;
			FMemory::Memzero(SLResource);
			SLResource.type = sl::ResourceType::eCount;

			sl::ResourceTag SLTag;
			SLTag.type = ToSL(Resource.StreamlineTag);
			// TODO: sl::ResourceLifecycle::eValidUntilPresent would be more efficient, are there any textures where it's applicable?
			if(isValidUntilEval)
			{
				SLTag.lifecycle = sl::ResourceLifecycle::eValidUntilEvaluate;
			}
			else
			{
				SLTag.lifecycle = sl::ResourceLifecycle::eOnlyValidNow;
			}

			if(Resource.Texture && Resource.Texture->IsValid())
			{

#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
				if (NeedExtraPassesForDebugLayerCompatibility())
				{
					check(Resource.DebugLayerCompatibilityHelperSource);
					check(Resource.DebugLayerCompatibilityHelperDest);
					DebugLayerCompatibilityHelperSource = Resource.DebugLayerCompatibilityHelperSource;
					DebugLayerCompatibilityHelperDest = Resource.DebugLayerCompatibilityHelperDest;
				}
#endif 
				SLResource.native = Resource.Texture->GetNativeResource();
				SLResource.type = sl::ResourceType::eTex2d;
				SLTag.extent = ToSL(Resource.ViewRect);

				check(Resource.StreamlineTag == EStreamlineResource::Backbuffer || Resource.ResourceRHIAccess != ERHIAccess::Unknown);
				
				const D3D12_RESOURCE_STATES ResourceStates = GetD3D12ResourceStateFromRHIAccess(Resource.ResourceRHIAccess);

				// for 5.5 and older we need to additionally transition the resource to the state since the RDG doesn't do the work
				// that also implicitely makes them resident.
#if UE_VERSION_OLDER_THAN(5,6,0)
				PreTagTransitions.Emplace (Resource.Texture, ResourceStates, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES );
#endif
				SLResource.state = ResourceStates;
			} // if resource is valid
			else
			{
				// explicitely nulltagging so SL removes it from it's internal book keeping
				SLResource.native = nullptr;
			}

			// order matters here so we first put the resource into our array and then point the sltag at the resource in the array
			// Note: we have an TInline Allocator so our memory is pre-allocated so we should not have a re-allocation here (which then would invalidate pointers previously stored)
			SLResources.Add(SLResource);
			SLTag.resource = &SLResources.Last();
			SLTags.Add(SLTag);
		} 
		
#if UE_VERSION_OLDER_THAN(5,6,0)
		{
			// if we nulltag D3D12Device is nullptr and PreTagTransitions  is empty
			// transition any resources before
			for (FStreamlineD3D12Transition& Transition : PreTagTransitions)
			{
				UpdateResidencyByTransitionBarrier(D3D12RHI, CmdList, Transition.Texture, Transition.AfterState, Transition.SubresourceIndex);
			}
		}
#endif

		{
			RHI_SCOPED_DRAW_EVENT(CmdList, FlushPendingRHIBarriers);
#if ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
			const FRHIGPUMask EffectiveMask = CmdList.GetGPUMask();
			for (uint32 GPUIndex : EffectiveMask)
			{
				D3D12RHI->RHIFlushResourceBarriers(CmdList, GPUIndex);
			}
#else

			// This is a workaround for older UE versions that don't have anexplicit plugin API to flush pending resources
			// we are using the sideffects of RHICopyTexture to get a call to Flush pending resource barriers on the *other* resources
			// this is only needed to increase compatibility with the D3D12 debug layers
			if (NeedExtraPassesForDebugLayerCompatibility())
			{
				check(DebugLayerCompatibilityHelperSource);
				check(DebugLayerCompatibilityHelperDest);

				// we are calling/adding RHICmdList methods while we are inside an RHICmdList execution. This triggers some !IsExecuting checks in the RHICommandLinst methods
				// We avoid this by following a pattern seen in the actual RHI implementation
				TRHICommandList_RecursiveHazardous<IRHICommandContext> RHICmdList(&CmdList.GetContext());

				RHI_SCOPED_DRAW_EVENT(RHICmdList, UE5_5AndOlderBackdoorViaUnrelatedCopy);
				FRHICopyTextureInfo CopyInfo;
				CopyInfo.Size = FIntVector(1, 1, 1); // explicitly only copying 1 pixel in case the texture that we get from RDG is larger 
				RHICmdList.GetContext().RHICopyTexture(DebugLayerCompatibilityHelperSource, DebugLayerCompatibilityHelperDest, CopyInfo);
			}
#endif
		}

	
		{
			RHI_SCOPED_DRAW_EVENT(CmdList, slSetTag);
			// note that NativeCmdList might be null if we only have resources to "Streamline nulltag"
			
			// when removing this deprecated path, we only need to keep the else block
			if (ShouldUseSlSetTag())
			{
				SLsetTag(sl::ViewportHandle(InViewID), SLTags.GetData(), SLTags.Num(), GetNativeCommandList(CmdList, InResources));
			}
			else
			{
				SLsetTagForFrame(FrameToken, sl::ViewportHandle(InViewID), SLTags.GetData(), SLTags.Num(), GetNativeCommandList(CmdList, InResources));
			}
		}
	}

	virtual void* GetCommandBuffer(FRHICommandList& CmdList, FRHITexture* Texture) override final
	{
		ID3D12GraphicsCommandList* NativeCmdList = D3D12RHI->RHIGetGraphicsCommandList(RHICMDLIST_ARG_PASSTHROUGH D3D12RHI->RHIGetResourceDeviceIndex(Texture));
		return static_cast<void*>(NativeCmdList);
	}


	void PostStreamlineFeatureEvaluation(FRHICommandList& CmdList, FRHITexture* Texture) final
	{
		const uint32 DeviceIndex = D3D12RHI->RHIGetResourceDeviceIndex(Texture);
		D3D12RHI->RHIFinishExternalComputeWork(RHICMDLIST_ARG_PASSTHROUGH DeviceIndex, D3D12RHI->RHIGetGraphicsCommandList(RHICMDLIST_ARG_PASSTHROUGH DeviceIndex));
	}

	virtual const sl::AdapterInfo* GetAdapterInfo() override final
	{
		return &SLAdapterInfo;
	}

	virtual bool IsDLSSGSupportedByRHI() const override final
	{
		return true;
	}
	
	virtual bool IsDeepDVCSupportedByRHI() const override final
	{
		return true;
	}

	virtual bool IsReflexSupportedByRHI() const override final
	{
		return true;
	}

	virtual bool IsPluginSideSwapchainProxyEnabled() const override final
	{
		return FStreamlineDXGISwapChainProxy::IsEnabled();
	}

	virtual void APIErrorHandler(const sl::APIError& LastError) const final
	{
		// Not all DXGI return codes are errors, e.g. DXGI_STATUS_OCCLUDED
		if (IsDXGIStatus(LastError.hres))
		{
			return;
		}

		TCHAR ErrorMessage[1024];
		FPlatformMisc::GetSystemErrorMessage(ErrorMessage, 1024, LastError.hres);
		UE_LOG(LogStreamlineD3D12RHI, Log, TEXT("DLSSG D3D12/DXGI Error 0x%x (%s)"), LastError.hres, ErrorMessage);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		D3D12RHI->RHIVerifyResult(static_cast<ID3D12Device*>(D3D12RHI->RHIGetNativeDevice()), LastError.hres, "Streamline/DLSSG present", __FILE__, __LINE__);
#else


	// that should be set in the 5.1 to 4.27 backport branches that have D3D12RHI_API for VerifyD3D12Result
	// and optionally a 5.2 NVRTX branch
#if!defined HAS_VERIFYD3D12_DLL_EXPORT
#define HAS_VERIFYD3D12_DLL_EXPORT (defined (ENGINE_STREAMLINE_VERSION) && ENGINE_STREAMLINE_VERSION >=3 ) 
#endif

#if IS_MONOLITHIC || HAS_VERIFYD3D12_DLL_EXPORT
		VerifyD3D12Result(LastError.hres, "Streamline/DLSSG present", __FILE__, __LINE__,static_cast<ID3D12Device*>(GDynamicRHI->RHIGetNativeDevice()));
#else
		using VerifyD3D12ResultPtrType = void (HRESULT, const ANSICHAR* , const ANSICHAR* , uint32 , ID3D12Device*, FString );
		VerifyD3D12ResultPtrType* VerifyD3D12ResultPtr = nullptr;
		const TCHAR* VerifyD3D12ResultDemangledName = TEXT("?VerifyD3D12Result@D3D12RHI@@YAXJPEBD0IPEAUID3D12Device@@VFString@@@Z");

		const FString D3D12RHIBinaryPath = FModuleManager::Get().GetModuleFilename(FName(TEXT("D3D12RHI")));
		void*D3D12BinaryDLL = FPlatformProcess::GetDllHandle(*D3D12RHIBinaryPath);

		VerifyD3D12ResultPtr = (VerifyD3D12ResultPtrType*)(FWindowsPlatformProcess::GetDllExport(D3D12BinaryDLL, VerifyD3D12ResultDemangledName));
		UE_LOG(LogStreamlineD3D12RHI, Log, TEXT("%s = %p"), VerifyD3D12ResultDemangledName, VerifyD3D12ResultPtr);

		if (VerifyD3D12ResultPtr)
		{
			VerifyD3D12ResultPtr(LastError.hres, "Streamline/DLSSG present", __FILE__, __LINE__, static_cast<ID3D12Device*>(GDynamicRHI->RHIGetNativeDevice()), FString());
		}
		else
		{
			UE_LOG(LogStreamlineD3D12RHI, Log, TEXT("Please add a D3D12RHI_API to the declaration of VerifyD3D12Result in D3D12Util.h to allow non monolithic builds to pipe handling of this error into the D3D12RHI DX/DXGI error handling system"));
		}
#endif

#endif
	}

	virtual bool IsStreamlineSwapchainProxy(void* NativeSwapchain) const override final
	{
		TRefCountPtr<IUnknown> NativeInterface;
		const sl::Result Result = SLgetNativeInterface(NativeSwapchain, IID_PPV_ARGS_Helper(NativeInterface.GetInitReference()));

		if (Result == sl::Result::eOk)
		{
			const bool bIsProxy = NativeInterface != NativeSwapchain;
			UE_LOG(LogStreamlineD3D12RHI, VeryVerbose, TEXT("%s %s NativeInterface=%p NativeSwapchain=%p isProxy=%u "), ANSI_TO_TCHAR(__FUNCTION__), *CurrentThreadName(), NativeSwapchain, NativeInterface.GetReference(), bIsProxy);
			return bIsProxy;
		}
		else
		{
			UE_LOG(LogStreamlineD3D12RHI, Log, TEXT("SLgetNativeInterface(%p) failed (%d, %s)"), NativeSwapchain,  Result, ANSI_TO_TCHAR(sl::getResultAsStr(Result)));
		}
		return false;
	}
	

protected:

private:
	ID3D12DynamicRHI* D3D12RHI = nullptr;
	LUID AdapterLuid;
	sl::AdapterInfo SLAdapterInfo;
	TUniquePtr<FStreamlineD3D12DXGISwapchainProvider> CustomSwapchainProvider;

};


/** IModuleInterface implementation */

void FStreamlineD3D12RHIModule::StartupModule()
{
	auto CVarInitializePlugin = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.InitializePlugin"));
	if (CVarInitializePlugin && !CVarInitializePlugin->GetBool() ||  (FParse::Param(FCommandLine::Get(), TEXT("slno"))))
	{
		UE_LOG(LogStreamlineD3D12RHI, Log, TEXT("Initialization of StreamlineD3D12RHI is disabled."));
		return;
	}

	UE_LOG(LogStreamlineD3D12RHI, Log, TEXT("%s Enter"), ANSI_TO_TCHAR(__FUNCTION__));
	const auto [bIsSupported, NotSupportedReason] = IsEngineExecutionModeSupported();
	if(bIsSupported)
	{
		if ((GDynamicRHI != nullptr) && (RHIGetInterfaceType() == ERHIInterfaceType::D3D12))
		{
			FStreamlineRHIModule& StreamlineRHIModule = FModuleManager::LoadModuleChecked<FStreamlineRHIModule>(TEXT("StreamlineRHI"));
			if (AreStreamlineFunctionsLoaded())
			{
				StreamlineRHIModule.InitializeStreamline();
				if (IsStreamlineSupported())
				{
					sl::Result Result = SLsetD3DDevice(GDynamicRHI->RHIGetNativeDevice());
					checkf(Result == sl::Result::eOk, TEXT("%s: SLsetD3DDevice failed (%s)"), ANSI_TO_TCHAR(__FUNCTION__), ANSI_TO_TCHAR(sl::getResultAsStr(Result)));
				}
			}
		}
		else
		{
			UE_LOG(LogStreamlineD3D12RHI, Log, TEXT("D3D12RHI is not the active DynamicRHI; skipping of setting up the custom swapchain factory"));
		}
	}
	else
	{
		UE_LOG(LogStreamlineD3D12RHI, Log, TEXT("Skipping Streamline initialization for this UE instance due to: '%s'"), NotSupportedReason);
	}
	UE_LOG(LogStreamlineD3D12RHI, Log, TEXT("%s Leave"), ANSI_TO_TCHAR(__FUNCTION__));
}

void FStreamlineD3D12RHIModule::ShutdownModule()
{
	auto CVarInitializePlugin = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.InitializePlugin"));
	if (CVarInitializePlugin && !CVarInitializePlugin->GetBool())
	{
		return;
	}

	UE_LOG(LogStreamlineD3D12RHI, Log, TEXT("%s Enter"), ANSI_TO_TCHAR(__FUNCTION__));
	UE_LOG(LogStreamlineD3D12RHI, Log, TEXT("%s Leave"), ANSI_TO_TCHAR(__FUNCTION__));
}

TUniquePtr<FStreamlineRHI> FStreamlineD3D12RHIModule::CreateStreamlineRHI(const FStreamlineRHICreateArguments& Arguments)
{
	TUniquePtr<FStreamlineRHI> Result(new FStreamlineD3D12RHI(Arguments));
	return Result;
}

IMPLEMENT_MODULE(FStreamlineD3D12RHIModule, StreamlineD3D12RHI )
#undef LOCTEXT_NAMESPACE
