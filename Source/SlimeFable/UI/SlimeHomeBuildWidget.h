// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "Hub/HomeBuild/SlimeHomeBuildTypes.h"
#include "SlimeHomeBuildWidget.generated.h"

class UButton;
class UEditableTextBox;
class UImage;
class UScrollBox;
class UTextBlock;
class UUniformGridPanel;
class USlimeBuildModeComponent;
class UStaticMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

UCLASS()
class SLIMEFABLE_API ASlimeHomeBuildPreview : public AActor
{
	GENERATED_BODY()

public:
	ASlimeHomeBuildPreview();
	virtual void Tick(float DeltaSeconds) override;
	void ShowMesh(UStaticMesh* Mesh);
	UTextureRenderTarget2D* GetTarget() const { return Target; }

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> Target;
};

UCLASS()
class SLIMEFABLE_API USlimeHomeBuildSlotProxy : public UObject
{
	GENERATED_BODY()

public:
	int32 Index = INDEX_NONE;
	TWeakObjectPtr<USlimeHomeBuildWidget> Owner;

	UFUNCTION()
	void HandleClick();
};

UCLASS()
class SLIMEFABLE_API USlimeHomeBuildWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	static void SetCreateMode(bool bHotbar);

	void OpenCatalog(USlimeBuildModeComponent* InBuild);
	void OpenHotbar(USlimeBuildModeComponent* InBuild, const TArray<FName>& Ids, const TArray<bool>& FromBag, int32 ActiveIndex, bool bClearMode);
	void SetStatusLine(const FText& Text);
	void SetModeHint(const FText& Text);
	void ShowModeToast(const FText& Text);
	void HideModeToast();

protected:
	UFUNCTION() void OnSearchChanged(const FText& Text);
	UFUNCTION() void OnBuildClicked();
	UFUNCTION() void OnClearClicked();
	UFUNCTION() void OnCloseClicked();

private:
	void BuildCatalogLayout();
	void BuildHotbarLayout();
	void RefreshCatalog();
	void ShowDetail(int32 Index, bool bCommit);
	void ApplyFonts();

	TWeakObjectPtr<USlimeBuildModeComponent> Build;
	bool bHotbar = false;
	ESlimeHomeBuildCategory Category = ESlimeHomeBuildCategory::Block;
	int32 SelectedIndex = 0;
	FString Search;

	struct FRow
	{
		FName Id;
		bool bFromBag = false;
		FText Name;
		FText Description;
		int32 Variant = 0;
		int32 FootX = 1;
		int32 FootY = 1;
		int32 Count = 0;
		TSoftObjectPtr<UTexture2D> Icon;
		TSoftObjectPtr<UTexture2D> FamilyIcon;
		TSoftObjectPtr<UStaticMesh> Mesh;
	};
	TArray<FRow> Rows;

	UPROPERTY()
	TArray<TObjectPtr<USlimeHomeBuildSlotProxy>> Proxies;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> SearchBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> DetailFoot;

	UPROPERTY()
	TObjectPtr<UButton> BuildButton;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> Grid;

	UPROPERTY()
	TObjectPtr<UTextBlock> DetailName;

	UPROPERTY()
	TObjectPtr<UTextBlock> DetailDesc;

	UPROPERTY()
	TObjectPtr<UTextBlock> ModeHint;

	UPROPERTY()
	TObjectPtr<UTextBlock> ModeToast;

	UPROPERTY()
	TObjectPtr<UImage> DetailIcon;

	UPROPERTY()
	TObjectPtr<UImage> PreviewImage;

	UPROPERTY()
	TObjectPtr<ASlimeHomeBuildPreview> PreviewActor;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> CategoryButtons;

	TArray<ESlimeHomeBuildCategory> TabCategories;

	friend class USlimeHomeBuildSlotProxy;
};
