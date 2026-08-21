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

#include "StreamlineDXGIRHI.h"
#include "StreamlineDXGISwapchainProxy.h"
#include "Modules/ModuleManager.h"

void FStreamlineDXGIRHIModule::StartupModule()
{
	UE_LOG(LogStreamlineDXGIRHI, Log, TEXT("StreamlineDXGIRHI module started"));
}

void FStreamlineDXGIRHIModule::ShutdownModule()
{
	UE_LOG(LogStreamlineDXGIRHI, Log, TEXT("StreamlineDXGIRHI module shutdown"));
}

IMPLEMENT_MODULE(FStreamlineDXGIRHIModule, StreamlineDXGIRHI)
