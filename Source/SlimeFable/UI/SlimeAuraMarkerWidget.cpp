// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeAuraMarkerWidget.h"
#include "UI/MenuUIStyle.h"
#include "SlimeElementTypes.h"
#include "SlimeCombatTypes.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"

namespace
{
	FText ElementShortName(ESlimeElement Element)
	{
		switch (Element)
		{
		case ESlimeElement::Water: return FText::FromString(TEXT("水"));
		case ESlimeElement::Wind: return FText::FromString(TEXT("风"));
		case ESlimeElement::Fire: return FText::FromString(TEXT("火"));
		case ESlimeElement::Lightning: return FText::FromString(TEXT("雷"));
		case ESlimeElement::Dark: return FText::FromString(TEXT("暗"));
		case ESlimeElement::Physical: return FText::FromString(TEXT("物"));
		default: return FText::FromString(TEXT("?"));
		}
	}
}

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

void USlimeAuraMarkerWidget::SetAura(ESlimeElement Element, bool bVisible)
{
	if (!LabelText)
	{
		RebuildWidget();
	}
	if (!LabelText)
	{
		return;
	}
	if (!bVisible)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	LabelText->SetText(ElementShortName(Element));
	FMenuUIStyle::ApplyBrushCJKFont(LabelText, 22.f, SlimeCombat::GetElementVfxColor(Element));
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
	SetVisibility(ESlateVisibility::HitTestInvisible);
	LabelText->SetText(ReactionName);
	FMenuUIStyle::ApplyBrushCJKFont(LabelText, 22.f, Color);
}
