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

namespace SlimeFloatingTextStack
{
	constexpr int32 MaxSlots = 8;
	constexpr float StackWindowSeconds = 0.35f;
	constexpr float ClusterRadiusCm = 120.f;

	struct FSlot
	{
		bool bOccupied = false;
		FVector WorldLocation = FVector::ZeroVector;
		double LastSpawnTime = 0.0;
	};

	FSlot GSlots[MaxSlots];
}

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

void USlimeFloatingTextWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickHandle);
	}
	ReleaseStackIndex(StackSlot);
	StackSlot = INDEX_NONE;
	Super::NativeDestruct();
}

void USlimeFloatingTextWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
}

void USlimeFloatingTextWidget::OnFloatTimerTick()
{
	TickFloat(0.016f);
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

int32 USlimeFloatingTextWidget::AllocateStackIndex(const FVector& WorldLocation, double NowSeconds)
{
	using namespace SlimeFloatingTextStack;
	int32 BestFree = INDEX_NONE;
	int32 ClusterCount = 0;
	for (int32 i = 0; i < MaxSlots; ++i)
	{
		FSlot& Slot = GSlots[i];
		if (!Slot.bOccupied)
		{
			if (BestFree == INDEX_NONE)
			{
				BestFree = i;
			}
			continue;
		}
		if ((NowSeconds - Slot.LastSpawnTime) > StackWindowSeconds)
		{
			Slot.bOccupied = false;
			if (BestFree == INDEX_NONE)
			{
				BestFree = i;
			}
			continue;
		}
		if (FVector::DistSquared(Slot.WorldLocation, WorldLocation) <= FMath::Square(ClusterRadiusCm))
		{
			++ClusterCount;
		}
	}

	const int32 Index = BestFree != INDEX_NONE ? BestFree : (ClusterCount % MaxSlots);
	GSlots[Index].bOccupied = true;
	GSlots[Index].WorldLocation = WorldLocation;
	GSlots[Index].LastSpawnTime = NowSeconds;
	return FMath::Clamp(ClusterCount, 0, MaxSlots - 1);
}

void USlimeFloatingTextWidget::ReleaseStackIndex(int32 Index)
{
	using namespace SlimeFloatingTextStack;
	if (Index >= 0 && Index < MaxSlots)
	{
		GSlots[Index].bOccupied = false;
	}
}

void USlimeFloatingTextWidget::InitFloating(
	const FText& InText,
	const FLinearColor& InColor,
	const FVector& InWorldLocation,
	bool bUseCjkFont,
	int32 StackIndex)
{
	BuildLayoutIfNeeded();
	WorldLocation = InWorldLocation + FVector(0.f, 0.f, StackIndex * 18.f);
	Age = 0.f;
	Lifetime = 1.5f;
	BaseColor = InColor;
	bCjkFont = bUseCjkFont;
	StackSlot = StackIndex;
	ScreenOffset = FVector2D(
		(StackIndex % 2 == 0 ? -1.f : 1.f) * static_cast<float>(StackIndex / 2 + 1) * 12.f,
		-static_cast<float>(StackIndex) * 36.f);
	if (bCjkFont)
	{
		FontSize = 38.f;
		RiseSpeed = 42.f;
	}
	else
	{
		FontSize = 34.f;
		RiseSpeed = 35.f;
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
			TickHandle, this, &USlimeFloatingTextWidget::OnFloatTimerTick, 0.016f, true);
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

void USlimeFloatingTextWidget::TickFloat(float DeltaSeconds)
{
	Age += DeltaSeconds;
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

	double Now = 0.0;
	if (const UWorld* World = WorldContext->GetWorld())
	{
		Now = World->GetTimeSeconds();
	}
	const int32 StackIndex = AllocateStackIndex(WorldLocation, Now);
	Widget->AddToViewport(bUseCjkFont ? 60 : 50);
	Widget->InitFloating(Text, Color, WorldLocation, bUseCjkFont, StackIndex);
}
