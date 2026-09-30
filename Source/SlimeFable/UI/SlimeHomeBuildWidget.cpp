// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeHomeBuildWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Hub/HomeBuild/SlimeBuildModeComponent.h"
#include "Hub/HomeBuild/SlimeHomeBuildCatalog.h"
#include "Hub/NPC/SlimeNpcCollectionSubsystem.h"
#include "Hub/NPC/SlimeNpcCatalog.h"
#include "Hub/HomeBuild/SlimeHomeBuildSubsystem.h"
#include "Inventory/SlimeInventorySubsystem.h"
#include "Inventory/SlimeItemDefinition.h"
#include "UI/MenuUIStyle.h"

namespace
{
	bool bCreateAsHotbar = false;
}

void USlimeHomeBuildWidget::SetCreateMode(bool bHotbar)
{
	bCreateAsHotbar = bHotbar;
}

void USlimeHomeBuildSlotProxy::HandleClick()
{
	if (USlimeHomeBuildWidget* Widget = Owner.Get())
	{
		Widget->ShowDetail(Index, false);
	}
}

ASlimeHomeBuildPreview::ASlimeHomeBuildPreview()
{
	PrimaryActorTick.bCanEverTick = true;
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(MeshComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetCastShadow(false);

	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(MeshComp);
	Capture->bCaptureEveryFrame = true;
	Capture->bCaptureOnMovement = true;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowOnlyActors.Add(this);
}

void ASlimeHomeBuildPreview::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AddActorWorldRotation(FRotator(0.f, 40.f * DeltaSeconds, 0.f));
}

void ASlimeHomeBuildPreview::ShowMesh(UStaticMesh* Mesh)
{
	if (!MeshComp || !Capture)
	{
		return;
	}
	MeshComp->EmptyOverrideMaterials();
	MeshComp->SetStaticMesh(Mesh);
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowOnlyActors.Reset();
	Capture->ShowOnlyActors.Add(this);
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetVolumetricFog(false);
	if (!Target)
	{
		Target = NewObject<UTextureRenderTarget2D>(this);
		Target->InitAutoFormat(256, 256);
		Target->ClearColor = FLinearColor(0.12f, 0.09f, 0.06f, 1.f);
		Capture->TextureTarget = Target;
	}
	const FBoxSphereBounds Local = Mesh ? Mesh->GetBounds() : FBoxSphereBounds(FVector::ZeroVector, FVector(25.f), 50.f);
	const FVector Center = Local.Origin;
	const float Dist = FMath::Max(Local.SphereRadius, 10.f) * 2.4f;
	const FVector CamLocal = Center + FVector(Dist, -Dist, Dist * 0.45f);
	Capture->SetRelativeLocation(CamLocal);
	Capture->SetRelativeRotation((Center - CamLocal).Rotation());
}

TSharedRef<SWidget> USlimeHomeBuildWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		bHotbar = bCreateAsHotbar;
		bCreateAsHotbar = false;
		if (bHotbar)
		{
			BuildHotbarLayout();
		}
		else
		{
			BuildCatalogLayout();
		}
	}
	return Super::RebuildWidget();
}

void USlimeHomeBuildWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	ApplyFonts();
	if (!bHotbar)
	{
		RefreshCatalog();
	}
}

void USlimeHomeBuildWidget::NativeDestruct()
{
	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}
	Super::NativeDestruct();
}

