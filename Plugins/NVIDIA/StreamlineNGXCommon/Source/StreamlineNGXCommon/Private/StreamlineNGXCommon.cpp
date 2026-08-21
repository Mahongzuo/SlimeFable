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
#include "StreamlineNGXCommon.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"

#define LOCTEXT_NAMESPACE "FStreamlineNGXCommonModule"
DEFINE_LOG_CATEGORY_STATIC(LogStreamlineNGXCommon, Log, All);

class FStreamlineNGXCommonModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override 
	{
		UE_LOG(LogStreamlineNGXCommon, Verbose, TEXT("%s Enter"), ANSI_TO_TCHAR(__FUNCTION__));
		UE_LOG(LogStreamlineNGXCommon, Verbose, TEXT("FApp::CanEverRender                    =%d"), FApp::CanEverRender());
		UE_LOG(LogStreamlineNGXCommon, Verbose, TEXT("FApp::CanEverRenderOrProduceRenderData =%d"), FApp::CanEverRenderOrProduceRenderData());
		UE_LOG(LogStreamlineNGXCommon, Verbose, TEXT("IsRunningCommandlet        =%d"), IsRunningCommandlet());
		UE_LOG(LogStreamlineNGXCommon, Verbose, TEXT("IsRunningCookCommandlet    =%d"), IsRunningCookCommandlet());
		UE_LOG(LogStreamlineNGXCommon, Verbose, TEXT("IsRunningDLCCookCommandlet =%d"), IsRunningDLCCookCommandlet());
		UE_LOG(LogStreamlineNGXCommon, Verbose, TEXT("IsRunningCookOnTheFly      =%d"), IsRunningCookOnTheFly());
		UE_LOG(LogStreamlineNGXCommon, Verbose, TEXT("IsAllowCommandletRendering =%d"), IsAllowCommandletRendering());

#if UE_VERSION_AT_LEAST(5,6,0)
		UE_LOG(LogStreamlineNGXCommon, Verbose, TEXT("GetRunningCommandletClass = '%s' GetCommandletNameFromCmdline() = '%s'"), 
			GetRunningCommandletClass() ? *GetRunningCommandletClass()->GetName() : TEXT("nullptr"), *GetCommandletNameFromCmdline());
#endif
		UE_LOG(LogStreamlineNGXCommon, Verbose, TEXT("%s Leave"), ANSI_TO_TCHAR(__FUNCTION__));
	};
	virtual void ShutdownModule() override 
	{
	};

};

IMPLEMENT_MODULE(FStreamlineNGXCommonModule, StreamlineNGXCommon);