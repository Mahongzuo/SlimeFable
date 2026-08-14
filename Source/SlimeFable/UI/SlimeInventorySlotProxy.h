// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SlimeInventorySlotProxy.generated.h"

class USlimeInventoryWidget;

UCLASS()
class SLIMEFABLE_API USlimeInventorySlotProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FName ItemId = NAME_None;

	UPROPERTY()
	TObjectPtr<USlimeInventoryWidget> Owner;

	UFUNCTION()
	void HandleClicked();
};