FReply USlimeHomeBuildWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (!bHotbar && (Key == EKeys::F1 || Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right))
	{
		if (USlimeBuildModeComponent* Mode = Build.Get())
		{
			Mode->RequestExit(Key == EKeys::F1 ? TEXT("f1") : TEXT("escape"));
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void USlimeHomeBuildWidget::OpenCatalog(USlimeBuildModeComponent* InBuild)
{
	Build = InBuild;
	bHotbar = false;
}

void USlimeHomeBuildWidget::OpenHotbar(USlimeBuildModeComponent* InBuild, const TArray<FName>& Ids, const TArray<bool>& FromBag, int32 ActiveIndex, bool bClearMode)
{
	Build = InBuild;
	bHotbar = true;
	Rows.Reset();
	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	USlimeHomeBuildSubsystem* Home = World ? World->GetSubsystem<USlimeHomeBuildSubsystem>() : nullptr;
	USlimeInventorySubsystem* Inv = GI ? GI->GetSubsystem<USlimeInventorySubsystem>() : nullptr;
	const USlimeHomeBuildCatalog* Catalog = Home ? Home->GetCatalog() : nullptr;
	for (int32 Index = 0; Index < Ids.Num(); ++Index)
	{
		FRow Row;
		Row.Id = Ids[Index];
		Row.bFromBag = FromBag.IsValidIndex(Index) && FromBag[Index];
		if (Row.bFromBag && Inv)
		{
			if (const USlimeItemDefinition* Def = Inv->FindDefinition(Row.Id))
			{
				Row.Name = Def->DisplayName;
				Row.Icon = Def->Icon;
			}
		}
		else if (Catalog)
		{
			if (const FSlimeHomeBuildEntry* Entry = Catalog->FindEntry(Row.Id))
			{
				Row.Name = Entry->DisplayName;
				Row.Icon = Entry->Icon;
				Row.FamilyIcon = Entry->FamilyIcon;
			}
		}
		Rows.Add(Row);
	}
	if (Grid)
	{
		Grid->ClearChildren();
		Proxies.Reset();
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			FButtonStyle ClearStyle;
			FSlateBrush Empty;
			Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
			ClearStyle.SetNormal(Empty);
			ClearStyle.SetHovered(Empty);
			ClearStyle.SetPressed(Empty);
			Button->SetStyle(ClearStyle);
			UVerticalBox* Cell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			Button->AddChild(Cell);
			UTexture2D* IconTex = Rows[Index].Icon.LoadSynchronous();
			if (!IconTex)
			{
				IconTex = Rows[Index].FamilyIcon.LoadSynchronous();
			}
			if (IconTex)
			{
				UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				Icon->SetBrush(FMenuUIStyle::MakeTextureBrush(IconTex, FVector2D(48.f, 48.f)));
				Cell->AddChildToVerticalBox(Icon);
			}
			UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Label->SetText(FText::FromString(FString::Printf(TEXT("%d %s"), Index + 1, *Rows[Index].Name.ToString())));
			FMenuUIStyle::ApplyBrushCJKFont(Label, 12.f, Index == ActiveIndex
				? FMenuUIStyle::TodayEdgeColor()
				: FMenuUIStyle::WarmTextColor());
			Cell->AddChildToVerticalBox(Label);
			Grid->AddChildToUniformGrid(Button, 0, Index);
		}
	}
	if (ModeHint)
	{
		ModeHint->SetText(FText::FromString(bClearMode
			? TEXT("清除模式　高度调整　中键切换")
			: TEXT("建造模式　高度调整　中键切换")));
		ModeHint->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (DetailDesc)
	{
		DetailDesc->SetText(FText::FromString(bClearMode
			? TEXT("清除模式：左键拆掉   R 旋转   X 放置   F1 退出")
			: TEXT("左键放置   R 旋转   X 清除   1-9 切换   F1 退出")));
	}
}

void USlimeHomeBuildWidget::OnSearchChanged(const FText& Text)
{
	Search = Text.ToString();
	SelectedIndex = 0;
	RefreshCatalog();
}

void USlimeHomeBuildWidget::OnBuildClicked()
{
	if (!Rows.IsValidIndex(SelectedIndex))
	{
		return;
	}
	if (USlimeBuildModeComponent* Mode = Build.Get())
	{
		const FRow& Row = Rows[SelectedIndex];
		Mode->NotifyCatalogChosen(Row.Id, Row.bFromBag);
	}
}

void USlimeHomeBuildWidget::OnClearClicked()
{
	if (USlimeBuildModeComponent* Mode = Build.Get())
	{
		Mode->NotifyCatalogClear();
	}
}

void USlimeHomeBuildWidget::OnCloseClicked()
{
	if (USlimeBuildModeComponent* Mode = Build.Get())
	{
		Mode->RequestExit(TEXT("button"));
	}
}

void USlimeHomeBuildWidget::SetStatusLine(const FText& Text)
{
	if (DetailDesc)
	{
		DetailDesc->SetText(Text);
	}
}

void USlimeHomeBuildWidget::SetModeHint(const FText& Text)
{
	if (!ModeHint)
	{
		return;
	}
	ModeHint->SetText(Text);
	ModeHint->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USlimeHomeBuildWidget::ShowModeToast(const FText& Text)
{
	if (!ModeToast)
	{
		return;
	}
	ModeToast->SetText(Text);
	ModeToast->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USlimeHomeBuildWidget::HideModeToast()
{
	if (ModeToast)
	{
		ModeToast->SetVisibility(ESlateVisibility::Collapsed);
	}
}

namespace HomeBuildUI
{
	constexpr float CellSize = 120.f;
	constexpr int32 Columns = 6;
	constexpr int32 VisibleRows = 4;

	void StyleSlot(UBorder* Border, bool bSelected)
	{
		if (!Border)
		{
			return;
		}
		const FLinearColor Fill = bSelected
			? FLinearColor(0.28f, 0.22f, 0.14f, 0.95f)
			: FLinearColor(0.1f, 0.085f, 0.07f, 0.88f);
		const FLinearColor Edge = bSelected
			? FMenuUIStyle::TodayEdgeColor()
			: FLinearColor(0.72f, 0.64f, 0.46f, 0.4f);
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

	void StyleActionButton(UButton* Button, FVector2D Size)
	{
		FMenuUIStyle::ApplyMaterialButtonStyle(Button, FMenuUIStyle::LoadButtonMaterial(), Size);
		if (Button)
		{
			if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
			{
				FMenuUIStyle::ApplyBrushCJKFont(Label, 18.f, FMenuUIStyle::WarmTextColor());
			}
			FMenuUIStyle::BindInkButtonHover(Button, Cast<UTextBlock>(Button->GetContent()));
		}
	}
}

void USlimeHomeBuildWidget::BuildCatalogLayout()
{
	using namespace HomeBuildUI;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UImage* DimOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DimOverlay"));
	if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(DimOverlay))
	{
		DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimSlot->SetOffsets(FMargin(0.f));
	}
	FSlateBrush DimBrush;
	DimBrush.DrawAs = ESlateBrushDrawType::Box;
	DimBrush.TintColor = FSlateColor(FLinearColor(0.04f, 0.03f, 0.02f, 0.55f));
	DimBrush.ImageSize = FVector2D(32.f, 32.f);
	DimOverlay->SetBrush(DimBrush);

	UBorder* PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetPadding(FMargin(28.f, 22.f));
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(PanelBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
	}
	FSlateBrush PanelBrush;
	PanelBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	PanelBrush.TintColor = FSlateColor(FLinearColor(0.05f, 0.045f, 0.035f, 0.88f));
	PanelBrush.OutlineSettings.CornerRadii = FVector4(16.f, 16.f, 16.f, 16.f);
	PanelBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	PanelBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.72f, 0.64f, 0.46f, 0.45f));
	PanelBrush.OutlineSettings.Width = 1.6f;
	PanelBorder->SetBrush(PanelBrush);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	PanelBorder->AddChild(Column);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	TitleText->SetText(FText::FromString(TEXT("家园建造")));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	SearchBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Search"));
	SearchBox->SetHintText(FText::FromString(TEXT("搜索")));
	SearchBox->OnTextChanged.AddDynamic(this, &USlimeHomeBuildWidget::OnSearchChanged);
	if (UVerticalBoxSlot* SearchSlot = Column->AddChildToVerticalBox(SearchBox))
	{
		SearchSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}

	UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Tabs"));
	if (UVerticalBoxSlot* TabsSlot = Column->AddChildToVerticalBox(Tabs))
	{
		TabsSlot->SetHorizontalAlignment(HAlign_Center);
		TabsSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	}
	const TArray<TPair<ESlimeHomeBuildCategory, FString>> TabDefs = {
		{ESlimeHomeBuildCategory::Block, TEXT("方块")},
		{ESlimeHomeBuildCategory::Ground, TEXT("地表")},
		{ESlimeHomeBuildCategory::Fluid, TEXT("流体")},
		{ESlimeHomeBuildCategory::Item, TEXT("物品")},
		{ESlimeHomeBuildCategory::Plant, TEXT("植物")},
		{ESlimeHomeBuildCategory::Rock, TEXT("岩石")},
		{ESlimeHomeBuildCategory::Crystal, TEXT("水晶")},
		{ESlimeHomeBuildCategory::Sky, TEXT("天空")},
		{ESlimeHomeBuildCategory::Bag, TEXT("背包")},
		{ESlimeHomeBuildCategory::Farm, TEXT("农田")},
		{ESlimeHomeBuildCategory::Fence, TEXT("栅栏")},
		{ESlimeHomeBuildCategory::NPC, TEXT("NPC")}
	};
	TabCategories.Reset();
	for (int32 Index = 0; Index < TabDefs.Num(); ++Index)
	{
		TabCategories.Add(TabDefs[Index].Key);
		USlimeHomeBuildSlotProxy* Proxy = NewObject<USlimeHomeBuildSlotProxy>(this);
		Proxy->Index = 1000 + static_cast<int32>(TabDefs[Index].Key);
		Proxy->Owner = this;
		Proxies.Add(Proxy);
		UButton* Tab = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(TabDefs[Index].Value));
		Tab->AddChild(Text);
		Tab->OnClicked.AddDynamic(Proxy, &USlimeHomeBuildSlotProxy::HandleClick);
		if (UHorizontalBoxSlot* TabPad = Tabs->AddChildToHorizontalBox(Tab))
		{
			TabPad->SetPadding(FMargin(4.f, 0.f));
		}
		CategoryButtons.Add(Tab);
	}

	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Body"));
	Column->AddChildToVerticalBox(Body);

	USizeBox* GridFrame = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("GridFrame"));
	GridFrame->SetWidthOverride(Columns * (CellSize + 12.f));
	GridFrame->SetHeightOverride(VisibleRows * (CellSize + 12.f));
	if (UHorizontalBoxSlot* FrameSlot = Body->AddChildToHorizontalBox(GridFrame))
	{
		FrameSlot->SetPadding(FMargin(0.f, 0.f, 18.f, 0.f));
		FrameSlot->SetVerticalAlignment(VAlign_Top);
	}
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Scroll"));
	Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	GridFrame->AddChild(Scroll);
	Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("Grid"));
	Grid->SetSlotPadding(FMargin(6.f));
	Scroll->AddChild(Grid);

	UVerticalBox* DetailCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DetailCol"));
	if (UHorizontalBoxSlot* DetailSlot = Body->AddChildToHorizontalBox(DetailCol))
	{
		DetailSlot->SetVerticalAlignment(VAlign_Top);
	}
	USizeBox* PreviewBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PreviewBox"));
	PreviewBox->SetWidthOverride(220.f);
	PreviewBox->SetHeightOverride(220.f);
	if (UVerticalBoxSlot* PreviewSlot = DetailCol->AddChildToVerticalBox(PreviewBox))
	{
		PreviewSlot->SetHorizontalAlignment(HAlign_Center);
		PreviewSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}
	PreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Preview"));
	PreviewBox->AddChild(PreviewImage);

	DetailName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DetailName"));
	DetailName->SetText(FText::FromString(TEXT("选择物品")));
	DetailCol->AddChildToVerticalBox(DetailName);

	DetailFoot = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DetailFoot"));
	if (UVerticalBoxSlot* FootSlot = DetailCol->AddChildToVerticalBox(DetailFoot))
	{
		FootSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}

	DetailDesc = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DetailDesc"));
	DetailDesc->SetAutoWrapText(true);
	DetailDesc->SetWrapTextAt(320.f);
	DetailDesc->SetMinDesiredWidth(280.f);
	if (UVerticalBoxSlot* DescSlot = DetailCol->AddChildToVerticalBox(DetailDesc))
	{
		DescSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}

	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Actions"));
	if (UVerticalBoxSlot* ActionSlot = Column->AddChildToVerticalBox(Actions))
	{
		ActionSlot->SetHorizontalAlignment(HAlign_Center);
		ActionSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
	}
	auto AddBtn = [this, Actions](const TCHAR* Label, UButton*& Out, const FName& Name)
	{
		Out = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Label));
		Out->AddChild(Text);
		if (UHorizontalBoxSlot* BtnSlot = Actions->AddChildToHorizontalBox(Out))
		{
			BtnSlot->SetPadding(FMargin(8.f, 0.f));
		}
	};
	UButton* BuildBtn = nullptr;
	UButton* ClearBtn = nullptr;
	UButton* CloseBtn = nullptr;
	AddBtn(TEXT("开始建造"), BuildBtn, TEXT("BuildBtn"));
	BuildButton = BuildBtn;
	AddBtn(TEXT("清除模式"), ClearBtn, TEXT("ClearBtn"));
	AddBtn(TEXT("退出"), CloseBtn, TEXT("CloseBtn"));
	BuildBtn->OnClicked.AddDynamic(this, &USlimeHomeBuildWidget::OnBuildClicked);
	ClearBtn->OnClicked.AddDynamic(this, &USlimeHomeBuildWidget::OnClearClicked);
	CloseBtn->OnClicked.AddDynamic(this, &USlimeHomeBuildWidget::OnCloseClicked);
	for (UButton* Tab : CategoryButtons)
	{
		StyleActionButton(Tab, FVector2D(88.f, 40.f));
	}
	StyleActionButton(BuildBtn, FVector2D(160.f, 48.f));
	StyleActionButton(ClearBtn, FVector2D(160.f, 48.f));
	StyleActionButton(CloseBtn, FVector2D(160.f, 48.f));
}

