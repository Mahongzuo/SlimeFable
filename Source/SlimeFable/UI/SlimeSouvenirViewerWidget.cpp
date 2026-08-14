// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeSouvenirViewerWidget.h"

#include "UI/MenuUIStyle.h"
#include "Inventory/SlimeItemDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
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
	Super::NativeDestruct();
}

void USlimeSouvenirViewerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
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

void USlimeSouvenirViewerWidget::BuildLayoutIfNeeded()
{
	if (TitleText && CloseButton && StoryImageBox && VideoImageBox)
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
	// Keep existing ResourceObject — only sync ImageSize so Fit never blanks the brush.
	FSlateBrush Brush = Image->GetBrush();
	if (Brush.GetResourceObject())
	{
		Brush.ImageSize = Size;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Image->SetBrush(Brush);
	}
}

void USlimeSouvenirViewerWidget::ApplyLook()
{
	if (DimOverlay)
	{
		FSlateBrush DimBrush;
		DimBrush.DrawAs = ESlateBrushDrawType::Box;
		DimBrush.TintColor = FSlateColor(FLinearColor(0.03f, 0.025f, 0.02f, 0.7f));
		DimBrush.ImageSize = FVector2D(32.f, 32.f);
		DimOverlay->SetBrush(DimBrush);
	}
	// Chinese title must use KuaiLe — Marker has no CJK and shows "字" placeholders.
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
		// Layout is built lazily in RebuildWidget; force it so callers may
		// invoke SetSouvenir before AddToViewport without losing the data.
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
	if (StoryImage && StoryImageBox)
	{
		if (UTexture2D* Tex = ResolveStoryTexture())
		{
			const int32 W = FMath::Max(Tex->GetSizeX(), 1);
			const int32 H = FMath::Max(Tex->GetSizeY(), 1);
			const float Aspect = static_cast<float>(W) / static_cast<float>(H);
			StoryImage->SetBrushFromTexture(Tex, true);
			FitMediaToBoxes(StoryImage, StoryImageBox, Aspect);
			// Fit may touch ImageSize — reassert texture resource explicitly.
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

void USlimeSouvenirViewerWidget::OnCloseClicked()
{
	StopVideo();
	if (APlayerController* PC = GetOwningPlayer())
	{
		UGameplayStatics::SetGamePaused(PC, false);
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
	RemoveFromParent();
}
