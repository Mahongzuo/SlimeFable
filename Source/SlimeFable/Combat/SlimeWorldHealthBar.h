// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeWorldHealthBar.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class USlimeHealthComponent;
class UVerticalBox;

UCLASS()
class SLIMEFABLE_API USlimeWorldHealthBar : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void SetHealth(USlimeHealthComponent* InHealth);
	void SetDualPhaseEnabled(bool bEnabled);
	void SetPhasePercents(float Phase1Percent, float Phase2Percent);

protected:
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> BarsRoot;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Bar;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Bar2;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BarMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Bar2MID;

	UPROPERTY(Transient)
	TWeakObjectPtr<USlimeHealthComponent> Health;

	bool bBuiltInCode = false;
	bool bDualPhase = false;
	float Phase1Percent = 1.f;
	float Phase2Percent = 1.f;
};
