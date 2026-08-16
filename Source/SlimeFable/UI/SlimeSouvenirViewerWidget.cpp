// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeSouvenirViewerWidget.h"

#include "UI/MenuUIStyle.h"
#include "Inventory/SlimeItemDefinition.h"
#include "Inventory/SlimeSouvenirPreviewActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "FileMediaSource.h"
#include "Materials/MaterialInterface.h"
#include "InputCoreTypes.h"
#include "SlimeFable.h"
#include "Kismet/GameplayStatics.h"

namespace SlimeSouvenirPaths
{
	static const TCHAR* PostcardTexture = TEXT("/Game/Movies/T_ZzmxPostcard.T_ZzmxPostcard");
	static constexpr float PreviewViewDepth = 80.f;
}

USlimeSouvenirViewerWidget::USlimeSouvenirViewerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

TSharedRef<SWidget> USlimeSouvenirViewerWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void USlimeSouvenirViewerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureDimBars();
	ApplyLook();
	if (PlayButton) PlayButton->OnClicked.AddUniqueDynamic(this, &USlimeSouvenirViewerWidget::OnPlayClicked);
	if (CloseButton) CloseButton->OnClicked.AddUniqueDynamic(this, &USlimeSouvenirViewerWidget::OnCloseClicked);

	if (!MediaPlayer)
	{
		MediaPlayer = NewObject<UMediaPlayer>(this);
		MediaTexture = NewObject<UMediaTexture>(this);
		if (MediaPlayer && MediaTexture)
		{
			MediaTexture->SetMediaPlayer(MediaPlayer);
			MediaTexture->UpdateResource();
			MediaPlayer->OnMediaOpened.AddDynamic(this, &USlimeSouvenirViewerWidget::OnMediaOpened);
		}
	}
}

void USlimeSouvenirViewerWidget::NativeDestruct()
{
	if (MediaPlayer)
	{
		MediaPlayer->OnMediaOpened.RemoveDynamic(this, &USlimeSouvenirViewerWidget::OnMediaOpened);
	}
	StopVideo();
	EndMeshPreview();
	Super::NativeDestruct();
}

void USlimeSouvenirViewerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bShowingMesh)
	{
		UpdatePreviewWindow();
	}
	if (!bAwaitingVideoAspect)
	{
		return;
	}
	VideoAspectRetrySeconds -= InDeltaTime;
	if (TryApplyVideoAspect() || VideoAspectRetrySeconds <= 0.f)
	{
		bAwaitingVideoAspect = false;
	}
}

FReply USlimeSouvenirViewerWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnCloseClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool USlimeSouvenirViewerWidget::IsPointerOverPreviewBox(const FPointerEvent& InMouseEvent) const
{
	if (!StoryImageBox || StoryImageBox->GetVisibility() == ESlateVisibility::Collapsed)
	{
		return false;
	}
	return StoryImageBox->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition());
}

FReply USlimeSouvenirViewerWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bShowingMesh && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
		&& IsPointerOverPreviewBox(InMouseEvent))
	{
		bDraggingPreview = true;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USlimeSouvenirViewerWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bDraggingPreview)
	{
		bDraggingPreview = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USlimeSouvenirViewerWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDraggingPreview && PreviewActor)
	{
		const FVector2D Delta = InMouseEvent.GetCursorDelta();
		PreviewActor->AddOrbit(Delta.X * 0.45f, -Delta.Y * 0.35f);
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply USlimeSouvenirViewerWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bShowingMesh && PreviewActor && IsPointerOverPreviewBox(InMouseEvent))
	{
		PreviewActor->AddZoom(InMouseEvent.GetWheelDelta());
		return FReply::Handled();
	}
	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void USlimeSouvenirViewerWidget::BuildLayoutIfNeeded()
{
	if (TitleText && CloseButton && StoryImageBox && VideoImageBox)
	{
		bBuiltInCode = false;
		EnsureDimBars();
		return;
	}
	bBuiltInCode = true;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	auto AddDim = [this, Root](const FName& Name) -> UImage*
	{
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		if (UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Image))
		{
			Slot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
			Slot->SetAlignment(FVector2D(0.f, 0.f));
			Slot->SetAutoSize(false);
		}
		Image->SetVisibility(ESlateVisibility::Collapsed);
		return Image;
	};
	DimTop = AddDim(TEXT("DimTop"));
	DimBottom = AddDim(TEXT("DimBottom"));
	DimLeft = AddDim(TEXT("DimLeft"));
	DimRight = AddDim(TEXT("DimRight"));

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

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	Panel->AddChildToVerticalBox(TitleText);

	StoryImageBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("StoryImageBox"));
	StoryImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("StoryImage"));
	StoryImageBox->AddChild(StoryImage);
	if (UVerticalBoxSlot* ImgSlot = Panel->AddChildToVerticalBox(StoryImageBox))
	{
		ImgSlot->SetPadding(FMargin(0.f, 8.f));
		ImgSlot->SetHorizontalAlignment(HAlign_Center);
	}

	VideoImageBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("VideoImageBox"));
	VideoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("VideoImage"));
	VideoImageBox->AddChild(VideoImage);
	if (UVerticalBoxSlot* VidSlot = Panel->AddChildToVerticalBox(VideoImageBox))
	{
		VidSlot->SetPadding(FMargin(0.f, 8.f));
		VidSlot->SetHorizontalAlignment(HAlign_Center);
	}
	VideoImageBox->SetVisibility(ESlateVisibility::Collapsed);

	StoryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StoryText"));
	StoryText->SetAutoWrapText(true);
	StoryText->SetWrapTextAt(520.f);
	StoryText->SetMinDesiredWidth(400.f);
	Panel->AddChildToVerticalBox(StoryText);

	PlayButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PlayButton"));
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlayLabel"));
		Label->SetText(FText::FromString(TEXT("播放故事视频")));
		PlayButton->AddChild(Label);
		Panel->AddChildToVerticalBox(PlayButton);
	}

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseLabel"));
		Label->SetText(FText::FromString(TEXT("关闭")));
		CloseButton->AddChild(Label);
		Panel->AddChildToVerticalBox(CloseButton);
	}
}

