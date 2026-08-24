// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeElementWheelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "SlimeElementComponent.h"
#include "Slime/SlimeElementProgressSubsystem.h"
#include "UI/MenuUIStyle.h"

#define LOCTEXT_NAMESPACE "SlimeElementWheel"

namespace SlimeWheelPrivate
{
	constexpr float SelectedScale = 1.18f;
	constexpr float IdleScale = 0.9f;
	constexpr float SelectedOpacity = 1.f;
	constexpr float IdleOpacity = 0.5f;

	/** Pulls the element colour towards the menu's earthy palette so it sits on the UI. */
	FLinearColor ToUITint(const FLinearColor& ElementColor, bool bSelected)
	{
		const FLinearColor Muted = ElementColor.Desaturate(bSelected ? 0.2f : 0.45f);
		return FLinearColor(Muted.R, Muted.G, Muted.B, bSelected ? 0.95f : 0.7f);
	}
}

USlimeElementWheelWidget::USlimeElementWheelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Never take focus: the wheel is read only chrome, and a focusable widget would swallow
	// the movement keys while it is open.
	SetIsFocusable(false);
}

TSharedRef<SWidget> USlimeElementWheelWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void USlimeElementWheelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::HitTestInvisible);

	// Applied here rather than at CreateWidget time: the tree is only guaranteed to exist now.
	RefreshSectors();
}

void USlimeElementWheelWidget::SetElementComponent(USlimeElementComponent* InElementComponent)
{
	ElementComponent = InElementComponent;
	if (ElementComponent)
	{
		HighlightedElement = ElementComponent->GetPreviewElement();
	}
	RefreshSectors();
}

void USlimeElementWheelWidget::SetHighlightedElement(ESlimeElement Element)
{
	HighlightedElement = Element;
	RefreshSectors();
}

FSlimeElementProfile USlimeElementWheelWidget::GetProfile(ESlimeElement Element) const
{
	return ElementComponent ? ElementComponent->GetProfile(Element) : USlimeElementDataAsset::MakeDefaultProfile(Element);
}

