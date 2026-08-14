// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeInventoryWidget.h"

#include "UI/SlimeInventorySlotProxy.h"
#include "UI/MenuUIStyle.h"
#include "Inventory/SlimeInventorySubsystem.h"
#include "Inventory/SlimeItemDefinition.h"
#include "Inventory/SlimeInteractComponent.h"
#include "Inventory/SlimePlacementComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"

USlimeInventoryWidget::USlimeInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

TSharedRef<SWidget> USlimeInventoryWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void USlimeInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyLook();

	if (CloseButton) CloseButton->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnCloseClicked);
	if (TabConsumable) TabConsumable->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnTabConsumable);
	if (TabPlaceable) TabPlaceable->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnTabPlaceable);
	if (TabSouvenir) TabSouvenir->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnTabSouvenir);
	if (PrimaryActionButton) PrimaryActionButton->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnPrimaryActionClicked);

	Refresh();
}

void USlimeInventoryWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

FReply USlimeInventoryWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::B)
	{
		OnCloseClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

USlimeInventorySubsystem* USlimeInventoryWidget::GetInventory() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<USlimeInventorySubsystem>() : nullptr;
}

void USlimeInventoryWidget::BuildLayoutIfNeeded()
{
	if (TitleText && ItemGrid && CloseButton)
	{
		bBuiltInCode = false;
		return;
	}
	bBuiltInCode = true;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	DimOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DimOverlay"));
	if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(DimOverlay))
	{
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(0.f));
	}

	UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Panel"));
	if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(Panel))
	{
		CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetAutoSize(true);
	}

	auto AddBtn = [this, Panel](const FName& Name, const FText& Label) -> UButton*
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(Name.ToString() + TEXT("_Lbl")));
		Text->SetText(Label);
		Btn->AddChild(Text);
		Panel->AddChildToVerticalBox(Btn);
		return Btn;
	};

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("背包")));
	Panel->AddChildToVerticalBox(TitleText);

	UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Tabs"));
	Panel->AddChildToVerticalBox(Tabs);
	auto AddTab = [this, Tabs](const FName& Name, const FText& Label) -> UButton*
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(Name.ToString() + TEXT("_Lbl")));
		Text->SetText(Label);
		Btn->AddChild(Text);
		Tabs->AddChildToHorizontalBox(Btn);
		return Btn;
	};
	TabConsumable = AddTab(TEXT("TabConsumable"), FText::FromString(TEXT("消耗品")));
	TabPlaceable = AddTab(TEXT("TabPlaceable"), FText::FromString(TEXT("放置品")));
	TabSouvenir = AddTab(TEXT("TabSouvenir"), FText::FromString(TEXT("纪念品")));

	ItemGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("ItemGrid"));
	if (UVerticalBoxSlot* GridSlot = Panel->AddChildToVerticalBox(ItemGrid))
	{
		GridSlot->SetPadding(FMargin(0.f, 12.f));
	}

	DetailName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DetailName"));
	Panel->AddChildToVerticalBox(DetailName);
	DetailDesc = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DetailDesc"));
	Panel->AddChildToVerticalBox(DetailDesc);

	PrimaryActionButton = AddBtn(TEXT("PrimaryActionButton"), FText::FromString(TEXT("使用")));

	HotbarAssignRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HotbarAssignRow"));
	Panel->AddChildToVerticalBox(HotbarAssignRow);
	for (int32 Index = 1; Index <= 6; ++Index)
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("HotbarBtn%d"), Index));
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("HotbarLbl%d"), Index));
		Text->SetText(FText::AsNumber(Index));
		Btn->AddChild(Text);
		HotbarAssignRow->AddChildToHorizontalBox(Btn);
		switch (Index)
		{
		case 1: Btn->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnHotbar1); break;
		case 2: Btn->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnHotbar2); break;
		case 3: Btn->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnHotbar3); break;
		case 4: Btn->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnHotbar4); break;
		case 5: Btn->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnHotbar5); break;
		case 6: Btn->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnHotbar6); break;
		default: break;
		}
	}

	CloseButton = AddBtn(TEXT("CloseButton"), FText::FromString(TEXT("关闭")));
}

