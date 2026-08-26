// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/TutorialMenuWidget.h"
#include "UI/MenuUIStyle.h"
#include "SlimeFablePlayerController.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"

namespace TutorialCopy
{
	static FString MakeBody()
	{
		return TEXT(
			"【基础操作】\n"
			"· WASD 移动，空格跳跃（可二段跳）\n"
			"· 左键：普通攻击连招（Combo 1→2→3→4 终结技）\n"
			"· Q / E / R：当前元素的三个技能\n"
			"· 右键：威胁区内翻滚；完美时机可完美闪避（短暂无敌+残影）\n"
			"· 安全区右键：闪现位移\n"
			"· 中键：锁定敌人\n"
			"· 1–6：按「属性编队」顺序切换六属性（跨关卡/死亡会记住当前属性）\n"
			"· L：打开属性编队（可改顺序；按键可在自定义中改绑）\n"
			"· Tab（按住）：快捷栏轮盘，滚轮选格，松手后选「使用 / 丢弃」\n"
			"· B 背包，J 任务史书，Esc 暂停\n"
			"\n"
			"【元素与形态】\n"
			"六种元素：水 / 风 / 火 / 雷 / 暗 / 物理。\n"
			"切换元素会改变技能与外观；普通连招也会带上当前元素。\n"
			"当前属性与编队顺序会全局保存，换关或重生不会回到默认水。\n"
			"L 打开编队后可拖动排序；最上为按键 1。\n"
			"\n"
			"【元素附着与反应】\n"
			"命中会把状态挂在敌人身上约 8 秒（身体持续闪该属性色，头顶闪「风蚀 8s」这类倒计时）。\n"
			"风蚀减攻击、磁暴增伤、潮湿拖慢出手、灼烧持续掉血、虚弱减速、湮灭让史莱姆吸血。\n"
			"若敌人已有另一种元素，再打上新元素就会触发「元素反应」：飘字显示反应名，并立刻清掉原状态。\n"
			"再攻击会按史莱姆当前属性挂上新状态。\n"
			"例：水+火→蒸发；火+雷→超载；物理打已有附着→破势。无附着时物理会挂虚弱。\n"
			"同元素重复附着一般只刷新持续时间，不会立刻反应。\n"
			"\n"
			"【战斗提示】\n"
			"连招第 4 下是终结技，伤害与表现更强。\n"
			"锁定后技能更容易对准目标；完美闪避窗口在敌人出手瞬间。\n"
			"受击会打断当前动作，注意翻滚时机。\n"
			"\n"
			"【探索】\n"
			"主菜单可选「今日关卡」或日历选关。\n"
			"日关卡大厅有传送门进入不同周目章节。\n"
			"按键可在「自定义按键」中修改。");
	}
}

TSharedRef<SWidget> UTutorialMenuWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void UTutorialMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyLook();
	if (BackButton)
	{
		BackButton->OnClicked.AddUniqueDynamic(this, &UTutorialMenuWidget::OnBackClicked);
	}
	if (BodyText)
	{
		BodyText->SetText(FText::FromString(TutorialCopy::MakeBody()));
	}
}

void UTutorialMenuWidget::SetReturnTarget(UUserWidget* InTarget)
{
	ReturnTarget = InTarget;
}

void UTutorialMenuWidget::BuildLayoutIfNeeded()
{
	if (TitleText && BodyScroll && BodyText && BackButton)
	{
		bBuiltInCode = false;
		return;
	}

	bBuiltInCode = true;
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = Root;

	BackgroundImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BackgroundImage"));
	if (UCanvasPanelSlot* BgSlot = Root->AddChildToCanvas(BackgroundImage))
	{
		BgSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BgSlot->SetOffsets(FMargin(0.f));
	}

	DimOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DimOverlay"));
	if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(DimOverlay))
	{
		DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimSlot->SetOffsets(FMargin(0.f));
	}

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelSize"));
	PanelSize->SetWidthOverride(640.f);
	PanelSize->SetHeightOverride(720.f);
	if (UCanvasPanelSlot* SizeSlot = Root->AddChildToCanvas(PanelSize))
	{
		SizeSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		SizeSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		SizeSlot->SetAutoSize(true);
	}

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuBox"));
	PanelSize->AddChild(VBox);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("操作与机制教程")));
	if (UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
	}

	BodyScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BodyScroll"));
	if (UVerticalBoxSlot* ScrollSlot = VBox->AddChildToVerticalBox(BodyScroll))
	{
		ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ScrollSlot->SetPadding(FMargin(8.f, 0.f, 8.f, 12.f));
	}

	BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BodyText"));
	BodyText->SetText(FText::FromString(TutorialCopy::MakeBody()));
	BodyText->SetAutoWrapText(true);
	if (UScrollBoxSlot* BodySlot = Cast<UScrollBoxSlot>(BodyScroll->AddChild(BodyText)))
	{
		BodySlot->SetPadding(FMargin(4.f));
	}

	USizeBox* BackSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BackButton_Size"));
	BackSize->SetWidthOverride(360.f);
	BackSize->SetHeightOverride(52.f);
	if (UVerticalBoxSlot* BackSlot = VBox->AddChildToVerticalBox(BackSize))
	{
		BackSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		BackSlot->SetHorizontalAlignment(HAlign_Center);
	}
	BackButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BackButton"));
	BackSize->AddChild(BackButton);
	UTextBlock* BackLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BackButton_Label"));
	BackLabel->SetText(FText::FromString(TEXT("返回")));
	BackLabel->SetJustification(ETextJustify::Center);
	BackButton->AddChild(BackLabel);
}

void UTutorialMenuWidget::ApplyLook()
{
	FMenuUIStyle::ApplyMenuBackground(BackgroundImage);
	if (DimOverlay)
	{
		FSlateBrush DimBrush;
		DimBrush.DrawAs = ESlateBrushDrawType::Box;
		DimBrush.TintColor = FSlateColor(FLinearColor(0.04f, 0.03f, 0.02f, 0.55f));
		DimBrush.Margin = FMargin(0.f);
		DimBrush.ImageSize = FVector2D(32.f, 32.f);
		DimOverlay->SetBrush(DimBrush);
	}

	FMenuUIStyle::ApplyBrushCJKFont(TitleText, 34.f, FMenuUIStyle::WarmTitleColor());
	FMenuUIStyle::ApplyBrushCJKFont(BodyText, 18.f, FMenuUIStyle::WarmTextColor());

	UMaterialInterface* BrushBtn = FMenuUIStyle::LoadButtonMaterial();
	FMenuUIStyle::ApplyMaterialButtonStyle(BackButton, BrushBtn, FVector2D(360.f, 52.f));
	if (BackButton && BackButton->GetChildrenCount() > 0)
	{
		if (UTextBlock* Label = Cast<UTextBlock>(BackButton->GetChildAt(0)))
		{
			FMenuUIStyle::ApplyBrushCJKFont(Label, 22.f, FMenuUIStyle::WarmTextColor());
		}
	}
}

void UTutorialMenuWidget::OnBackClicked()
{
	SetVisibility(ESlateVisibility::Collapsed);
	if (UUserWidget* Target = ReturnTarget.Get())
	{
		Target->SetVisibility(ESlateVisibility::Visible);
		if (ASlimeFablePlayerController* PC = Cast<ASlimeFablePlayerController>(GetOwningPlayer()))
		{
			PC->RetargetUIFocus(Target);
		}
	}
}
