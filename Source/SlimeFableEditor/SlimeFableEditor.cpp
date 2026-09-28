// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Misc/App.h"

class FSlimeFableEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (IsRunningCommandlet() || FApp::IsUnattended() || !GIsEditor)
		{
			return;
		}

		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FSlimeFableEditorModule::StartMcp),
			1.f);
	}

	virtual void ShutdownModule() override
	{
		if (TickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			TickerHandle.Reset();
		}
	}

private:
	bool StartMcp(float DeltaSeconds)
	{
		(void)DeltaSeconds;
		if (bStarted || IsRunningCommandlet() || FApp::IsUnattended() || !GIsEditor || !GEngine)
		{
			return !bStarted && GIsEditor;
		}
		bStarted = true;
		GEngine->Exec(nullptr, TEXT("ModelContextProtocol.StartServer 8010"));
		return false;
	}

	FTSTicker::FDelegateHandle TickerHandle;
	bool bStarted = false;
};

IMPLEMENT_MODULE(FSlimeFableEditorModule, SlimeFableEditor);
