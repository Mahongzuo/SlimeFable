// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeTouchHUDWidget.h"

#include "UI/MenuUIStyle.h"
#include "Settings/SlimeInputSettings.h"
#include "SlimeFablePlayerController.h"
#include "Inventory/SlimeInteractComponent.h"
#include "Combat/SlimeDevourComponent.h"
#include "Slime/SlimeMorphComponent.h"
#include "Quest/QuestSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Styling/SlateTypes.h"

namespace
{
	struct FTouchClusterSpec
	{
		FVector2D Size;
		FVector2D Pos;
		float Font = 22.f;
	};

	// Compact bottom-right cluster. Keep it below QER and inside the right
	// ~200px so those skill buttons no longer eat 攻/跳 taps.
	static const FTouchClusterSpec AttackSpec{ {132.f, 132.f}, {-150.f, -158.f}, 28.f };
	static const FTouchClusterSpec JumpSpec{ {116.f, 116.f}, {-286.f, -248.f}, 24.f };
	static const FTouchClusterSpec DodgeSpec{ {104.f, 104.f}, {-56.f, -140.f}, 22.f };
	static const FTouchClusterSpec SprintSpec{ {92.f, 92.f}, {-150.f, -280.f}, 20.f };
	static const FTouchClusterSpec FlattenSpec{ {88.f, 88.f}, {-286.f, -140.f}, 18.f };
	static const FTouchClusterSpec LaunchSpec{ {88.f, 88.f}, {-286.f, -72.f}, 18.f };
	static const FTouchClusterSpec InteractSpec{ {80.f, 80.f}, {-370.f, -248.f}, 16.f };
	static const FTouchClusterSpec AbsorbSpec{ {80.f, 80.f}, {-370.f, -168.f}, 16.f };
	static const FTouchClusterSpec MorphSpec{ {80.f, 80.f}, {-370.f, -328.f}, 16.f };
	static const FTouchClusterSpec LockSpec{ {80.f, 80.f}, {-56.f, -236.f}, 16.f };

	void PlaceClusterButton(UButton* Button, float AnchorX, float Sign, const FTouchClusterSpec& Spec)
	{
		UWidget* Target = Button && Button->GetParent() ? Button->GetParent() : Cast<UWidget>(Button);
		if (USizeBox* Box = Cast<USizeBox>(Target))
		{
			Box->SetWidthOverride(Spec.Size.X);
			Box->SetHeightOverride(Spec.Size.Y);
		}
		if (UCanvasPanelSlot* Slot = Target ? Cast<UCanvasPanelSlot>(Target->Slot) : nullptr)
		{
			Slot->SetAnchors(FAnchors(AnchorX, 1.f));
			Slot->SetAlignment(FVector2D(0.5f, 0.5f));
			Slot->SetPosition(FVector2D(Sign * FMath::Abs(Spec.Pos.X), Spec.Pos.Y));
			Slot->SetSize(Spec.Size);
		}
	}

	bool IsScreenOverWidget(UWidget* Target, FVector2D ScreenPos, float Pad)
	{
		if (!Target || Target->GetVisibility() == ESlateVisibility::Collapsed)
		{
			return false;
		}
		const FGeometry& Geo = Target->GetCachedGeometry();
		const FVector2D Local = Geo.AbsoluteToLocal(ScreenPos);
		const FVector2D Size = Geo.GetLocalSize();
		return Local.X >= -Pad && Local.Y >= -Pad
			&& Local.X <= Size.X + Pad && Local.Y <= Size.Y + Pad;
	}
}

void USlimeTouchActionProxy::HandlePressed()
{
	if (Owner)
	{
		Owner->SetVirtualAction(Action, true);
	}
}

void USlimeTouchActionProxy::HandleReleased()
{
	if (Owner)
	{
		Owner->SetVirtualAction(Action, false);
	}
}

