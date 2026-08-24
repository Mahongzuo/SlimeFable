// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeHotbarWheelWidget.h"

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
#include "Engine/GameInstance.h"
#include "Inventory/SlimeInventorySubsystem.h"
#include "UI/MenuUIStyle.h"

#define LOCTEXT_NAMESPACE "SlimeHotbarWheel"

namespace SlimeHotbarWheelPrivate
{
	constexpr float SelectedScale = 1.18f;
	constexpr float IdleScale = 0.9f;
	constexpr float SelectedOpacity = 1.f;
	constexpr float IdleOpacity = 0.5f;
	constexpr int32 SlotCount = 6;

	FString SlotTitle(int32 Index)
	{
		if (Index < 3)
		{
			return FString::Printf(TEXT("Use %d"), Index + 1);
		}
		return FString::Printf(TEXT("Place %d"), Index - 2);
	}
}

USlimeHotbarWheelWidget::USlimeHotbarWheelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
}

TSharedRef<SWidget> USlimeHotbarWheelWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void USlimeHotbarWheelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshSlots();
}

void USlimeHotbarWheelWidget::SetHighlightedSlot(int32 SlotIndex)
{
	HighlightedSlot = ((SlotIndex % SlimeHotbarWheelPrivate::SlotCount) + SlimeHotbarWheelPrivate::SlotCount) % SlimeHotbarWheelPrivate::SlotCount;
	RefreshSlots();
}

void USlimeHotbarWheelWidget::BuildLayoutIfNeeded()
{
	if (SectorRoots.Num() == SlimeHotbarWheelPrivate::SlotCount)
	{
		return;
	}
	if (CenterLabel)
	{
		bBuiltInCode = false;
		return;
	}

	bBuiltInCode = true;
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HotbarWheelCanvas"));
	WidgetTree->RootWidget = Root;

	UMaterialInterface* InkMaterial = FMenuUIStyle::LoadButtonMaterial();
	SectorRoots.Reset();
	SectorImages.Reset();
	SectorNames.Reset();

	for (int32 Index = 0; Index < SlimeHotbarWheelPrivate::SlotCount; ++Index)
	{
		const FString Suffix = FString::FromInt(Index);
		USizeBox* Sector = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("HBSector_%s"), *Suffix));
		Sector->SetWidthOverride(SectorSize);
		Sector->SetHeightOverride(SectorSize);

		if (UCanvasPanelSlot* SectorSlot = Root->AddChildToCanvas(Sector))
		{
			const float Angle = FMath::DegreesToRadians(Index * (360.f / SlimeHotbarWheelPrivate::SlotCount));
			const FVector2D Offset(WheelRadius * FMath::Sin(Angle), -WheelRadius * FMath::Cos(Angle));
			SectorSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			SectorSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			SectorSlot->SetAutoSize(true);
			SectorSlot->SetPosition(Offset);
		}

		UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *FString::Printf(TEXT("HBStack_%s"), *Suffix));
		Sector->AddChild(Stack);

		UImage* Disc = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("HBDisc_%s"), *Suffix));
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

		UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("HBName_%s"), *Suffix));
		FMenuUIStyle::ApplyBrushCJKFont(Name, 22.f, FMenuUIStyle::WarmTextColor());
		if (UOverlaySlot* NameSlot = Stack->AddChildToOverlay(Name))
		{
			NameSlot->SetHorizontalAlignment(HAlign_Center);
			NameSlot->SetVerticalAlignment(VAlign_Center);
		}

		SectorRoots.Add(Sector);
		SectorImages.Add(Disc);
		SectorNames.Add(Name);
	}

	UVerticalBox* CenterBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HBCenterBox"));
	if (UCanvasPanelSlot* CenterSlot = Root->AddChildToCanvas(CenterBox))
	{
		CenterSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CenterSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CenterSlot->SetAutoSize(true);
	}

	CenterLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HBCenterLabel"));
	FMenuUIStyle::ApplyBrushCJKFont(CenterLabel, 22.f, FMenuUIStyle::WarmTitleColor());
	CenterBox->AddChildToVerticalBox(CenterLabel);

	HintLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HBHintLabel"));
	HintLabel->SetText(LOCTEXT("HotbarWheelHint", "Scroll - release TAB"));
	FMenuUIStyle::ApplyMixedMenuFont(HintLabel, 14.f, FMenuUIStyle::WarmMutedTextColor());
	if (UVerticalBoxSlot* HintSlot = CenterBox->AddChildToVerticalBox(HintLabel))
	{
		HintSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		HintSlot->SetHorizontalAlignment(HAlign_Center);
	}
}

void USlimeHotbarWheelWidget::RefreshSlots()
{
	if (SectorRoots.Num() != SlimeHotbarWheelPrivate::SlotCount)
	{
		return;
	}

	const USlimeInventorySubsystem* Inv = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		Inv = GI->GetSubsystem<USlimeInventorySubsystem>();
	}

	for (int32 Index = 0; Index < SlimeHotbarWheelPrivate::SlotCount; ++Index)
	{
		const bool bSelected = Index == HighlightedSlot;
		FString Label = SlimeHotbarWheelPrivate::SlotTitle(Index);
		if (Inv)
		{
			const FName ItemId = Inv->GetHotbarItem(Index);
			if (!ItemId.IsNone())
			{
				Label = ItemId.ToString();
			}
			else
			{
				Label = TEXT("-");
			}
		}

		if (UWidget* Sector = SectorRoots[Index])
		{
			Sector->SetRenderScale(FVector2D(bSelected ? SlimeHotbarWheelPrivate::SelectedScale : SlimeHotbarWheelPrivate::IdleScale));
			Sector->SetRenderOpacity(bSelected ? SlimeHotbarWheelPrivate::SelectedOpacity : SlimeHotbarWheelPrivate::IdleOpacity);
		}
		if (UImage* Disc = SectorImages[Index])
		{
			const FLinearColor Tint = bSelected
				? FLinearColor(0.72f, 0.55f, 0.32f, 0.95f)
				: FLinearColor(0.45f, 0.38f, 0.28f, 0.7f);
			Disc->SetColorAndOpacity(Tint);
		}
		if (UTextBlock* Name = SectorNames[Index])
		{
			Name->SetText(FText::FromString(Label));
			Name->SetColorAndOpacity(FSlateColor(bSelected ? FMenuUIStyle::WarmTitleColor() : FMenuUIStyle::WarmTextColor()));
		}
	}

	if (CenterLabel)
	{
		FString Center = SlimeHotbarWheelPrivate::SlotTitle(HighlightedSlot);
		if (Inv)
		{
			const FName ItemId = Inv->GetHotbarItem(HighlightedSlot);
			Center = ItemId.IsNone() ? TEXT("-") : ItemId.ToString();
		}
		CenterLabel->SetText(FText::FromString(Center));
	}
}

#undef LOCTEXT_NAMESPACE
