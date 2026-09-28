// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeHubStatusWidget.generated.h"

class UBorder;
class UTextBlock;

/** Corner badge: museum clock and the next weather roll. */
UCLASS()
class SLIMEFABLE_API USlimeHubStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	void SetStatusText(const FText& InText);

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;
};
