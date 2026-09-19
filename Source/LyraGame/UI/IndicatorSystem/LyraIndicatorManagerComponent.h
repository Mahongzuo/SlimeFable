// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ControllerComponent.h"

#include "LyraIndicatorManagerComponent.generated.h"

#define UE_API LYRAGAME_API

class AController;
class UIndicatorDescriptor;
class UObject;
struct FFrame;

/**
 * @class ULyraIndicatorManagerComponent
 */
UCLASS(MinimalAPI, BlueprintType, Blueprintable)
class ULyraIndicatorManagerComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	UE_API ULyraIndicatorManagerComponent(const FObjectInitializer& ObjectInitializer);

	static UE_API ULyraIndicatorManagerComponent* GetComponent(AController* Controller);

	UFUNCTION(BlueprintCallable, Category = Indicator)
	UE_API void AddIndicator(UIndicatorDescriptor* IndicatorDescriptor);
	
	UFUNCTION(BlueprintCallable, Category = Indicator)
	UE_API void RemoveIndicator(UIndicatorDescriptor* IndicatorDescriptor);

	DECLARE_EVENT_OneParam(ULyraIndicatorManagerComponent, FIndicatorEvent, UIndicatorDescriptor* Descriptor)
	FIndicatorEvent OnIndicatorAdded;
	FIndicatorEvent OnIndicatorRemoved;

	const TArray<UIndicatorDescriptor*>& GetIndicators() const { return Indicators; }

private:
	UPROPERTY()
	TArray<TObjectPtr<UIndicatorDescriptor>> Indicators;
};

#undef UE_API
