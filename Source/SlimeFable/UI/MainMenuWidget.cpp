// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MainMenuWidget.h"
#include "UI/LevelSelectWidget.h"
#include "UI/KeybindSettingsWidget.h"
#include "UI/GraphicsSettingsWidget.h"
#include "UI/MenuUIStyle.h"
#include "DayLevel/DayLevelSubsystem.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/GameInstance.h"

UMainMenuWidget::UMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LevelSelectClassPath = TSoftClassPtr<ULevelSelectWidget>(
		FSoftObjectPath(TEXT("/Game/UI/WBP_LevelSelect.WBP_LevelSelect_C")));
	KeybindSettingsClassPath = TSoftClassPtr<UKeybindSettingsWidget>(
		FSoftObjectPath(TEXT("/Game/UI/WBP_KeybindSettings.WBP_KeybindSettings_C")));
	GraphicsSettingsClassPath = TSoftClassPtr<UGraphicsSettingsWidget>(
		FSoftObjectPath(TEXT("/Game/UI/WBP_GraphicsSettings.WBP_GraphicsSettings_C")));
}

TSharedRef<SWidget> UMainMenuWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveLevelSelectClass();
	ResolveKeybindClass();
	ResolveGraphicsClass();
	ApplyMaterialLabLook();

	if (PlayTodayButton)
	{
		PlayTodayButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnPlayTodayClicked);
	}
	if (SelectLevelButton)
	{
		SelectLevelButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnSelectLevelClicked);
	}
	if (KeybindButton)
	{
		KeybindButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnKeybindClicked);
	}
	if (GraphicsButton)
	{
		GraphicsButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnGraphicsClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnQuitClicked);
	}

	RefreshTodayInfo();
}

void UMainMenuWidget::ResolveLevelSelectClass()
{
	if (!LevelSelectClass && !LevelSelectClassPath.IsNull())
	{
		LevelSelectClass = LevelSelectClassPath.LoadSynchronous();
	}
}

void UMainMenuWidget::ResolveKeybindClass()
{
	if (!KeybindSettingsClass && !KeybindSettingsClassPath.IsNull())
	{
		KeybindSettingsClass = KeybindSettingsClassPath.LoadSynchronous();
	}
}

void UMainMenuWidget::ResolveGraphicsClass()
{
	if (!GraphicsSettingsClass && !GraphicsSettingsClassPath.IsNull())
	{
		GraphicsSettingsClass = GraphicsSettingsClassPath.LoadSynchronous();
	}
}

void UMainMenuWidget::BuildLayoutIfNeeded()
{
	// Require new settings buttons too; otherwise rebuild full code layout (covers older WBPs).
	if (TitleText && PlayTodayButton && SelectLevelButton && KeybindButton && GraphicsButton && QuitButton)
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

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuBox"));
	if (UCanvasPanelSlot* VBoxSlot = Root->AddChildToCanvas(VBox))
	{
		VBoxSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		VBoxSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		VBoxSlot->SetAutoSize(true);
	}

	auto AddText = [this, VBox](const FName& Name, const FText& Text) -> UTextBlock*
	{
		UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Block->SetText(Text);
		UVerticalBoxSlot* Slot = VBox->AddChildToVerticalBox(Block);
		Slot->SetPadding(FMargin(0.f, 10.f));
		Slot->SetHorizontalAlignment(HAlign_Center);
		return Block;
	};

	auto AddButton = [this, VBox](const FName& Name, const FText& Label) -> UButton*
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			*FString::Printf(TEXT("%s_Size"), *Name.ToString()));
		SizeBox->SetWidthOverride(360.f);
		SizeBox->SetHeightOverride(64.f);
		UVerticalBoxSlot* SizeSlot = VBox->AddChildToVerticalBox(SizeBox);
		SizeSlot->SetPadding(FMargin(0.f, 8.f));
		SizeSlot->SetHorizontalAlignment(HAlign_Center);

		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		SizeBox->AddChild(Button);
		UTextBlock* LabelBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			*FString::Printf(TEXT("%s_Label"), *Name.ToString()));
		LabelBlock->SetText(Label);
		LabelBlock->SetJustification(ETextJustify::Center);
		Button->AddChild(LabelBlock);
		return Button;
	};

	TitleText = AddText(TEXT("TitleText"), FText::FromString(TEXT("SlimeFable")));
	TodayText = AddText(TEXT("TodayText"), FText::GetEmpty());
	StatusText = AddText(TEXT("StatusText"), FText::GetEmpty());
	PlayTodayButton = AddButton(TEXT("PlayTodayButton"), FText::FromString(TEXT("进入今日关卡")));
	SelectLevelButton = AddButton(TEXT("SelectLevelButton"), FText::FromString(TEXT("选择关卡")));
	KeybindButton = AddButton(TEXT("KeybindButton"), FText::FromString(TEXT("自定义按键")));
	GraphicsButton = AddButton(TEXT("GraphicsButton"), FText::FromString(TEXT("画质选择")));
	QuitButton = AddButton(TEXT("QuitButton"), FText::FromString(TEXT("退出游戏")));
}

