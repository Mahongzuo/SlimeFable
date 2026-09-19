// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/CommonBoundActionButton.h"

#include "LyraBoundActionButton.generated.h"

#define UE_API LYRAGAME_API

class UCommonButtonStyle;
class UObject;

/**
 * 
 */
UCLASS(MinimalAPI, Abstract, meta = (DisableNativeTick))
class ULyraBoundActionButton : public UCommonBoundActionButton
{
	GENERATED_BODY()
	
protected:
	UE_API virtual void NativeConstruct() override;

private:
	void HandleInputMethodChanged(ECommonInputType NewInputMethod);

	UPROPERTY(EditAnywhere, Category = "Styles")
	TSubclassOf<UCommonButtonStyle> KeyboardStyle;

	UPROPERTY(EditAnywhere, Category = "Styles")
	TSubclassOf<UCommonButtonStyle> GamepadStyle;

	UPROPERTY(EditAnywhere, Category = "Styles")
	TSubclassOf<UCommonButtonStyle> TouchStyle;
};

#undef UE_API
