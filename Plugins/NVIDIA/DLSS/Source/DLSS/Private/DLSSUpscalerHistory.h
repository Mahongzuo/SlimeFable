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

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Misc/EngineVersionComparison.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "TemporalUpscaler.h"
#else
#include "SceneRendering.h"
#endif

#include "DLSSUpscalerPrivate.h"
#include "NGXRHI.h"
#include "StreamlineNGXCommon.h"

#define UE_API DLSS_API
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8
class FDLSSUpscalerHistory final : public ITemporalUpscaler::IHistory, public FRefCountedObject
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
class FDLSSUpscalerHistory final : public ITemporalUpscaler::IHistory, public FRefCountBase
#else
class FDLSSUpscalerHistory final : public ICustomTemporalAAHistory, public FRefCountBase
#endif
{
public:
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	virtual const TCHAR* GetDebugName() const final override;
	virtual uint64 GetGPUSizeBytes() const final override;
#endif

private:
	friend class FDLSSSceneViewFamilyUpscaler;

	FDLSSStateRef DLSSState;
	// in 5.3+ the debug name must match the upscaler's debug name, and since the name includes whether we're running DLSS-RR the history needs to know the denoiser mode
	ENGXDLSSDenoiserMode DenoiserMode;

#if UE_VERSION_AT_LEAST(5,8,0)
	virtual void AddRef() const final override
	{
		FRefCountedObject::AddRef();
	}

	virtual FReturnedRefCountValue Release() const final override
	{
		return FRefCountedObject::Release();
	}

	virtual FReturnedRefCountValue GetRefCount() const final override
	{
		return FRefCountedObject::GetRefCount();
	}
#else 

#if UE_VERSION_OLDER_THAN(5,6,0)
	virtual uint32 AddRef() const final
#else
	virtual FReturnedRefCountValue AddRef() const final
#endif

	{
		return FRefCountBase::AddRef();
	}

	virtual uint32 Release() const final
	{
		return FRefCountBase::Release();
	}

	virtual uint32 GetRefCount() const final
	{
		return FRefCountBase::GetRefCount();
	}
#endif

	FDLSSUpscalerHistory(FDLSSStateRef InDLSSState, ENGXDLSSDenoiserMode InDenoiserMode);
	~FDLSSUpscalerHistory();

};

#undef UE_API
