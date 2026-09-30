// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeSeedPickerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Farm/SlimeFarmPlot.h"
#include "Farm/SlimeFarmSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "SlimeFablePlayerController.h"
#include "UI/MenuUIStyle.h"

TWeakObjectPtr<USlimeSeedPickerWidget> USlimeSeedPickerWidget::OpenWidget;

namespace
{
	const TCHAR* CategoryLabel(ESlimeCropCategory Category)
	{
		switch (Category)
		{
		case ESlimeCropCategory::Fruit: return TEXT("水果");
		case ESlimeCropCategory::Grain: return TEXT("谷物");
		case ESlimeCropCategory::Flower: return TEXT("花草");
		case ESlimeCropCategory::Cash: return TEXT("经济");
		default: return TEXT("蔬菜");
		}
	}

	const TCHAR* ElementLabel(ESlimeElement Element)
	{
		switch (Element)
		{
		case ESlimeElement::Wind: return TEXT("风");
		case ESlimeElement::Fire: return TEXT("火");
		case ESlimeElement::Lightning: return TEXT("雷");
		case ESlimeElement::Dark: return TEXT("暗");
		case ESlimeElement::Physical: return TEXT("物");
		default: return TEXT("水");
		}
	}
}

void USlimeSeedCardProxy::HandleClick()
{
	if (Owner)
	{
		Owner->SelectCrop(CropId, true);
	}
}

USlimeSeedPickerWidget::USlimeSeedPickerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void USlimeSeedPickerWidget::OpenForPlot(ASlimeFarmPlot* InPlot)
{
	if (!InPlot)
	{
		return;
	}
	if (OpenWidget.IsValid())
	{
		CloseOpen();
	}
	APlayerController* PC = InPlot->GetWorld() ? InPlot->GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}
	USlimeSeedPickerWidget* Widget = CreateWidget<USlimeSeedPickerWidget>(PC, StaticClass());
	if (!Widget)
	{
		return;
	}
	Widget->Plot = InPlot;
	if (UGameInstance* GI = InPlot->GetGameInstance())
	{
		if (const USlimeFarmSubsystem* Farm = GI->GetSubsystem<USlimeFarmSubsystem>())
		{
			Widget->SelectedCropId = Farm->GetLastPlantedCrop();
		}
	}
	Widget->AddToViewport(45);
	OpenWidget = Widget;
	if (ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(PC))
	{
		SlimePC->PushUIInput(ESlimeUIInputReason::SeedPicker, Widget);
	}
	else
	{
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(Widget->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}
}

bool USlimeSeedPickerWidget::IsOpen()
{
	return OpenWidget.IsValid();
}

bool USlimeSeedPickerWidget::CloseOpen()
{
	if (!OpenWidget.IsValid())
	{
		return false;
	}
	OpenWidget->Close();
	return true;
}

TSharedRef<SWidget> USlimeSeedPickerWidget::RebuildWidget()
{
	BuildLayout();
	return Super::RebuildWidget();
}

void USlimeSeedPickerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyLook();
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &USlimeSeedPickerWidget::OnCloseClicked);
	}
	if (PlantButton)
	{
		PlantButton->OnClicked.AddUniqueDynamic(this, &USlimeSeedPickerWidget::OnPlantClicked);
	}
	RebuildCards();
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetShowMouseCursor(true);
	}
}

