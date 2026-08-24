// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeElementFormationWidget.h"
#include "UI/MenuUIStyle.h"
#include "Slime/SlimeElementProgressSubsystem.h"
#include "Slime/SlimeAbilityComponent.h"
#include "SlimeFablePlayerController.h"
#include "SlimeCombatTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	FString FormationElementName(ESlimeElement Element)
	{
		switch (Element)
		{
		case ESlimeElement::Water: return TEXT("水");
		case ESlimeElement::Wind: return TEXT("风");
		case ESlimeElement::Fire: return TEXT("火");
		case ESlimeElement::Lightning: return TEXT("雷");
		case ESlimeElement::Dark: return TEXT("暗");
		case ESlimeElement::Physical: return TEXT("物理");
		default: return TEXT("?");
		}
	}
}

TSharedRef<SWidget> USlimeElementFormationWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void USlimeElementFormationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	ApplyLook();
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &USlimeElementFormationWidget::OnCloseClicked);
	}
	RefreshRows();
}

USlimeElementProgressSubsystem* USlimeElementFormationWidget::GetProgress() const
{
	if (UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<USlimeElementProgressSubsystem>();
	}
	return nullptr;
}

void USlimeElementFormationWidget::BuildLayoutIfNeeded()
{
	if (TitleText && RowBox && CloseButton && RowLabels.Num() == 6)
	{
		bBuiltInCode = false;
		return;
	}
	bBuiltInCode = true;
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	DimOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DimOverlay"));
	if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(DimOverlay))
	{
		DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimSlot->SetOffsets(FMargin(0.f));
	}

	USizeBox* Panel = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Panel"));
	Panel->SetWidthOverride(420.f);
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
	}

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VBox"));
	Panel->AddChild(VBox);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("属性编队")));
	if (UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
	}

	HintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HintText"));
	HintText->SetText(FText::FromString(TEXT("拖动属性到目标位置；最上为 1，对应按键切换")));
	HintText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* HintSlot = VBox->AddChildToVerticalBox(HintText))
	{
		HintSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}

	RowBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RowBox"));
	VBox->AddChildToVerticalBox(RowBox);

	RowLabels.Reset();
	RowBorders.Reset();
	for (int32 Index = 0; Index < 6; ++Index)
	{
		UBorder* Border = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), *FString::Printf(TEXT("RowBorder%d"), Index));
		Border->SetPadding(FMargin(12.f, 10.f));
		if (UVerticalBoxSlot* RowSlot = RowBox->AddChildToVerticalBox(Border))
		{
			RowSlot->SetPadding(FMargin(0.f, 4.f));
		}

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), *FString::Printf(TEXT("Row%d"), Index));
		Border->AddChild(Row);

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("Lbl%d"), Index));
		if (UHorizontalBoxSlot* LSlot = Row->AddChildToHorizontalBox(Label))
		{
			LSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LSlot->SetVerticalAlignment(VAlign_Center);
		}
		RowLabels.Add(Label);
		RowBorders.Add(Border);
	}

	USizeBox* CloseSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CloseSize"));
	CloseSize->SetWidthOverride(320.f);
	CloseSize->SetHeightOverride(52.f);
	if (UVerticalBoxSlot* CSlot = VBox->AddChildToVerticalBox(CloseSize))
	{
		CSlot->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
		CSlot->SetHorizontalAlignment(HAlign_Center);
	}
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	CloseSize->AddChild(CloseButton);
	UTextBlock* CloseLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseLbl"));
	CloseLbl->SetText(FText::FromString(TEXT("关闭")));
	CloseLbl->SetJustification(ETextJustify::Center);
	CloseButton->AddChild(CloseLbl);
}

