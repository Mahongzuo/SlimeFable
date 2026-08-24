// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeFloatingTextWidget.generated.h"

class UTextBlock;
class APlayerController;

/** Screen-space rising combat popup (damage / combo / reaction). */
UCLASS()
class SLIMEFABLE_API USlimeFloatingTextWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void InitFloating(const FText& InText, const FLinearColor& InColor, const FVector& InWorldLocation, bool bUseCjkFont = false);

	static void Spawn(
		UObject* WorldContext,
		const FVector& WorldLocation,
		const FText& Text,
		const FLinearColor& Color,
		bool bUseCjkFont = false);

protected:
	void BuildLayoutIfNeeded();
	bool UpdateScreenPosition();
	void TickFloat();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	FVector WorldLocation = FVector::ZeroVector;
	float Age = 0.f;
	float Lifetime = 1.8f;
	float RiseSpeed = 35.f;
	float FontSize = 34.f;
	FVector2D ScreenOffset = FVector2D::ZeroVector;
	FLinearColor BaseColor = FLinearColor::White;
	bool bCjkFont = false;
	FTimerHandle TickHandle;
	bool bBuiltInCode = false;
};
