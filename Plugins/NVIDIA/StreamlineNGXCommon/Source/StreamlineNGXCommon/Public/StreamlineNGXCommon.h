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
#include "Misc/EngineVersionComparison.h"
#include "CoreGlobals.h"
#include "Misc/App.h"

#define UE_VERSION_AT_LEAST(MajorVersion, MinorVersion, PatchVersion) (!UE_VERSION_OLDER_THAN(MajorVersion, MinorVersion, PatchVersion))

inline TPair<bool, const TCHAR*> IsEngineExecutionModeSupported()
{

	if (!FApp::CanEverRender())
	{
		return MakeTuple(false, TEXT("Cannot ever render"));
	}

	/*
		With IsAllowCommandletRendering() = 1, we make it here. This can happen e.g. with WorldPartitionBuilderCommandlet that sets -AllowCommandletRendering 
		However in that case Slate is not initialized. The Streamline plugin (particularly DLSS-FG, but also Reflex/PCL needs some slate callbacks for correct functionality.
		Rather than trying to get a partial subset of the Streamline plugin to work (e.g. for just DeepDVC) we just treat it as an unsupported execution mode.

		The NGX/DLSS-SR/RR plugin does not have those FSlate dependencies, but initializing NGX and then not using it seems wasteful.

		Note: 'Remote' Movie Render Scene are rendered as a regular -game instances and are not impacted by this.

	*/
	if (IsRunningCommandlet())
	{
		return MakeTuple(false,TEXT("A command let is running"));
	}


	return MakeTuple(true, TEXT(""));
}