void USlimeHomeBuildWidget::BuildHotbarLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;
	ModeHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ModeHint"));
	ModeHint->SetText(FText::FromString(TEXT("建造模式　高度调整　中键切换")));
	FMenuUIStyle::ApplyMixedMenuFont(ModeHint, 22.f, FLinearColor(0.95f, 0.92f, 0.82f, 0.95f));
	if (UCanvasPanelSlot* HintSlot = Root->AddChildToCanvas(ModeHint))
	{
		HintSlot->SetAnchors(FAnchors(0.f, 0.f));
		HintSlot->SetAlignment(FVector2D(0.f, 0.f));
		HintSlot->SetPosition(FVector2D(24.f, 24.f));
		HintSlot->SetAutoSize(true);
	}
	ModeToast = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ModeToast"));
	ModeToast->SetText(FText::GetEmpty());
	ModeToast->SetVisibility(ESlateVisibility::Collapsed);
	FMenuUIStyle::ApplyMixedMenuFont(ModeToast, 28.f, FLinearColor(0.97f, 0.94f, 0.84f, 1.f));
	if (UCanvasPanelSlot* ToastSlot = Root->AddChildToCanvas(ModeToast))
	{
		ToastSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		ToastSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ToastSlot->SetPosition(FVector2D(0.f, -72.f));
		ToastSlot->SetAutoSize(true);
	}
	UTextBlock* Crosshair = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Crosshair"));
	Crosshair->SetText(FText::FromString(TEXT("+")));
	FMenuUIStyle::ApplyMarkerFont(Crosshair, 28.f, FLinearColor(0.95f, 0.92f, 0.82f, 0.9f));
	if (UCanvasPanelSlot* CrossSlot = Root->AddChildToCanvas(Crosshair))
	{
		CrossSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CrossSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CrossSlot->SetAutoSize(true);
	}
	USizeBox* Panel = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Bar"));
	Panel->SetHeightOverride(72.f);
	if (UCanvasPanelSlot* BarSlot = Root->AddChildToCanvas(Panel))
	{
		BarSlot->SetAnchors(FAnchors(0.5f, 1.f));
		BarSlot->SetAlignment(FVector2D(0.5f, 1.f));
		BarSlot->SetPosition(FVector2D(0.f, -154.f));
		BarSlot->SetAutoSize(true);
	}
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->AddChild(Column);
	Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	DetailDesc = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Column->AddChildToVerticalBox(Grid);
	Column->AddChildToVerticalBox(DetailDesc);
}

