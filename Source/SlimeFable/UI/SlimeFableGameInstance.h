// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"
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
	void EnsureLoadingBackgroundBrush();
	void EnsureLoadingStatusFont();
	void BeginLoadingScreen(const FString& MapName);
	void EndLoadingScreen(UWorld* LoadedWorld);
	void ShowLoadingGate(UWorld* LoadedWorld);

	UFUNCTION()
	void HandleLoadingGateFinished();

	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle PostLoadMapHandle;

	/** Built once on the game thread; reused so PreLoadMap never touches RHI/UObjects. */
	TSharedPtr<FSlateBrush> CachedLoadingBackground;

	/** Path-based composite font for MoviePlayer (no UFont*). */
	FSlateFontInfo CachedLoadingStatusFont;
	bool bHasCachedLoadingStatusFont = false;

	UPROPERTY()
	TObjectPtr<USlimeLoadingGateWidget> ActiveLoadingGate;

	bool bPrevScreenMessagesEnabled = true;
};
