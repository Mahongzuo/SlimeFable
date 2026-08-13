// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeWorldHealthBar.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "SlimeHealthComponent.h"

TSharedRef<SWidget> USlimeWorldHealthBar::RebuildWidget()
{
	if (!Bar)
	{
		bBuiltInCode = true;
		Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
		WidgetTree->RootWidget = Bar;
		Bar->SetFillColorAndOpacity(FLinearColor(0.55f, 0.22f, 0.16f, 0.95f));
	}
	return Super::RebuildWidget();
}

void USlimeWorldHealthBar::SetHealth(USlimeHealthComponent* InHealth)
{
	Health = InHealth;
}

void USlimeWorldHealthBar::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Bar && Health.IsValid())
	{
		Bar->SetPercent(Health->GetHealthPercent());
	}
}
