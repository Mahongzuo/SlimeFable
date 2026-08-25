// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeInventoryWidget.h"

#include "UI/SlimeInventorySlotProxy.h"
#include "UI/MenuUIStyle.h"
#include "Inventory/SlimeInventorySubsystem.h"
#include "Inventory/SlimeItemDefinition.h"
#include "Inventory/SlimeInteractComponent.h"
#include "Inventory/SlimePlacementComponent.h"
#include "Settings/SlimeInputSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
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
	EnsureDiscardButton();
	ApplyLook();

	if (CloseButton) CloseButton->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnCloseClicked);
	if (TabConsumable) TabConsumable->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnTabConsumable);
	if (TabPlaceable) TabPlaceable->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnTabPlaceable);
	if (TabSouvenir) TabSouvenir->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnTabSouvenir);
	if (PrimaryActionButton) PrimaryActionButton->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnPrimaryActionClicked);
	if (DiscardButton) DiscardButton->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnDiscardClicked);

	Refresh();
}

void USlimeInventoryWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

FReply USlimeInventoryWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	FKey CloseKey = EKeys::B;
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const USlimeInputSettings* InputSettings = GI->GetSubsystem<USlimeInputSettings>())
		{
			CloseKey = InputSettings->GetKey(ESlimeInputAction::Inventory);
		}
	}
	if (InKeyEvent.GetKey() == CloseKey)
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

void USlimeInventoryWidget::StyleSlotChrome(UBorder* Border, UImage* SlotBg, bool bSelected) const
{
	// Flat rock-brown fill — do not use MI_UI_Slot (lab axe/smoke texture is too busy).
	const FLinearColor Fill = bSelected
		? FLinearColor(0.28f, 0.22f, 0.14f, 0.95f)
		: FLinearColor(0.1f, 0.085f, 0.07f, 0.88f);
	const FLinearColor Edge = bSelected
		? FMenuUIStyle::TodayEdgeColor()
		: FLinearColor(0.72f, 0.64f, 0.46f, 0.4f);

	if (Border)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings.CornerRadii = FVector4(10.f, 10.f, 10.f, 10.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Color = FSlateColor(Edge);
		Brush.OutlineSettings.Width = bSelected ? 2.4f : 1.2f;
		Border->SetBrush(Brush);
		Border->SetPadding(FMargin(3.f));
	}

	if (SlotBg)
	{
		FSlateBrush Inner;
		Inner.DrawAs = ESlateBrushDrawType::RoundedBox;
		Inner.TintColor = FSlateColor(FLinearColor(0.06f, 0.05f, 0.04f, bSelected ? 0.55f : 0.72f));
		Inner.OutlineSettings.CornerRadii = FVector4(7.f, 7.f, 7.f, 7.f);
		Inner.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Inner.OutlineSettings.Width = 0.f;
		SlotBg->SetBrush(Inner);
		SlotBg->SetColorAndOpacity(FLinearColor::White);
	}
}

