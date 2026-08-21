// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeCombatHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
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
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Widget.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "SlimeCombatComponent.h"
#include "SlimeAbilityComponent.h"
#include "SlimeCharacter.h"
#include "SlimeElementComponent.h"
#include "SlimeLockOnComponent.h"
#include "SlimeDevourComponent.h"
#include "SlimeHealthComponent.h"
#include "EnemyCharacter.h"
#include "SlimeEnemyCharacter.h"
#include "Components/Border.h"
#include "Inventory/SlimeInventorySubsystem.h"
#include "Inventory/SlimeItemDefinition.h"
#include "Inventory/SlimeInteractComponent.h"
#include "Inventory/SlimeWorldPickup.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Styling/SlateTypes.h"
#include "Types/SlateEnums.h"
#include "UI/MenuUIStyle.h"
#include "Quest/QuestSubsystem.h"
#include "Settings/SlimeInputSettings.h"
#include "Settings/SlimeInputTypes.h"
#include "Engine/GameInstance.h"

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
	// Combat HUD is read/click chrome only. Focusable widgets swallow Tab (element wheel)
	// and Space (jump) via Slate navigation / button activation.
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

TSharedRef<SWidget> USlimeCombatHUDWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void USlimeCombatHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
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
	RefreshLockOnBar(InDeltaTime);
}