void USlimeSouvenirViewerWidget::EnsureDimBars()
{
	if ((DimTop && DimBottom && DimLeft && DimRight) || !WidgetTree)
	{
		return;
	}

	UCanvasPanel* Root = Cast<UCanvasPanel>(GetRootWidget());
	if (!Root)
	{
		Root = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	}
	if (!Root)
	{
		return;
	}

	auto AddDim = [this, Root](TObjectPtr<UImage>& Target, const FName& Name)
	{
		if (Target)
		{
			return;
		}
		Target = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Root->InsertChildAt(0, Target)))
		{
			CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
			CanvasSlot->SetAlignment(FVector2D(0.f, 0.f));
			CanvasSlot->SetAutoSize(false);
		}
		Target->SetVisibility(ESlateVisibility::Collapsed);
	};
	AddDim(DimTop, TEXT("DimTop"));
	AddDim(DimBottom, TEXT("DimBottom"));
	AddDim(DimLeft, TEXT("DimLeft"));
	AddDim(DimRight, TEXT("DimRight"));
}

void USlimeSouvenirViewerWidget::FitMediaToBoxes(
	UImage* Image, USizeBox* Box, float AspectRatio, float MaxWidth, float MaxHeight)
{
	if (!Image || AspectRatio <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	float Width = MaxWidth;
	float Height = Width / AspectRatio;
	if (Height > MaxHeight)
	{
		Height = MaxHeight;
		Width = Height * AspectRatio;
	}
	const FVector2D Size(Width, Height);
	Image->SetDesiredSizeOverride(Size);
	if (Box)
	{
		Box->SetWidthOverride(Width);
		Box->SetHeightOverride(Height);
	}
	FSlateBrush Brush = Image->GetBrush();
	if (Brush.GetResourceObject())
	{
		Brush.ImageSize = Size;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Image->SetBrush(Brush);
	}
}

void USlimeSouvenirViewerWidget::ApplyDimBrush(UImage* Image)
{
	if (!Image)
	{
		return;
	}
	FSlateBrush DimBrush;
	DimBrush.DrawAs = ESlateBrushDrawType::Box;
	DimBrush.TintColor = FSlateColor(FLinearColor(0.03f, 0.025f, 0.02f, 0.7f));
	DimBrush.ImageSize = FVector2D(32.f, 32.f);
	Image->SetBrush(DimBrush);
}

void USlimeSouvenirViewerWidget::ApplyLook()
{
	ApplyDimBrush(DimOverlay);
	ApplyDimBrush(DimTop);
	ApplyDimBrush(DimBottom);
	ApplyDimBrush(DimLeft);
	ApplyDimBrush(DimRight);

	FMenuUIStyle::ApplyBrushCJKFont(TitleText, 32.f, FMenuUIStyle::WarmTitleColor());
	FMenuUIStyle::ApplyBrushCJKFont(StoryText, 18.f, FMenuUIStyle::WarmTextColor());

	UMaterialInterface* BrushBtn = FMenuUIStyle::LoadButtonMaterial();
	FMenuUIStyle::ApplyMaterialButtonStyle(PlayButton, BrushBtn, FVector2D(280.f, 52.f));
	FMenuUIStyle::ApplyMaterialButtonStyle(CloseButton, BrushBtn, FVector2D(220.f, 48.f));
	FMenuUIStyle::BindInkButtonHover(PlayButton, Cast<UTextBlock>(PlayButton ? PlayButton->GetContent() : nullptr));
	FMenuUIStyle::BindInkButtonHover(CloseButton, Cast<UTextBlock>(CloseButton ? CloseButton->GetContent() : nullptr));
	if (PlayButton)
	{
		if (UTextBlock* Label = Cast<UTextBlock>(PlayButton->GetContent()))
		{
			FMenuUIStyle::ApplyBrushCJKFont(Label, 18.f, FMenuUIStyle::WarmTextColor());
		}
	}
	if (CloseButton)
	{
		if (UTextBlock* Label = Cast<UTextBlock>(CloseButton->GetContent()))
		{
			FMenuUIStyle::ApplyBrushCJKFont(Label, 18.f, FMenuUIStyle::WarmTextColor());
		}
	}
}

UTexture2D* USlimeSouvenirViewerWidget::ResolveStoryTexture() const
{
	if (Souvenir)
	{
		if (UTexture2D* Tex = Souvenir->StoryImage.LoadSynchronous())
		{
			return Tex;
		}
		if (UTexture2D* Icon = Souvenir->Icon.LoadSynchronous())
		{
			return Icon;
		}
	}
	return LoadObject<UTexture2D>(nullptr, SlimeSouvenirPaths::PostcardTexture);
}

void USlimeSouvenirViewerWidget::SetSouvenir(USlimeSouvenirDefinition* InSouvenir)
{
	Souvenir = InSouvenir;
	bAwaitingVideoAspect = false;
	if (!Souvenir)
	{
		return;
	}
	if (!StoryImage)
	{
		TakeWidget();
	}
	if (TitleText)
	{
		TitleText->SetText(Souvenir->DisplayName);
	}
	if (StoryText)
	{
		StoryText->SetText(Souvenir->StoryText);
	}
	EndMeshPreview();
	bShowingMesh = Souvenir && !Souvenir->StoryMesh.IsNull();
	if (bShowingMesh)
	{
		BeginMeshPreview();
	}
	else if (StoryImage && StoryImageBox)
	{
		SetDimBarsVisible(false);
		if (UTexture2D* Tex = ResolveStoryTexture())
		{
			const int32 W = FMath::Max(Tex->GetSizeX(), 1);
			const int32 H = FMath::Max(Tex->GetSizeY(), 1);
			const float Aspect = static_cast<float>(W) / static_cast<float>(H);
			StoryImage->SetBrushFromTexture(Tex, true);
			FitMediaToBoxes(StoryImage, StoryImageBox, Aspect);
			{
				FSlateBrush Brush = StoryImage->GetBrush();
				Brush.SetResourceObject(Tex);
				Brush.DrawAs = ESlateBrushDrawType::Image;
				if (StoryImageBox)
				{
					Brush.ImageSize = FVector2D(
						StoryImageBox->GetWidthOverride(),
						StoryImageBox->GetHeightOverride());
				}
				StoryImage->SetBrush(Brush);
			}
			StoryImageBox->SetVisibility(ESlateVisibility::Visible);
			StoryImage->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			UE_LOG(LogSlimeFable, Warning,
				TEXT("Souvenir %s: failed to load /Game/Movies/T_ZzmxPostcard"),
				*Souvenir->ItemId.ToString());
			StoryImageBox->SetVisibility(ESlateVisibility::Collapsed);
			StoryImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (VideoImageBox)
	{
		VideoImageBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	const bool bHasVideo = !Souvenir->StoryVideo.IsNull();
	if (PlayButton)
	{
		PlayButton->SetVisibility(bHasVideo ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void USlimeSouvenirViewerWidget::OnPlayClicked()
{
	if (!Souvenir || !MediaPlayer || !MediaTexture)
	{
		return;
	}
	UFileMediaSource* Source = Souvenir->StoryVideo.LoadSynchronous();
	if (!Source)
	{
		UE_LOG(LogSlimeFable, Warning,
			TEXT("Souvenir %s: StoryVideo failed to load; assign a FileMediaSource under /Game/Movies"),
			*Souvenir->ItemId.ToString());
		return;
	}
	MediaPlayer->OpenSource(Source);
	MediaPlayer->Play();
	if (VideoImage && VideoImageBox)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(MediaTexture);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		VideoImage->SetBrush(Brush);
		FitMediaToBoxes(VideoImage, VideoImageBox, 16.f / 9.f);
		VideoImageBox->SetVisibility(ESlateVisibility::Visible);
		VideoImage->SetVisibility(ESlateVisibility::Visible);
	}
	if (StoryImageBox)
	{
		StoryImageBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	EndMeshPreview();
	SetDimBarsVisible(false);
	bAwaitingVideoAspect = true;
	VideoAspectRetrySeconds = 2.5f;
	TryApplyVideoAspect();
}

bool USlimeSouvenirViewerWidget::TryApplyVideoAspect()
{
	if (!MediaPlayer || !VideoImage || !VideoImageBox)
	{
		return false;
	}
	const int32 TrackIndex = MediaPlayer->GetSelectedTrack(EMediaPlayerTrack::Video);
	FIntPoint Dim(0, 0);
	if (TrackIndex != INDEX_NONE)
	{
		Dim = MediaPlayer->GetVideoTrackDimensions(TrackIndex, 0);
	}
	if (Dim.X <= 0 || Dim.Y <= 0)
	{
		return false;
	}
	const float Aspect = static_cast<float>(Dim.X) / static_cast<float>(Dim.Y);
	FSlateBrush Brush = VideoImage->GetBrush();
	Brush.SetResourceObject(MediaTexture);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	VideoImage->SetBrush(Brush);
	FitMediaToBoxes(VideoImage, VideoImageBox, Aspect);
	return true;
}

void USlimeSouvenirViewerWidget::OnMediaOpened(FString OpenedUrl)
{
	if (TryApplyVideoAspect())
	{
		bAwaitingVideoAspect = false;
	}
	else
	{
		bAwaitingVideoAspect = true;
		VideoAspectRetrySeconds = 2.5f;
	}
}

void USlimeSouvenirViewerWidget::StopVideo()
{
	bAwaitingVideoAspect = false;
	if (MediaPlayer)
	{
		MediaPlayer->Close();
	}
}

void USlimeSouvenirViewerWidget::SetDimBarsVisible(bool bVisible)
{
	const ESlateVisibility BarVis = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (DimTop) DimTop->SetVisibility(BarVis);
	if (DimBottom) DimBottom->SetVisibility(BarVis);
	if (DimLeft) DimLeft->SetVisibility(BarVis);
	if (DimRight) DimRight->SetVisibility(BarVis);
	if (DimOverlay)
	{
		DimOverlay->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void USlimeSouvenirViewerWidget::BeginMeshPreview()
{
	if (!Souvenir || !StoryImage || !StoryImageBox)
	{
		return;
	}
	UStaticMesh* Mesh = Souvenir->StoryMesh.LoadSynchronous();
	UWorld* World = GetWorld();
	if (!Mesh || !World)
	{
		bShowingMesh = false;
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	FVector SpawnLoc = FVector::ZeroVector;
	if (PC && PC->PlayerCameraManager)
	{
		SpawnLoc = PC->PlayerCameraManager->GetCameraLocation()
			+ PC->PlayerCameraManager->GetCameraRotation().Vector() * SlimeSouvenirPaths::PreviewViewDepth;
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PreviewActor = World->SpawnActor<ASlimeSouvenirPreviewActor>(
		ASlimeSouvenirPreviewActor::StaticClass(),
		SpawnLoc,
		FRotator::ZeroRotator,
		Params);
	if (PreviewActor)
	{
		PreviewActor->SetTickableWhenPaused(true);
		PreviewActor->SetupPreview(Mesh);
	}

	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
	Brush.ImageSize = FVector2D(720.f, 480.f);
	StoryImage->SetBrush(Brush);
	StoryImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f));
	FitMediaToBoxes(StoryImage, StoryImageBox, 720.f / 480.f);
	StoryImageBox->SetWidthOverride(720.f);
	StoryImageBox->SetHeightOverride(480.f);
	StoryImageBox->SetVisibility(ESlateVisibility::Visible);
	StoryImage->SetVisibility(ESlateVisibility::Visible);
	SetDimBarsVisible(true);
	UpdatePreviewWindow();
}

void USlimeSouvenirViewerWidget::EndMeshPreview()
{
	bDraggingPreview = false;
	bShowingMesh = false;
	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}
	if (StoryImage)
	{
		StoryImage->SetColorAndOpacity(FLinearColor::White);
	}
}

void USlimeSouvenirViewerWidget::UpdatePreviewWindow()
{
	UpdateDimHole();
	UpdateMeshFit();
}

void USlimeSouvenirViewerWidget::UpdateDimHole()
{
	if (!StoryImageBox || !DimTop || !DimBottom || !DimLeft || !DimRight)
	{
		return;
	}

	const FGeometry& RootGeo = GetCachedGeometry();
	const FGeometry& BoxGeo = StoryImageBox->GetCachedGeometry();
	const FVector2D RootSize = RootGeo.GetLocalSize();
	const FVector2D BoxSize = BoxGeo.GetLocalSize();
	if (RootSize.X < 1.f || RootSize.Y < 1.f || BoxSize.X < 1.f || BoxSize.Y < 1.f)
	{
		return;
	}

	const FVector2D BoxTL = RootGeo.AbsoluteToLocal(BoxGeo.LocalToAbsolute(FVector2D::ZeroVector));
	const float Left = FMath::Clamp(BoxTL.X, 0.f, RootSize.X);
	const float Top = FMath::Clamp(BoxTL.Y, 0.f, RootSize.Y);
	const float Right = FMath::Clamp(BoxTL.X + BoxSize.X, 0.f, RootSize.X);
	const float Bottom = FMath::Clamp(BoxTL.Y + BoxSize.Y, 0.f, RootSize.Y);

	auto PlaceBar = [](UImage* Image, float X, float Y, float W, float H)
	{
		if (!Image)
		{
			return;
		}
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Image->Slot))
		{
			Slot->SetAnchors(FAnchors(0.f, 0.f));
			Slot->SetAlignment(FVector2D(0.f, 0.f));
			Slot->SetOffsets(FMargin(X, Y, W, H));
		}
	};

	PlaceBar(DimTop, 0.f, 0.f, RootSize.X, Top);
	PlaceBar(DimBottom, 0.f, Bottom, RootSize.X, RootSize.Y - Bottom);
	PlaceBar(DimLeft, 0.f, Top, Left, Bottom - Top);
	PlaceBar(DimRight, Right, Top, RootSize.X - Right, Bottom - Top);
}

void USlimeSouvenirViewerWidget::UpdateMeshFit()
{
	if (!PreviewActor || !StoryImageBox)
	{
		return;
	}
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	const FGeometry& BoxGeo = StoryImageBox->GetCachedGeometry();
	const FVector2D BoxSize = BoxGeo.GetLocalSize();
	if (BoxSize.X < 1.f || BoxSize.Y < 1.f)
	{
		return;
	}

	FVector2D PixelTL, PixelBR, ViewportUnused;
	USlateBlueprintLibrary::LocalToViewport(this, BoxGeo, FVector2D::ZeroVector, PixelTL, ViewportUnused);
	USlateBlueprintLibrary::LocalToViewport(this, BoxGeo, BoxSize, PixelBR, ViewportUnused);

	const FVector2D PixelCenter = (PixelTL + PixelBR) * 0.5f;
	const float MidY = (PixelTL.Y + PixelBR.Y) * 0.5f;
	const float MidX = (PixelTL.X + PixelBR.X) * 0.5f;

	auto ProjectAtDepth = [PC](float ScreenX, float ScreenY) -> FVector
	{
		FVector World, Dir;
		if (!PC->DeprojectScreenPositionToWorld(ScreenX, ScreenY, World, Dir))
		{
			return FVector::ZeroVector;
		}
		return World + Dir * SlimeSouvenirPaths::PreviewViewDepth;
	};

	const FVector Center = ProjectAtDepth(PixelCenter.X, PixelCenter.Y);
	const FVector Left = ProjectAtDepth(PixelTL.X, MidY);
	const FVector Right = ProjectAtDepth(PixelBR.X, MidY);
	const FVector Top = ProjectAtDepth(MidX, PixelTL.Y);
	const FVector Bottom = ProjectAtDepth(MidX, PixelBR.Y);
	const float Width = FVector::Dist(Left, Right);
	const float Height = FVector::Dist(Top, Bottom);
	if (Width < 1.f || Height < 1.f)
	{
		return;
	}

	FRotator ViewRot = FRotator::ZeroRotator;
	if (PC->PlayerCameraManager)
	{
		ViewRot = PC->PlayerCameraManager->GetCameraRotation();
	}
	PreviewActor->FitToWorldRect(Center, ViewRot, Width, Height);
}

void USlimeSouvenirViewerWidget::OnCloseClicked()
{
	StopVideo();
	EndMeshPreview();
	if (APlayerController* PC = GetOwningPlayer())
	{
		UGameplayStatics::SetGamePaused(PC, false);
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
	RemoveFromParent();
}
