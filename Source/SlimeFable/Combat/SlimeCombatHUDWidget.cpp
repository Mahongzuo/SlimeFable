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
#include "Materials/MaterialInterface.h"
#include "SlimeCombatComponent.h"
#include "SlimeCharacter.h"
#include "Inventory/SlimeInventorySubsystem.h"
#include "Inventory/SlimeItemDefinition.h"
#include "Inventory/SlimeInteractComponent.h"
#include "Inventory/SlimeWorldPickup.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Styling/SlateTypes.h"
#include "Types/SlateEnums.h"
#include "UI/MenuUIStyle.h"
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
}

void USlimeCombatHUDWidget::BuildLayoutIfNeeded()
{
	if (SlotKeys.Num() == 3 && UltimateBar && UnstuckButton && HotbarLabels.Num() == 6 && InteractPrompt)
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
