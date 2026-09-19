// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "LyraWidgetFactory.generated.h"

#define UE_API LYRAGAME_API

template <class TClass> class TSubclassOf;

class UUserWidget;
struct FFrame;

UCLASS(MinimalAPI, Abstract, Blueprintable, BlueprintType, EditInlineNew)
class ULyraWidgetFactory : public UObject
{
	GENERATED_BODY()

public:
	ULyraWidgetFactory() { }

	UFUNCTION(BlueprintNativeEvent)
	UE_API TSubclassOf<UUserWidget> FindWidgetClassForData(const UObject* Data) const;
};

#undef UE_API
