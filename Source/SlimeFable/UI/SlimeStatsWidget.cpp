// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeStatsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "SlimeCombatComponent.h"
#include "SlimeElementComponent.h"
#include "SlimeHealthComponent.h"
#include "UI/MenuUIStyle.h"

USlimeStatsWidget::USlimeStatsWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

TSharedRef<SWidget> USlimeStatsWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("StatsRoot"));
		WidgetTree->RootWidget = Root;

		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatsPanel"));
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor(0.05f, 0.045f, 0.035f, 0.88f));
		Brush.OutlineSettings.CornerRadii = FVector4(14.f, 14.f, 14.f, 14.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Color = FSlateColor(FLinearColor(0.72f, 0.64f, 0.46f, 0.45f));
		Brush.OutlineSettings.Width = 1.4f;
		Panel->SetBrush(Brush);
		Panel->SetPadding(FMargin(18.f, 14.f));
		if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
		{
			PanelSlot->SetAnchors(FAnchors(0.f, 0.5f));
			PanelSlot->SetAlignment(FVector2D(0.f, 0.5f));
			PanelSlot->SetPosition(FVector2D(28.f, 0.f));
			PanelSlot->SetAutoSize(true);
		}

		Body = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatsBody"));
		Body->SetText(FText::FromString(TEXT("属性")));
		FMenuUIStyle::ApplyBrushCJKFont(Body, 18.f, FMenuUIStyle::WarmTextColor());
		Panel->AddChild(Body);
	}
	return Super::RebuildWidget();
}

void USlimeStatsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Refresh();
}

void USlimeStatsWidget::Refresh()
{
	if (!Body)
	{
		return;
	}
	APawn* Pawn = GetOwningPlayerPawn();
	const USlimeHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<USlimeHealthComponent>() : nullptr;
	const USlimeCombatComponent* Combat = Pawn ? Pawn->FindComponentByClass<USlimeCombatComponent>() : nullptr;
	const USlimeElementComponent* Element = Pawn ? Pawn->FindComponentByClass<USlimeElementComponent>() : nullptr;

	const int32 Hp = Health ? FMath::RoundToInt(Health->CurrentHP) : 0;
	const int32 MaxHp = Health ? FMath::RoundToInt(Health->MaxHP) : 0;
	const int32 Attack = Combat ? FMath::RoundToInt(Combat->GetAttackPower()) : 0;
	const TCHAR* ElementName = TEXT("水");
	if (Element)
	{
		switch (Element->CurrentElement)
		{
		case ESlimeElement::Fire: ElementName = TEXT("火"); break;
		case ESlimeElement::Wind: ElementName = TEXT("风"); break;
		case ESlimeElement::Lightning: ElementName = TEXT("雷"); break;
		case ESlimeElement::Dark: ElementName = TEXT("暗"); break;
		case ESlimeElement::Physical: ElementName = TEXT("物"); break;
		default: ElementName = TEXT("水"); break;
		}
	}

	FString Text = FString::Printf(
		TEXT("属性\n生命    %d / %d\n攻击力    %d\n当前属性    %s"),
		Hp, MaxHp, Attack, ElementName);
	if (Combat && Combat->GetDamageBuffRemaining() > 0.f && !FMath::IsNearlyEqual(Combat->GetOutgoingDamageMul(), 1.f))
	{
		Text += FString::Printf(
			TEXT("\n攻击加成    ×%.2f    还剩 %d 秒"),
			Combat->GetOutgoingDamageMul(),
			FMath::CeilToInt(Combat->GetDamageBuffRemaining()));
	}
	const FText Next = FText::FromString(Text);
	if (!Body->GetText().EqualTo(Next))
	{
		Body->SetText(Next);
	}
}
