// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimePlotStatusWidget.generated.h"

class UTextBlock;

UCLASS()
class SLIMEFABLE_API USlimePlotStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	void SetStatusText(const FText& InText);

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;
};