void USlimeInventoryWidget::ApplyItemIcon(UImage* Image, const USlimeItemDefinition* Def, FVector2D Size) const
{
	if (!Image)
	{
		return;
	}
	UTexture2D* Tex = Def ? Def->Icon.LoadSynchronous() : nullptr;
	if (Tex)
	{
		Image->SetBrush(FMenuUIStyle::MakeTextureBrush(Tex, Size));
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		Image->SetColorAndOpacity(FLinearColor::White);
	}
	else
	{
		Image->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USlimeInventoryWidget::BuildLayoutIfNeeded()
{
	if (TitleText && ItemGrid && CloseButton)
	{
		bBuiltInCode = false;
		EnsureDiscardButton();
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

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetPadding(FMargin(28.f, 22.f));
	if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(PanelBorder))
	{
		CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetAutoSize(true);
	}

	UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Panel"));
	PanelBorder->AddChild(Panel);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("背包")));
	if (UVerticalBoxSlot* TitleSlot = Panel->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
	}

	UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Tabs"));
	if (UVerticalBoxSlot* TabSlot = Panel->AddChildToVerticalBox(Tabs))
	{
		TabSlot->SetHorizontalAlignment(HAlign_Center);
		TabSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	}
	auto AddTab = [this, Tabs](const FName& Name, const FText& Label) -> UButton*
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(Name.ToString() + TEXT("_Lbl")));
		Text->SetText(Label);
		Btn->AddChild(Text);
		if (UHorizontalBoxSlot* Slot = Tabs->AddChildToHorizontalBox(Btn))
		{
			Slot->SetPadding(FMargin(6.f, 0.f));
		}
		return Btn;
	};
	TabConsumable = AddTab(TEXT("TabConsumable"), FText::FromString(TEXT("消耗品")));
	TabPlaceable = AddTab(TEXT("TabPlaceable"), FText::FromString(TEXT("放置品")));
	TabSouvenir = AddTab(TEXT("TabSouvenir"), FText::FromString(TEXT("纪念品")));

	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Body"));
	if (UVerticalBoxSlot* BodySlot = Panel->AddChildToVerticalBox(Body))
	{
		BodySlot->SetPadding(FMargin(0.f, 4.f));
	}

	ItemGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("ItemGrid"));
	ItemGrid->SetSlotPadding(FMargin(6.f));
	if (UHorizontalBoxSlot* GridSlot = Body->AddChildToHorizontalBox(ItemGrid))
	{
		GridSlot->SetPadding(FMargin(0.f, 0.f, 18.f, 0.f));
		GridSlot->SetVerticalAlignment(VAlign_Top);
	}

	UVerticalBox* DetailCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DetailCol"));
	if (UHorizontalBoxSlot* DetailSlot = Body->AddChildToHorizontalBox(DetailCol))
	{
		DetailSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		DetailSlot->SetVerticalAlignment(VAlign_Top);
	}

	USizeBox* DetailIconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DetailIconBox"));
	DetailIconBox->SetWidthOverride(96.f);
	DetailIconBox->SetHeightOverride(96.f);
	if (UVerticalBoxSlot* IconBoxSlot = DetailCol->AddChildToVerticalBox(DetailIconBox))
	{
		IconBoxSlot->SetHorizontalAlignment(HAlign_Center);
		IconBoxSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}
	DetailIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DetailIcon"));
	DetailIconBox->AddChild(DetailIcon);

	DetailName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DetailName"));
	DetailName->SetText(FText::FromString(TEXT("选择物品")));
	DetailCol->AddChildToVerticalBox(DetailName);

	DetailDesc = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DetailDesc"));
	DetailDesc->SetAutoWrapText(true);
	DetailDesc->SetWrapTextAt(260.f);
	DetailDesc->SetMinDesiredWidth(220.f);
	if (UVerticalBoxSlot* DescSlot = DetailCol->AddChildToVerticalBox(DetailDesc))
	{
		DescSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}

	ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ActionRow"));
	if (UVerticalBoxSlot* ActionSlot = Panel->AddChildToVerticalBox(ActionRow))
	{
		ActionSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 6.f));
		ActionSlot->SetHorizontalAlignment(HAlign_Center);
	}
	auto AddRowBtn = [this](const FName& Name, const FText& Label) -> UButton*
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(Name.ToString() + TEXT("_Lbl")));
		Text->SetText(Label);
		Btn->AddChild(Text);
		if (UHorizontalBoxSlot* Slot = ActionRow->AddChildToHorizontalBox(Btn))
		{
			Slot->SetPadding(FMargin(8.f, 0.f));
		}
		return Btn;
	};
	PrimaryActionButton = AddRowBtn(TEXT("PrimaryActionButton"), FText::FromString(TEXT("使用")));
	DiscardButton = AddRowBtn(TEXT("DiscardButton"), FText::FromString(TEXT("丢弃")));

	UTextBlock* HotbarHintLocal = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HotbarHint"));
	HotbarHint = HotbarHintLocal;
	HotbarHint->SetText(FText::FromString(TEXT("配到快捷栏（点数字格）")));
	if (UVerticalBoxSlot* HintSlot = Panel->AddChildToVerticalBox(HotbarHint))
	{
		HintSlot->SetHorizontalAlignment(HAlign_Center);
		HintSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 4.f));
	}

	HotbarAssignRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HotbarAssignRow"));
	if (UVerticalBoxSlot* HotbarSlot = Panel->AddChildToVerticalBox(HotbarAssignRow))
	{
		HotbarSlot->SetHorizontalAlignment(HAlign_Center);
		HotbarSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}
	BuildHotbarAssignButtonsIfNeeded();

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	UTextBlock* CloseLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseButton_Lbl"));
	CloseLbl->SetText(FText::FromString(TEXT("关闭")));
	CloseButton->AddChild(CloseLbl);
	if (UVerticalBoxSlot* CloseSlot = Panel->AddChildToVerticalBox(CloseButton))
	{
		CloseSlot->SetHorizontalAlignment(HAlign_Center);
		CloseSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}
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

	if (PanelBorder)
	{
		FSlateBrush PanelBrush;
		PanelBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		PanelBrush.TintColor = FSlateColor(FLinearColor(0.05f, 0.045f, 0.035f, 0.88f));
		PanelBrush.OutlineSettings.CornerRadii = FVector4(16.f, 16.f, 16.f, 16.f);
		PanelBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		PanelBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.72f, 0.64f, 0.46f, 0.45f));
		PanelBrush.OutlineSettings.Width = 1.6f;
		PanelBorder->SetBrush(PanelBrush);
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
	StyleBtn(PrimaryActionButton, FVector2D(180.f, 48.f));
	StyleBtn(DiscardButton, FVector2D(180.f, 48.f));
	StyleBtn(CloseButton, FVector2D(220.f, 48.f));
	if (HotbarHint)
	{
		FMenuUIStyle::ApplyBrushCJKFont(HotbarHint, 14.f, FMenuUIStyle::WarmMutedTextColor());
	}
	BuildHotbarAssignButtonsIfNeeded();
	RefreshHotbarAssign();
}

