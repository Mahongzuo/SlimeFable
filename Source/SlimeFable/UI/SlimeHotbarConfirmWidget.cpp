// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeHotbarConfirmWidget.h"
#include "UI/MenuUIStyle.h"
#include "Slime/SlimeAbilityComponent.h"
#include "SlimeFablePlayerController.h"
#include "Inventory/SlimeInventorySubsystem.h"
#include "Inventory/SlimeItemDefinition.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"

TSharedRef<SWidget> USlimeHotbarConfirmWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void USlimeHotbarConfirmWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	ApplyLook();
	if (UseButton)
	{
		UseButton->OnClicked.AddUniqueDynamic(this, &USlimeHotbarConfirmWidget::OnUseClicked);
	}
	if (DiscardButton)
	{
		DiscardButton->OnClicked.AddUniqueDynamic(this, &USlimeHotbarConfirmWidget::OnDiscardClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddUniqueDynamic(this, &USlimeHotbarConfirmWidget::OnCancelClicked);
	}
}

void USlimeHotbarConfirmWidget::Setup(int32 InSlotIndex, FName InItemId, const FText& InDisplayName)
{
	SlotIndex = InSlotIndex;
	ItemId = InItemId;
	BuildLayoutIfNeeded();
	if (TitleText)
	{
		TitleText->SetText(InDisplayName.IsEmpty()
			? FText::FromString(TEXT("快捷栏"))
			: InDisplayName);
	}
	if (ItemIcon)
	{
		UTexture2D* Tex = nullptr;
		if (UGameInstance* GI = GetGameInstance())
		{
			if (USlimeInventorySubsystem* Inv = GI->GetSubsystem<USlimeInventorySubsystem>())
			{
				if (const USlimeItemDefinition* Def = Inv->FindDefinition(InItemId))
				{
					Tex = Def->Icon.LoadSynchronous();
				}
			}
		}
		if (Tex)
		{
			ItemIcon->SetBrush(FMenuUIStyle::MakeTextureBrush(Tex, FVector2D(72.f)));
			ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void USlimeHotbarConfirmWidget::BuildLayoutIfNeeded()
{
	if (TitleText && UseButton && DiscardButton && CancelButton)
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
	Panel->SetWidthOverride(360.f);
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
	}

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VBox"));
	Panel->AddChild(VBox);

	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ItemIconBox"));
	IconBox->SetWidthOverride(72.f);
	IconBox->SetHeightOverride(72.f);
	if (UVerticalBoxSlot* IconSlot = VBox->AddChildToVerticalBox(IconBox))
	{
		IconSlot->SetHorizontalAlignment(HAlign_Center);
		IconSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}
	ItemIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ItemIcon"));
	ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
	IconBox->AddChild(ItemIcon);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("快捷栏")));
	if (UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
	}

	auto MakeBtn = [this, VBox](const TCHAR* Name, const TCHAR* Label, TObjectPtr<UButton>& OutBtn)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("%sSize"), Name));
		Size->SetWidthOverride(300.f);
		Size->SetHeightOverride(48.f);
		if (UVerticalBoxSlot* Slot = VBox->AddChildToVerticalBox(Size))
		{
			Slot->SetPadding(FMargin(0.f, 6.f));
			Slot->SetHorizontalAlignment(HAlign_Center);
		}
		OutBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Size->AddChild(OutBtn);
		UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("%sLbl"), Name));
		Lbl->SetText(FText::FromString(Label));
		Lbl->SetJustification(ETextJustify::Center);
		OutBtn->AddChild(Lbl);
	};
	MakeBtn(TEXT("UseButton"), TEXT("使用"), UseButton);
	MakeBtn(TEXT("DiscardButton"), TEXT("丢弃"), DiscardButton);
	MakeBtn(TEXT("CancelButton"), TEXT("取消"), CancelButton);
}

void USlimeHotbarConfirmWidget::ApplyLook()
{
	if (DimOverlay)
	{
		FSlateBrush DimBrush;
		DimBrush.DrawAs = ESlateBrushDrawType::Box;
		DimBrush.TintColor = FSlateColor(FLinearColor(0.04f, 0.03f, 0.02f, 0.55f));
		DimBrush.ImageSize = FVector2D(32.f, 32.f);
		DimOverlay->SetBrush(DimBrush);
	}
	FMenuUIStyle::ApplyBrushCJKFont(TitleText, 28.f, FMenuUIStyle::WarmTitleColor());
	UMaterialInterface* Brush = FMenuUIStyle::LoadButtonMaterial();
	auto StyleBtn = [Brush](UButton* Btn)
	{
		FMenuUIStyle::ApplyMaterialButtonStyle(Btn, Brush, FVector2D(300.f, 48.f));
		if (Btn && Btn->GetChildrenCount() > 0)
		{
			if (UTextBlock* L = Cast<UTextBlock>(Btn->GetChildAt(0)))
			{
				FMenuUIStyle::ApplyBrushCJKFont(L, 22.f, FMenuUIStyle::WarmTextColor());
			}
		}
	};
	StyleBtn(UseButton);
	StyleBtn(DiscardButton);
	StyleBtn(CancelButton);
}

void USlimeHotbarConfirmWidget::CloseSelf()
{
	RemoveFromParent();
	if (ASlimeFablePlayerController* PC = Cast<ASlimeFablePlayerController>(GetOwningPlayer()))
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			if (USlimeAbilityComponent* Ability = Pawn->FindComponentByClass<USlimeAbilityComponent>())
			{
				Ability->CloseHotbarConfirm();
				return;
			}
		}
		PC->PopUIInput(ESlimeUIInputReason::HotbarConfirm);
	}
}

void USlimeHotbarConfirmWidget::OnUseClicked()
{
	if (APawn* Pawn = GetOwningPlayerPawn())
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (USlimeInventorySubsystem* Inv = GI->GetSubsystem<USlimeInventorySubsystem>())
			{
				Inv->ActivateHotbar(SlotIndex, Pawn);
			}
		}
	}
	CloseSelf();
}

void USlimeHotbarConfirmWidget::OnDiscardClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USlimeInventorySubsystem* Inv = GI->GetSubsystem<USlimeInventorySubsystem>())
		{
			if (!ItemId.IsNone())
			{
				Inv->RemoveItem(ItemId, 1);
			}
			Inv->ClearHotbar(SlotIndex);
		}
	}
	CloseSelf();
}

void USlimeHotbarConfirmWidget::OnCancelClicked()
{
	CloseSelf();
}

FReply USlimeHotbarConfirmWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseSelf();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