void USlimeElementFormationWidget::ApplyLook()
{
	if (DimOverlay)
	{
		FSlateBrush DimBrush;
		DimBrush.DrawAs = ESlateBrushDrawType::Box;
		DimBrush.TintColor = FSlateColor(FLinearColor(0.04f, 0.03f, 0.02f, 0.55f));
		DimBrush.ImageSize = FVector2D(32.f, 32.f);
		DimOverlay->SetBrush(DimBrush);
	}
	FMenuUIStyle::ApplyBrushCJKFont(TitleText, 32.f, FMenuUIStyle::WarmTitleColor());
	FMenuUIStyle::ApplyBrushCJKFont(HintText, 16.f, FMenuUIStyle::WarmMutedTextColor());
	UMaterialInterface* Brush = FMenuUIStyle::LoadButtonMaterial();
	FMenuUIStyle::ApplyMaterialButtonStyle(CloseButton, Brush, FVector2D(320.f, 52.f));
	if (CloseButton && CloseButton->GetChildrenCount() > 0)
	{
		if (UTextBlock* L = Cast<UTextBlock>(CloseButton->GetChildAt(0)))
		{
			FMenuUIStyle::ApplyBrushCJKFont(L, 22.f, FMenuUIStyle::WarmTextColor());
		}
	}
	for (int32 Index = 0; Index < RowBorders.Num(); ++Index)
	{
		if (UBorder* Border = RowBorders[Index])
		{
			FSlateBrush RowBrush;
			RowBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
			RowBrush.TintColor = FSlateColor(FLinearColor(0.08f, 0.06f, 0.04f, 0.82f));
			RowBrush.OutlineSettings.CornerRadii = FVector4(8.f, 8.f, 8.f, 8.f);
			RowBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			RowBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.55f, 0.45f, 0.3f, 0.5f));
			RowBrush.OutlineSettings.Width = 1.2f;
			Border->SetBrush(RowBrush);
		}
		if (RowLabels.IsValidIndex(Index))
		{
			FMenuUIStyle::ApplyBrushCJKFont(RowLabels[Index], 22.f, FMenuUIStyle::WarmTextColor());
		}
	}
}

void USlimeElementFormationWidget::RefreshRows()
{
	USlimeElementProgressSubsystem* Progress = GetProgress();
	if (!Progress)
	{
		return;
	}
	const TArray<ESlimeElement> Order = Progress->GetElementOrder();
	for (int32 Index = 0; Index < 6; ++Index)
	{
		if (!RowLabels.IsValidIndex(Index) || !RowLabels[Index])
		{
			continue;
		}
		const ESlimeElement El = Order.IsValidIndex(Index) ? Order[Index] : SlimeElement::FromIndex(Index);
		RowLabels[Index]->SetText(FText::FromString(
			FString::Printf(TEXT("%d  %s"), Index + 1, *FormationElementName(El))));
		FMenuUIStyle::ApplyBrushCJKFont(RowLabels[Index], 22.f, SlimeCombat::GetElementVfxColor(El));
	}
}

int32 USlimeElementFormationWidget::HitTestRow(const FVector2D& ScreenPos) const
{
	for (int32 Index = 0; Index < RowBorders.Num(); ++Index)
	{
		UBorder* Border = RowBorders[Index];
		if (!Border)
		{
			continue;
		}
		const FGeometry& Geo = Border->GetCachedGeometry();
		if (Geo.IsUnderLocation(ScreenPos))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FReply USlimeElementFormationWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const int32 Hit = HitTestRow(InMouseEvent.GetScreenSpacePosition());
		if (Hit != INDEX_NONE)
		{
			bDragging = true;
			DragFromIndex = Hit;
			return FReply::Handled().CaptureMouse(TakeWidget());
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USlimeElementFormationWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging && DragFromIndex != INDEX_NONE)
	{
		const int32 Hit = HitTestRow(InMouseEvent.GetScreenSpacePosition());
		if (Hit != INDEX_NONE && Hit != DragFromIndex)
		{
			if (USlimeElementProgressSubsystem* Progress = GetProgress())
			{
				Progress->MoveOrder(DragFromIndex, Hit);
				DragFromIndex = Hit;
				RefreshRows();
			}
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply USlimeElementFormationWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = false;
		DragFromIndex = INDEX_NONE;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void USlimeElementFormationWidget::CloseSelf()
{
	if (ASlimeFablePlayerController* PC = Cast<ASlimeFablePlayerController>(GetOwningPlayer()))
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			if (USlimeAbilityComponent* Ability = Pawn->FindComponentByClass<USlimeAbilityComponent>())
			{
				Ability->CloseFormation();
				return;
			}
		}
		PC->PopUIInput(ESlimeUIInputReason::ElementFormation);
	}
	RemoveFromParent();
}

void USlimeElementFormationWidget::OnCloseClicked()
{
	CloseSelf();
}

FReply USlimeElementFormationWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::L)
	{
		CloseSelf();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
