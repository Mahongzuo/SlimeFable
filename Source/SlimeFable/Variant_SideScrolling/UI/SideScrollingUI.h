// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SideScrollingUI.generated.h"

/**
 *  Simple Side Scrolling game UI
 *  Displays and manages a pickup counter
 */
UCLASS(abstract)
class USideScrollingUI : public UUserWidget
{
	GENERATED_BODY()
	
public:

	/** Update the widget's pickup counter */
	UFUNCTION(BlueprintImplementableEvent, Category="UI")
	void UpdatePickups(int32 Amount);
};