void USlimeInventoryWidget::BuildHotbarAssignButtonsIfNeeded()
{
	if (!HotbarAssignRow || !WidgetTree)
	{
		return;
	}
	if (HotbarButtons.Num() == SlimeHotbarSlotCount)
	{
		return;
	}

	HotbarAssignRow->ClearChildren();
	HotbarButtons.Reset();
	HotbarBorders.Reset();
	HotbarIcons.Reset();
	HotbarIndexLabels.Reset();

	for (int32 Index = 0; Index < SlimeHotbarSlotCount; ++Index)
	{
		const int32 Display = Index + 1;
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("HotbarBox%d"), Display));
		Box->SetWidthOverride(HotbarCellSize);
		Box->SetHeightOverride(HotbarCellSize);
		if (UHorizontalBoxSlot* BoxSlot = HotbarAssignRow->AddChildToHorizontalBox(Box))
		{
			BoxSlot->SetPadding(FMargin(5.f, 0.f));
		}

		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("HotbarBtn%d"), Display));
		{
			FButtonStyle ClearStyle;
			FSlateBrush Empty;
			Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
			ClearStyle.SetNormal(Empty);
			ClearStyle.SetHovered(Empty);
			ClearStyle.SetPressed(Empty);
			ClearStyle.SetDisabled(Empty);
			ClearStyle.SetNormalPadding(FMargin(0.f));
			ClearStyle.SetPressedPadding(FMargin(0.f));
			Btn->SetStyle(ClearStyle);
			Btn->SetBackgroundColor(FLinearColor::Transparent);
		}
		Box->AddChild(Btn);

		UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *FString::Printf(TEXT("HotbarStack%d"), Display));
		Btn->AddChild(Stack);

		UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("HotbarBorder%d"), Display));
		if (UOverlaySlot* BorderSlot = Stack->AddChildToOverlay(Border))
		{
			BorderSlot->SetHorizontalAlignment(HAlign_Fill);
			BorderSlot->SetVerticalAlignment(VAlign_Fill);
		}

		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("HotbarIcon%d"), Display));
		Icon->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* IconSlot = Stack->AddChildToOverlay(Icon))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(8.f));
		}

		UTextBlock* IndexLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("HotbarIdx%d"), Display));
		IndexLbl->SetText(FText::AsNumber(Display));
		IndexLbl->SetJustification(ETextJustify::Center);
		if (UOverlaySlot* IdxSlot = Stack->AddChildToOverlay(IndexLbl))
		{
			IdxSlot->SetHorizontalAlignment(HAlign_Left);
			IdxSlot->SetVerticalAlignment(VAlign_Top);
			IdxSlot->SetPadding(FMargin(4.f, 2.f, 0.f, 0.f));
		}

		switch (Display)
		{
		case 1: Btn->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnHotbar1); break;
		case 2: Btn->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnHotbar2); break;
		case 3: Btn->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnHotbar3); break;
		case 4: Btn->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnHotbar4); break;
		case 5: Btn->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnHotbar5); break;
		case 6: Btn->OnClicked.AddUniqueDynamic(this, &USlimeInventoryWidget::OnHotbar6); break;
		default: break;
		}

		HotbarButtons.Add(Btn);
		HotbarBorders.Add(Border);
		HotbarIcons.Add(Icon);
		HotbarIndexLabels.Add(IndexLbl);
	}
}