void USlimeInventoryWidget::ApplyLook()
{
	if (DimOverlay)
	{
		FSlateBrush DimBrush;
		DimBrush.DrawAs = ESlateBrushDrawType::Box;
		DimBrush.TintColor = FSlateColor(FLinearColor(0.04f, 0.03f, 0.02f, 0.55f));
		DimBrush.ImageSize = FVector2D(32.f, 32.f);
		DimOverlay->SetBrush(DimBrush);
	}

	FMenuUIStyle::ApplyBrushCJKFont(TitleText, 36.f, FMenuUIStyle::WarmTitleColor());
	FMenuUIStyle::ApplyBrushCJKFont(DetailName, 22.f, FMenuUIStyle::WarmTextColor());
	FMenuUIStyle::ApplyBrushCJKFont(DetailDesc, 16.f, FMenuUIStyle::WarmMutedTextColor());

	UMaterialInterface* BrushBtn = FMenuUIStyle::LoadButtonMaterial();
	auto StyleBtn = [BrushBtn](UButton* Button, FVector2D Size)
	{
		FMenuUIStyle::ApplyMaterialButtonStyle(Button, BrushBtn, Size);
		if (Button)
		{
			if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
			{
				FMenuUIStyle::ApplyBrushCJKFont(Label, 18.f, FMenuUIStyle::WarmTextColor());
			}
			FMenuUIStyle::BindInkButtonHover(Button, Cast<UTextBlock>(Button ? Button->GetContent() : nullptr));
		}
	};
	StyleBtn(TabConsumable, FVector2D(120.f, 44.f));
	StyleBtn(TabPlaceable, FVector2D(120.f, 44.f));
	StyleBtn(TabSouvenir, FVector2D(120.f, 44.f));
	StyleBtn(PrimaryActionButton, FVector2D(220.f, 48.f));
	StyleBtn(CloseButton, FVector2D(220.f, 48.f));
	if (HotbarAssignRow)
	{
		for (int32 Index = 0; Index < HotbarAssignRow->GetChildrenCount(); ++Index)
		{
			StyleBtn(Cast<UButton>(HotbarAssignRow->GetChildAt(Index)), FVector2D(44.f, 44.f));
		}
	}
}

void USlimeInventoryWidget::Refresh()
{
	USlimeInventorySubsystem* Inv = GetInventory();
	if (!Inv || !ItemGrid)
	{
		return;
	}

	ItemGrid->ClearChildren();
	SlotProxies.Reset();

	const TArray<FSlimeInventoryEntry> Entries = Inv->GetEntriesByCategory(ActiveCategory);
	UMaterialInterface* BrushBtn = FMenuUIStyle::LoadButtonMaterial();
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FSlimeInventoryEntry& Entry = Entries[Index];
		const USlimeItemDefinition* Def = Inv->FindDefinition(Entry.ItemId);

		UButton* SlotBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("Item_%d"), Index));
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("ItemLbl_%d"), Index));
		const FString Text = FString::Printf(TEXT("%s x%d"), Def ? *Def->DisplayName.ToString() : *Entry.ItemId.ToString(), Entry.Count);
		Label->SetText(FText::FromString(Text));
		FMenuUIStyle::ApplyBrushCJKFont(Label, 14.f, FMenuUIStyle::WarmTextColor());
		SlotBtn->AddChild(Label);
		FMenuUIStyle::ApplyMaterialButtonStyle(SlotBtn, BrushBtn, FVector2D(140.f, 64.f));

		if (UUniformGridSlot* GridSlot = ItemGrid->AddChildToUniformGrid(SlotBtn, Index / 3, Index % 3))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		USlimeInventorySlotProxy* Proxy = NewObject<USlimeInventorySlotProxy>(this);
		Proxy->ItemId = Entry.ItemId;
		Proxy->Owner = this;
		SlotBtn->OnClicked.AddUniqueDynamic(Proxy, &USlimeInventorySlotProxy::HandleClicked);
		SlotProxies.Add(Proxy);
	}

	SelectItem(SelectedItemId);
}

void USlimeInventoryWidget::HandleSlotClicked(FName ItemId)
{
	SelectItem(ItemId);
}