TSharedRef<SWidget> USlimeTouchHUDWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void USlimeTouchHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::Visible);
	ApplyLook();
	ApplyHandedness();

	if (PauseButton)
	{
		PauseButton->OnClicked.AddUniqueDynamic(this, &USlimeTouchHUDWidget::OnPauseClicked);
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		PauseButton->IsFocusable = false;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	if (QuestButton)
	{
		QuestButton->OnClicked.AddUniqueDynamic(this, &USlimeTouchHUDWidget::OnQuestClicked);
	}
	if (InventoryButton)
	{
		InventoryButton->OnClicked.AddUniqueDynamic(this, &USlimeTouchHUDWidget::OnInventoryClicked);
	}
}

void USlimeTouchHUDWidget::NativeDestruct()
{
	if (USlimeInputSettings* Settings = GetInputSettings())
	{
		Settings->ClearVirtualActions();
	}
	Super::NativeDestruct();
}

void USlimeTouchHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshContextualButtons();
}

USlimeInputSettings* USlimeTouchHUDWidget::GetInputSettings() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<USlimeInputSettings>();
	}
	return nullptr;
}

void USlimeTouchHUDWidget::SetVirtualAction(ESlimeInputAction Action, bool bDown)
{
	if (USlimeInputSettings* Settings = GetInputSettings())
	{
		Settings->SetVirtualActionDown(Action, bDown);
	}
}

UButton* USlimeTouchHUDWidget::AddRoundButton(UCanvasPanel* Root, const FName& Name, const FText& Label,
	FVector2D Position, FVector2D Size, ESlimeInputAction Action, bool bHidden)
{
	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), *FString::Printf(TEXT("%s_Size"), *Name.ToString()));
	SizeBox->SetWidthOverride(Size.X);
	SizeBox->SetHeightOverride(Size.Y);
	if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(SizeBox))
	{
		CanvasSlot->SetAnchors(FAnchors(1.f, 1.f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetPosition(Position);
		CanvasSlot->SetAutoSize(false);
		CanvasSlot->SetSize(Size);
		CanvasSlot->SetZOrder(5);
	}

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Button->IsFocusable = false;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	Button->SetClickMethod(EButtonClickMethod::MouseDown);
	Button->SetTouchMethod(EButtonTouchMethod::DownAndUp);
	Button->SetPressMethod(EButtonPressMethod::ButtonPress);
	SizeBox->AddChild(Button);
	UTextBlock* LabelBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), *FString::Printf(TEXT("%s_Label"), *Name.ToString()));
	LabelBlock->SetText(Label);
	LabelBlock->SetJustification(ETextJustify::Center);
	Button->AddChild(LabelBlock);

	USlimeTouchActionProxy* Proxy = NewObject<USlimeTouchActionProxy>(this);
	Proxy->Action = Action;
	Proxy->Owner = this;
	Button->OnPressed.AddUniqueDynamic(Proxy, &USlimeTouchActionProxy::HandlePressed);
	Button->OnReleased.AddUniqueDynamic(Proxy, &USlimeTouchActionProxy::HandleReleased);
	ActionProxies.Add(Proxy);

	if (bHidden)
	{
		SizeBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	return Button;
}

