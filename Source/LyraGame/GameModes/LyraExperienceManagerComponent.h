// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/GameStateComponent.h"
#include "LoadingProcessInterface.h"

#include "LyraExperienceManagerComponent.generated.h"

#define UE_API LYRAGAME_API

namespace UE::GameFeatures { struct FResult; }

class ULyraExperienceDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLyraExperienceLoaded, const ULyraExperienceDefinition* /*Experience*/);

enum class ELyraExperienceLoadState
{
	Unloaded,
	Loading,
	LoadingGameFeatures,
	LoadingChaosTestingDelay,
	ExecutingActions,
	Loaded,
	Deactivating
};

UCLASS(MinimalAPI)
class ULyraExperienceManagerComponent final : public UGameStateComponent, public ILoadingProcessInterface
{
	GENERATED_BODY()

public:

	UE_API ULyraExperienceManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UActorComponent interface
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	//~ILoadingProcessInterface interface
	UE_API virtual bool ShouldShowLoadingScreen(FString& OutReason) const override;
	//~End of ILoadingProcessInterface

	// Tries to set the current experience, either a UI or gameplay one
	UE_API void SetCurrentExperience(FPrimaryAssetId ExperienceId);

	// Ensures the delegate is called once the experience has been loaded,
	// before others are called.
	// However, if the experience has already loaded, calls the delegate immediately.
	UE_API void CallOrRegister_OnExperienceLoaded_HighPriority(FOnLyraExperienceLoaded::FDelegate&& Delegate);

	// Ensures the delegate is called once the experience has been loaded
	// If the experience has already loaded, calls the delegate immediately
	UE_API void CallOrRegister_OnExperienceLoaded(FOnLyraExperienceLoaded::FDelegate&& Delegate);

	// Ensures the delegate is called once the experience has been loaded
	// If the experience has already loaded, calls the delegate immediately
	UE_API void CallOrRegister_OnExperienceLoaded_LowPriority(FOnLyraExperienceLoaded::FDelegate&& Delegate);

	// This returns the current experience if it is fully loaded, asserting otherwise
	// (i.e., if you called it too soon)
	UE_API const ULyraExperienceDefinition* GetCurrentExperienceChecked() const;

	// Returns true if the experience is fully loaded
	UE_API bool IsExperienceLoaded() const;

private:
	UFUNCTION()
	void OnRep_CurrentExperience();

	void StartExperienceLoad();
	void OnExperienceLoadComplete();
	void OnGameFeaturePluginLoadComplete(const UE::GameFeatures::FResult& Result);
	void OnExperienceFullLoadCompleted();

	void OnActionDeactivationCompleted();
	void OnAllActionsDeactivated();

private:
	UPROPERTY(ReplicatedUsing=OnRep_CurrentExperience)
	TObjectPtr<const ULyraExperienceDefinition> CurrentExperience;

	ELyraExperienceLoadState LoadState = ELyraExperienceLoadState::Unloaded;

	int32 NumGameFeaturePluginsLoading = 0;
	TArray<FString> GameFeaturePluginURLs;

	int32 NumObservedPausers = 0;
	int32 NumExpectedPausers = 0;

	/**
	 * Delegate called when the experience has finished loading just before others
	 * (e.g., subsystems that set up for regular gameplay)
	 */
	FOnLyraExperienceLoaded OnExperienceLoaded_HighPriority;

	/** Delegate called when the experience has finished loading */
	FOnLyraExperienceLoaded OnExperienceLoaded;

	/** Delegate called when the experience has finished loading */
	FOnLyraExperienceLoaded OnExperienceLoaded_LowPriority;
};

#undef UE_API