FReply USlimeSeedPickerWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::F)
	{
		Close();
		return FReply::Handled();
	}
	if (Key == EKeys::Enter)
	{
		Confirm();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply USlimeSeedPickerWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		Close();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void USlimeSeedPickerWidget::BuildLayout()
{
	if (WidgetTree->RootWidget)
	{
		return;
	}
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	DimOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Dim"));
	DimOverlay->SetVisibility(ESlateVisibility::Visible);
	if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(DimOverlay))
	{
		DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimSlot->SetOffsets(FMargin(0.f));
	}

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(PanelBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetSize(FVector2D(980.f, 680.f));
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	PanelBorder->AddChild(Column);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	TitleText->SetText(FText::FromString(TEXT("选择作物")));
	Column->AddChildToVerticalBox(TitleText);

	TabRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Tabs"));
	if (UVerticalBoxSlot* TabSlot = Column->AddChildToVerticalBox(TabRow))
	{
		TabSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 8.f));
	}
	const TCHAR* TabNames[] = {TEXT("全部"), TEXT("蔬菜"), TEXT("水果"), TEXT("谷物"), TEXT("花草"), TEXT("经济")};
	UButton* TabPtrs[] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
	for (int32 Index = 0; Index < 6; ++Index)
	{
		UButton* Tab = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Label->SetText(FText::FromString(TabNames[Index]));
		Tab->AddChild(Label);
		if (UHorizontalBoxSlot* Pad = TabRow->AddChildToHorizontalBox(Tab))
		{
			Pad->SetPadding(FMargin(4.f, 0.f));
		}
		TabButtons.Add(Tab);
		TabPtrs[Index] = Tab;
	}
	TabPtrs[0]->OnClicked.AddUniqueDynamic(this, &USlimeSeedPickerWidget::OnTabAll);
	TabPtrs[1]->OnClicked.AddUniqueDynamic(this, &USlimeSeedPickerWidget::OnTabVegetable);
	TabPtrs[2]->OnClicked.AddUniqueDynamic(this, &USlimeSeedPickerWidget::OnTabFruit);
	TabPtrs[3]->OnClicked.AddUniqueDynamic(this, &USlimeSeedPickerWidget::OnTabGrain);
	TabPtrs[4]->OnClicked.AddUniqueDynamic(this, &USlimeSeedPickerWidget::OnTabFlower);
	TabPtrs[5]->OnClicked.AddUniqueDynamic(this, &USlimeSeedPickerWidget::OnTabCash);

	Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Scroll"));
	if (UVerticalBoxSlot* ScrollSlot = Column->AddChildToVerticalBox(Scroll))
	{
		ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ScrollSlot->SetPadding(FMargin(0.f, 4.f));
	}
	CardGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("Cards"));
	CardGrid->SetSlotPadding(FMargin(6.f));
	Scroll->AddChild(CardGrid);

	DetailText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Detail"));
	DetailText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* DetailSlot = Column->AddChildToVerticalBox(DetailText))
	{
		DetailSlot->SetPadding(FMargin(0.f, 8.f));
	}

	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Actions"));
	if (UVerticalBoxSlot* ActionSlot = Column->AddChildToVerticalBox(Actions))
	{
		ActionSlot->SetHorizontalAlignment(HAlign_Center);
		ActionSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}
	PlantButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Plant"));
	UTextBlock* PlantLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlantLbl"));
	PlantLabel->SetText(FText::FromString(TEXT("种下")));
	PlantButton->AddChild(PlantLabel);
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Close"));
	UTextBlock* CloseLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseLbl"));
	CloseLabel->SetText(FText::FromString(TEXT("关闭")));
	CloseButton->AddChild(CloseLabel);
	if (UHorizontalBoxSlot* PlantSlot = Actions->AddChildToHorizontalBox(PlantButton))
	{
		PlantSlot->SetPadding(FMargin(8.f, 0.f));
	}
	if (UHorizontalBoxSlot* CloseSlot = Actions->AddChildToHorizontalBox(CloseButton))
	{
		CloseSlot->SetPadding(FMargin(8.f, 0.f));
	}
}

void USlimeSeedPickerWidget::ApplyLook()
{
	if (DimOverlay)
	{
		FSlateBrush DimBrush;
		DimBrush.DrawAs = ESlateBrushDrawType::Box;
		DimBrush.TintColor = FSlateColor(FLinearColor(0.04f, 0.03f, 0.02f, 0.55f));
		DimOverlay->SetBrush(DimBrush);
	}
	if (PanelBorder)
	{
		FSlateBrush PanelBrush;
		PanelBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		PanelBrush.TintColor = FSlateColor(FLinearColor(0.05f, 0.045f, 0.035f, 0.92f));
		PanelBrush.OutlineSettings.CornerRadii = FVector4(16.f, 16.f, 16.f, 16.f);
		PanelBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		PanelBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.72f, 0.64f, 0.46f, 0.45f));
		PanelBrush.OutlineSettings.Width = 1.6f;
		PanelBorder->SetBrush(PanelBrush);
		PanelBorder->SetPadding(FMargin(22.f, 18.f));
	}
	FMenuUIStyle::ApplyBrushCJKFont(TitleText, 32.f, FMenuUIStyle::WarmTitleColor());
	FMenuUIStyle::ApplyBrushCJKFont(DetailText, 16.f, FMenuUIStyle::WarmMutedTextColor());
	UMaterialInterface* Brush = FMenuUIStyle::LoadButtonMaterial();
	auto Style = [Brush](UButton* Button, FVector2D Size)
	{
		FMenuUIStyle::ApplyMaterialButtonStyle(Button, Brush, Size);
		if (UTextBlock* Label = Button ? Cast<UTextBlock>(Button->GetContent()) : nullptr)
		{
			FMenuUIStyle::ApplyBrushCJKFont(Label, 18.f, FMenuUIStyle::WarmTextColor());
			FMenuUIStyle::BindInkButtonHover(Button, Label);
		}
	};
	for (UButton* Tab : TabButtons)
	{
		Style(Tab, FVector2D(110.f, 42.f));
	}
	Style(PlantButton, FVector2D(180.f, 48.f));
	Style(CloseButton, FVector2D(180.f, 48.f));
}