void USlimeElementWheelWidget::BuildLayoutIfNeeded()
{
	if (SectorRoots.Num() == SlimeElement::Count)
	{
		return;
	}

	// A designer authored shell wins as soon as it binds CenterLabel, matching how the menu
	// widgets decide between a WBP layout and the code built one.
	if (CenterLabel)
	{
		bBuiltInCode = false;
		return;
	}

	bBuiltInCode = true;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WheelCanvas"));
	WidgetTree->RootWidget = Root;

	UMaterialInterface* InkMaterial = FMenuUIStyle::LoadButtonMaterial();

	SectorRoots.Reset();
	SectorImages.Reset();
	SectorNames.Reset();
	SectorTags.Reset();

	for (int32 Index = 0; Index < SlimeElement::Count; ++Index)
	{
		const ESlimeElement Element = SlimeElement::FromIndex(Index);
		const FSlimeElementProfile Profile = GetProfile(Element);
		const FString Suffix = FString::FromInt(Index);

		USizeBox* Sector = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("Sector_%s"), *Suffix));
		Sector->SetWidthOverride(SectorSize);
		Sector->SetHeightOverride(SectorSize);

		if (UCanvasPanelSlot* SectorSlot = Root->AddChildToCanvas(Sector))
		{
			// Clockwise from noon, matching the enum order.
			const float Angle = FMath::DegreesToRadians(Index * (360.f / SlimeElement::Count));
			const FVector2D Offset(WheelRadius * FMath::Sin(Angle), -WheelRadius * FMath::Cos(Angle));
			SectorSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			SectorSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			SectorSlot->SetAutoSize(true);
			SectorSlot->SetPosition(Offset);
		}

		UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *FString::Printf(TEXT("SectorStack_%s"), *Suffix));
		Sector->AddChild(Stack);

		UImage* Disc = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("SectorDisc_%s"), *Suffix));
		if (InkMaterial)
		{
			FMenuUIStyle::ApplyImageMaterial(Disc, InkMaterial);
		}
		Disc->SetDesiredSizeOverride(FVector2D(SectorSize, SectorSize));
		if (UOverlaySlot* DiscSlot = Stack->AddChildToOverlay(Disc))
		{
			DiscSlot->SetHorizontalAlignment(HAlign_Fill);
			DiscSlot->SetVerticalAlignment(VAlign_Fill);
		}

		UVerticalBox* Labels = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("SectorLabels_%s"), *Suffix));
		if (UOverlaySlot* LabelSlot = Stack->AddChildToOverlay(Labels))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Center);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("SectorName_%s"), *Suffix));
		Name->SetText(Profile.DisplayName);
		FMenuUIStyle::ApplyBrushCJKFont(Name, 30.f, FMenuUIStyle::WarmTextColor());
		if (UVerticalBoxSlot* NameSlot = Labels->AddChildToVerticalBox(Name))
		{
			NameSlot->SetHorizontalAlignment(HAlign_Center);
		}

		UTextBlock* Tag = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("SectorTag_%s"), *Suffix));
		Tag->SetText(FText::FromString(Profile.Tag));
		FMenuUIStyle::ApplyMarkerFont(Tag, 12.f, FMenuUIStyle::WarmMutedTextColor());
		if (UVerticalBoxSlot* TagSlot = Labels->AddChildToVerticalBox(Tag))
		{
			TagSlot->SetHorizontalAlignment(HAlign_Center);
		}

		SectorRoots.Add(Sector);
		SectorImages.Add(Disc);
		SectorNames.Add(Name);
		SectorTags.Add(Tag);
	}

	UVerticalBox* CenterBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CenterBox"));
	if (UCanvasPanelSlot* CenterSlot = Root->AddChildToCanvas(CenterBox))
	{
		CenterSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CenterSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CenterSlot->SetAutoSize(true);
	}

	CenterLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CenterLabel"));
	FMenuUIStyle::ApplyBrushCJKFont(CenterLabel, 24.f, FMenuUIStyle::WarmTitleColor());
	if (UVerticalBoxSlot* LabelSlot = CenterBox->AddChildToVerticalBox(CenterLabel))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
	}

	HintLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HintLabel"));
	HintLabel->SetText(LOCTEXT("WheelHint", "滚轮切换 · 松开确认"));
	FMenuUIStyle::ApplyMixedMenuFont(HintLabel, 14.f, FMenuUIStyle::WarmMutedTextColor());
	if (UVerticalBoxSlot* HintSlot = CenterBox->AddChildToVerticalBox(HintLabel))
	{
		HintSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		HintSlot->SetHorizontalAlignment(HAlign_Center);
	}
}

void USlimeElementWheelWidget::RefreshSectors()
{
	if (SectorRoots.Num() != SlimeElement::Count)
	{
		return;
	}

	for (int32 Index = 0; Index < SlimeElement::Count; ++Index)
	{
		ESlimeElement Element = SlimeElement::FromIndex(Index);
		if (const USlimeElementProgressSubsystem* Progress = USlimeElementProgressSubsystem::Get(this))
		{
			Element = Progress->GetOrderedElement(Index);
		}
		const bool bSelected = Element == HighlightedElement;
		const FSlimeElementProfile Profile = GetProfile(Element);

		if (UWidget* Sector = SectorRoots[Index])
		{
			// Scale and opacity only: no coloured outline, per the menu style rules.
			Sector->SetRenderScale(FVector2D(bSelected ? SlimeWheelPrivate::SelectedScale : SlimeWheelPrivate::IdleScale));
			Sector->SetRenderOpacity(bSelected ? SlimeWheelPrivate::SelectedOpacity : SlimeWheelPrivate::IdleOpacity);
		}

		if (UImage* Disc = SectorImages[Index])
		{
			Disc->SetColorAndOpacity(SlimeWheelPrivate::ToUITint(Profile.BaseColor, bSelected));
		}

		if (UTextBlock* Name = SectorNames[Index])
		{
			Name->SetText(Profile.DisplayName);
			Name->SetColorAndOpacity(FSlateColor(bSelected ? FMenuUIStyle::WarmTitleColor() : FMenuUIStyle::WarmTextColor()));
		}

		if (UTextBlock* Tag = SectorTags[Index])
		{
			Tag->SetText(FText::FromString(Profile.Tag));
		}
	}

	if (CenterLabel)
	{
		CenterLabel->SetText(GetProfile(HighlightedElement).DisplayName);
	}
}

#undef LOCTEXT_NAMESPACE