void USlimeTouchHUDWidget::BuildLayoutIfNeeded()
{
	if (RootCanvas && AttackButton && JumpButton && PauseButton)
	{
		bBuiltInCode = false;
		if (UImage* OldLook = Cast<UImage>(WidgetTree ? WidgetTree->FindWidget(TEXT("LookPad")) : nullptr))
		{
			OldLook->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	bBuiltInCode = true;
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TouchRoot"));
	RootCanvas->SetVisibility(ESlateVisibility::Visible);
	WidgetTree->RootWidget = RootCanvas;

	StickBase = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("StickBase"));
	if (UCanvasPanelSlot* BaseSlot = RootCanvas->AddChildToCanvas(StickBase))
	{
		BaseSlot->SetAnchors(FAnchors(0.f, 1.f));
		BaseSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		BaseSlot->SetPosition(StickCenterLocal);
		BaseSlot->SetSize(FVector2D(StickRadius * 2.f, StickRadius * 2.f));
		BaseSlot->SetZOrder(1);
	}

	StickKnob = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("StickKnob"));
	if (UCanvasPanelSlot* KnobSlot = RootCanvas->AddChildToCanvas(StickKnob))
	{
		KnobSlot->SetAnchors(FAnchors(0.f, 1.f));
		KnobSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		KnobSlot->SetPosition(StickCenterLocal);
		KnobSlot->SetSize(FVector2D(64.f, 64.f));
		KnobSlot->SetZOrder(2);
	}

	auto AddTopButton = [this](const FName& Name, const FText& Label, float X) -> UButton*
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), *FString::Printf(TEXT("%s_Size"), *Name.ToString()));
		SizeBox->SetWidthOverride(88.f);
		SizeBox->SetHeightOverride(44.f);
		if (UCanvasPanelSlot* Slot = RootCanvas->AddChildToCanvas(SizeBox))
		{
			Slot->SetAnchors(FAnchors(0.f, 0.f));
			Slot->SetAlignment(FVector2D(0.f, 0.f));
			Slot->SetPosition(FVector2D(X, 16.f));
			Slot->SetAutoSize(true);
			Slot->SetZOrder(8);
		}
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Button->IsFocusable = false;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		SizeBox->AddChild(Button);
		UTextBlock* LabelBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("%s_Label"), *Name.ToString()));
		LabelBlock->SetText(Label);
		LabelBlock->SetJustification(ETextJustify::Center);
		Button->AddChild(LabelBlock);
		return Button;
	};

	PauseButton = AddTopButton(TEXT("PauseButton"), FText::FromString(TEXT("菜单")), 16.f);
	QuestButton = AddTopButton(TEXT("QuestButton"), FText::FromString(TEXT("史书")), 112.f);
	InventoryButton = AddTopButton(TEXT("InvButton"), FText::FromString(TEXT("背包")), 208.f);

	AttackButton = AddRoundButton(RootCanvas, TEXT("AttackBtn"), FText::FromString(TEXT("攻")),
		AttackSpec.Pos, AttackSpec.Size, ESlimeInputAction::Attack);
	DodgeButton = AddRoundButton(RootCanvas, TEXT("DodgeBtn"), FText::FromString(TEXT("闪")),
		DodgeSpec.Pos, DodgeSpec.Size, ESlimeInputAction::Dodge);
	JumpButton = AddRoundButton(RootCanvas, TEXT("JumpBtn"), FText::FromString(TEXT("跳")),
		JumpSpec.Pos, JumpSpec.Size, ESlimeInputAction::Jump);
	SprintButton = AddRoundButton(RootCanvas, TEXT("SprintBtn"), FText::FromString(TEXT("冲")),
		SprintSpec.Pos, SprintSpec.Size, ESlimeInputAction::Sprint);
	FlattenButton = AddRoundButton(RootCanvas, TEXT("FlatBtn"), FText::FromString(TEXT("扁")),
		FlattenSpec.Pos, FlattenSpec.Size, ESlimeInputAction::Flatten);
	LaunchButton = AddRoundButton(RootCanvas, TEXT("LaunchBtn"), FText::FromString(TEXT("射")),
		LaunchSpec.Pos, LaunchSpec.Size, ESlimeInputAction::Launch);
	InteractButton = AddRoundButton(RootCanvas, TEXT("InteractBtn"), FText::FromString(TEXT("互")),
		InteractSpec.Pos, InteractSpec.Size, ESlimeInputAction::Interact, true);
	AbsorbButton = AddRoundButton(RootCanvas, TEXT("AbsorbBtn"), FText::FromString(TEXT("吸")),
		AbsorbSpec.Pos, AbsorbSpec.Size, ESlimeInputAction::Absorb, true);
	MorphButton = AddRoundButton(RootCanvas, TEXT("MorphBtn"), FText::FromString(TEXT("形")),
		MorphSpec.Pos, MorphSpec.Size, ESlimeInputAction::Morph, true);
	LockOnButton = AddRoundButton(RootCanvas, TEXT("LockBtn"), FText::FromString(TEXT("锁")),
		LockSpec.Pos, LockSpec.Size, ESlimeInputAction::LockOn, true);
	Skill1Button = AddRoundButton(RootCanvas, TEXT("Skill1Btn"), FText::FromString(TEXT("技1")),
		FVector2D(-500.f, -250.f), FVector2D(72.f, 72.f), ESlimeInputAction::Skill1, true);
	Skill2Button = AddRoundButton(RootCanvas, TEXT("Skill2Btn"), FText::FromString(TEXT("技2")),
		FVector2D(-580.f, -250.f), FVector2D(72.f, 72.f), ESlimeInputAction::Skill2, true);
	Skill3Button = AddRoundButton(RootCanvas, TEXT("Skill3Btn"), FText::FromString(TEXT("技3")),
		FVector2D(-660.f, -250.f), FVector2D(72.f, 72.f), ESlimeInputAction::Skill3, true);
}

