// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeWorldHealthBar.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SlimeHealthComponent.h"
#include "UI/MenuUIStyle.h"

TSharedRef<SWidget> USlimeWorldHealthBar::RebuildWidget()
{
	if (!Bar)
	{
		bBuiltInCode = true;
		Bar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("HealthBar"));
		WidgetTree->RootWidget = Bar;
		BarMID = FMenuUIStyle::CreateHealthBarMID(this);
		FMenuUIStyle::ApplyHealthBarImage(Bar, BarMID, FVector2D(110.f, 14.f));
		FMenuUIStyle::SetHealthBarValues(BarMID, 1.f, 1.f, 0.f, 110.f / 14.f);
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
	if (!BarMID || !Health.IsValid())
	{
		return;
	}

	const float Percent = Health->GetHealthPercent();
	const FVector2D Size = MyGeometry.GetLocalSize();
	const float Aspect = (Size.Y > 1.f) ? (Size.X / Size.Y) : (110.f / 14.f);
	FMenuUIStyle::SetHealthBarValues(BarMID, Percent, Percent, 0.f, Aspect);
}
