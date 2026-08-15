// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeSouvenirViewerWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class USizeBox;
class USlimeSouvenirDefinition;
class UMediaPlayer;
class UMediaTexture;
class UFileMediaSource;
class UTextureRenderTarget2D;
class ASlimeSouvenirPreviewActor;

UCLASS()
class SLIMEFABLE_API USlimeSouvenirViewerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USlimeSouvenirViewerWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	void SetSouvenir(USlimeSouvenirDefinition* InSouvenir);

protected:
	void BuildLayoutIfNeeded();
	void ApplyLook();
	void StopVideo();
	void BeginMeshPreview();
	void EndMeshPreview();
	void FitMediaToBoxes(UImage* Image, USizeBox* Box, float AspectRatio, float MaxWidth = 720.f, float MaxHeight = 480.f);
	bool TryApplyVideoAspect();
	UTexture2D* ResolveStoryTexture() const;

	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void OnPlayClicked();
	UFUNCTION() void OnMediaOpened(FString OpenedUrl);

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> DimOverlay;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USizeBox> StoryImageBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> StoryImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USizeBox> VideoImageBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> VideoImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StoryText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> PlayButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CloseButton;

	UPROPERTY(Transient)
	TObjectPtr<USlimeSouvenirDefinition> Souvenir;

	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> MediaPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UMediaTexture> MediaTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PreviewRT;

	UPROPERTY(Transient)
	TObjectPtr<ASlimeSouvenirPreviewActor> PreviewActor;

	bool bBuiltInCode = false;
	bool bAwaitingVideoAspect = false;
	float VideoAspectRetrySeconds = 0.f;
	bool bDraggingPreview = false;
	bool bShowingMesh = false;
};