void USlimeTouchHUDWidget::ApplyLook()
{
	auto StyleCircle = [](UImage* Image, float Alpha)
	{
		if (!Image)
		{
			return;
		}
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor(0.08f, 0.07f, 0.05f, Alpha));
		Brush.OutlineSettings.Color = FLinearColor(0.72f, 0.64f, 0.46f, 0.45f);
		Brush.OutlineSettings.Width = 2.f;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		Brush.OutlineSettings.bUseBrushTransparency = true;
		Image->SetBrush(Brush);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	};
	StyleCircle(StickBase, 0.35f);
	StyleCircle(StickKnob, 0.7f);

	UMaterialInterface* BrushBtn = FMenuUIStyle::LoadButtonMaterial();
	auto StyleBtn = [BrushBtn](UButton* Button, FVector2D Size, float FontSize)
	{
		if (!Button)
		{
			return;
		}
		FMenuUIStyle::ApplyMaterialButtonStyle(Button, BrushBtn, Size);
		if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
		{
			FMenuUIStyle::ApplyBrushCJKFont(Label, FontSize, FMenuUIStyle::WarmTextColor());
		}
	};

	StyleBtn(PauseButton, FVector2D(88.f, 44.f), 16.f);
	StyleBtn(QuestButton, FVector2D(88.f, 44.f), 16.f);
	StyleBtn(InventoryButton, FVector2D(88.f, 44.f), 16.f);
	StyleBtn(AttackButton, AttackSpec.Size, AttackSpec.Font);
	StyleBtn(JumpButton, JumpSpec.Size, JumpSpec.Font);
	StyleBtn(DodgeButton, DodgeSpec.Size, DodgeSpec.Font);
	StyleBtn(FlattenButton, FlattenSpec.Size, FlattenSpec.Font);
	StyleBtn(SprintButton, SprintSpec.Size, SprintSpec.Font);
	StyleBtn(LaunchButton, LaunchSpec.Size, LaunchSpec.Font);
	StyleBtn(InteractButton, InteractSpec.Size, InteractSpec.Font);
	StyleBtn(AbsorbButton, AbsorbSpec.Size, AbsorbSpec.Font);
	StyleBtn(MorphButton, MorphSpec.Size, MorphSpec.Font);
	StyleBtn(LockOnButton, LockSpec.Size, LockSpec.Font);
}

