// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "LyraVerbMessageHelpers.generated.h"

#define UE_API LYRAGAME_API

struct FGameplayCueParameters;
struct FLyraVerbMessage;

class APlayerController;
class APlayerState;
class UObject;
struct FFrame;


UCLASS(MinimalAPI)
class ULyraVerbMessageHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Lyra")
	static UE_API APlayerState* GetPlayerStateFromObject(UObject* Object);

	UFUNCTION(BlueprintCallable, Category = "Lyra")
	static UE_API APlayerController* GetPlayerControllerFromObject(UObject* Object);

	UFUNCTION(BlueprintCallable, Category = "Lyra")
	static UE_API FGameplayCueParameters VerbMessageToCueParameters(const FLyraVerbMessage& Message);

	UFUNCTION(BlueprintCallable, Category = "Lyra")
	static UE_API FLyraVerbMessage CueParametersToVerbMessage(const FGameplayCueParameters& Params);
};

#undef UE_API
