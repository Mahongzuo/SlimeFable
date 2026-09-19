// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"

#include "LyraContextEffectsSubsystem.generated.h"

#define UE_API LYRAGAME_API

enum EPhysicalSurface : int;

class AActor;
class UAudioComponent;
class ULyraContextEffectsLibrary;
class UNiagaraComponent;
class USceneComponent;
struct FFrame;
struct FGameplayTag;
struct FGameplayTagContainer;

/**
 *
 */
UCLASS(MinimalAPI, config = Game, defaultconfig, meta = (DisplayName = "LyraContextEffects"))
class ULyraContextEffectsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	//
	UPROPERTY(config, EditAnywhere)
	TMap<TEnumAsByte<EPhysicalSurface>, FGameplayTag> SurfaceTypeToContextMap;
};

/**
 *
 */
UCLASS(MinimalAPI)
class ULyraContextEffectsSet : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	TSet<TObjectPtr<ULyraContextEffectsLibrary>> LyraContextEffectsLibraries;
};


/**
 * 
 */
UCLASS(MinimalAPI)
class ULyraContextEffectsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	/** */
	UFUNCTION(BlueprintCallable, Category = "ContextEffects")
	UE_API void SpawnContextEffects(
		const AActor* SpawningActor
		, USceneComponent* AttachToComponent
		, const FName AttachPoint
		, const FVector LocationOffset
		, const FRotator RotationOffset
		, FGameplayTag Effect
		, FGameplayTagContainer Contexts
		, TArray<UAudioComponent*>& AudioOut
		, TArray<UNiagaraComponent*>& NiagaraOut
		, FVector VFXScale = FVector(1)
		, float AudioVolume = 1
		, float AudioPitch = 1);

	/** */
	UFUNCTION(BlueprintCallable, Category = "ContextEffects")
	UE_API bool GetContextFromSurfaceType(TEnumAsByte<EPhysicalSurface> PhysicalSurface, FGameplayTag& Context);

	/** */
	UFUNCTION(BlueprintCallable, Category = "ContextEffects")
	UE_API void LoadAndAddContextEffectsLibraries(AActor* OwningActor, TSet<TSoftObjectPtr<ULyraContextEffectsLibrary>> ContextEffectsLibraries);

	/** */
	UFUNCTION(BlueprintCallable, Category = "ContextEffects")
	UE_API void UnloadAndRemoveContextEffectsLibraries(AActor* OwningActor);

private:

	UPROPERTY(Transient)
	TMap<TObjectPtr<AActor>, TObjectPtr<ULyraContextEffectsSet>> ActiveActorEffectsMap;

};

#undef UE_API