void UMainMenuWidget::ApplyMaterialLabLook()
{
	FMenuUIStyle::ApplyMenuBackground(BackgroundImage);
	if (DimOverlay)
	{
		FSlateBrush DimBrush;
		DimBrush.DrawAs = ESlateBrushDrawType::Box;
		DimBrush.TintColor = FSlateColor(FLinearColor(0.04f, 0.03f, 0.02f, 0.45f));
		DimBrush.Margin = FMargin(0.f);
		DimBrush.ImageSize = FVector2D(32.f, 32.f);
		DimOverlay->SetBrush(DimBrush);
		DimOverlay->SetColorAndOpacity(FLinearColor::White);
	}

	FMenuUIStyle::ApplyTitleFont(TitleText, 56.f, FMenuUIStyle::WarmTitleColor());
	if (TodayText)
	{
		TodayText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (StatusText)
	{
		StatusText->SetVisibility(ESlateVisibility::Collapsed);
	}

	UMaterialInterface* BrushBtn = FMenuUIStyle::LoadButtonMaterial();
	const FVector2D Size(360.f, 64.f);
	FMenuUIStyle::ApplyMaterialButtonStyle(PlayTodayButton, BrushBtn, Size);
	FMenuUIStyle::ApplyMaterialButtonStyle(SelectLevelButton, BrushBtn, Size);
	FMenuUIStyle::ApplyMaterialButtonStyle(KeybindButton, BrushBtn, Size);
	FMenuUIStyle::ApplyMaterialButtonStyle(GraphicsButton, BrushBtn, Size);
	FMenuUIStyle::ApplyMaterialButtonStyle(QuitButton, BrushBtn, Size);

	auto StyleChildLabel = [](UButton* Button, float FontSize)
	{
		if (!Button)
		{
			return;
		}
		if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
		{
			FMenuUIStyle::ApplyMixedMenuFont(Label, FontSize, FMenuUIStyle::WarmTextColor());
		}
	};
	StyleChildLabel(PlayTodayButton, 24.f);
	StyleChildLabel(SelectLevelButton, 22.f);
	StyleChildLabel(KeybindButton, 22.f);
	StyleChildLabel(GraphicsButton, 22.f);
	StyleChildLabel(QuitButton, 22.f);

	auto BindHover = [](UButton* Button)
	{
		UTextBlock* Label = Button ? Cast<UTextBlock>(Button->GetContent()) : nullptr;
		FMenuUIStyle::BindInkButtonHover(Button, Label);
	};
	BindHover(PlayTodayButton);
	BindHover(SelectLevelButton);
	BindHover(KeybindButton);
	BindHover(GraphicsButton);
	BindHover(QuitButton);
}

void UMainMenuWidget::RefreshTodayInfo()
{
	auto SetPlayLabel = [this](const FString& Label)
	{
		if (!PlayTodayButton)
		{
			return;
		}
		if (UTextBlock* Text = Cast<UTextBlock>(PlayTodayButton->GetContent()))
		{
			Text->SetText(FText::FromString(Label));
			FMenuUIStyle::ApplyMixedMenuFont(Text, 24.f, FMenuUIStyle::WarmTextColor());
		}
	};

	auto ShowStatusError = [this](const FString& Message)
	{
		if (StatusText)
		{
			StatusText->SetVisibility(ESlateVisibility::Visible);
			StatusText->SetText(FText::FromString(Message));
			FMenuUIStyle::ApplyBrushCJKFont(StatusText, 16.f, FMenuUIStyle::WarmMutedTextColor());
		}
	};

	if (TodayText)
	{
		TodayText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (StatusText)
	{
		StatusText->SetVisibility(ESlateVisibility::Collapsed);
	}

	UDayLevelSubsystem* DayLevels = GetDayLevelSubsystem();
	if (!DayLevels)
	{
		SetPlayLabel(TEXT("进入今日关卡"));
		ShowStatusError(TEXT("无法获取日关卡系统"));
		if (PlayTodayButton)
		{
			PlayTodayButton->SetIsEnabled(false);
		}
		return;
	}

	const FString DayId = DayLevels->GetTodayDayId().Id.ToString();
	SetPlayLabel(FString::Printf(TEXT("进入今日关卡（%s）"), *DayId));

	const bool bHasRegistry = DayLevels->HasRegistry();
	TSoftObjectPtr<UWorld> TodayLevel;
	const bool bHasToday = bHasRegistry && DayLevels->GetTodayLevel(TodayLevel);

	if (PlayTodayButton)
	{
		PlayTodayButton->SetIsEnabled(bHasToday);
	}
	if (SelectLevelButton)
	{
		SelectLevelButton->SetIsEnabled(bHasRegistry);
	}

	if (!bHasRegistry)
	{
		ShowStatusError(TEXT("日关卡注册表未加载"));
	}
	else if (!bHasToday)
	{
		ShowStatusError(TEXT("今日关卡条目缺失"));
	}
}

UDayLevelSubsystem* UMainMenuWidget::GetDayLevelSubsystem() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<UDayLevelSubsystem>();
	}
	return nullptr;
}

