/*
* Copyright (c) 2020 - 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
* NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
* property and proprietary rights in and to this material, related
* documentation and any modifications thereto. Any use, reproduction,
* disclosure or distribution of this material and related documentation
* without an express license agreement from NVIDIA CORPORATION or
* its affiliates is strictly prohibited.
*/

#include "NGXD3D12RHI.h"

#include "nvsdk_ngx.h"
#include "nvsdk_ngx_helpers.h"
#include "nvsdk_ngx_helpers_dlssd.h"

#include "ID3D12DynamicRHI.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/EngineVersionComparison.h"
#include "StreamlineNGXRHI.h"


DEFINE_LOG_CATEGORY_STATIC(LogDLSSNGXD3D12RHI, Log, All);

#define LOCTEXT_NAMESPACE "FNGXD3D12RHIModule"


class FD3D12NGXDLSSFeature final : public NGXDLSSFeature
{

public:
	using NGXDLSSFeature::NGXDLSSFeature;

	virtual ~FD3D12NGXDLSSFeature()
	{
		check(!IsRunningRHIInSeparateThread() || IsInRHIThread());
		check(NGXRHI::NGXInitialized());
		NVSDK_NGX_Result ResultReleaseFeature = NVSDK_NGX_D3D12_ReleaseFeature(Feature);
		checkf(NVSDK_NGX_SUCCEED(ResultReleaseFeature), TEXT("NVSDK_NGX_D3D12_ReleaseFeature failed! (%u %s), %s"), ResultReleaseFeature, GetNGXResultAsString(ResultReleaseFeature), *Desc.GetDebugDescription());

		if (Parameter != nullptr)
		{
			NVSDK_NGX_Result ResultDestroyParameter = NVSDK_NGX_D3D12_DestroyParameters(Parameter);
			checkf(NVSDK_NGX_SUCCEED(ResultDestroyParameter), TEXT("NVSDK_NGX_D3D12_DestroyParameters failed! (%u %s), %s"), ResultDestroyParameter, GetNGXResultAsString(ResultDestroyParameter), *Desc.GetDebugDescription());
		}
	}
};

class FNGXD3D12RHI final : public NGXRHI
{

public:
	FNGXD3D12RHI(const FNGXRHICreateArguments& Arguments);
	virtual void ExecuteDLSS(FRHICommandList& CmdList, const FRHIDLSSArguments& InArguments, FDLSSStateRef InDLSSState) final;
	virtual ~FNGXD3D12RHI();
	virtual bool IsRRSupportedByRHI() const override { return true; }

#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
	virtual bool NeedExtraPassesForDebugLayerCompatibility() final;
#endif 

private:
	NVSDK_NGX_Result Init_NGX_D3D12(const FNGXRHICreateArguments& InArguments, const wchar_t* InApplicationDataPath, ID3D12Device* InHandle, const NVSDK_NGX_FeatureCommonInfo* InFeatureInfo);
	static bool IsIncompatibleAPICaptureToolActive(ID3D12Device* InDirect3DDevice);

	ID3D12DynamicRHI* D3D12RHI = nullptr;

};

bool FNGXD3D12RHI::IsIncompatibleAPICaptureToolActive(ID3D12Device* InDirect3DDevice)
{
	// RenderDoc
	{
		IID RenderDocID;
		if (SUCCEEDED(IIDFromString(L"{A7AA6116-9C8D-4BBA-9083-B4D816B71B78}", &RenderDocID)))
		{
			TRefCountPtr<IUnknown> RenderDoc;
			if (SUCCEEDED(InDirect3DDevice->QueryInterface(RenderDocID, (void**)RenderDoc.GetInitReference())))
			{
				return true;
			}
		}
	}
	return false;
}

