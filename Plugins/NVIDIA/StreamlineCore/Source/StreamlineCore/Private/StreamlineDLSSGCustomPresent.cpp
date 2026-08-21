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

#include "StreamlineDLSSGCustomPresent.h"

#include "StreamlineCorePrivate.h"
#include "StreamlineViewExtension.h"

#include "CoreMinimal.h"

void FStreamlineDLSSGCustomPresent::OnBackBufferResize()
{
	// Clear tracked views before engine destroys backbuffers because tracked views might still have references to them
	TArray<FTrackedView>& TrackedViews = FStreamlineViewExtension::GetTrackedViews();
	UE_LOG(LogStreamline, Verbose, TEXT("OnBackBufferResize clearing %d TrackedViews"), TrackedViews.Num());
	TrackedViews.Empty();
}
