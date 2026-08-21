/*
* Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
* NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
* property and proprietary rights in and to this material, related
* documentation and any modifications thereto. Any use, reproduction,
* disclosure or distribution of this material and related documentation
* without an express license agreement from NVIDIA CORPORATION or
* its affiliates is strictly prohibited.
*/
#pragma once

// engine includes
#include "CoreMinimal.h"
#include "Misc/EngineVersionComparison.h"
#include "RHIResources.h"

// local includes
#include "StreamlineCorePrivate.h"


class FStreamlineDLSSGCustomPresent final : public FRHICustomPresent
{
public:
	// Called when viewport is resized, right before backbuffers get removed.
	// The whole reason we implement this custom present is so we can clean up our own backbuffer references before the
	// engine starts destroying things on viewport resize.
	virtual void OnBackBufferResize() override final;

	// Engine should use its normal Present
	virtual bool NeedsNativePresent() override final { return true; }

	// Engine should do its normal advancement of backbuffer indices
	virtual bool NeedsAdvanceBackbuffer() override final { return true; }

	// Engine should use its normal Present
#if !UE_VERSION_OLDER_THAN(5,8,0)
	virtual bool Present(FRHIViewport*, IRHICommandContext&, int32&) override final { return true; };
	virtual bool PresentOnSubmissionThread(int32& InOutSyncInterval) override final { return true; };
#elif !UE_VERSION_OLDER_THAN(5,5,0)
	virtual bool Present(IRHICommandContext& RHICmdContext, int32& InOutSyncInterval) override final { return true; };
#else
	virtual bool Present(int32& InOutSyncInterval) override final { return true; }
#endif

};

