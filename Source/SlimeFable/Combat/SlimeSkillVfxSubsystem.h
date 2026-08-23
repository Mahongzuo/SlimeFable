// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SlimeSkillVfxSubsystem.generated.h"

struct FStreamableHandle;
class UNiagaraSystem;

UCLASS()
class SLIMEFABLE_API USlimeSkillVfxSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool IsPreloadComplete() const { return bPreloadComplete; }
	int32 GetRequestedAssetCount() const { return RequestedPaths.Num(); }
	int32 GetFailedAssetCount() const { return FailedAssetCount; }

	/** Resolves only an already loaded system. This function never starts a sync or async load. */
	static UNiagaraSystem* ResolveLoadedSystem(
		const TSoftObjectPtr<UNiagaraSystem>& SoftSystem,
		const UObject* Requester);

private:
	void CollectSkillVfxPaths();
	void HandlePreloadComplete();

	TArray<FSoftObjectPath> RequestedPaths;
	TSharedPtr<FStreamableHandle> PreloadHandle;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraSystem>> LoadedSystems;

	int32 FailedAssetCount = 0;
	bool bPreloadComplete = false;
};
