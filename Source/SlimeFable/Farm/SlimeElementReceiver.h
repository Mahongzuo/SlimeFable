// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeElementTypes.h"
#include "SlimeElementReceiver.generated.h"

class AActor;

UINTERFACE(MinimalAPI, Blueprintable)
class USlimeElementReceiver : public UInterface
{
	GENERATED_BODY()
};

/** World props that react to slime element hits (farm plots, later toys). */
class ISlimeElementReceiver
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Slime|Element")
	void ReceiveElement(ESlimeElement Element, AActor* SourceActor, float Strength);
};

namespace SlimeElementDelivery
{
	FORCEINLINE void NotifyActor(AActor* Target, ESlimeElement Element, AActor* Instigator, float Strength)
	{
		if (!Target || Target == Instigator)
		{
			return;
		}
		if (!Target->GetClass()->ImplementsInterface(USlimeElementReceiver::StaticClass()))
		{
			return;
		}
		ISlimeElementReceiver::Execute_ReceiveElement(Target, Element, Instigator, Strength);
	}
}
