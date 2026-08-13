// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeWorldHealthBar.generated.h"

class UProgressBar;
class USlimeHealthComponent;

UCLASS()
class SLIMEFABLE_API USlimeWorldHealthBar : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void SetHealth(USlimeHealthComponent* InHealth);

protected:
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(Transient)
	TWeakObjectPtr<USlimeHealthComponent> Health;

	bool bBuiltInCode = false;
};
