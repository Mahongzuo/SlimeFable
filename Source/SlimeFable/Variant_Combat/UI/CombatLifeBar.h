// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatLifeBar.generated.h"

/**
 *  A basic life bar user widget.
 */
UCLASS(abstract)
class UCombatLifeBar : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Sets the life bar to the provided 0-1 percentage value*/
	UFUNCTION(BlueprintImplementableEvent, Category="Life Bar")
	void SetLifePercentage(float Percent);

	// Sets the life bar fill color
	UFUNCTION(BlueprintImplementableEvent, Category="Life Bar")
	void SetBarColor(FLinearColor Color);
};
