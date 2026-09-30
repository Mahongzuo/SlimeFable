// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimePlotStatusWidget.generated.h"

class UProgressBar;
class USizeBox;
class UTextBlock;

UCLASS()
class SLIMEFABLE_API USlimePlotStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	void SetStatus(const FText& InText, float Progress, float BarWidth, bool bShowBar);

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<USizeBox> BarBox;

	UPROPERTY()
	TObjectPtr<UProgressBar> Bar;
};