void USlimeInventoryWidget::RefreshHotbarAssign()
{
	BuildHotbarAssignButtonsIfNeeded();
	USlimeInventorySubsystem* Inv = GetInventory();
	const USlimeItemDefinition* SelectedDef = Inv ? Inv->FindDefinition(SelectedItemId) : nullptr;

	for (int32 Index = 0; Index < SlimeHotbarSlotCount; ++Index)
	{
		if (!HotbarBorders.IsValidIndex(Index) || !HotbarIcons.IsValidIndex(Index) || !HotbarButtons.IsValidIndex(Index))
		{
			continue;
		}

		const FName AssignedId = Inv ? Inv->GetHotbarItem(Index) : NAME_None;
		const USlimeItemDefinition* AssignedDef = Inv ? Inv->FindDefinition(AssignedId) : nullptr;
		const bool bMatchesSelected = !SelectedItemId.IsNone() && AssignedId == SelectedItemId;
		const bool bJustFlashed = FlashHotbarSlot == Index;
		const bool bHighlight = bMatchesSelected || bJustFlashed;

		bool bSlotCompatible = true;
		if (SelectedDef)
		{
			if (Index <= 2)
			{
				bSlotCompatible = SelectedDef->Category == ESlimeItemCategory::Consumable;
			}
			else
			{
				bSlotCompatible = SelectedDef->Category == ESlimeItemCategory::Placeable;
			}
		}

		FSlateBrush BorderBrush;
		BorderBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		BorderBrush.TintColor = FSlateColor(FLinearColor(0.12f, 0.1f, 0.08f, bSlotCompatible ? 0.95f : 0.45f));
		BorderBrush.OutlineSettings.CornerRadii = FVector4(8.f, 8.f, 8.f, 8.f);
		BorderBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		BorderBrush.OutlineSettings.Color = FSlateColor(
			bHighlight ? FMenuUIStyle::TodayEdgeColor() : FLinearColor(0.72f, 0.64f, 0.46f, 0.35f));
		BorderBrush.OutlineSettings.Width = bHighlight ? 2.5f : 1.2f;
		HotbarBorders[Index]->SetBrush(BorderBrush);
		HotbarBorders[Index]->SetPadding(FMargin(2.f));

		ApplyItemIcon(HotbarIcons[Index], AssignedDef, FVector2D(HotbarCellSize * 0.55f));
		if (!AssignedDef && HotbarIcons[Index])
		{
			HotbarIcons[Index]->SetVisibility(ESlateVisibility::Collapsed);
		}

		if (HotbarIndexLabels.IsValidIndex(Index) && HotbarIndexLabels[Index])
		{
			FMenuUIStyle::ApplyMarkerFont(
				HotbarIndexLabels[Index],
				14.f,
				bHighlight ? FMenuUIStyle::TodayEdgeColor() : FMenuUIStyle::WarmMutedTextColor());
		}

		if (HotbarButtons[Index])
		{
			HotbarButtons[Index]->SetIsEnabled(!SelectedItemId.IsNone() && bSlotCompatible);
			HotbarButtons[Index]->SetRenderOpacity(bSlotCompatible ? 1.f : 0.45f);
		}
	}

	if (FlashHotbarSlot != INDEX_NONE)
	{
		FlashHotbarSlot = INDEX_NONE;
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
	const int32 SlotCount = GridColumns * GridRows;

	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		const bool bHasItem = Entries.IsValidIndex(Index);
		const FName ItemId = bHasItem ? Entries[Index].ItemId : NAME_None;
		const int32 Count = bHasItem ? Entries[Index].Count : 0;
		const USlimeItemDefinition* Def = bHasItem ? Inv->FindDefinition(ItemId) : nullptr;
		const bool bSelected = bHasItem && ItemId == SelectedItemId;

		USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("Cell_%d"), Index));
		Cell->SetWidthOverride(CellSize);
		Cell->SetHeightOverride(CellSize);

		UButton* SlotBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("Item_%d"), Index));
		{
			FButtonStyle ClearStyle;
			FSlateBrush Empty;
			Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
			ClearStyle.SetNormal(Empty);
			ClearStyle.SetHovered(Empty);
			ClearStyle.SetPressed(Empty);
			ClearStyle.SetDisabled(Empty);
			ClearStyle.SetNormalPadding(FMargin(0.f));
			ClearStyle.SetPressedPadding(FMargin(0.f));
			SlotBtn->SetStyle(ClearStyle);
			SlotBtn->SetBackgroundColor(FLinearColor::Transparent);
		}
		Cell->AddChild(SlotBtn);

		UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *FString::Printf(TEXT("Stack_%d"), Index));
		SlotBtn->AddChild(Stack);

		UBorder* Chrome = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("Chrome_%d"), Index));
		if (UOverlaySlot* ChromeSlot = Stack->AddChildToOverlay(Chrome))
		{
			ChromeSlot->SetHorizontalAlignment(HAlign_Fill);
			ChromeSlot->SetVerticalAlignment(VAlign_Fill);
		}

		UImage* SlotBg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("SlotBg_%d"), Index));
		Chrome->AddChild(SlotBg);
		StyleSlotChrome(Chrome, SlotBg, bSelected);

		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("Icon_%d"), Index));
		if (UOverlaySlot* IconSlot = Stack->AddChildToOverlay(Icon))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(12.f));
		}
		ApplyItemIcon(Icon, Def, FVector2D(CellSize * 0.62f));

		UTextBlock* CountLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("Cnt_%d"), Index));
		if (bHasItem && Count > 0)
		{
			CountLbl->SetText(FText::AsNumber(Count));
			FMenuUIStyle::ApplyMarkerFont(CountLbl, 16.f, FMenuUIStyle::WarmTitleColor());
			CountLbl->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			CountLbl->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (UOverlaySlot* CountSlot = Stack->AddChildToOverlay(CountLbl))
		{
			CountSlot->SetHorizontalAlignment(HAlign_Right);
			CountSlot->SetVerticalAlignment(VAlign_Bottom);
			CountSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 6.f));
		}

		if (UUniformGridSlot* GridSlot = ItemGrid->AddChildToUniformGrid(Cell, Index / GridColumns, Index % GridColumns))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}

		if (bHasItem)
		{
			USlimeInventorySlotProxy* Proxy = NewObject<USlimeInventorySlotProxy>(this);
			Proxy->ItemId = ItemId;
			Proxy->Owner = this;
			SlotBtn->OnClicked.AddUniqueDynamic(Proxy, &USlimeInventorySlotProxy::HandleClicked);
			SlotProxies.Add(Proxy);
			if (bSelected)
			{
				Cell->SetRenderScale(FVector2D(1.06f));
			}
		}
		else
		{
			SlotBtn->SetIsEnabled(false);
		}
	}

	SelectItem(SelectedItemId);
	RefreshHotbarAssign();
}