void USlimeInventoryWidget::SelectCategory(ESlimeItemCategory Category)
{
	ActiveCategory = Category;
	SelectedItemId = NAME_None;
	Refresh();
}

void USlimeInventoryWidget::SelectItem(FName ItemId)
{
	SelectedItemId = ItemId;
	USlimeInventorySubsystem* Inv = GetInventory();
	const USlimeItemDefinition* Def = Inv ? Inv->FindDefinition(ItemId) : nullptr;
	if (DetailName)
	{
		DetailName->SetText(Def ? Def->DisplayName : FText::FromString(TEXT("选择物品")));
	}
	if (DetailDesc)
	{
		DetailDesc->SetText(Def ? Def->Description : FText::GetEmpty());
	}
	if (PrimaryActionButton)
	{
		FString Action = TEXT("使用");
		if (Def)
		{
			switch (Def->Category)
			{
			case ESlimeItemCategory::Placeable: Action = TEXT("放置"); break;
			case ESlimeItemCategory::Souvenir: Action = TEXT("查看"); break;
			default: Action = TEXT("使用"); break;
			}
		}
		if (UTextBlock* Label = Cast<UTextBlock>(PrimaryActionButton->GetContent()))
		{
			Label->SetText(FText::FromString(Action));
		}
		const bool bShowHotbar = Def && (Def->Category == ESlimeItemCategory::Consumable || Def->Category == ESlimeItemCategory::Placeable);
		if (HotbarAssignRow)
		{
			HotbarAssignRow->SetVisibility(bShowHotbar ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
}

void USlimeInventoryWidget::OnCloseClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			if (USlimeInteractComponent* Interact = Pawn->FindComponentByClass<USlimeInteractComponent>())
			{
				Interact->CloseInventory();
				return;
			}
		}
	}
	RemoveFromParent();
}

void USlimeInventoryWidget::OnTabConsumable() { SelectCategory(ESlimeItemCategory::Consumable); }
void USlimeInventoryWidget::OnTabPlaceable() { SelectCategory(ESlimeItemCategory::Placeable); }
void USlimeInventoryWidget::OnTabSouvenir() { SelectCategory(ESlimeItemCategory::Souvenir); }

void USlimeInventoryWidget::OnPrimaryActionClicked()
{
	USlimeInventorySubsystem* Inv = GetInventory();
	APlayerController* PC = GetOwningPlayer();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const USlimeItemDefinition* Def = Inv ? Inv->FindDefinition(SelectedItemId) : nullptr;
	if (!Inv || !Def || !Pawn)
	{
		return;
	}

	switch (Def->Category)
	{
	case ESlimeItemCategory::Consumable:
		Inv->UseConsumable(SelectedItemId, Pawn);
		Refresh();
		break;
	case ESlimeItemCategory::Placeable:
		if (USlimeInteractComponent* Interact = Pawn->FindComponentByClass<USlimeInteractComponent>())
		{
			Interact->CloseInventory();
		}
		Inv->BeginPlaceItem(SelectedItemId, Pawn);
		break;
	case ESlimeItemCategory::Souvenir:
		if (USlimeInteractComponent* Interact = Pawn->FindComponentByClass<USlimeInteractComponent>())
		{
			Interact->CloseInventory();
		}
		Inv->OpenSouvenir(SelectedItemId, PC);
		break;
	default:
		break;
	}
}

void USlimeInventoryWidget::AssignSelectedToHotbar(int32 SlotIndex)
{
	if (USlimeInventorySubsystem* Inv = GetInventory())
	{
		Inv->AssignHotbar(SlotIndex, SelectedItemId);
		Refresh();
	}
}

void USlimeInventoryWidget::OnHotbar1() { AssignSelectedToHotbar(0); }
void USlimeInventoryWidget::OnHotbar2() { AssignSelectedToHotbar(1); }
void USlimeInventoryWidget::OnHotbar3() { AssignSelectedToHotbar(2); }
void USlimeInventoryWidget::OnHotbar4() { AssignSelectedToHotbar(3); }
void USlimeInventoryWidget::OnHotbar5() { AssignSelectedToHotbar(4); }
void USlimeInventoryWidget::OnHotbar6() { AssignSelectedToHotbar(5); }
