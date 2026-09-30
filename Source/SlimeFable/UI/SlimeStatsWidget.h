// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeStatsWidget.generated.h"

class UTextBlock;

UCLASS()
class SLIMEFABLE_API USlimeStatsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USlimeStatsWidget(const FObjectInitializer& ObjectInitializer);
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Refresh();

	UPROPERTY()
	TObjectPtr<UTextBlock> Body;
};
