// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UI/LyraSimulatedInputWidget.h"

#include "LyraTouchRegion.generated.h"

#define UE_API LYRAGAME_API

class UObject;
struct FFrame;
struct FGeometry;
struct FPointerEvent;

/**
 * A "Touch Region" is used to define an area on the screen that should trigger some
 * input when the user presses a finger on it
 */
UCLASS(MinimalAPI, meta=( DisplayName="Lyra Touch Region" ))
class ULyraTouchRegion : public ULyraSimulatedInputWidget
{
	GENERATED_BODY()
	
public:
	
	//~ Begin UUserWidget
	UE_API virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	UE_API virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	UE_API virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	UE_API virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget interface

	UFUNCTION(BlueprintCallable)
	bool ShouldSimulateInput() const { return bShouldSimulateInput; }

protected:

	/** True while this widget is being touched */
	bool bShouldSimulateInput = false;
};

#undef UE_API