void USlimeSeedPickerWidget::SelectCategory(int32 CategoryIndex)
{
	CategoryFilter = CategoryIndex;
	RebuildCards();
}

void USlimeSeedPickerWidget::OnTabAll() { SelectCategory(INDEX_NONE); }
void USlimeSeedPickerWidget::OnTabVegetable() { SelectCategory(static_cast<int32>(ESlimeCropCategory::Vegetable)); }
void USlimeSeedPickerWidget::OnTabFruit() { SelectCategory(static_cast<int32>(ESlimeCropCategory::Fruit)); }
void USlimeSeedPickerWidget::OnTabGrain() { SelectCategory(static_cast<int32>(ESlimeCropCategory::Grain)); }
void USlimeSeedPickerWidget::OnTabFlower() { SelectCategory(static_cast<int32>(ESlimeCropCategory::Flower)); }
void USlimeSeedPickerWidget::OnTabCash() { SelectCategory(static_cast<int32>(ESlimeCropCategory::Cash)); }

void USlimeSeedPickerWidget::RebuildCards()
{
	if (!CardGrid || !WidgetTree)
	{
		return;
	}
	CardGrid->ClearChildren();
	CardProxies.Reset();
	CardCropIds.Reset();
	CardNames.Reset();
	CardIcons.Reset();

	UGameInstance* GI = GetGameInstance();
	USlimeFarmSubsystem* Farm = GI ? GI->GetSubsystem<USlimeFarmSubsystem>() : nullptr;
	if (!Farm)
	{
		return;
	}
	TArray<USlimeCropDefinition*> Crops;
	Farm->GetAllCrops(Crops);
	TArray<USlimeCropDefinition*> ShownCrops;
	for (USlimeCropDefinition* Crop : Crops)
	{
		if (!Crop)
		{
			continue;
		}
		if (CategoryFilter != INDEX_NONE && static_cast<int32>(Crop->Category) != CategoryFilter)
		{
			continue;
		}
		ShownCrops.Add(Crop);
	}
	const bool bSelectionVisible = ShownCrops.ContainsByPredicate([this](const USlimeCropDefinition* Crop)
	{
		return Crop && Crop->CropId == SelectedCropId;
	});
	if (ShownCrops.Num() == 0)
	{
		SelectedCropId = NAME_None;
	}
	else if (!bSelectionVisible)
	{
		SelectedCropId = ShownCrops[0]->CropId;
	}

	constexpr int32 Columns = 6;
	TArray<FSoftObjectPath> PendingIcons;
	for (int32 Shown = 0; Shown < ShownCrops.Num(); ++Shown)
	{
		USlimeCropDefinition* Crop = ShownCrops[Shown];
		USlimeSeedCardProxy* Proxy = NewObject<USlimeSeedCardProxy>(this);
		Proxy->CropId = Crop->CropId;
		Proxy->Owner = this;
		CardProxies.Add(Proxy);

		UButton* Card = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Card->OnClicked.AddUniqueDynamic(Proxy, &USlimeSeedCardProxy::HandleClick);
		UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Card->AddChild(Body);
		USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		IconBox->SetWidthOverride(72.f);
		IconBox->SetHeightOverride(72.f);
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		IconBox->AddChild(Icon);
		if (UTexture2D* Texture = Crop->Icon.Get())
		{
			Icon->SetBrush(FMenuUIStyle::MakeTextureBrush(Texture, FVector2D(72.f, 72.f)));
		}
		else
		{
			FSlateBrush Fallback;
			Fallback.DrawAs = ESlateBrushDrawType::RoundedBox;
			Fallback.ImageSize = FVector2D(72.f, 72.f);
			Fallback.TintColor = FSlateColor(FLinearColor(0.45f, 0.38f, 0.22f, 1.f));
			Icon->SetBrush(Fallback);
			if (!Crop->Icon.IsNull())
			{
				PendingIcons.AddUnique(Crop->Icon.ToSoftObjectPath());
			}
		}
		if (UVerticalBoxSlot* IconSlot = Body->AddChildToVerticalBox(IconBox))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
			IconSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 2.f));
		}
		UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Name->SetText(Crop->DisplayName);
		Name->SetJustification(ETextJustify::Center);
		FMenuUIStyle::ApplyBrushCJKFont(Name, 14.f, Crop->CropId == SelectedCropId
			? FMenuUIStyle::WarmTitleColor()
			: FMenuUIStyle::WarmTextColor());
		Body->AddChildToVerticalBox(Name);
		FMenuUIStyle::ApplyMaterialButtonStyle(Card, FMenuUIStyle::LoadButtonMaterial(), FVector2D(140.f, 128.f));
		if (UUniformGridSlot* GridSlot = CardGrid->AddChildToUniformGrid(Card, Shown / Columns, Shown % Columns))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		CardCropIds.Add(Crop->CropId);
		CardNames.Add(Name);
		CardIcons.Add(Icon);
	}

	RefreshDetail();
	if (PendingIcons.Num() > 0)
	{
		IconHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
			PendingIcons,
			FStreamableDelegate::CreateUObject(this, &USlimeSeedPickerWidget::ApplyLoadedIcons));
	}
}

