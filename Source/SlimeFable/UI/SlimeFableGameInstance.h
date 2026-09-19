// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "System/LyraGameInstance.h"
#include "SlimeFableGameInstance.generated.h"

class USlimeLoadingGateWidget;

UCLASS()
class SLIMEFABLE_API USlimeFableGameInstance : public ULyraGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	/** Null-OSS Listen Server. Default map is LyraShooterLab. */
	UFUNCTION(BlueprintCallable, Exec, Category = "Slime|Session")
	void SlimeHostListen(const FString& MapPackage = TEXT("/Game/Maps/Sandbox/FeatureLabs/LyraShooterLab"));

	/** Join a listen host, e.g. 127.0.0.1. */
	UFUNCTION(BlueprintCallable, Exec, Category = "Slime|Session")
	void SlimeJoin(const FString& Address);

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
