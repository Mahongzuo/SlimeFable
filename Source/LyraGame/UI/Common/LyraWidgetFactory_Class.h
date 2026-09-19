// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LyraWidgetFactory.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"

#include "LyraWidgetFactory_Class.generated.h"

#define UE_API LYRAGAME_API

class UObject;
class UUserWidget;

UCLASS(MinimalAPI)
class ULyraWidgetFactory_Class : public ULyraWidgetFactory
{
	GENERATED_BODY()

public:
	ULyraWidgetFactory_Class() { }

	UE_API virtual TSubclassOf<UUserWidget> FindWidgetClassForData_Implementation(const UObject* Data) const override;
	
protected:
	UPROPERTY(EditAnywhere, Category = ListEntries, meta = (AllowAbstract))
	TMap<TSoftClassPtr<UObject>, TSubclassOf<UUserWidget>> EntryWidgetForClass;
};

#undef UE_API
