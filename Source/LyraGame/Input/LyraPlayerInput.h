// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnhancedPlayerInput.h"

#include "LyraPlayerInput.generated.h"

/**
 * Custom player input class for Lyra. This extends the functionality of Enhanced Input to also include
 * some input latency tracking on key press.
 *
 * Extend this class if you have any special logic which you may want to run relating to when keys are pressed
 * or when input is flushed.
 */
UCLASS(config = Input, transient)
class ULyraPlayerInput : public UEnhancedPlayerInput
{
	GENERATED_BODY()

public:
	ULyraPlayerInput();
	virtual ~ULyraPlayerInput() override;

protected:
	//~ Begin UEnhancedPlayerInput Interface
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	//~ End of UEnhancedPlayerInput interface

	void ProcessInputEventForLatencyMarker(const FInputKeyEventArgs& Params);
	void BindToLatencyMarkerSettingChange();
	void UnbindLatencyMarkerSettingChangeListener();
	void HandleLatencyMarkerSettingChanged();

	bool bShouldTriggerLatencyFlash = false;
};