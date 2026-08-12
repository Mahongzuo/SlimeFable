// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"

class UMaterialInterface;
class UTexture2D;
class UFont;
class UButton;
class UTextBlock;
class UImage;

/** Loads menu visuals and applies a cohesive earthy slate look. */
struct FMenuUIStyle
{
	static FLinearColor WarmTextColor();
	static FLinearColor WarmMutedTextColor();
	static FLinearColor WarmTitleColor();

	static UTexture2D* LoadMenuBackgroundTexture();
	static UMaterialInterface* LoadButtonMaterial();
	static UFont* LoadTitleFont();
	static UFont* LoadBrushCJKFontAsset();

	/** Permanent Marker — Latin/title/digits. */
	static FSlateFontInfo MakeMarkerFont(float Size);

	/** ZCOOL KuaiLe (project TTF) with YaHei fallback — Chinese UI. */
	static FSlateFontInfo MakeBrushCJKFont(float Size);

	/**
	 * Mixed menu strings: Marker for Latin/digits, KuaiLe for CJK ranges.
	 * Use for buttons like "进入今日关卡（0812）".
	 */
	static FSlateFontInfo MakeMixedMenuFont(float Size);

	static FSlateBrush MakeMaterialBrush(UMaterialInterface* Material, FVector2D ImageSize);
	static FSlateBrush MakeTextureBrush(UTexture2D* Texture, FVector2D ImageSize);

	static void ApplyMarkerFont(UTextBlock* Text, float Size, FLinearColor Color);
	static void ApplyBrushCJKFont(UTextBlock* Text, float Size, FLinearColor Color);
	static void ApplyMixedMenuFont(UTextBlock* Text, float Size, FLinearColor Color);
	static void ApplyTitleFont(UTextBlock* Text, float Size, FLinearColor Color);

	static void ApplyMaterialButtonStyle(UButton* Button, UMaterialInterface* Material, FVector2D Size);
	static void ApplyFlatButtonStyle(UButton* Button, FLinearColor Fill, FVector2D Size, FMargin Padding = FMargin(18.f, 10.f));
	static void ApplyMenuBackground(UImage* Image);
	static void ApplyImageMaterial(UImage* Image, UMaterialInterface* Material);
};