NVSDK_NGX_Result FNGXD3D12RHI::Init_NGX_D3D12(const FNGXRHICreateArguments& InArguments, const wchar_t* InApplicationDataPath, ID3D12Device* InHandle, const NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
{
	NVSDK_NGX_Result Result = NVSDK_NGX_Result_Fail;
	int32 APIVersion = NVSDK_NGX_VERSION_API_MACRO;
	do 
	{
		if (InArguments.InitializeNGXWithNGXApplicationID())
		{
			Result = NVSDK_NGX_D3D12_Init(InArguments.NGXAppId, InApplicationDataPath, InHandle, InFeatureInfo, static_cast<NVSDK_NGX_Version>(APIVersion));
			UE_LOG(LogDLSSNGXD3D12RHI, Log, TEXT("NVSDK_NGX_D3D12_Init(AppID= %u, APIVersion = 0x%x, Device=%p) -> (%u %s)"), InArguments.NGXAppId, APIVersion, InHandle, Result, GetNGXResultAsString(Result));
		}
		else
		{
			Result = NVSDK_NGX_D3D12_Init_with_ProjectID(TCHAR_TO_UTF8(*InArguments.UnrealProjectID), NVSDK_NGX_ENGINE_TYPE_UNREAL, TCHAR_TO_UTF8(*InArguments.UnrealEngineVersion), InApplicationDataPath, InHandle, InFeatureInfo, static_cast<NVSDK_NGX_Version>(APIVersion));
			UE_LOG(LogDLSSNGXD3D12RHI, Log, TEXT("NVSDK_NGX_D3D12_Init_with_ProjectID(ProjectID = %s, EngineVersion=%s, APIVersion = 0x%x, Device=%p) -> (%u %s)"), *InArguments.UnrealProjectID, *InArguments.UnrealEngineVersion, APIVersion, InHandle,  Result, GetNGXResultAsString(Result));
		}

		if (NVSDK_NGX_FAILED(Result) && IsSafeToShutdownNGX())
		{
			NVSDK_NGX_D3D12_Shutdown1(InHandle);
		}
		
		--APIVersion;
	} while (NVSDK_NGX_FAILED(Result) && APIVersion >= NVSDK_NGX_VERSION_API_MACRO_BASE_LINE);

	if (NVSDK_NGX_SUCCEED(Result) && (APIVersion + 1 < NVSDK_NGX_VERSION_API_MACRO_WITH_LOGGING))
	{
		UE_LOG(LogDLSSNGXD3D12RHI, Log, TEXT("Warning: NVSDK_NGX_D3D12_Init succeeded, but the driver installed on this system is too old the support the NGX logging API. The console variables r.NGX.LogLevel and r.NGX.EnableOtherLoggingSinks will have no effect and NGX logs will only show up in their own log files, and not in UE's log files."));
	}

	return Result;
}



FNGXD3D12RHI::FNGXD3D12RHI(const FNGXRHICreateArguments& Arguments)
	: NGXRHI(Arguments)
	, D3D12RHI(CastDynamicRHI<ID3D12DynamicRHI>(Arguments.DynamicRHI))

{
	// TODO: adapter index
	ID3D12Device* Direct3DDevice = D3D12RHI->RHIGetDevice(0);

	ensure(D3D12RHI);
	ensure(Direct3DDevice);
	bIsIncompatibleAPICaptureToolActive = IsIncompatibleAPICaptureToolActive(Direct3DDevice);

	const FString NGXLogDir = GetNGXLogDirectory();

	NVSDK_NGX_Result ResultInit = Init_NGX_D3D12(Arguments, *NGXLogDir, Direct3DDevice, CommonFeatureInfo());
	UE_LOG(LogDLSSNGXD3D12RHI, Log, TEXT("NVSDK_NGX_D3D12_Init (Log %s) -> (%u %s)"), *NGXLogDir, ResultInit, GetNGXResultAsString(ResultInit));
	
	// store for the higher level code interpret
	NGXQueryFeature.NGXInitResult = ResultInit;

	if (NVSDK_NGX_Result_FAIL_OutOfDate == ResultInit)
	{
		NGXQueryFeature.NGXDriverRequirements.DriverUpdateRequired = true;
	}
	else if (NVSDK_NGX_SUCCEED(ResultInit))
	{
		bNGXInitialized = true;

		NVSDK_NGX_Result ResultGetParameters = NVSDK_NGX_D3D12_GetCapabilityParameters(&NGXQueryFeature.CapabilityParameters);

		UE_LOG(LogDLSSNGXD3D12RHI, Log, TEXT("NVSDK_NGX_D3D12_GetCapabilityParameters -> (%u %s)"), ResultGetParameters, GetNGXResultAsString(ResultGetParameters));

		if (NVSDK_NGX_Result_FAIL_OutOfDate == ResultGetParameters)
		{
			NGXQueryFeature.NGXDriverRequirements.DriverUpdateRequired = true;
		}

		if (NVSDK_NGX_SUCCEED(ResultGetParameters))
		{
			NGXQueryFeature.QueryDLSSSupport();
		}
	}
#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
	UE_CLOG(NeedExtraPassesForDebugLayerCompatibility(), LogDLSSNGXD3D12RHI, Warning, TEXT("Adding extra renderpasses for NGX D3D debug layer compatibility. See NGXRHI.h for alternatives"));
#endif 
}

FNGXD3D12RHI::~FNGXD3D12RHI()
{
	UE_LOG(LogDLSSNGXD3D12RHI, Log, TEXT("%s Enter"), ANSI_TO_TCHAR(__FUNCTION__));
	if (bNGXInitialized)
	{
		// Destroy the parameters and features before we call NVSDK_NGX_D3D12_Shutdown1
		ReleaseAllocatedFeatures();
		
		NVSDK_NGX_Result Result;
		if (NGXQueryFeature.CapabilityParameters != nullptr)
		{
			Result = NVSDK_NGX_D3D12_DestroyParameters(NGXQueryFeature.CapabilityParameters);
			UE_LOG(LogDLSSNGXD3D12RHI, Log, TEXT("NVSDK_NGX_D3D12_DestroyParameters -> (%u %s)"), Result, GetNGXResultAsString(Result));
		}
		if (IsSafeToShutdownNGX())
		{
			// TODO: adapter index
			ID3D12Device* Direct3DDevice = D3D12RHI->RHIGetDevice(0);
			Result = NVSDK_NGX_D3D12_Shutdown1(Direct3DDevice);
			UE_LOG(LogDLSSNGXD3D12RHI, Log, TEXT("NVSDK_NGX_D3D12_Shutdown1 -> (%u %s)"), Result, GetNGXResultAsString(Result));
		}
		bNGXInitialized = false;
	}
	UE_LOG(LogDLSSNGXD3D12RHI, Log, TEXT("%s Leave"), ANSI_TO_TCHAR(__FUNCTION__));
}
namespace
{

	ID3D12Resource* GetResidentD3D12Resource(
		ID3D12DynamicRHI* D3D12RHI,
		FRHICommandList& CmdList,
		FRHITexture* InTexture,
		bool bInIsInputTexture
		)
	{
		if (InTexture)
		{
// we are not checking for ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS since for 5.5 and older we still need to add resource state transitions
// since in 5.5 or older the RDG transitions are not reliable
// via ID3D12DynamicRHI::RHITransitionResource which also calls UpdateResidency. So we can skip that redudant call
#if UE_VERSION_AT_LEAST(5,6,0)
			D3D12RHI->RHIUpdateResourceResidency(CmdList, D3D12RHI->RHIGetResourceDeviceIndex(InTexture), InTexture);
#else
		// This is a workaround for a GPU memory fault in 5.5 when the output texture is not resident
		// DLSSArguments.OutputColor is the one that's not resident and causes the GPU mem fault in 5.5 when under memory pressure
		// 5.6 has a method to allow plugins to make textures resident .
		// for older UE versions we are using a side-effect of IFD3D12DynamicRHI::RHITransitionResource, which makes the resource resident
		// at the begin before actually going into the transition logic so we use that to make all DLSS input and output resources resident
		// Note: This also adds a pending resource state transitions to the D3D12RHI state tracker that then need to get flushed explicitely 
		//       at the calls site. That's done since only when the d3d debug layer is active since the underlying implementation is not too picky
		//       about resource states. But having d3d debug layer compatiblity is useful for developers

			D3D12RHI->RHITransitionResource(CmdList, InTexture, 
				bInIsInputTexture ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
#endif

			return D3D12RHI->RHIGetResource(InTexture);
		}
		return nullptr;
	}
}


template <typename T>
static T GetCommonEvalParams(ID3D12DynamicRHI* D3D12RHI, FRHICommandList& CmdList,  const FRHIDLSSArguments& InArguments)
{
	T EvalParams;
	FMemory::Memzero(EvalParams);

	EvalParams.InOutputSubrectBase.X = InArguments.DestRect.Min.X;
	EvalParams.InOutputSubrectBase.Y = InArguments.DestRect.Min.Y;

	EvalParams.InRenderSubrectDimensions.Width = InArguments.SrcRect.Width();
	EvalParams.InRenderSubrectDimensions.Height = InArguments.SrcRect.Height();

	EvalParams.InColorSubrectBase.X = InArguments.SrcRect.Min.X;
	EvalParams.InColorSubrectBase.Y = InArguments.SrcRect.Min.Y;

	EvalParams.pInDepth = GetResidentD3D12Resource(D3D12RHI, CmdList, InArguments.InputDepth, true);
	EvalParams.InDepthSubrectBase.X = InArguments.SrcRect.Min.X;
	EvalParams.InDepthSubrectBase.Y = InArguments.SrcRect.Min.Y;

	EvalParams.pInMotionVectors = GetResidentD3D12Resource(D3D12RHI, CmdList, InArguments.InputMotionVectors, true);
	// The VelocityCombine pass puts the motion vectors into the top left corner
	EvalParams.InMVSubrectBase.X = 0;
	EvalParams.InMVSubrectBase.Y = 0;

	EvalParams.pInBiasCurrentColorMask = InArguments.bUseBiasCurrentColorMask ? GetResidentD3D12Resource(D3D12RHI, CmdList, InArguments.InputBiasCurrentColorMask, true) : nullptr;
	EvalParams.InBiasCurrentColorSubrectBase.X = InArguments.SrcRect.Min.X;
	EvalParams.InBiasCurrentColorSubrectBase.Y = InArguments.SrcRect.Min.Y;

	EvalParams.pInExposureTexture = InArguments.bUseAutoExposure? nullptr : GetResidentD3D12Resource(D3D12RHI, CmdList, InArguments.InputExposure, true);
	EvalParams.InPreExposure = InArguments.PreExposure;

	EvalParams.InJitterOffsetX = InArguments.JitterOffset.X;
	EvalParams.InJitterOffsetY = InArguments.JitterOffset.Y;

	EvalParams.InMVScaleX = InArguments.MotionVectorScale.X;
	EvalParams.InMVScaleY = InArguments.MotionVectorScale.Y;
	EvalParams.InReset = InArguments.bReset;

	EvalParams.InFrameTimeDeltaInMsec = InArguments.DeltaTimeMS;

	return EvalParams;
}

#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
bool FNGXD3D12RHI::NeedExtraPassesForDebugLayerCompatibility()
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

void FNGXD3D12RHI::ExecuteDLSS(FRHICommandList& CmdList, const FRHIDLSSArguments& InArguments, FDLSSStateRef InDLSSState)
{
	check(!IsRunningRHIInSeparateThread() || IsInRHIThread());
	check(IsDLSSAvailable());
	if (!IsDLSSAvailable()) return;

	InArguments.Validate();

	const uint32 DeviceIndex = D3D12RHI->RHIGetResourceDeviceIndex(InArguments.InputColor);
	ID3D12GraphicsCommandList* D3DGraphicsCommandList = D3D12RHI->RHIGetGraphicsCommandList(RHICMDLIST_ARG_PASSTHROUGH DeviceIndex);

	if (InDLSSState->RequiresFeatureRecreation(InArguments))
	{
		check(!InDLSSState->DLSSFeature || InDLSSState->HasValidFeature());
		InDLSSState->DLSSFeature = nullptr;
	}

	if (InArguments.bReset)
	{
		check(!InDLSSState->DLSSFeature);
		InDLSSState->DLSSFeature = FindFreeFeature(InArguments);
	}

	if (!InDLSSState->DLSSFeature)
	{
		NVSDK_NGX_Parameter* NewNGXParameterHandle = nullptr;
		NVSDK_NGX_Result Result = NVSDK_NGX_D3D12_AllocateParameters(&NewNGXParameterHandle);
		checkf(NVSDK_NGX_SUCCEED(Result), TEXT("NVSDK_NGX_D3D12_AllocateParameters failed! (%u %s)"), Result, GetNGXResultAsString(Result));

		ApplyCommonNGXParameterSettings(NewNGXParameterHandle, InArguments);

		NVSDK_NGX_Handle* NewNGXFeatureHandle = nullptr;

		const uint32 CreationNodeMask = 1 << InArguments.GPUNode;
		const uint32 VisibilityNodeMask = InArguments.GPUVisibility;

		static_assert (int(ENGXDLSSDenoiserMode::MaxValue) == 1, "dear DLSS plugin NVIDIA developer, please update this code to handle the new ENGXDLSSDenoiserMode enum values");
		if (InArguments.DenoiserMode == ENGXDLSSDenoiserMode::DLSSRR)
		{
			// DLSS-RR feature creation
			NVSDK_NGX_DLSSD_Create_Params DlssRRCreateParams = InArguments.GetNGXDLSSRRCreateParams();
			NVSDK_NGX_Result ResultCreate = NGX_D3D12_CREATE_DLSSD_EXT(
				D3DGraphicsCommandList,
				CreationNodeMask,
				VisibilityNodeMask,
				&NewNGXFeatureHandle,
				NewNGXParameterHandle,
				&DlssRRCreateParams
			);
			if (NVSDK_NGX_SUCCEED(ResultCreate))
			{
				InDLSSState->DLSSFeature = MakeShared<FD3D12NGXDLSSFeature>(NewNGXFeatureHandle, NewNGXParameterHandle, InArguments.GetFeatureDesc(), FrameCounter);
				InDLSSState->DLSSFeature->bHasDLSSRR = true;
			}
			else
			{
				UE_LOG(LogDLSSNGXD3D12RHI, Error,
					TEXT("NGX_D3D12_CREATE_DLSSD_EXT (CreationNodeMask=0x%x VisibilityNodeMask=0x%x) failed, falling back to DLSS-SR! (%u %s), %s"),
					CreationNodeMask,
					VisibilityNodeMask,
					ResultCreate,
					GetNGXResultAsString(ResultCreate),
					*InArguments.GetFeatureDesc().GetDebugDescription());
				InDLSSState->DLSSFeature.Reset();
			}
		}
		if (!InDLSSState->DLSSFeature.IsValid())
		{
			// DLSS-SR feature creation
			NVSDK_NGX_DLSS_Create_Params DlssCreateParams = InArguments.GetNGXDLSSCreateParams();
			NVSDK_NGX_Result ResultCreate = NGX_D3D12_CREATE_DLSS_EXT(
				D3DGraphicsCommandList,
				CreationNodeMask,
				VisibilityNodeMask,
				&NewNGXFeatureHandle,
				NewNGXParameterHandle,
				&DlssCreateParams
			);
			checkf(NVSDK_NGX_SUCCEED(ResultCreate), TEXT("NGX_D3D12_CREATE_DLSS_EXT (CreationNodeMask=0x%x VisibilityNodeMask=0x%x) failed! (%u %s), %s"), CreationNodeMask, VisibilityNodeMask, ResultCreate, GetNGXResultAsString(ResultCreate), *InArguments.GetFeatureDesc().GetDebugDescription());
			InDLSSState->DLSSFeature = MakeShared<FD3D12NGXDLSSFeature>(NewNGXFeatureHandle, NewNGXParameterHandle, InArguments.GetFeatureDesc(), FrameCounter);
		}

		RegisterFeature(InDLSSState->DLSSFeature);
	}

	check(InDLSSState->HasValidFeature());

	// execute
	//TODO: replaced with what in 5.1? will something be missing from gpu profiling? see commit 305e264e
#if 0
	if (Device->GetCommandContext().IsDefaultContext())
	{
		Device->RegisterGPUWork(1);
	}
#endif

	auto FlushResourceBarriers = [this, &InArguments](FRHICommandList& CmdList)
	{
		RHI_SCOPED_DRAW_EVENT(CmdList, FlushPendingRHIBarriers);
#if ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
		const FRHIGPUMask EffectiveMask = CmdList.GetGPUMask();
		for (uint32 GPUIndex : EffectiveMask)
		{
			D3D12RHI->RHIFlushResourceBarriers(CmdList, GPUIndex);
		}
	};
#else
	// This is a workaround for older UE versions that don't have an explicit plugin API to flush pending resources
	// we are using the sideffects of RHICopyTexture to get a call to Flush pending resource barriers on the *other* resources
	// this is only needed to increase compatibility with the D3D12 debug layers
		if (NeedExtraPassesForDebugLayerCompatibility())
		{
			// we are calling/adding RHICmdList methods while we are inside an RHICmdList execution. This triggers some !IsExecuting checks in the RHICommandLinst methods
			// We avoid this by following a pattern seen in the actual RHI implementation
			TRHICommandList_RecursiveHazardous<IRHICommandContext> RHICmdList(&CmdList.GetContext());

			RHI_SCOPED_DRAW_EVENT(RHICmdList, UE5_5AndOlderBackdoorViaUnrelatedCopy);
			FRHICopyTextureInfo CopyInfo;
			CopyInfo.Size = FIntVector(1, 1, 1); // explicitly only copying 1 pixel in case the texture that we get from RDG is larger 
			RHICmdList.GetContext().RHICopyTexture(InArguments.DebugLayerCompatibilityHelperSource, InArguments.DebugLayerCompatibilityHelperDest, CopyInfo);
		}
	};

#endif // ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS

	if (!InDLSSState->DLSSFeature->bHasDLSSRR)
	{
		NVSDK_NGX_D3D12_DLSS_Eval_Params DlssEvalParams = GetCommonEvalParams<NVSDK_NGX_D3D12_DLSS_Eval_Params>(D3D12RHI, CmdList, InArguments);

		//TODO: does RHIGetResource do the right thing with multiple GPUs?
		DlssEvalParams.Feature.pInOutput = GetResidentD3D12Resource(D3D12RHI, CmdList, InArguments.OutputColor, false);
		DlssEvalParams.Feature.pInColor = GetResidentD3D12Resource(D3D12RHI, CmdList, InArguments.InputColor, true);
		DlssEvalParams.InExposureScale = InArguments.ExposureScale;

		FlushResourceBarriers(CmdList);

		NVSDK_NGX_Result ResultEvaluate = NGX_D3D12_EVALUATE_DLSS_EXT(
			D3DGraphicsCommandList,
			InDLSSState->DLSSFeature->Feature,
			InDLSSState->DLSSFeature->Parameter,
			&DlssEvalParams
		);
		checkf(NVSDK_NGX_SUCCEED(ResultEvaluate), TEXT("NGX_D3D12_EVALUATE_DLSS_EXT failed! (%u %s), %s"), ResultEvaluate, GetNGXResultAsString(ResultEvaluate), *InDLSSState->DLSSFeature->Desc.GetDebugDescription());
	}
	else
	{
		NVSDK_NGX_D3D12_DLSSD_Eval_Params DlssRREvalParams = GetCommonEvalParams<NVSDK_NGX_D3D12_DLSSD_Eval_Params>(D3D12RHI, CmdList, InArguments);

		DlssRREvalParams.pInOutput = GetResidentD3D12Resource(D3D12RHI, CmdList, InArguments.OutputColor, false);
		DlssRREvalParams.pInColor = GetResidentD3D12Resource(D3D12RHI, CmdList, InArguments.InputColor, true);

		// The GBufferResolve pass puts the albedos into the top left corner
		DlssRREvalParams.pInDiffuseAlbedo = GetResidentD3D12Resource(D3D12RHI, CmdList, InArguments.InputDiffuseAlbedo, true);
		DlssRREvalParams.InDiffuseAlbedoSubrectBase.X = 0;
		DlssRREvalParams.InDiffuseAlbedoSubrectBase.Y = 0;

		DlssRREvalParams.pInSpecularAlbedo = GetResidentD3D12Resource(D3D12RHI, CmdList, InArguments.InputSpecularAlbedo, true);
		DlssRREvalParams.InSpecularAlbedoSubrectBase.X = 0;
		DlssRREvalParams.InSpecularAlbedoSubrectBase.Y = 0;

		DlssRREvalParams.pInNormals = GetResidentD3D12Resource(D3D12RHI, CmdList, InArguments.InputNormals, true);
		DlssRREvalParams.InNormalsSubrectBase.X = 0;
		DlssRREvalParams.InNormalsSubrectBase.Y = 0;

		DlssRREvalParams.pInRoughness = GetResidentD3D12Resource(D3D12RHI, CmdList, InArguments.InputRoughness, true);
		DlssRREvalParams.InRoughnessSubrectBase.X = 0;
		DlssRREvalParams.InRoughnessSubrectBase.Y = 0;


#if SUPPORT_GUIDE_GBUFFER
		if (InArguments.InputReflectionHitDistance)
		{
			DlssRREvalParams.pInSpecularHitDistance = GetResidentD3D12Resource(D3D12RHI, CmdList, InArguments.InputReflectionHitDistance, true);
			DlssRREvalParams.InSpecularHitDistanceSubrectBase.X = 0;
			DlssRREvalParams.InSpecularHitDistanceSubrectBase.Y = 0;

			// Yes, the interface takes a non-const ptr as an argument
			DlssRREvalParams.pInWorldToViewMatrix = const_cast<float*>(InArguments.ViewMatrix);
			DlssRREvalParams.pInViewToClipMatrix = const_cast<float*>(InArguments.ProjectionMatrix);
		}
#endif

#if SUPPORT_GUIDE_SSS_DOF
		if (InArguments.InputSSS)
		{
			DlssRREvalParams.pInScreenSpaceSubsurfaceScatteringGuide = GetResidentD3D12Resource(D3D12RHI, CmdList, InArguments.InputSSS, true);
			DlssRREvalParams.InScreenSpaceSubsurfaceScatteringGuideSubrectBase.X = 0;
			DlssRREvalParams.InScreenSpaceSubsurfaceScatteringGuideSubrectBase.Y = 0;
		}

		if (InArguments.InputDOF)
		{
			DlssRREvalParams.pInDepthOfFieldGuide = GetResidentD3D12Resource(D3D12RHI, CmdList, InArguments.InputDOF, true);
			DlssRREvalParams.InDepthOfFieldGuideSubrectBase.X = 0;
			DlssRREvalParams.InDepthOfFieldGuideSubrectBase.Y = 0;
		}
#endif
		
		FlushResourceBarriers(CmdList);

		NVSDK_NGX_Result ResultEvaluate = NGX_D3D12_EVALUATE_DLSSD_EXT(
			D3DGraphicsCommandList,
			InDLSSState->DLSSFeature->Feature,
			InDLSSState->DLSSFeature->Parameter,
			&DlssRREvalParams
		);
		checkf(NVSDK_NGX_SUCCEED(ResultEvaluate), TEXT("NGX_D3D12_EVALUATE_DLSSD_EXT failed! (%u %s), %s"), ResultEvaluate, GetNGXResultAsString(ResultEvaluate), *InDLSSState->DLSSFeature->Desc.GetDebugDescription());
	}

	InDLSSState->DLSSFeature->Tick(FrameCounter);

	D3D12RHI->RHIFinishExternalComputeWork(RHICMDLIST_ARG_PASSTHROUGH DeviceIndex, D3DGraphicsCommandList);
}

/** IModuleInterface implementation */

void FNGXD3D12RHIModule::StartupModule()
{
	// NGXRHI module should be loaded to ensure logging state is initialized
	FModuleManager::LoadModuleChecked<INGXRHIModule>(TEXT("NGXRHI"));
}

void FNGXD3D12RHIModule::ShutdownModule()
{
}

TUniquePtr<NGXRHI> FNGXD3D12RHIModule::CreateNGXRHI(const FNGXRHICreateArguments& Arguments)
{
	TUniquePtr<NGXRHI> Result(new FNGXD3D12RHI(Arguments));
	return Result;
}

IMPLEMENT_MODULE(FNGXD3D12RHIModule, NGXD3D12RHI)

#undef LOCTEXT_NAMESPACE




