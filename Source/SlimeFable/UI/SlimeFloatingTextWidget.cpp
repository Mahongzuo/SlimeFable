// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeFloatingTextWidget.h"
#include "UI/MenuUIStyle.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

TSharedRef<SWidget> USlimeFloatingTextWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void USlimeFloatingTextWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USlimeFloatingTextWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TickFloat();
}

void USlimeFloatingTextWidget::BuildLayoutIfNeeded()
{
	if (LabelText)
	{
		bBuiltInCode = false;
		return;
	}
	bBuiltInCode = true;
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;
	LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LabelText"));
	LabelText->SetJustification(ETextJustify::Center);
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(LabelText))
	{
		PanelSlot->SetAutoSize(true);
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	}
}

void USlimeFloatingTextWidget::InitFloating(
	const FText& InText,
	const FLinearColor& InColor,
	const FVector& InWorldLocation,
	bool bUseCjkFont)
{
	BuildLayoutIfNeeded();
	WorldLocation = InWorldLocation;
	Age = 0.f;
	BaseColor = InColor;
	bCjkFont = bUseCjkFont;
	if (bCjkFont)
	{
		FontSize = 38.f;
		RiseSpeed = 42.f;
		Lifetime = 2.2f;
	}
	if (LabelText)
	{
		LabelText->SetText(InText);
		if (bCjkFont)
		{
			FMenuUIStyle::ApplyBrushCJKFont(LabelText, FontSize, InColor);
		}
		else
		{
			FMenuUIStyle::ApplyMarkerFont(LabelText, FontSize, InColor);
		}
	}
	UpdateScreenPosition();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TickHandle, this, &USlimeFloatingTextWidget::TickFloat, 0.016f, true);
	}
}

bool USlimeFloatingTextWidget::UpdateScreenPosition()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		PC = UGameplayStatics::GetPlayerController(this, 0);
	}
	if (!PC || !LabelText)
	{
		return false;
	}

	FVector2D ScreenPos;
	const FVector DrawLoc = WorldLocation + FVector(0.f, 0.f, Age * RiseSpeed);
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, DrawLoc, ScreenPos, false))
	{
		return false;
	}

	ScreenPos += ScreenOffset;
	if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(LabelText->Slot))
	{
		PanelSlot->SetPosition(ScreenPos);
	}
	return true;
}

void USlimeFloatingTextWidget::TickFloat()
{
	const float Delta = 0.016f;
	Age += Delta;
	const float Alpha = 1.f - FMath::Clamp(Age / Lifetime, 0.f, 1.f);
	if (LabelText)
	{
		FLinearColor Color = BaseColor;
		Color.A = Alpha;
		LabelText->SetColorAndOpacity(FSlateColor(Color));
		const float Scale = 1.f + (1.f - Alpha) * 0.15f;
		LabelText->SetRenderScale(FVector2D(Scale, Scale));
	}
	UpdateScreenPosition();
	if (Age >= Lifetime)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(TickHandle);
		}
		RemoveFromParent();
	}
}

void USlimeFloatingTextWidget::Spawn(
	UObject* WorldContext,
	const FVector& WorldLocation,
	const FText& Text,
	const FLinearColor& Color,
	bool bUseCjkFont)
{
	if (!WorldContext)
	{
		return;
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContext, 0);
	if (!PC)
	{
		return;
	}
	USlimeFloatingTextWidget* Widget = CreateWidget<USlimeFloatingTextWidget>(PC, USlimeFloatingTextWidget::StaticClass());
	if (!Widget)
	{
		return;
	}
	Widget->AddToViewport(bUseCjkFont ? 60 : 50);
	Widget->InitFloating(Text, Color, WorldLocation, bUseCjkFont);
}