void USlimeCombatHUDWidget::BuildLayoutIfNeeded()
{
	if (SlotKeys.Num() == 3 && UltimateBar && UnstuckButton && HotbarLabels.Num() == 6 && InteractPrompt && LockOnPanel && LaunchChargeBar && DevourHoldBar && SlotCdTexts.Num() == 3 && PlayerHealthBar)
	{
		return;
	}

	bBuiltInCode = true;
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CombatHudRoot"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
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
	SlotCdTexts.Reset();
	SlotBackgrounds.Reset();

	UMaterialInterface* ButtonMat = FMenuUIStyle::LoadButtonMaterial();
	UMaterialInterface* ProgressMat = LoadProgressBarMaterial();

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
		Key->SetText(FText::FromString(FString::FromInt(Index + 1)));
		Labels->AddChildToVerticalBox(Key);

		UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("Name%d"), Index));
		Labels->AddChildToVerticalBox(Name);

		UTextBlock* CdText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("CdText%d"), Index));
		CdText->SetJustification(ETextJustify::Center);
		if (UOverlaySlot* CdTextSlot = Cell->AddChildToOverlay(CdText))
		{
			CdTextSlot->SetHorizontalAlignment(HAlign_Right);
			CdTextSlot->SetVerticalAlignment(VAlign_Top);
			CdTextSlot->SetPadding(FMargin(0.f, 4.f, 8.f, 0.f));
		}

		SlotKeys.Add(Key);
		SlotNames.Add(Name);
		SlotCds.Add(Cd);
		SlotCdTexts.Add(CdText);
	}

	// Keep bars for early-out guard; hide permanently (resources unused).
	ResonanceBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ResonanceBar"));
	ResonanceBar->SetVisibility(ESlateVisibility::Collapsed);
	Stack->AddChildToVerticalBox(ResonanceBar);

	UltimateBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("UltimateBar"));
	UltimateBar->SetVisibility(ESlateVisibility::Collapsed);
	Stack->AddChildToVerticalBox(UltimateBar);

	UnstuckButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("UnstuckButton"));
	UnstuckButton->SetVisibility(ESlateVisibility::Visible);
	// Public property applied at slate build; keeps Space/Enter from activating Unstuck.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UnstuckButton->IsFocusable = false;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	UnstuckButton->SetClickMethod(EButtonClickMethod::MouseDown);
	UnstuckButton->SetTouchMethod(EButtonTouchMethod::DownAndUp);
	UnstuckButton->SetPressMethod(EButtonPressMethod::DownAndUp);
	UTextBlock* UnstuckLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("UnstuckLabel"));
	UnstuckLabel->SetText(FText::FromString(TEXT("脱离卡死")));
	UnstuckLabel->SetJustification(ETextJustify::Center);
	UnstuckButton->AddChild(UnstuckLabel);

	if (UCanvasPanelSlot* UnstuckSlot = Root->AddChildToCanvas(UnstuckButton))
	{
		UnstuckSlot->SetAnchors(FAnchors(1.f, 0.f));
		UnstuckSlot->SetAlignment(FVector2D(1.f, 0.f));
		UnstuckSlot->SetPosition(FVector2D(-24.f, 24.f));
		UnstuckSlot->SetSize(FVector2D(200.f, 48.f));
		UnstuckSlot->SetZOrder(20);
	}

	UMaterialInterface* InkMat = FMenuUIStyle::LoadButtonMaterial();
	FMenuUIStyle::ApplyMaterialButtonStyle(UnstuckButton, InkMat, FVector2D(200.f, 48.f));
	FMenuUIStyle::ApplyBrushCJKFont(UnstuckLabel, 18.f, FMenuUIStyle::WarmTextColor());
	// Prevent Tab / directional focus navigation from selecting this button.
	UnstuckButton->SetNavigationRuleBase(EUINavigation::Next, EUINavigationRule::Escape);
	UnstuckButton->SetNavigationRuleBase(EUINavigation::Previous, EUINavigationRule::Escape);
	UnstuckButton->SetNavigationRuleBase(EUINavigation::Left, EUINavigationRule::Escape);
	UnstuckButton->SetNavigationRuleBase(EUINavigation::Right, EUINavigationRule::Escape);
	UnstuckButton->SetNavigationRuleBase(EUINavigation::Up, EUINavigationRule::Escape);
	UnstuckButton->SetNavigationRuleBase(EUINavigation::Down, EUINavigationRule::Escape);
	UnstuckButton->OnClicked.AddDynamic(this, &USlimeCombatHUDWidget::HandleUnstuckClicked);

	LaunchChargeTrack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LaunchChargeTrack"));
	LaunchChargeTrack->SetBrushColor(FLinearColor(0.08f, 0.07f, 0.05f, 0.72f));
	LaunchChargeTrack->SetPadding(FMargin(4.f, 3.f));
	LaunchChargeTrack->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* ChargeSlot = Root->AddChildToCanvas(LaunchChargeTrack))
	{
		ChargeSlot->SetAnchors(FAnchors(0.5f, 1.f));
		ChargeSlot->SetAlignment(FVector2D(0.5f, 1.f));
		ChargeSlot->SetPosition(FVector2D(0.f, -100.f));
		ChargeSlot->SetSize(FVector2D(280.f, 18.f));
		ChargeSlot->SetZOrder(15);
	}

	LaunchChargeBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("LaunchChargeBar"));
	LaunchChargeBar->SetPercent(0.f);
	LaunchChargeBar->SetFillColorAndOpacity(FLinearColor(0.72f, 0.58f, 0.32f, 0.95f));
	if (ProgressMat)
	{
		FProgressBarStyle ChargeStyle = LaunchChargeBar->GetWidgetStyle();
		FSlateBrush ChargeFill = FMenuUIStyle::MakeMaterialBrush(ProgressMat, FVector2D(272.f, 12.f));
		ChargeFill.TintColor = FSlateColor(FLinearColor(0.92f, 0.78f, 0.48f, 0.95f));
		ChargeStyle.SetFillImage(ChargeFill);
		FSlateBrush ChargeEmpty;
		ChargeEmpty.DrawAs = ESlateBrushDrawType::NoDrawType;
		ChargeStyle.SetBackgroundImage(ChargeEmpty);
		LaunchChargeBar->SetWidgetStyle(ChargeStyle);
	}
	LaunchChargeTrack->AddChild(LaunchChargeBar);

	DevourHoldTrack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DevourHoldTrack"));
	DevourHoldTrack->SetBrushColor(FLinearColor(0.08f, 0.07f, 0.05f, 0.72f));
	DevourHoldTrack->SetPadding(FMargin(4.f, 3.f));
	DevourHoldTrack->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* HoldSlot = Root->AddChildToCanvas(DevourHoldTrack))
	{
		HoldSlot->SetAnchors(FAnchors(0.f, 0.f));
		HoldSlot->SetAlignment(FVector2D(0.5f, 0.f));
		HoldSlot->SetPosition(FVector2D(0.f, 0.f));
		HoldSlot->SetSize(FVector2D(160.f, 14.f));
		HoldSlot->SetZOrder(16);
	}
	DevourHoldBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("DevourHoldBar"));
	DevourHoldBar->SetPercent(0.f);
	DevourHoldBar->SetFillColorAndOpacity(FLinearColor(0.72f, 0.58f, 0.32f, 0.95f));
	if (ProgressMat)
	{
		FProgressBarStyle HoldStyle = DevourHoldBar->GetWidgetStyle();
		FSlateBrush HoldFill = FMenuUIStyle::MakeMaterialBrush(ProgressMat, FVector2D(152.f, 8.f));
		HoldFill.TintColor = FSlateColor(FLinearColor(0.92f, 0.78f, 0.48f, 0.95f));
		HoldStyle.SetFillImage(HoldFill);
		FSlateBrush HoldEmpty;
		HoldEmpty.DrawAs = ESlateBrushDrawType::NoDrawType;
		HoldStyle.SetBackgroundImage(HoldEmpty);
		DevourHoldBar->SetWidgetStyle(HoldStyle);
	}
	DevourHoldTrack->AddChild(DevourHoldBar);

	DigestTrack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DigestTrack"));
	DigestTrack->SetBrushColor(FLinearColor(0.08f, 0.07f, 0.05f, 0.72f));
	DigestTrack->SetPadding(FMargin(4.f, 3.f));
	DigestTrack->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* DigestSlot = Root->AddChildToCanvas(DigestTrack))
	{
		DigestSlot->SetAnchors(FAnchors(0.5f, 1.f));
		DigestSlot->SetAlignment(FVector2D(0.5f, 1.f));
		DigestSlot->SetPosition(FVector2D(0.f, -124.f));
		DigestSlot->SetSize(FVector2D(240.f, 14.f));
		DigestSlot->SetZOrder(15);
	}
	DigestBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("DigestBar"));
	DigestBar->SetPercent(0.f);
	DigestBar->SetFillColorAndOpacity(FLinearColor(0.55f, 0.42f, 0.24f, 0.95f));
	DigestTrack->AddChild(DigestBar);

	Skill1ChargeTrack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Skill1ChargeTrack"));
	Skill1ChargeTrack->SetBrushColor(FLinearColor(0.08f, 0.07f, 0.05f, 0.72f));
	Skill1ChargeTrack->SetPadding(FMargin(4.f, 3.f));
	Skill1ChargeTrack->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* HoldSlot = Root->AddChildToCanvas(Skill1ChargeTrack))
	{
		HoldSlot->SetAnchors(FAnchors(0.5f, 1.f));
		HoldSlot->SetAlignment(FVector2D(0.5f, 1.f));
		HoldSlot->SetPosition(FVector2D(0.f, -146.f));
		HoldSlot->SetSize(FVector2D(180.f, 12.f));
		HoldSlot->SetZOrder(15);
	}
	Skill1ChargeBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Skill1ChargeBar"));
	Skill1ChargeBar->SetPercent(0.f);
	Skill1ChargeBar->SetFillColorAndOpacity(FMenuUIStyle::TodayEdgeColor());
	Skill1ChargeTrack->AddChild(Skill1ChargeBar);

	PhantomCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PhantomCountText"));
	PhantomCountText->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* CountSlot = Root->AddChildToCanvas(PhantomCountText))
	{
		CountSlot->SetAnchors(FAnchors(0.f, 1.f));
		CountSlot->SetAlignment(FVector2D(0.f, 1.f));
		CountSlot->SetPosition(FVector2D(28.f, -28.f));
		CountSlot->SetAutoSize(true);
		CountSlot->SetZOrder(15);
	}
	FMenuUIStyle::ApplyMarkerFont(PhantomCountText, 18.f, FMenuUIStyle::WarmMutedTextColor());

	UHorizontalBox* HotbarRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HotbarRow"));
	if (UCanvasPanelSlot* HotbarSlot = Root->AddChildToCanvas(HotbarRow))
	{
		HotbarSlot->SetAnchors(FAnchors(0.5f, 1.f));
		HotbarSlot->SetAlignment(FVector2D(0.5f, 1.f));
		HotbarSlot->SetPosition(FVector2D(0.f, -28.f));
		HotbarSlot->SetAutoSize(true);
	}
	HotbarLabels.Reset();
	for (int32 Index = 0; Index < 6; ++Index)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("HotBox%d"), Index));
		Box->SetWidthOverride(56.f);
		Box->SetHeightOverride(56.f);
		HotbarRow->AddChildToHorizontalBox(Box);

		UOverlay* Cell = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *FString::Printf(TEXT("HotCell%d"), Index));
		Box->AddChild(Cell);

		UImage* Bg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("HotBg%d"), Index));
		if (ButtonMat)
		{
			FMenuUIStyle::ApplyImageMaterial(Bg, ButtonMat);
		}
		if (UOverlaySlot* BgSlot = Cell->AddChildToOverlay(Bg))
		{
			BgSlot->SetHorizontalAlignment(HAlign_Fill);
			BgSlot->SetVerticalAlignment(VAlign_Fill);
		}

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("HotLbl%d"), Index));
		Label->SetJustification(ETextJustify::Center);
		if (UOverlaySlot* LabelSlot = Cell->AddChildToOverlay(Label))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Center);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		HotbarLabels.Add(Label);
	}

	InteractPrompt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InteractPrompt"));
	InteractPrompt->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* PromptSlot = Root->AddChildToCanvas(InteractPrompt))
	{
		PromptSlot->SetAnchors(FAnchors(0.f, 0.f));
		PromptSlot->SetAlignment(FVector2D(0.5f, 1.f));
		PromptSlot->SetAutoSize(true);
		PromptSlot->SetPosition(FVector2D(-1000.f, -1000.f));
	}

	LockOnPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LockOnPanel"));
	LockOnPanel->SetVisibility(ESlateVisibility::Collapsed);
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor(0.05f, 0.045f, 0.035f, 0.78f));
		Brush.OutlineSettings.CornerRadii = FVector4(8.f, 8.f, 8.f, 8.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Color = FSlateColor(FLinearColor(0.72f, 0.64f, 0.46f, 0.45f));
		Brush.OutlineSettings.Width = 1.4f;
		LockOnPanel->SetBrush(Brush);
		LockOnPanel->SetPadding(FMargin(12.f, 8.f, 16.f, 10.f));
	}
	if (UCanvasPanelSlot* LockSlot = Root->AddChildToCanvas(LockOnPanel))
	{
		LockSlot->SetAnchors(FAnchors(0.5f, 0.f));
		LockSlot->SetAlignment(FVector2D(0.5f, 0.f));
		LockSlot->SetPosition(FVector2D(0.f, 22.f));
		LockSlot->SetAutoSize(true);
		LockSlot->SetZOrder(12);
	}

	UHorizontalBox* LockRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LockRow"));
	LockOnPanel->SetContent(LockRow);

	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LockIconBox"));
	IconBox->SetWidthOverride(36.f);
	IconBox->SetHeightOverride(36.f);
	if (UHorizontalBoxSlot* IconSlot = LockRow->AddChildToHorizontalBox(IconBox))
	{
		IconSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		IconSlot->SetVerticalAlignment(VAlign_Center);
	}
	UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("LockIcon"));
	if (ButtonMat)
	{
		FMenuUIStyle::ApplyImageMaterial(Icon, ButtonMat);
	}
	else
	{
		Icon->SetColorAndOpacity(FMenuUIStyle::TodayEdgeColor());
	}
	IconBox->AddChild(Icon);

	UVerticalBox* LockTexts = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LockTexts"));
	if (UHorizontalBoxSlot* TextSlot = LockRow->AddChildToHorizontalBox(LockTexts))
	{
		TextSlot->SetSize(ESlateSizeRule::Fill);
		TextSlot->SetVerticalAlignment(VAlign_Center);
	}

	LockOnName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LockOnName"));
	LockTexts->AddChildToVerticalBox(LockOnName);

	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LockBarBox"));
	BarBox->SetWidthOverride(520.f);
	BarBox->SetHeightOverride(20.f);
	if (UVerticalBoxSlot* BarSlot = LockTexts->AddChildToVerticalBox(BarBox))
	{
		BarSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}

	LockOnBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("LockOnBar"));
	LockOnBarMID = FMenuUIStyle::CreateHealthBarMID(this);
	FMenuUIStyle::ApplyHealthBarImage(LockOnBar, LockOnBarMID, FVector2D(520.f, 20.f));
	FMenuUIStyle::SetHealthBarValues(LockOnBarMID, 1.f, 1.f, 0.f, 26.f);
	BarBox->AddChild(LockOnBar);

	PlayerHealthBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PlayerHealthBar"));
	PlayerHealthBarMID = FMenuUIStyle::CreateHealthBarMID(this);
	FMenuUIStyle::ApplyHealthBarImage(PlayerHealthBar, PlayerHealthBarMID, FVector2D(280.f, 18.f));
	FMenuUIStyle::SetHealthBarValues(PlayerHealthBarMID, 1.f, 1.f, 0.f, 15.5f);
	if (UCanvasPanelSlot* HpSlot = Root->AddChildToCanvas(PlayerHealthBar))
	{
		HpSlot->SetAnchors(FAnchors(0.f, 1.f));
		HpSlot->SetAlignment(FVector2D(0.f, 1.f));
		HpSlot->SetPosition(FVector2D(28.f, -28.f));
		HpSlot->SetSize(FVector2D(280.f, 18.f));
		HpSlot->SetZOrder(16);
	}

	WeekText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("WeekText"));
	if (UCanvasPanelSlot* WeekSlot = Root->AddChildToCanvas(WeekText))
	{
		WeekSlot->SetAnchors(FAnchors(0.f, 1.f));
		WeekSlot->SetAlignment(FVector2D(0.f, 1.f));
		WeekSlot->SetPosition(FVector2D(28.f, -52.f));
		WeekSlot->SetAutoSize(true);
		WeekSlot->SetZOrder(16);
	}

	DeathText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DeathText"));
	DeathText->SetText(FText::FromString(TEXT("被击倒")));
	DeathText->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* DeathSlot = Root->AddChildToCanvas(DeathText))
	{
		DeathSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		DeathSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		DeathSlot->SetAutoSize(true);
		DeathSlot->SetZOrder(30);
	}
}