void USlimeInventoryWidget::HandleSlotClicked(FName ItemId)
{
	SelectItem(ItemId);
	Refresh();
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
	ApplyItemIcon(DetailIcon, Def, FVector2D(88.f));
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
		const ESlateVisibility ActionVis = Def ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
		PrimaryActionButton->SetVisibility(ActionVis);
		const bool bShowHotbar = Def && (Def->Category == ESlimeItemCategory::Consumable || Def->Category == ESlimeItemCategory::Placeable);
		if (HotbarAssignRow)
		{
			HotbarAssignRow->SetVisibility(bShowHotbar ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
		if (HotbarHint)
		{
			HotbarHint->SetVisibility(bShowHotbar ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			if (bShowHotbar)
			{
				const FString Hint = Def->Category == ESlimeItemCategory::Consumable
					? TEXT("配到快捷栏（1–3 消耗品）")
					: TEXT("配到快捷栏（4–6 放置品）");
				HotbarHint->SetText(FText::FromString(Hint));
			}
		}
	}
	if (DiscardButton)
	{
		DiscardButton->SetVisibility(Def ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ActionRow)
	{
		ActionRow->SetVisibility(Def ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	RefreshHotbarAssign();
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

void USlimeInventoryWidget::EnsureDiscardButton()
{
	if (DiscardButton || !WidgetTree)
	{
		return;
	}

	UPanelWidget* InsertParent = nullptr;
	int32 InsertIndex = 0;
	if (PrimaryActionButton)
	{
		if (UHorizontalBox* ExistingRow = Cast<UHorizontalBox>(PrimaryActionButton->GetParent()))
		{
			ActionRow = ExistingRow;
			InsertParent = ExistingRow;
			InsertIndex = ExistingRow->GetChildIndex(PrimaryActionButton) + 1;
		}
		else if (UPanelWidget* Direct = Cast<UPanelWidget>(PrimaryActionButton->GetParent()))
		{
			InsertParent = Direct;
			InsertIndex = Direct->GetChildIndex(PrimaryActionButton) + 1;
		}
	}
	if (!InsertParent)
	{
		return;
	}

	DiscardButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DiscardButton"));
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DiscardButton_Lbl"));
	Label->SetText(FText::FromString(TEXT("丢弃")));
	DiscardButton->AddChild(Label);
	if (UPanelSlot* Inserted = InsertParent->InsertChildAt(InsertIndex, DiscardButton))
	{
		if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Inserted))
		{
			HSlot->SetPadding(FMargin(8.f, 0.f));
		}
	}
}

void USlimeInventoryWidget::OnDiscardClicked()
{
	if (SelectedItemId.IsNone())
	{
		return;
	}
	USlimeInventorySubsystem* Inv = GetInventory();
	if (!Inv)
	{
		return;
	}
	const int32 Count = Inv->GetItemCount(SelectedItemId);
	if (Count <= 0 || !Inv->RemoveItem(SelectedItemId, Count))
	{
		return;
	}
	SelectedItemId = NAME_None;
	Refresh();
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
		if (Inv->AssignHotbar(SlotIndex, SelectedItemId))
		{
			FlashHotbarSlot = SlotIndex;
			if (HotbarHint)
			{
				const USlimeItemDefinition* Def = Inv->FindDefinition(SelectedItemId);
				const FString Name = Def ? Def->DisplayName.ToString() : SelectedItemId.ToString();
				HotbarHint->SetText(FText::FromString(FString::Printf(TEXT("已配到快捷栏 %d：%s"), SlotIndex + 1, *Name)));
				FMenuUIStyle::ApplyBrushCJKFont(HotbarHint, 14.f, FMenuUIStyle::TodayEdgeColor());
			}
			RefreshHotbarAssign();
		}
	}
}

void USlimeInventoryWidget::OnHotbar1() { AssignSelectedToHotbar(0); }
void USlimeInventoryWidget::OnHotbar2() { AssignSelectedToHotbar(1); }
void USlimeInventoryWidget::OnHotbar3() { AssignSelectedToHotbar(2); }
void USlimeInventoryWidget::OnHotbar4() { AssignSelectedToHotbar(3); }
void USlimeInventoryWidget::OnHotbar5() { AssignSelectedToHotbar(4); }
void USlimeInventoryWidget::OnHotbar6() { AssignSelectedToHotbar(5); }
