// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeWorldHealthBar.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SlimeHealthComponent.h"
#include "UI/MenuUIStyle.h"

TSharedRef<SWidget> USlimeWorldHealthBar::RebuildWidget()
{
	if (!Bar)
	{
		bBuiltInCode = true;
		BarsRoot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BarsRoot"));
		WidgetTree->RootWidget = BarsRoot;

		Bar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("HealthBar"));
		BarMID = FMenuUIStyle::CreateHealthBarMID(this);
		FMenuUIStyle::ApplyHealthBarImage(Bar, BarMID, FVector2D(72.f, 8.f));
		FMenuUIStyle::SetHealthBarValues(BarMID, 1.f, 1.f, 0.f, 72.f / 8.f);
		if (UVerticalBoxSlot* BarSlot = BarsRoot->AddChildToVerticalBox(Bar))
		{
			BarSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
		}

		Bar2 = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("HealthBar2"));
		Bar2MID = FMenuUIStyle::CreateHealthBarMID(this);
		FMenuUIStyle::ApplyHealthBarImage(Bar2, Bar2MID, FVector2D(72.f, 8.f));
		FMenuUIStyle::SetHealthBarValues(Bar2MID, 1.f, 1.f, 0.f, 72.f / 8.f);
		BarsRoot->AddChildToVerticalBox(Bar2);
		Bar2->SetVisibility(ESlateVisibility::Collapsed);
	}
	return Super::RebuildWidget();
}

void USlimeWorldHealthBar::SetHealth(USlimeHealthComponent* InHealth)
{
	Health = InHealth;
}

void USlimeWorldHealthBar::SetDualPhaseEnabled(bool bEnabled)
{
	bDualPhase = bEnabled;
	if (Bar2)
	{
		Bar2->SetVisibility(bEnabled ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void USlimeWorldHealthBar::SetPhasePercents(float InPhase1Percent, float InPhase2Percent)
{
	Phase1Percent = FMath::Clamp(InPhase1Percent, 0.f, 1.f);
	Phase2Percent = FMath::Clamp(InPhase2Percent, 0.f, 1.f);
}

void USlimeWorldHealthBar::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!BarMID)
	{
		return;
	}

	const FVector2D Size = MyGeometry.GetLocalSize();
	const float SingleH = bDualPhase ? FMath::Max(Size.Y * 0.5f, 1.f) : FMath::Max(Size.Y, 1.f);
	const float Aspect = (SingleH > 1.f) ? (Size.X / SingleH) : (72.f / 8.f);

	if (bDualPhase)
	{
		FMenuUIStyle::SetHealthBarValues(BarMID, Phase1Percent, Phase1Percent, 0.f, Aspect);
		if (Bar2MID)
		{
			FMenuUIStyle::SetHealthBarValues(Bar2MID, Phase2Percent, Phase2Percent, 0.f, Aspect);
		}
		return;
	}

	if (!Health.IsValid())
	{
		return;
	}
	const float Percent = Health->GetHealthPercent();
	FMenuUIStyle::SetHealthBarValues(BarMID, Percent, Percent, 0.f, Aspect);
}