void USlimeSeedPickerWidget::RefreshSelection()
{
	for (int32 Index = 0; Index < CardNames.Num(); ++Index)
	{
		const bool bSelected = CardCropIds.IsValidIndex(Index) && CardCropIds[Index] == SelectedCropId;
		FMenuUIStyle::ApplyBrushCJKFont(CardNames[Index], 14.f, bSelected
			? FMenuUIStyle::WarmTitleColor()
			: FMenuUIStyle::WarmTextColor());
	}
	RefreshDetail();
}

void USlimeSeedPickerWidget::RefreshDetail()
{
	if (!DetailText)
	{
		return;
	}
	UGameInstance* GI = GetGameInstance();
	USlimeFarmSubsystem* Farm = GI ? GI->GetSubsystem<USlimeFarmSubsystem>() : nullptr;
	USlimeCropDefinition* Selected = Farm ? Farm->FindCrop(SelectedCropId) : nullptr;
	if (!Selected)
	{
		DetailText->SetText(FText::FromString(TEXT("没有作物")));
		return;
	}
	const int32 Minutes = FMath::Max(1, FMath::RoundToInt(FMath::Clamp(Selected->GrowSeconds, 10.f, 1800.f) / 60.f));
	const FString Real = Selected->RealWorldSpan.IsEmpty() ? TEXT("要等一段时间") : Selected->RealWorldSpan.ToString();
	DetailText->SetText(FText::FromString(FString::Printf(
		TEXT("%s    %s    偏好%s\n现实里%s。这儿 %d 分钟就能熟。%s"),
		*Selected->DisplayName.ToString(),
		CategoryLabel(Selected->Category),
		ElementLabel(Selected->PreferredElement),
		*Real,
		Minutes,
		Selected->bRegrowAfterHarvest ? TEXT(" 摘完还会再长。") : TEXT(""))));
}

void USlimeSeedPickerWidget::ApplyLoadedIcons()
{
	UGameInstance* GI = GetGameInstance();
	USlimeFarmSubsystem* Farm = GI ? GI->GetSubsystem<USlimeFarmSubsystem>() : nullptr;
	if (!Farm)
	{
		return;
	}
	for (int32 Index = 0; Index < CardIcons.Num(); ++Index)
	{
		UImage* Icon = CardIcons[Index];
		if (!Icon || !CardCropIds.IsValidIndex(Index))
		{
			continue;
		}
		const USlimeCropDefinition* Crop = Farm->FindCrop(CardCropIds[Index]);
		if (Crop)
		{
			if (UTexture2D* Texture = Crop->Icon.Get())
			{
				Icon->SetBrush(FMenuUIStyle::MakeTextureBrush(Texture, FVector2D(72.f, 72.f)));
			}
		}
	}
}

void USlimeSeedPickerWidget::SelectCrop(FName CropId, bool bPlantIfRepeat)
{
	const double Now = FPlatformTime::Seconds();
	const bool bDouble = bPlantIfRepeat && CropId == LastClickCrop && (Now - LastClickSeconds) < 0.35;
	LastClickCrop = CropId;
	LastClickSeconds = Now;
	SelectedCropId = CropId;
	if (bDouble)
	{
		Confirm();
		return;
	}
	RefreshSelection();
}

void USlimeSeedPickerWidget::OnPlantClicked()
{
	Confirm();
}

void USlimeSeedPickerWidget::OnCloseClicked()
{
	Close();
}

void USlimeSeedPickerWidget::Confirm()
{
	ASlimeFarmPlot* Target = Plot.Get();
	UGameInstance* GI = GetGameInstance();
	USlimeFarmSubsystem* Farm = GI ? GI->GetSubsystem<USlimeFarmSubsystem>() : nullptr;
	USlimeCropDefinition* Crop = Farm ? Farm->FindCrop(SelectedCropId) : nullptr;
	if (Target && Crop)
	{
		Target->PlantCrop(Crop);
	}
	Close();
}

void USlimeSeedPickerWidget::Close()
{
	APlayerController* PC = GetOwningPlayer();
	if (ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(PC))
	{
		SlimePC->PopUIInput(ESlimeUIInputReason::SeedPicker);
	}
	else if (PC)
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
	RemoveFromParent();
	if (OpenWidget.Get() == this)
	{
		OpenWidget.Reset();
	}
}