void USlimeTouchHUDWidget::ApplyHandedness()
{
	const USlimeInputSettings* Settings = GetInputSettings();
	const bool bLeft = Settings && Settings->GetTouchHandedness() == ESlimeTouchHandedness::Left;
	const float StickAnchorX = bLeft ? 1.f : 0.f;
	const float ClusterAnchorX = bLeft ? 0.f : 1.f;
	const float StickX = bLeft ? -160.f : 160.f;
	HomeStickLocal = FVector2D(StickX, -200.f);
	if (StickPointer == INDEX_NONE)
	{
		StickCenterLocal = HomeStickLocal;
	}

	auto SetAnchorPos = [](UWidget* Widget, float AnchorX, float AnchorY, FVector2D Pos)
	{
		UWidget* Target = Widget;
		if (UButton* Button = Cast<UButton>(Widget))
		{
			Target = Button->GetParent();
		}
		if (UCanvasPanelSlot* Slot = Target ? Cast<UCanvasPanelSlot>(Target->Slot) : nullptr)
		{
			Slot->SetAnchors(FAnchors(AnchorX, AnchorY));
			Slot->SetPosition(Pos);
		}
	};

	PlaceStickVisuals();
	if (UCanvasPanelSlot* BaseSlot = StickBase ? Cast<UCanvasPanelSlot>(StickBase->Slot) : nullptr)
	{
		BaseSlot->SetAnchors(FAnchors(StickAnchorX, 1.f));
	}
	if (UCanvasPanelSlot* KnobSlot = StickKnob ? Cast<UCanvasPanelSlot>(StickKnob->Slot) : nullptr)
	{
		KnobSlot->SetAnchors(FAnchors(StickAnchorX, 1.f));
	}

	SetAnchorPos(PauseButton, 0.f, 0.f, FVector2D(16.f, 16.f));
	SetAnchorPos(QuestButton, 0.f, 0.f, FVector2D(112.f, 16.f));
	SetAnchorPos(InventoryButton, 0.f, 0.f, FVector2D(208.f, 16.f));

	const float Sign = bLeft ? 1.f : -1.f;
	PlaceClusterButton(AttackButton, ClusterAnchorX, Sign, AttackSpec);
	PlaceClusterButton(DodgeButton, ClusterAnchorX, Sign, DodgeSpec);
	PlaceClusterButton(JumpButton, ClusterAnchorX, Sign, JumpSpec);
	PlaceClusterButton(SprintButton, ClusterAnchorX, Sign, SprintSpec);
	PlaceClusterButton(FlattenButton, ClusterAnchorX, Sign, FlattenSpec);
	PlaceClusterButton(LaunchButton, ClusterAnchorX, Sign, LaunchSpec);
	PlaceClusterButton(InteractButton, ClusterAnchorX, Sign, InteractSpec);
	PlaceClusterButton(AbsorbButton, ClusterAnchorX, Sign, AbsorbSpec);
	PlaceClusterButton(MorphButton, ClusterAnchorX, Sign, MorphSpec);
	PlaceClusterButton(LockOnButton, ClusterAnchorX, Sign, LockSpec);
}

void USlimeTouchHUDWidget::PlaceStickVisuals()
{
	if (UCanvasPanelSlot* BaseSlot = StickBase ? Cast<UCanvasPanelSlot>(StickBase->Slot) : nullptr)
	{
		BaseSlot->SetPosition(StickCenterLocal);
	}
	if (UCanvasPanelSlot* KnobSlot = StickKnob ? Cast<UCanvasPanelSlot>(StickKnob->Slot) : nullptr)
	{
		KnobSlot->SetPosition(StickCenterLocal);
	}
}

