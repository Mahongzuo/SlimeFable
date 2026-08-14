// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SlimeSliceable.generated.h"

class UProceduralMeshComponent;

UINTERFACE(MinimalAPI, Blueprintable)
class USlimeSliceable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Actors that can be cut by slime combat hits (procedural mesh fruit, props, etc.).
 * Attack probes call SliceAt with the impact plane and the specific mesh component hit.
 */
class ISlimeSliceable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Slice")
	void SliceAt(FVector PlanePosition, FVector PlaneNormal, UProceduralMeshComponent* MeshToSlice);
};
