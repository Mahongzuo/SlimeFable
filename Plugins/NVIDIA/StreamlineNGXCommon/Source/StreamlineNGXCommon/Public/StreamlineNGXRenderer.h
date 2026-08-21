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

#if UE_VERSION_AT_LEAST(5,5,0)
#define NV_RDG_EVENT_SCOPE(GraphBuilder, StatName, Format, ...) RDG_EVENT_SCOPE_STAT(GraphBuilder, StatName,Format, ##__VA_ARGS__);
#else 
#define NV_RDG_EVENT_SCOPE(GraphBuilder, StatName, Format, ...) RDG_EVENT_SCOPE(GraphBuilder, Format, ##__VA_ARGS__); 
#endif