void USlimeTouchHUDWidget::SetButtonVisible(UButton* Button, bool bVisible)
{
	UWidget* Target = Button ? Button->GetParent() : nullptr;
	if (!Target)
	{
		Target = Button;
	}
	if (Target)
	{
		Target->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void USlimeTouchHUDWidget::RefreshContextualButtons()
{
	APlayerController* PC = GetOwningPlayer();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	bool bHasInteract = false;
	bool bHasMorph = false;
	if (Pawn)
	{
		if (const USlimeInteractComponent* Interact = Pawn->FindComponentByClass<USlimeInteractComponent>())
		{
			bHasInteract = !Interact->GetFocusedPromptVerb().IsEmpty();
		}
		if (const USlimeMorphComponent* Morph = Pawn->FindComponentByClass<USlimeMorphComponent>())
		{
			bHasMorph = Morph->IsMorphed();
		}
		if (const USlimeDevourComponent* Devour = Pawn->FindComponentByClass<USlimeDevourComponent>())
		{
			bHasMorph = bHasMorph || Devour->GetPhantomSlotCount() > 0;
		}
	}
	SetButtonVisible(InteractButton, bHasInteract);
	SetButtonVisible(AbsorbButton, true);
	SetButtonVisible(MorphButton, bHasMorph);
	SetButtonVisible(LockOnButton, true);
	SetButtonVisible(Skill1Button, false);
	SetButtonVisible(Skill2Button, false);
	SetButtonVisible(Skill3Button, false);
}

void USlimeTouchHUDWidget::ApplyClusterLayout()
{
	ApplyHandedness();
}

void USlimeTouchHUDWidget::PlaceActionButton(UButton* Button, float AnchorX, FVector2D Pos, FVector2D Size)
{
	const float Sign = Pos.X >= 0.f ? 1.f : -1.f;
	FTouchClusterSpec Spec;
	Spec.Size = Size;
	Spec.Pos = Pos;
	PlaceClusterButton(Button, AnchorX, Sign, Spec);
}

UButton* USlimeTouchHUDWidget::FindActionButtonAt(FVector2D ScreenPos) const
{
	UButton* Buttons[] = {
		AttackButton, JumpButton, DodgeButton, FlattenButton, SprintButton, LaunchButton,
		InteractButton, AbsorbButton, MorphButton, LockOnButton,
		PauseButton, QuestButton, InventoryButton
	};
	for (UButton* Button : Buttons)
	{
		if (!Button)
		{
			continue;
		}
		UWidget* Target = Button->GetParent() ? Button->GetParent() : Cast<UWidget>(Button);
		if (IsScreenOverWidget(Target, ScreenPos, 8.f))
		{
			return Button;
		}
	}
	return nullptr;
}

ESlimeInputAction USlimeTouchHUDWidget::ActionForButton(const UButton* Button) const
{
	if (Button == AttackButton) return ESlimeInputAction::Attack;
	if (Button == JumpButton) return ESlimeInputAction::Jump;
	if (Button == DodgeButton) return ESlimeInputAction::Dodge;
	if (Button == FlattenButton) return ESlimeInputAction::Flatten;
	if (Button == SprintButton) return ESlimeInputAction::Sprint;
	if (Button == LaunchButton) return ESlimeInputAction::Launch;
	if (Button == InteractButton) return ESlimeInputAction::Interact;
	if (Button == AbsorbButton) return ESlimeInputAction::Absorb;
	if (Button == MorphButton) return ESlimeInputAction::Morph;
	if (Button == LockOnButton) return ESlimeInputAction::LockOn;
	if (Button == InventoryButton) return ESlimeInputAction::Inventory;
	if (Button == QuestButton) return ESlimeInputAction::QuestLog;
	return ESlimeInputAction::Attack;
}

bool USlimeTouchHUDWidget::TryBeginVirtualAction(FVector2D ScreenPos, int32 PointerIndex)
{
	UButton* Hit = FindActionButtonAt(ScreenPos);
	if (!Hit || Hit == PauseButton || Hit == QuestButton || Hit == InventoryButton)
	{
		return false;
	}
	HeldPointerAction = ActionForButton(Hit);
	ActionPointer = PointerIndex;
	SetVirtualAction(HeldPointerAction, true);
	RefreshTouchPointerBusy();
	return true;
}

bool USlimeTouchHUDWidget::IsOverInteractiveControl(FVector2D ScreenPos) const
{
	UButton* Buttons[] = {
		PauseButton, QuestButton, InventoryButton,
		AttackButton, JumpButton, DodgeButton, FlattenButton, SprintButton, LaunchButton,
		InteractButton, AbsorbButton, MorphButton, LockOnButton
	};
	for (UButton* Button : Buttons)
	{
		if (!Button)
		{
			continue;
		}
		UWidget* Target = Button->GetParent() ? Button->GetParent() : Cast<UWidget>(Button);
		if (!Target || Target->GetVisibility() == ESlateVisibility::Collapsed)
		{
			continue;
		}
		if (IsScreenOverWidget(Target, ScreenPos, 8.f))
		{
			return true;
		}
	}
	return false;
}

bool USlimeTouchHUDWidget::HandlePointerDown(const FGeometry& InGeometry, FVector2D ScreenPos, int32 PointerIndex)
{
	if (TryBeginVirtualAction(ScreenPos, PointerIndex))
	{
		return false;
	}
	if (IsOverInteractiveControl(ScreenPos))
	{
		return false;
	}

	const FVector2D Local = InGeometry.AbsoluteToLocal(ScreenPos);
	const FVector2D Size = InGeometry.GetLocalSize();
	if (Size.X < 1.f || Size.Y < 1.f)
	{
		return false;
	}

	const USlimeInputSettings* Settings = GetInputSettings();
	const bool bLeft = Settings && Settings->GetTouchHandedness() == ESlimeTouchHandedness::Left;
	const bool bMoveOnLeft = !bLeft;
	const bool bInMoveHalf = bMoveOnLeft ? (Local.X < Size.X * 0.45f) : (Local.X > Size.X * 0.55f);
	const bool bInLookHalf = bMoveOnLeft ? (Local.X > Size.X * 0.55f) : (Local.X < Size.X * 0.45f);

	if (bInMoveHalf)
	{
		StickPointer = PointerIndex;
		StickCenterLocal = FVector2D(
			bLeft ? Local.X - Size.X : Local.X,
			Local.Y - Size.Y);
		PlaceStickVisuals();
		RefreshTouchPointerBusy();
		HandlePointerMove(InGeometry, ScreenPos, PointerIndex, FVector2D::ZeroVector);
		return true;
	}

	if (bInLookHalf)
	{
		if (LookPointer != PointerIndex)
		{
			LookPointer = PointerIndex;
			LastLookScreen = ScreenPos;
			RefreshTouchPointerBusy();
		}
		return true;
	}
	return false;
}

void USlimeTouchHUDWidget::RefreshTouchPointerBusy()
{
	if (USlimeInputSettings* Settings = GetInputSettings())
	{
		const bool bActionBlocksMouseAttack =
			ActionPointer != INDEX_NONE && HeldPointerAction != ESlimeInputAction::Attack;
		Settings->SetTouchPointerBusy(
			StickPointer != INDEX_NONE || LookPointer != INDEX_NONE || bActionBlocksMouseAttack);
	}
}

bool USlimeTouchHUDWidget::HandlePointerMove(const FGeometry& InGeometry, FVector2D ScreenPos, int32 PointerIndex, FVector2D CursorDelta)
{
	USlimeInputSettings* Settings = GetInputSettings();
	if (!Settings || !Settings->ShouldUseTouchHud())
	{
		return false;
	}

	if (PointerIndex == StickPointer)
	{
		const FVector2D Local = InGeometry.AbsoluteToLocal(ScreenPos);
		const FVector2D Size = InGeometry.GetLocalSize();
		const bool bLeft = Settings->GetTouchHandedness() == ESlimeTouchHandedness::Left;
		const FVector2D StickCenter(
			bLeft ? Size.X + StickCenterLocal.X : StickCenterLocal.X,
			Size.Y + StickCenterLocal.Y);
		FVector2D Offset = Local - StickCenter;
		if (Offset.Size() > StickRadius)
		{
			Offset = Offset.GetSafeNormal() * StickRadius;
		}
		Settings->SetVirtualMoveAxis(FVector2D(Offset.X / StickRadius, -Offset.Y / StickRadius));
		if (UCanvasPanelSlot* KnobSlot = StickKnob ? Cast<UCanvasPanelSlot>(StickKnob->Slot) : nullptr)
		{
			KnobSlot->SetPosition(StickCenterLocal + Offset);
		}
		return true;
	}

	if (PointerIndex == LookPointer)
	{
		FVector2D Delta = CursorDelta;
		if (Delta.IsNearlyZero())
		{
			Delta = ScreenPos - LastLookScreen;
		}
		LastLookScreen = ScreenPos;
		if (Delta.Size() < 2.f)
		{
			return true;
		}
		Delta.X = FMath::Clamp(Delta.X, -40.f, 40.f);
		Delta.Y = FMath::Clamp(Delta.Y, -40.f, 40.f);
		const FVector2D ViewSize = InGeometry.GetLocalSize();
		const float ShortSide = FMath::Max(1.f, FMath::Min(ViewSize.X, ViewSize.Y));
		const float Scale = 0.06f * (720.f / ShortSide);
		Settings->AddVirtualLookDelta(FVector2D(Delta.X * Scale, -Delta.Y * Scale));
		return true;
	}
	return false;
}

void USlimeTouchHUDWidget::HandlePointerUp(int32 PointerIndex)
{
	if (PointerIndex == ActionPointer)
	{
		SetVirtualAction(HeldPointerAction, false);
		ActionPointer = INDEX_NONE;
	}
	if (PointerIndex == StickPointer)
	{
		StickPointer = INDEX_NONE;
		StickCenterLocal = HomeStickLocal;
		if (USlimeInputSettings* Settings = GetInputSettings())
		{
			Settings->SetVirtualMoveAxis(FVector2D::ZeroVector);
		}
		PlaceStickVisuals();
	}
	if (PointerIndex == LookPointer)
	{
		LookPointer = INDEX_NONE;
	}
	RefreshTouchPointerBusy();
}

FReply USlimeTouchHUDWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (HandlePointerDown(InGeometry, InMouseEvent.GetScreenSpacePosition(), 0))
	{
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return FReply::Unhandled();
}

FReply USlimeTouchHUDWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (HandlePointerDown(InGeometry, InMouseEvent.GetScreenSpacePosition(), 0))
	{
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USlimeTouchHUDWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (HandlePointerMove(InGeometry, InMouseEvent.GetScreenSpacePosition(), 0, InMouseEvent.GetCursorDelta()))
	{
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply USlimeTouchHUDWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	HandlePointerUp(0);
	if (HasMouseCapture())
	{
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USlimeTouchHUDWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	if (HasMouseCapture() || LookPointer == 0 || StickPointer == 0)
	{
		return FReply::Unhandled();
	}
	if (HandlePointerDown(InGeometry, InGestureEvent.GetScreenSpacePosition(), InGestureEvent.GetPointerIndex()))
	{
		return FReply::Handled();
	}
	return Super::NativeOnTouchStarted(InGeometry, InGestureEvent);
}

FReply USlimeTouchHUDWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	// FakeMouseWithTouches already drives mouse move; don't add a second look path.
	if (HasMouseCapture() || LookPointer == 0 || StickPointer == 0)
	{
		return FReply::Unhandled();
	}
	if (HandlePointerMove(InGeometry, InGestureEvent.GetScreenSpacePosition(), InGestureEvent.GetPointerIndex(), InGestureEvent.GetCursorDelta()))
	{
		return FReply::Handled();
	}
	return Super::NativeOnTouchMoved(InGeometry, InGestureEvent);
}

FReply USlimeTouchHUDWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	HandlePointerUp(InGestureEvent.GetPointerIndex());
	return Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
}

void USlimeTouchHUDWidget::OnPauseClicked()
{
	if (ASlimeFablePlayerController* PC = Cast<ASlimeFablePlayerController>(GetOwningPlayer()))
	{
		PC->RequestTogglePauseMenu();
	}
}

void USlimeTouchHUDWidget::OnQuestClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UQuestSubsystem* Quests = GI->GetSubsystem<UQuestSubsystem>())
		{
			Quests->ToggleQuestLog();
		}
	}
}

void USlimeTouchHUDWidget::OnInventoryClicked()
{
	APlayerController* PC = GetOwningPlayer();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (USlimeInteractComponent* Interact = Pawn ? Pawn->FindComponentByClass<USlimeInteractComponent>() : nullptr)
	{
		Interact->ToggleInventory();
	}
}
