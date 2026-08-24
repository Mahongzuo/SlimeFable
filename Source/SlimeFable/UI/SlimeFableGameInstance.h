// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SlimeFableGameInstance.generated.h"

class USlimeLoadingGateWidget;

UCLASS()
class SLIMEFABLE_API USlimeFableGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

protected:
	/** Suppresses engine "Preparing Shaders" chrome during travel; LoadingGate restores it. */
	void BeginLoadingScreen(const FString& MapName);
	void EndLoadingScreen(UWorld* LoadedWorld);
	void ShowLoadingGate(UWorld* LoadedWorld);

	UFUNCTION()
	void HandleLoadingGateFinished();

	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle PostLoadMapHandle;

	UPROPERTY()
	TObjectPtr<USlimeLoadingGateWidget> ActiveLoadingGate;

	bool bPrevScreenMessagesEnabled = true;
};