void USlimeHomeBuildWidget::RefreshCatalog()
{
	if (!Grid)
	{
		return;
	}
	Rows.Reset();
	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	USlimeHomeBuildSubsystem* Home = World ? World->GetSubsystem<USlimeHomeBuildSubsystem>() : nullptr;
	const USlimeHomeBuildCatalog* Catalog = Home ? Home->GetCatalog() : nullptr;
	USlimeInventorySubsystem* Inv = GI ? GI->GetSubsystem<USlimeInventorySubsystem>() : nullptr;

	if (Category == ESlimeHomeBuildCategory::Bag && Inv)
	{
		const TArray<FSlimeInventoryEntry> Bag = Inv->GetEntriesByCategory(ESlimeItemCategory::Placeable);
		for (const FSlimeInventoryEntry& BagEntry : Bag)
		{
			const USlimeItemDefinition* Def = Inv->FindDefinition(BagEntry.ItemId);
			if (!Def || BagEntry.Count <= 0)
			{
				continue;
			}
			if (!Search.IsEmpty() && !Def->DisplayName.ToString().Contains(Search))
			{
				continue;
			}
			FRow Row;
			Row.Id = BagEntry.ItemId;
			Row.bFromBag = true;
			Row.Name = Def->DisplayName;
			Row.Description = Def->Description;
			Row.Icon = Def->Icon;
			Row.Count = BagEntry.Count;
			if (const USlimePlaceableDefinition* Placeable = Cast<USlimePlaceableDefinition>(Def))
			{
				Row.Mesh = Placeable->PreviewMesh;
				if (UStaticMesh* Mesh = Placeable->PreviewMesh.LoadSynchronous())
				{
					const FBox Box = Mesh->GetBoundingBox();
					const FVector Size = (Box.Max - Box.Min) * Placeable->PlacedMeshScale;
					Row.FootX = FMath::Max(FMath::RoundToInt(Size.X / 50.f), 1);
					Row.FootY = FMath::Max(FMath::RoundToInt(Size.Y / 50.f), 1);
				}
			}
			Rows.Add(Row);
		}
	}
	else if (Catalog)
	{
		for (const FSlimeHomeBuildEntry& Entry : Catalog->Entries)
		{
			if (!Entry.NpcSpeciesId.IsNone())
   {
    auto* Collection = GI ? GI->GetSubsystem<USlimeNpcCollectionSubsystem>() : nullptr;
    const USlimeNpcCatalog* NpcCatalog = Collection ? Collection->GetCatalog() : nullptr;
    const FSlimeNpcSpecies* Species = NpcCatalog ? NpcCatalog->Find(Entry.NpcSpeciesId) : nullptr;
    if (!Collection || !Collection->IsUnlocked(Entry.NpcSpeciesId) || !Species || Species->Layers.IsEmpty()) continue;
   }
   if (Entry.Category != Category)
			{
				continue;
			}
			if (Entry.Category == ESlimeHomeBuildCategory::Fluid && Entry.EntryId != TEXT("Kub_FluidPool"))
			{
				continue;
			}
			if (!Search.IsEmpty() && !Entry.DisplayName.ToString().Contains(Search))
			{
				continue;
			}
			FRow Row;
			Row.Id = Entry.EntryId;
			Row.Name = Entry.DisplayName;
			Row.Description = Entry.Description;
			Row.Variant = Entry.VariantIndex;
			Row.FootX = FMath::Max(Entry.FootprintX, 1);
			Row.FootY = FMath::Max(Entry.FootprintY, 1);
			Row.Icon = Entry.Icon;
			Row.FamilyIcon = Entry.FamilyIcon;
			Row.Mesh = Entry.Mesh;
   Row.NpcSpeciesId = Entry.NpcSpeciesId;
   if (!Row.NpcSpeciesId.IsNone())
   {
    auto* Collection = GI->GetSubsystem<USlimeNpcCollectionSubsystem>();
    const auto* Species = Collection->GetCatalog()->Find(Row.NpcSpeciesId);
    Row.NpcLimit = Species && !Species->Layers.IsEmpty() ? Species->Limit() : 0;
    Row.Count = Home->CountNpc(Row.NpcSpeciesId);
    Row.Name = FText::FromString(FString::Printf(TEXT("%s %d/%d"), *Entry.DisplayName.ToString(), Row.Count, Row.NpcLimit));
   }
			Rows.Add(Row);
		}
	}

	for (int32 Index = 0; Index < CategoryButtons.Num(); ++Index)
	{
		UButton* Tab = CategoryButtons[Index];
		UTextBlock* Label = Tab ? Cast<UTextBlock>(Tab->GetChildAt(0)) : nullptr;
		const bool bActive = Label && TabCategories.IsValidIndex(Index) && TabCategories[Index] == Category;
		if (Label)
		{
			FMenuUIStyle::ApplyBrushCJKFont(Label, 16.f, bActive
				? FMenuUIStyle::TodayEdgeColor()
				: FMenuUIStyle::WarmTextColor());
		}
	}

	if (!Rows.IsValidIndex(SelectedIndex))
	{
		SelectedIndex = 0;
	}

	Grid->ClearChildren();
	Proxies.RemoveAll([](const USlimeHomeBuildSlotProxy* Proxy) { return Proxy && Proxy->Index < 1000; });
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const bool bSelected = Index == SelectedIndex;
		USlimeHomeBuildSlotProxy* Proxy = NewObject<USlimeHomeBuildSlotProxy>(this);
		Proxy->Index = Index;
		Proxy->Owner = this;
		Proxies.Add(Proxy);

		USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Cell->SetWidthOverride(HomeBuildUI::CellSize);
		Cell->SetHeightOverride(HomeBuildUI::CellSize);
		if (bSelected)
		{
			Cell->SetRenderScale(FVector2D(1.06f));
		}

		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle ClearStyle;
		FSlateBrush Empty;
		Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
		ClearStyle.SetNormal(Empty);
		ClearStyle.SetHovered(Empty);
		ClearStyle.SetPressed(Empty);
		ClearStyle.SetNormalPadding(FMargin(0.f));
		ClearStyle.SetPressedPadding(FMargin(0.f));
		Button->SetStyle(ClearStyle);
		Button->SetBackgroundColor(FLinearColor::Transparent);
		Cell->AddChild(Button);

		UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Button->AddChild(Stack);

		UBorder* Chrome = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		HomeBuildUI::StyleSlot(Chrome, bSelected);
		if (UOverlaySlot* ChromeSlot = Stack->AddChildToOverlay(Chrome))
		{
			ChromeSlot->SetHorizontalAlignment(HAlign_Fill);
			ChromeSlot->SetVerticalAlignment(VAlign_Fill);
		}

		UTexture2D* IconTex = Rows[Index].Icon.LoadSynchronous();
		if (!IconTex)
		{
			IconTex = Rows[Index].FamilyIcon.LoadSynchronous();
		}
		if (IconTex)
		{
			UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Icon->SetBrush(FMenuUIStyle::MakeTextureBrush(IconTex, FVector2D(HomeBuildUI::CellSize * 0.62f)));
			Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UOverlaySlot* IconSlot = Stack->AddChildToOverlay(Icon))
			{
				IconSlot->SetHorizontalAlignment(HAlign_Center);
				IconSlot->SetVerticalAlignment(VAlign_Center);
				IconSlot->SetPadding(FMargin(10.f, 8.f, 10.f, 22.f));
			}
		}

		UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Name->SetText(Rows[Index].Name);
		Name->SetJustification(ETextJustify::Center);
		Name->SetVisibility(ESlateVisibility::HitTestInvisible);
		FMenuUIStyle::ApplyBrushCJKFont(Name, 12.f, FMenuUIStyle::WarmTextColor());
		if (UOverlaySlot* NameSlot = Stack->AddChildToOverlay(Name))
		{
			NameSlot->SetHorizontalAlignment(HAlign_Center);
			NameSlot->SetVerticalAlignment(VAlign_Bottom);
			NameSlot->SetPadding(FMargin(4.f, 0.f, 4.f, 4.f));
		}

		if (Rows[Index].Variant > 0)
		{
			UTextBlock* Variant = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Variant->SetText(FText::AsNumber(Rows[Index].Variant));
			Variant->SetVisibility(ESlateVisibility::HitTestInvisible);
			FMenuUIStyle::ApplyMarkerFont(Variant, 14.f, FMenuUIStyle::WarmTitleColor());
			if (UOverlaySlot* VariantSlot = Stack->AddChildToOverlay(Variant))
			{
				VariantSlot->SetHorizontalAlignment(HAlign_Right);
				VariantSlot->SetVerticalAlignment(VAlign_Top);
				VariantSlot->SetPadding(FMargin(0.f, 6.f, 8.f, 0.f));
			}
		}

		Button->OnClicked.AddDynamic(Proxy, &USlimeHomeBuildSlotProxy::HandleClick);
		Grid->AddChildToUniformGrid(Cell, Index / HomeBuildUI::Columns, Index % HomeBuildUI::Columns);
	}

	if (BuildButton)
	{
		BuildButton->SetIsEnabled(Rows.Num() > 0);
	}
	if (Rows.Num() > 0)
	{
		ShowDetail(SelectedIndex, false);
	}
	else if (DetailName)
	{
		DetailName->SetText(FText::FromString(Category == ESlimeHomeBuildCategory::NPC ? TEXT("击败敌人后可在这里放置NPC") : TEXT("没有物品")));
		if (DetailDesc)
		{
			DetailDesc->SetText(FText::GetEmpty());
		}
		if (DetailFoot)
		{
			DetailFoot->SetText(FText::GetEmpty());
		}
		if (PreviewImage)
		{
			PreviewImage->SetBrush(FSlateBrush());
			PreviewImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void USlimeHomeBuildWidget::ShowDetail(int32 Index, bool bCommit)
{
	if (Index >= 1000)
	{
		Category = static_cast<ESlimeHomeBuildCategory>(Index - 1000);
		SelectedIndex = 0;
		RefreshCatalog();
		return;
	}
	if (!Rows.IsValidIndex(Index))
	{
		return;
	}
	if (Index != SelectedIndex)
	{
		SelectedIndex = Index;
		RefreshCatalog();
		return;
	}
	const FRow& Row = Rows[Index];
 if (BuildButton) BuildButton->SetIsEnabled(Row.NpcSpeciesId.IsNone() || Row.Count < Row.NpcLimit);
 if (DetailName)
	{
		DetailName->SetText(Row.Name);
	}
	if (DetailFoot)
	{
		FString Foot = FString::Printf(TEXT("%d×%d 格"), Row.FootX, Row.FootY);
		if (Row.bFromBag)
		{
			Foot += FString::Printf(TEXT("    剩余 %d"), Row.Count);
		}
		if (!Row.NpcSpeciesId.IsNone()) Foot = FString::Printf(TEXT("已放置 %d / %d　闲逛范围10米"), Row.Count, Row.NpcLimit);
		DetailFoot->SetText(FText::FromString(Foot));
	}
	if (DetailDesc)
	{
		DetailDesc->SetText(Row.Description);
	}
	if (bCommit)
	{
		if (USlimeBuildModeComponent* Mode = Build.Get())
		{
			Mode->NotifyCatalogChosen(Row.Id, Row.bFromBag);
		}
	}
	if (PreviewImage)
	{
		UTexture2D* IconTex = Row.Icon.LoadSynchronous();
		if (!IconTex)
		{
			IconTex = Row.FamilyIcon.LoadSynchronous();
		}
		if (IconTex)
		{
			PreviewImage->SetBrush(FMenuUIStyle::MakeTextureBrush(IconTex, FVector2D(220.f, 220.f)));
			PreviewImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			PreviewImage->SetBrush(FSlateBrush());
			PreviewImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void USlimeHomeBuildWidget::ApplyFonts()
{
	FMenuUIStyle::ApplyBrushCJKFont(TitleText, 36.f, FMenuUIStyle::WarmTitleColor());
	FMenuUIStyle::ApplyBrushCJKFont(DetailName, 22.f, FMenuUIStyle::WarmTextColor());
	FMenuUIStyle::ApplyBrushCJKFont(DetailFoot, 16.f, FMenuUIStyle::WarmMutedTextColor());
	FMenuUIStyle::ApplyBrushCJKFont(DetailDesc, 16.f, FMenuUIStyle::WarmMutedTextColor());
	if (bHotbar)
	{
		return;
	}
	for (UButton* Tab : CategoryButtons)
	{
		HomeBuildUI::StyleActionButton(Tab, FVector2D(88.f, 40.f));
	}
	HomeBuildUI::StyleActionButton(BuildButton, FVector2D(160.f, 48.f));
	if (WidgetTree)
	{
		if (UButton* ClearBtn = Cast<UButton>(WidgetTree->FindWidget(TEXT("ClearBtn"))))
		{
			HomeBuildUI::StyleActionButton(ClearBtn, FVector2D(160.f, 48.f));
		}
		if (UButton* CloseBtn = Cast<UButton>(WidgetTree->FindWidget(TEXT("CloseBtn"))))
		{
			HomeBuildUI::StyleActionButton(CloseBtn, FVector2D(160.f, 48.f));
		}
	}
}