void UMainMenuWidget::OnPlayTodayClicked()
{
	if (UDayLevelSubsystem* DayLevels = GetDayLevelSubsystem())
	{
		if (!DayLevels->TravelToToday(this) && StatusText)
		{
			StatusText->SetVisibility(ESlateVisibility::Visible);
			StatusText->SetText(FText::FromString(TEXT("无法进入今日关卡")));
			FMenuUIStyle::ApplyBrushCJKFont(StatusText, 16.f, FMenuUIStyle::WarmMutedTextColor());
		}
	}
}

void UMainMenuWidget::OpenLevelSelect()
{
	ResolveLevelSelectClass();
	if (!LevelSelectWidget)
	{
		const TSubclassOf<ULevelSelectWidget> ClassToSpawn =
			LevelSelectClass ? LevelSelectClass : TSubclassOf<ULevelSelectWidget>(ULevelSelectWidget::StaticClass());
		LevelSelectWidget = CreateWidget<ULevelSelectWidget>(GetOwningPlayer(), ClassToSpawn);
		if (LevelSelectWidget)
		{
			LevelSelectWidget->SetParentMenu(this);
		}
	}

	if (LevelSelectWidget)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		if (!LevelSelectWidget->IsInViewport())
		{
			LevelSelectWidget->AddToViewport(1);
		}
		else
		{
			LevelSelectWidget->SetVisibility(ESlateVisibility::Visible);
		}
		LevelSelectWidget->JumpToTodayMonth();
		LevelSelectWidget->RefreshForCurrentMonth();
	}
}

void UMainMenuWidget::OpenKeybindSettings()
{
	ResolveKeybindClass();
	if (!KeybindSettingsWidget)
	{
		const TSubclassOf<UKeybindSettingsWidget> ClassToSpawn =
			KeybindSettingsClass
				? KeybindSettingsClass
				: TSubclassOf<UKeybindSettingsWidget>(UKeybindSettingsWidget::StaticClass());
		KeybindSettingsWidget = CreateWidget<UKeybindSettingsWidget>(GetOwningPlayer(), ClassToSpawn);
		if (KeybindSettingsWidget)
		{
			KeybindSettingsWidget->SetReturnTarget(this);
		}
	}

	if (KeybindSettingsWidget)
	{
		KeybindSettingsWidget->SetReturnTarget(this);
		SetVisibility(ESlateVisibility::Collapsed);
		if (!KeybindSettingsWidget->IsInViewport())
		{
			KeybindSettingsWidget->AddToViewport(1);
		}
		else
		{
			KeybindSettingsWidget->SetVisibility(ESlateVisibility::Visible);
		}
		KeybindSettingsWidget->RefreshList();
	}
}

void UMainMenuWidget::OpenGraphicsSettings()
{
	ResolveGraphicsClass();
	if (!GraphicsSettingsWidget)
	{
		const TSubclassOf<UGraphicsSettingsWidget> ClassToSpawn =
			GraphicsSettingsClass
				? GraphicsSettingsClass
				: TSubclassOf<UGraphicsSettingsWidget>(UGraphicsSettingsWidget::StaticClass());
		GraphicsSettingsWidget = CreateWidget<UGraphicsSettingsWidget>(GetOwningPlayer(), ClassToSpawn);
		if (GraphicsSettingsWidget)
		{
			GraphicsSettingsWidget->SetReturnTarget(this);
		}
	}

	if (GraphicsSettingsWidget)
	{
		GraphicsSettingsWidget->SetReturnTarget(this);
		SetVisibility(ESlateVisibility::Collapsed);
		if (!GraphicsSettingsWidget->IsInViewport())
		{
			GraphicsSettingsWidget->AddToViewport(1);
		}
		else
		{
			GraphicsSettingsWidget->SetVisibility(ESlateVisibility::Visible);
		}
		GraphicsSettingsWidget->RefreshSelection();
	}
}

void UMainMenuWidget::OnSelectLevelClicked()
{
	OpenLevelSelect();
}

void UMainMenuWidget::OnKeybindClicked()
{
	OpenKeybindSettings();
}

void UMainMenuWidget::OnGraphicsClicked()
{
	OpenGraphicsSettings();
}

void UMainMenuWidget::OnQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
