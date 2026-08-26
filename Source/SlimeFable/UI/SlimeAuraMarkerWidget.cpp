// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeAuraMarkerWidget.h"
#include "UI/MenuUIStyle.h"
#include "SlimeCombatTypes.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"

TSharedRef<SWidget> USlimeAuraMarkerWidget::RebuildWidget()
{
	if (!LabelText)
	{
		bBuiltInCode = true;
		LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LabelText"));
		LabelText->SetJustification(ETextJustify::Center);
		WidgetTree->RootWidget = LabelText;
	}
	return Super::RebuildWidget();
}

void USlimeAuraMarkerWidget::SetAura(ESlimeElement Element, float RemainingSec, bool bVisible)
{
	if (!LabelText)
	{
		RebuildWidget();
	}
	if (!LabelText)
	{
		return;
	}
	if (!bVisible || RemainingSec <= 0.f)
	{
		bAuraVisible = false;
		DisplayedRemaining = 0.f;
		SetRenderOpacity(1.f);
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	bAuraVisible = true;
	DisplayedRemaining = RemainingSec;
	SetVisibility(ESlateVisibility::HitTestInvisible);

	const int32 Seconds = FMath::Max(1, FMath::CeilToInt(RemainingSec));
	LabelText->SetText(FText::FromString(FString::Printf(
		TEXT("%s %ds"),
		*SlimeCombat::GetAuraStatusDisplayName(Element).ToString(),
		Seconds)));
	FMenuUIStyle::ApplyBrushCJKFont(LabelText, 22.f, SlimeCombat::GetElementVfxColor(Element));
	DisplayedElement = Element;
}

void USlimeAuraMarkerWidget::SetReactionResidue(const FText& ReactionName, const FLinearColor& Color)
{
	if (!LabelText)
	{
		RebuildWidget();
	}
	if (!LabelText)
	{
		return;
	}
	bAuraVisible = false;
	DisplayedRemaining = 0.f;
	SetRenderOpacity(1.f);
	SetVisibility(ESlateVisibility::HitTestInvisible);
	LabelText->SetText(ReactionName);
	FMenuUIStyle::ApplyBrushCJKFont(LabelText, 22.f, Color);
}

void USlimeAuraMarkerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bAuraVisible || GetVisibility() == ESlateVisibility::Collapsed)
	{
		return;
	}
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Hz = DisplayedRemaining < 2.f ? 4.5f : 2.5f;
	const float Pulse = 0.55f + 0.45f * (0.5f + 0.5f * FMath::Sin(World->GetTimeSeconds() * Hz * 2.f * PI));
	SetRenderOpacity(Pulse);
}