void USlimeCombatHUDWidget::ApplyProgressBarFill(UProgressBar* Bar, const FLinearColor& Fill)
{
	if (!Bar)
	{
		return;
	}
	Bar->SetFillColorAndOpacity(Fill);
	FProgressBarStyle Style = Bar->GetWidgetStyle();
	FSlateBrush FillImage = Style.FillImage;
	FillImage.TintColor = FSlateColor(Fill);
	Style.SetFillImage(FillImage);
	Bar->SetWidgetStyle(Style);
}

FLinearColor USlimeCombatHUDWidget::GetSlimeHudTint() const
{
	if (APawn* Pawn = GetOwningPlayerPawn())
	{
		if (const USlimeElementComponent* Element = Pawn->FindComponentByClass<USlimeElementComponent>())
		{
			FLinearColor Color = Element->GetCurrentProfile().BaseColor;
			Color.A = 0.95f;
			return Color;
		}
	}
	return FLinearColor(0.72f, 0.58f, 0.32f, 0.95f);
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
			FText KeyText = FText::FromString(FString::FromInt(Index + 1));
			if (const UGameInstance* GI = GetGameInstance())
			{
				if (const USlimeInputSettings* InputSettings = GI->GetSubsystem<USlimeInputSettings>())
				{
					const ESlimeInputAction Actions[3] = {
						ESlimeInputAction::Skill1,
						ESlimeInputAction::Skill2,
						ESlimeInputAction::Skill3
					};
					KeyText = InputSettings->GetKeyDisplayName(Actions[Index]);
				}
			}
			SlotKeys[Index]->SetText(KeyText);
			FMenuUIStyle::ApplyMarkerFont(SlotKeys[Index], 22.f, FMenuUIStyle::WarmTitleColor());
		}
		if (SlotCds[Index])
		{
			const float Remaining = Combat->GetSkillCooldownRemaining(Slots[Index]);
			const float MaxCd = FMath::Max(Defs[Index]->Cooldown, 0.01f);
			SlotCds[Index]->SetPercent(Remaining <= 0.f ? 1.f : 1.f - Remaining / MaxCd);
		}
		if (SlotCdTexts.IsValidIndex(Index) && SlotCdTexts[Index])
		{
			const float Remaining = Combat->GetSkillCooldownRemaining(Slots[Index]);
			if (Remaining > 0.f)
			{
				SlotCdTexts[Index]->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Remaining)));
				FMenuUIStyle::ApplyMarkerFont(SlotCdTexts[Index], 16.f, FMenuUIStyle::WarmTitleColor());
			}
			else
			{
				SlotCdTexts[Index]->SetText(FText::GetEmpty());
			}
		}
	}

	if (ComboText)
	{
		FMenuUIStyle::ApplyMarkerFont(ComboText, 18.f, FMenuUIStyle::WarmTitleColor());
		const int32 Combo = Combat->GetComboIndex();
		ComboText->SetText(Combo > 0 ? FText::FromString(FString::Printf(TEXT("%d / 4"), Combo)) : FText::GetEmpty());
	}

	if (UnstuckButton && UnstuckButton->GetChildrenCount() > 0)
	{
		if (UTextBlock* Label = Cast<UTextBlock>(UnstuckButton->GetChildAt(0)))
		{
			FMenuUIStyle::ApplyBrushCJKFont(Label, 18.f, FMenuUIStyle::WarmTextColor());
		}
	}

	const UGameInstance* GI = GetGameInstance();
	const USlimeInputSettings* InputSettings = GI ? GI->GetSubsystem<USlimeInputSettings>() : nullptr;
	USlimeInventorySubsystem* Inv = GI ? GI->GetSubsystem<USlimeInventorySubsystem>() : nullptr;
	static const ESlimeInputAction HotbarActions[6] = {
		ESlimeInputAction::Hotbar1, ESlimeInputAction::Hotbar2, ESlimeInputAction::Hotbar3,
		ESlimeInputAction::Hotbar4, ESlimeInputAction::Hotbar5, ESlimeInputAction::Hotbar6
	};
	for (int32 Index = 0; Index < HotbarLabels.Num(); ++Index)
	{
		UTextBlock* Label = HotbarLabels[Index];
		if (!Label)
		{
			continue;
		}
		FString Line = InputSettings ? InputSettings->GetKeyDisplayName(HotbarActions[Index]).ToString() : FString::FromInt(Index + 1);
		if (Inv)
		{
			const FName ItemId = Inv->GetHotbarItem(Index);
			if (!ItemId.IsNone())
			{
				if (const USlimeItemDefinition* Def = Inv->FindDefinition(ItemId))
				{
					Line = FString::Printf(TEXT("%s\n%s"), *Line, *Def->DisplayName.ToString());
				}
			}
		}
		Label->SetText(FText::FromString(Line));
		FMenuUIStyle::ApplyMixedMenuFont(Label, 14.f, FMenuUIStyle::WarmTitleColor());
	}

	if (InteractPrompt)
	{
		bool bShow = false;
		FText Prompt = FText::GetEmpty();
		FVector2D ScreenPos = FVector2D::ZeroVector;
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				if (USlimeInteractComponent* Interact = Pawn->FindComponentByClass<USlimeInteractComponent>())
				{
					FVector WorldPos;
					if (Interact->GetFocusedPromptWorldLocation(WorldPos))
					{
						FVector2D Projected;
						if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
								PC, WorldPos, Projected, false))
						{
							bShow = true;
							ScreenPos = Projected;
							const FString KeyName = InputSettings
								? InputSettings->GetKeyDisplayName(ESlimeInputAction::Interact).ToString()
								: TEXT("F");
							const FString Verb = Interact->GetFocusedPromptVerb().ToString();
							if (Verb == TEXT("吞噬"))
							{
								ScreenPos.Y -= 36.f;
							}
							Prompt = FText::FromString(FString::Printf(
								TEXT("%s %s"),
								*KeyName,
								Verb.IsEmpty() ? TEXT("拾取") : *Verb));
						}
					}
				}
			}
		}
		InteractPrompt->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		InteractPrompt->SetText(Prompt);
		FMenuUIStyle::ApplyMixedMenuFont(InteractPrompt, 22.f, FMenuUIStyle::TodayEdgeColor());
		if (bShow)
		{
			if (UCanvasPanelSlot* PromptSlot = Cast<UCanvasPanelSlot>(InteractPrompt->Slot))
			{
				PromptSlot->SetAnchors(FAnchors(0.f, 0.f));
				PromptSlot->SetAlignment(FVector2D(0.5f, 1.f));
				PromptSlot->SetAutoSize(true);
				PromptSlot->SetPosition(ScreenPos);
			}
		}

		if (DevourHoldTrack && DevourHoldBar)
		{
			float Hold = 0.f;
			bool bShowHold = false;
			if (APawn* Pawn = GetOwningPlayerPawn())
			{
				if (USlimeDevourComponent* Devour = Pawn->FindComponentByClass<USlimeDevourComponent>())
				{
					Hold = Devour->GetHoldProgress();
					bShowHold = Hold > KINDA_SMALL_NUMBER && bShow;
				}
			}
			DevourHoldTrack->SetVisibility(bShowHold ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			DevourHoldBar->SetPercent(Hold);
			ApplyProgressBarFill(DevourHoldBar, GetSlimeHudTint());
			if (bShowHold)
			{
				if (UCanvasPanelSlot* HoldSlot = Cast<UCanvasPanelSlot>(DevourHoldTrack->Slot))
				{
					HoldSlot->SetAnchors(FAnchors(0.f, 0.f));
					HoldSlot->SetAlignment(FVector2D(0.5f, 0.f));
					HoldSlot->SetPosition(ScreenPos + FVector2D(0.f, 6.f));
				}
			}
		}
	}

	if (LaunchChargeBar && LaunchChargeTrack)
	{
		bool bShowCharge = false;
		float Charge = 0.f;
		FLinearColor Fill = FMenuUIStyle::TodayEdgeColor();
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				if (USlimeAbilityComponent* Abilities = Pawn->FindComponentByClass<USlimeAbilityComponent>())
				{
					bShowCharge = Abilities->IsChargingLaunch();
					Charge = Abilities->GetLaunchCharge();
					Fill = Abilities->GetLaunchPreviewColor();
				}
			}
		}
		LaunchChargeTrack->SetVisibility(bShowCharge ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		LaunchChargeBar->SetPercent(Charge);
		ApplyProgressBarFill(LaunchChargeBar, Fill);
	}

	if (APawn* Pawn = GetOwningPlayerPawn())
	{
		if (USlimeDevourComponent* Devour = Pawn->FindComponentByClass<USlimeDevourComponent>())
		{
			if (DigestTrack && DigestBar)
			{
				const bool bDigest = Devour->GetPhase() == ESlimeDevourPhase::Digest;
				DigestTrack->SetVisibility(bDigest ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
				DigestBar->SetPercent(Devour->GetDigestAlpha());
				ApplyProgressBarFill(DigestBar, GetSlimeHudTint());
			}
			if (PhantomCountText)
			{
				PhantomCountText->SetText(FText::FromString(
					FString::Printf(TEXT("%d/6"), Devour->GetPhantomSlotCount())));
			}
			if (Skill1ChargeTrack && Skill1ChargeBar && Combat)
			{
				const float Hold = Combat->GetSkill1HoldFraction();
				const bool bShowHold = Hold > KINDA_SMALL_NUMBER && !Devour->IsPhantomWheelOpen();
				Skill1ChargeTrack->SetVisibility(bShowHold ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
				Skill1ChargeBar->SetPercent(Hold);
			}
		}
	}

	if (PlayerHealthBar && PlayerHealthBarMID)
	{
		float Percent = 1.f;
		if (APawn* Pawn = GetOwningPlayerPawn())
		{
			if (const USlimeHealthComponent* Health = Pawn->FindComponentByClass<USlimeHealthComponent>())
			{
				Percent = Health->GetHealthPercent();
			}
		}
		FMenuUIStyle::SetHealthBarValues(PlayerHealthBarMID, Percent, Percent, 0.f, 15.5f);
	}

	if (WeekText)
	{
		int32 Week = 1;
		if (const UGameInstance* WeekGI = GetGameInstance())
		{
			if (const UQuestSubsystem* Quests = WeekGI->GetSubsystem<UQuestSubsystem>())
			{
				Week = Quests->GetWeekIndex();
			}
		}
		WeekText->SetText(FText::FromString(FString::Printf(TEXT("第 %d 周目"), Week)));
		FMenuUIStyle::ApplyMixedMenuFont(WeekText, 16.f, FMenuUIStyle::WarmMutedTextColor());
	}
}

namespace
{
	bool LooksLikeInternalActorName(const FString& Name)
	{
		if (Name.IsEmpty())
		{
			return true;
		}
		for (const TCHAR Char : Name)
		{
			if (Char >= 0x4E00 && Char <= 0x9FFF)
			{
				return false;
			}
		}
		if (Name.Contains(TEXT(" ")))
		{
			return false;
		}
		const FString Upper = Name.ToUpper();
		return Upper.Contains(TEXT("ENEMY"))
			|| Upper.Contains(TEXT("CHARACTER"))
			|| Upper.Contains(TEXT("ACTOR"))
			|| Upper.StartsWith(TEXT("BP_"))
			|| (Name.Len() > 0 && FChar::IsDigit(Name[0]));
	}

	FString StripInternalActorName(FString Raw)
	{
		if (Raw.StartsWith(TEXT("UEDPIE_")))
		{
			const int32 Second = Raw.Find(TEXT("_"), ESearchCase::IgnoreCase, ESearchDir::FromStart, 7);
			if (Second != INDEX_NONE)
			{
				Raw.RightChopInline(Second + 1);
			}
		}
		if (Raw.EndsWith(TEXT("_C")))
		{
			Raw.LeftChopInline(2);
		}
		int32 Underscore = INDEX_NONE;
		if (Raw.FindLastChar(TEXT('_'), Underscore))
		{
			if (Raw.Mid(Underscore + 1).IsNumeric())
			{
				Raw.LeftInline(Underscore);
			}
		}
		if (Raw.StartsWith(TEXT("BP_")))
		{
			Raw.RightChopInline(3);
		}
		return Raw;
	}

	FText ResolveLockOnDisplayName(AActor* Target)
	{
		if (const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Target))
		{
			return Enemy->GetResolvedDisplayName();
		}
		if (const ASlimeEnemyCharacter* SlimeEnemy = Cast<ASlimeEnemyCharacter>(Target))
		{
			if (!SlimeEnemy->DisplayName.IsEmpty())
			{
				return SlimeEnemy->DisplayName;
			}
			return FText::FromString(TEXT("敌人"));
		}

		const FString Cleaned = Target ? StripInternalActorName(Target->GetActorNameOrLabel()) : FString();
		if (LooksLikeInternalActorName(Cleaned))
		{
			return FText::FromString(TEXT("敌人"));
		}
		return FText::FromString(Cleaned);
	}
}

