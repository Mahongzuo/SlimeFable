// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeCombatHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Materials/MaterialInterface.h"
#include "SlimeCombatComponent.h"
#include "Styling/SlateTypes.h"
#include "UI/MenuUIStyle.h"

namespace
{
	UMaterialInterface* LoadProgressBarMaterial()
	{
		return LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Game/UIMaterialLab/Widgets/ComponentMaterials/MaterialInstances/MI_UI_ProgressBar_1.MI_UI_ProgressBar_1"));
	}
}

USlimeCombatHUDWidget::USlimeCombatHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

TSharedRef<SWidget> USlimeCombatHUDWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void USlimeCombatHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	Refresh();
}

void USlimeCombatHUDWidget::SetCombat(USlimeCombatComponent* InCombat)
{
	Combat = InCombat;
	Refresh();
}

void USlimeCombatHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Refresh();
}

void USlimeCombatHUDWidget::BuildLayoutIfNeeded()
{
	if (SlotKeys.Num() == 3 && UltimateBar)
	{
		return;
	}

	bBuiltInCode = true;
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CombatHudRoot"));
	WidgetTree->RootWidget = Root;

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HudStack"));
	if (UCanvasPanelSlot* StackSlot = Root->AddChildToCanvas(Stack))
	{
		// Right-middle, resolution-safe inset from the right edge.
		StackSlot->SetAnchors(FAnchors(1.f, 0.5f, 1.f, 0.5f));
		StackSlot->SetAlignment(FVector2D(1.f, 0.5f));
		StackSlot->SetAutoSize(true);
		StackSlot->SetPosition(FVector2D(-32.f, 40.f));
	}

	ComboText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ComboText"));
	if (UVerticalBoxSlot* ComboSlot = Stack->AddChildToVerticalBox(ComboText))
	{
		ComboSlot->SetHorizontalAlignment(HAlign_Right);
		ComboSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 10.f));
	}

	SlotKeys.Reset();
	SlotNames.Reset();
	SlotCds.Reset();
	SlotBackgrounds.Reset();

	UMaterialInterface* ButtonMat = FMenuUIStyle::LoadButtonMaterial();
	UMaterialInterface* ProgressMat = LoadProgressBarMaterial();
	const TCHAR* Keys[] = { TEXT("1"), TEXT("2"), TEXT("3") };

	for (int32 Index = 0; Index < 3; ++Index)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("Box%d"), Index));
		Box->SetWidthOverride(128.f);
		Box->SetHeightOverride(72.f);
		if (UVerticalBoxSlot* BoxSlot = Stack->AddChildToVerticalBox(Box))
		{
			BoxSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
			BoxSlot->SetHorizontalAlignment(HAlign_Right);
		}

		UOverlay* Cell = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *FString::Printf(TEXT("Slot%d"), Index));
		Box->AddChild(Cell);

		UImage* Bg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("Bg%d"), Index));
		if (ButtonMat)
		{
			FMenuUIStyle::ApplyImageMaterial(Bg, ButtonMat);
		}
		else
		{
			Bg->SetColorAndOpacity(FLinearColor(0.18f, 0.14f, 0.09f, 0.92f));
		}
		if (UOverlaySlot* BgSlot = Cell->AddChildToOverlay(Bg))
		{
			BgSlot->SetHorizontalAlignment(HAlign_Fill);
			BgSlot->SetVerticalAlignment(VAlign_Fill);
		}
		SlotBackgrounds.Add(Bg);

		UProgressBar* Cd = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), *FString::Printf(TEXT("Cd%d"), Index));
		Cd->SetPercent(1.f);
		Cd->SetFillColorAndOpacity(FLinearColor(0.72f, 0.58f, 0.32f, 0.85f));
		if (ProgressMat)
		{
			FProgressBarStyle Style = Cd->GetWidgetStyle();
			FSlateBrush Fill = FMenuUIStyle::MakeMaterialBrush(ProgressMat, FVector2D(128.f, 72.f));
			Fill.TintColor = FSlateColor(FLinearColor(0.92f, 0.78f, 0.48f, 0.9f));
			Style.SetFillImage(Fill);
			FSlateBrush Empty;
			Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
			Style.SetBackgroundImage(Empty);
			Cd->SetWidgetStyle(Style);
		}
		if (UOverlaySlot* CdSlot = Cell->AddChildToOverlay(Cd))
		{
			CdSlot->SetHorizontalAlignment(HAlign_Fill);
			CdSlot->SetVerticalAlignment(VAlign_Bottom);
			CdSlot->SetPadding(FMargin(10.f, 0.f, 10.f, 6.f));
		}

		UVerticalBox* Labels = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("Labels%d"), Index));
		if (UOverlaySlot* LabelSlot = Cell->AddChildToOverlay(Labels))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Center);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
			LabelSlot->SetPadding(FMargin(12.f, 8.f));
		}

		UTextBlock* Key = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("Key%d"), Index));
		Key->SetText(FText::FromString(Keys[Index]));
		Labels->AddChildToVerticalBox(Key);

		UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("Name%d"), Index));
		Labels->AddChildToVerticalBox(Name);

		SlotKeys.Add(Key);
		SlotNames.Add(Name);
		SlotCds.Add(Cd);
	}

	// Keep bars for early-out guard; hide permanently (resources unused).
	ResonanceBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ResonanceBar"));
	ResonanceBar->SetVisibility(ESlateVisibility::Collapsed);
	Stack->AddChildToVerticalBox(ResonanceBar);

	UltimateBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("UltimateBar"));
	UltimateBar->SetVisibility(ESlateVisibility::Collapsed);
	Stack->AddChildToVerticalBox(UltimateBar);
}

void USlimeCombatHUDWidget::Refresh()
{
	if (!Combat)
	{
		return;
	}

	const FSlimeElementKitData Kit = Combat->GetCurrentKit();
	const ESlimeSkillSlot Slots[3] = { ESlimeSkillSlot::Skill1, ESlimeSkillSlot::Skill2, ESlimeSkillSlot::Skill3 };
	const FSlimeSkillDef* Defs[3] = { &Kit.Skill1, &Kit.Skill2, &Kit.Skill3 };

	for (int32 Index = 0; Index < 3 && Index < SlotNames.Num(); ++Index)
	{
		if (SlotNames[Index])
		{
			FMenuUIStyle::ApplyBrushCJKFont(SlotNames[Index], 15.f, FMenuUIStyle::WarmTextColor());
			SlotNames[Index]->SetText(Defs[Index]->DisplayName);
		}
		if (SlotKeys[Index])
		{
			FMenuUIStyle::ApplyMarkerFont(SlotKeys[Index], 22.f, FMenuUIStyle::WarmTitleColor());
		}
		if (SlotCds[Index])
		{
			const float Remaining = Combat->GetSkillCooldownRemaining(Slots[Index]);
			const float MaxCd = FMath::Max(Defs[Index]->Cooldown, 0.01f);
			SlotCds[Index]->SetPercent(Remaining <= 0.f ? 1.f : 1.f - Remaining / MaxCd);
		}
	}

	if (ComboText)
	{
		FMenuUIStyle::ApplyMarkerFont(ComboText, 18.f, FMenuUIStyle::WarmTitleColor());
		const int32 Combo = Combat->GetComboIndex();
		ComboText->SetText(Combo > 0 ? FText::FromString(FString::Printf(TEXT("%d / 4"), Combo)) : FText::GetEmpty());
	}
}
