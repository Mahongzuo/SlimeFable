/*
* Copyright (c) 2022 - 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
* NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
* property and proprietary rights in and to this material, related
* documentation and any modifications thereto. Any use, reproduction,
* disclosure or distribution of this material and related documentation
* without an express license agreement from NVIDIA CORPORATION or
* its affiliates is strictly prohibited.
*/

#pragma once
#include "StreamlineNGXCommon.h"

// Commits needed in your engine tree if it's older than 5.6
// 2be94f9e1642ca026c07cbcc980ac922afc17b00 Add RHIUpdateResourceResidency API to ID3D12DynamicRHI
// 642d7f4108155528627f27eec161f06936d97659 Allow DX12 Resource barrier flushing trough ID3D12DynamicRHI
// then if you backport those to your engine tree, also add this to RHIDefinitions.h (and not ID3D12DynamicRHI.h) 
// #define ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS 1
// this allows to make the call sites in DLSSUpscaler.cpp avoid extra code to support this

#ifndef ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS

#if (UE_VERSION_AT_LEAST(5,6,0))
#define ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS 1
#else
#define ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS 0
#endif
#endif


#if ENGINE_ID3D12DYNAMICRHI_NEEDS_CMDLIST
#define RHICMDLIST_ARG_PASSTHROUGH CmdList,
#else
#define RHICMDLIST_ARG_PASSTHROUGH 
#endif

#if UE_VERSION_AT_LEAST(5,6,0)
#define RHI_SCOPED_DRAW_EVENT(RHICmdList, Name) SCOPED_DRAW_EVENT(RHICmdList, Name)
#elif UE_VERSION_AT_LEAST(5,5,0)
#define RHI_SCOPED_DRAW_EVENT(RHICmdList, Name)  SCOPED_DRAW_EVENT(RHICmdList.GetContext(), Name)
#else
#define RHI_SCOPED_DRAW_EVENT(RHICmdList, Name) SCOPED_RHI_DRAW_EVENT(RHICmdList.GetComputeContext(), Name)
#endif 