// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimePhantomWheelWidget.h"

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
#include "UI/MenuUIStyle.h"

#define LOCTEXT_NAMESPACE "SlimePhantomWheel"

namespace SlimePhantomWheelPrivate
{
	constexpr float SelectedScale = 1.18f;
	constexpr float IdleScale = 0.9f;
	constexpr float SelectedOpacity = 1.f;
	constexpr float IdleOpacity = 0.5f;
	constexpr float EmptyOpacity = 0.28f;

	FLinearColor ToUITint(const FLinearColor& Color, bool bSelected, bool bEmpty)
	{
		if (bEmpty)
		{
			return FLinearColor(0.18f, 0.16f, 0.12f, 0.45f);
		}
		const FLinearColor Muted = Color.Desaturate(bSelected ? 0.15f : 0.4f);
		return FLinearColor(Muted.R, Muted.G, Muted.B, bSelected ? 0.95f : 0.7f);
	}
}

USlimePhantomWheelWidget::USlimePhantomWheelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
}

TSharedRef<SWidget> USlimePhantomWheelWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void USlimePhantomWheelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshSectors();
}

void USlimePhantomWheelWidget::SetSlots(const TArray<FSlimeDevourCapture>& Slots, int32 SelectedIndex, int32 Capacity)
{
	CachedSlots = Slots;
	CachedCapacity = FMath::Clamp(Capacity, 1, 8);
	CachedSelected = FMath::Clamp(SelectedIndex, 0, CachedCapacity - 1);
	RefreshSectors();
}

void USlimePhantomWheelWidget::BuildLayoutIfNeeded()
{
	if (SectorRoots.Num() == CachedCapacity && CachedCapacity > 0)
	{
		return;
	}

	if (CenterLabel)
	{
		bBuiltInCode = false;
		return;
	}

	if (!WidgetTree)
	{
		return;
	}

	bBuiltInCode = true;
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PhantomWheelCanvas"));
	WidgetTree->RootWidget = Root;

	UMaterialInterface* InkMaterial = FMenuUIStyle::LoadButtonMaterial();

	SectorRoots.Reset();
	SectorImages.Reset();
	SectorNames.Reset();
	SectorTags.Reset();

	const int32 Count = FMath::Max(CachedCapacity, 6);
	CachedCapacity = Count;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FString Suffix = FString::FromInt(Index);
		USizeBox* Sector = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("PSector_%s"), *Suffix));
		Sector->SetWidthOverride(SectorSize);
		Sector->SetHeightOverride(SectorSize);

		if (UCanvasPanelSlot* SectorSlot = Root->AddChildToCanvas(Sector))
		{
			const float Angle = FMath::DegreesToRadians(Index * (360.f / Count));
			const FVector2D Offset(WheelRadius * FMath::Sin(Angle), -WheelRadius * FMath::Cos(Angle));
			SectorSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			SectorSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			SectorSlot->SetAutoSize(true);
			SectorSlot->SetPosition(Offset);
		}

		UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *FString::Printf(TEXT("PStack_%s"), *Suffix));
		Sector->AddChild(Stack);

		UImage* Disc = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("PDisc_%s"), *Suffix));
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

		UVerticalBox* Labels = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("PLabels_%s"), *Suffix));
		if (UOverlaySlot* LabelSlot = Stack->AddChildToOverlay(Labels))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Center);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("PName_%s"), *Suffix));
		FMenuUIStyle::ApplyBrushCJKFont(Name, 22.f, FMenuUIStyle::WarmTextColor());
		if (UVerticalBoxSlot* NameSlot = Labels->AddChildToVerticalBox(Name))
		{
			NameSlot->SetHorizontalAlignment(HAlign_Center);
		}

		UTextBlock* Tag = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("PTag_%s"), *Suffix));
		FMenuUIStyle::ApplyMarkerFont(Tag, 14.f, FMenuUIStyle::WarmMutedTextColor());
		if (UVerticalBoxSlot* TagSlot = Labels->AddChildToVerticalBox(Tag))
		{
			TagSlot->SetHorizontalAlignment(HAlign_Center);
		}

		SectorRoots.Add(Sector);
		SectorImages.Add(Disc);
		SectorNames.Add(Name);
		SectorTags.Add(Tag);
	}

	UVerticalBox* CenterBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PCenterBox"));
	if (UCanvasPanelSlot* CenterSlot = Root->AddChildToCanvas(CenterBox))
	{
		CenterSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CenterSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CenterSlot->SetAutoSize(true);
	}

	CenterLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PCenterLabel"));
	FMenuUIStyle::ApplyBrushCJKFont(CenterLabel, 24.f, FMenuUIStyle::WarmTitleColor());
	if (UVerticalBoxSlot* LabelSlot = CenterBox->AddChildToVerticalBox(CenterLabel))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
	}

	HintLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PHintLabel"));
	HintLabel->SetText(LOCTEXT("PhantomHint", "滚轮切换 · 松开召唤"));
	FMenuUIStyle::ApplyMixedMenuFont(HintLabel, 14.f, FMenuUIStyle::WarmMutedTextColor());
	if (UVerticalBoxSlot* HintSlot = CenterBox->AddChildToVerticalBox(HintLabel))
	{
		HintSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		HintSlot->SetHorizontalAlignment(HAlign_Center);
	}
}

void USlimePhantomWheelWidget::RefreshSectors()
{
	if (SectorRoots.Num() == 0)
	{
		BuildLayoutIfNeeded();
	}
	if (SectorRoots.Num() != CachedCapacity)
	{
		return;
	}

	for (int32 Index = 0; Index < CachedCapacity; ++Index)
	{
		const bool bFilled = CachedSlots.IsValidIndex(Index) && CachedSlots[Index].IsValidCapture();
		const bool bSelected = Index == CachedSelected;
		const FText NameText = bFilled ? CachedSlots[Index].DisplayName : LOCTEXT("EmptySlot", "空");
		const FLinearColor Tint = bFilled ? CachedSlots[Index].WheelTint : FLinearColor(0.2f, 0.18f, 0.14f);

		if (UWidget* Sector = SectorRoots[Index])
		{
			Sector->SetRenderScale(FVector2D(bSelected ? SlimePhantomWheelPrivate::SelectedScale : SlimePhantomWheelPrivate::IdleScale));
			Sector->SetRenderOpacity(bFilled
				? (bSelected ? SlimePhantomWheelPrivate::SelectedOpacity : SlimePhantomWheelPrivate::IdleOpacity)
				: SlimePhantomWheelPrivate::EmptyOpacity);
		}
		if (UImage* Disc = SectorImages[Index])
		{
			Disc->SetColorAndOpacity(SlimePhantomWheelPrivate::ToUITint(Tint, bSelected, !bFilled));
		}
		if (UTextBlock* Name = SectorNames[Index])
		{
			Name->SetText(NameText);
			Name->SetColorAndOpacity(FSlateColor(bSelected ? FMenuUIStyle::WarmTitleColor() : FMenuUIStyle::WarmMutedTextColor()));
		}
		if (UTextBlock* Tag = SectorTags[Index])
		{
			Tag->SetText(FText::AsNumber(Index + 1));
		}
	}

	if (CenterLabel)
	{
		if (CachedSlots.IsValidIndex(CachedSelected) && CachedSlots[CachedSelected].IsValidCapture())
		{
			CenterLabel->SetText(CachedSlots[CachedSelected].DisplayName);
		}
		else
		{
			CenterLabel->SetText(LOCTEXT("PickPhantom", "选择幻形"));
		}
	}
}

#undef LOCTEXT_NAMESPACE
