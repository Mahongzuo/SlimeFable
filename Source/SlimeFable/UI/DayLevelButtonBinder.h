// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DayLevelButtonBinder.generated.h"

class ULevelSelectWidget;

/** Bridges UButton::OnClicked (dynamic) to a DayId callback. */
UCLASS()
class SLIMEFABLE_API UDayLevelButtonBinder : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FName DayId;

	UPROPERTY()
	TObjectPtr<ULevelSelectWidget> Owner;

	UFUNCTION()
	void HandleClicked();
};