void USlimeCombatHUDWidget::RefreshLockOnBar(float DeltaTime)
{
	if (!LockOnPanel || !LockOnName || !LockOnBar)
	{
		return;
	}

	AActor* Target = nullptr;
	if (APawn* Pawn = GetOwningPlayerPawn())
	{
		if (const USlimeLockOnComponent* Lock = Pawn->FindComponentByClass<USlimeLockOnComponent>())
		{
			Target = Lock->GetLockedTarget();
		}
	}

	if (!Target)
	{
		LockOnPanel->SetVisibility(ESlateVisibility::Collapsed);
		LastLockTarget.Reset();
		LockOnGhostDelay = 0.f;
		LockOnFlash = 0.f;
		return;
	}

	LockOnPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	LockOnName->SetText(ResolveLockOnDisplayName(Target));
	FMenuUIStyle::ApplyMixedMenuFont(LockOnName, 18.f, FMenuUIStyle::WarmTitleColor());

	float Percent = 1.f;
	if (const USlimeHealthComponent* Health = Target->FindComponentByClass<USlimeHealthComponent>())
	{
		Percent = Health->GetHealthPercent();
	}

	if (LastLockTarget.Get() != Target)
	{
		LastLockTarget = Target;
		LockOnHealthPercent = Percent;
		LockOnGhostPercent = Percent;
		LockOnGhostDelay = 0.f;
		LockOnFlash = 0.f;
	}
	else if (Percent < LockOnHealthPercent - 0.001f)
	{
		LockOnGhostDelay = 0.15f;
		LockOnFlash = 1.f;
	}

	LockOnHealthPercent = Percent;

	if (Percent > LockOnGhostPercent)
	{
		LockOnGhostPercent = Percent;
		LockOnGhostDelay = 0.f;
	}

	if (LockOnGhostDelay > 0.f)
	{
		LockOnGhostDelay -= DeltaTime;
	}
	else
	{
		LockOnGhostPercent = FMath::FInterpConstantTo(LockOnGhostPercent, Percent, DeltaTime, 1.2f);
	}

	LockOnFlash = FMath::FInterpTo(LockOnFlash, 0.f, DeltaTime, 12.f);
	FMenuUIStyle::SetHealthBarValues(LockOnBarMID, Percent, LockOnGhostPercent, LockOnFlash, 26.f);
}

void USlimeCombatHUDWidget::SetDeathVisible(bool bVisible)
{
	if (DeathText)
	{
		DeathText->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		FMenuUIStyle::ApplyBrushCJKFont(DeathText, 42.f, FMenuUIStyle::WarmTitleColor());
	}
}

void USlimeCombatHUDWidget::HandleUnstuckClicked()
{
	APlayerController* PC = GetOwningPlayer();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		Pawn = GetOwningPlayerPawn();
	}
	if (ASlimeCharacter* Slime = Cast<ASlimeCharacter>(Pawn))
	{
		Slime->Unstuck();
	}
}
