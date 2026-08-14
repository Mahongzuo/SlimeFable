// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MenuUIStyle.h"
#include "UI/MenuButtonHoverHelper.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Materials/MaterialInterface.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/Paths.h"
#include "SlimeFable.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "Materials/MaterialInterface.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"

namespace MenuUIStylePrivate
{
	template <typename T>
	static T* LoadObj(const TCHAR* Path)
	{
		return LoadObject<T>(nullptr, Path);
	}

	static FString FindYaHeiPath()
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		const TArray<FString> Candidates = {
			TEXT("C:/Windows/Fonts/msyh.ttc"),
			TEXT("C:/Windows/Fonts/msyhbd.ttc"),
			TEXT("C:/Windows/Fonts/msjh.ttc"),
			TEXT("C:/Windows/Fonts/simhei.ttf"),
			TEXT("C:/Windows/Fonts/simsun.ttc"),
		};
		for (const FString& Path : Candidates)
		{
			if (PlatformFile.FileExists(*Path))
			{
				return Path;
			}
		}
		return FString();
	}

	static FString FindKuaiLePath()
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		const FString ProjectFont = FPaths::ProjectContentDir() / TEXT("UI/Fonts/ZCOOLKuaiLe-Regular.ttf");
		if (PlatformFile.FileExists(*ProjectFont))
		{
			return FPaths::ConvertRelativePathToFull(ProjectFont);
		}
		return FString();
	}

	static void AppendCjkRanges(FCompositeSubFont& Sub)
	{
		Sub.CharacterRanges.Add(FInt32Range(0x3000, 0x303F));
		Sub.CharacterRanges.Add(FInt32Range(0x3040, 0x30FF));
		Sub.CharacterRanges.Add(FInt32Range(0x3400, 0x4DBF));
		Sub.CharacterRanges.Add(FInt32Range(0x4E00, 0x9FFF));
		Sub.CharacterRanges.Add(FInt32Range(0xF900, 0xFAFF));
		Sub.CharacterRanges.Add(FInt32Range(0xFF00, 0xFFEF));
	}

	static void AppendCjkSubFontFromUFont(FStandaloneCompositeFont& Font, UFont* CjkFont)
	{
		if (!CjkFont)
		{
			return;
		}
		const FCompositeFont* CjkComposite = CjkFont->GetCompositeFont();
		if (!CjkComposite)
		{
			return;
		}
		FCompositeSubFont& Sub = Font.SubTypefaces.AddDefaulted_GetRef();
		Sub.Typeface = CjkComposite->DefaultTypeface;
		AppendCjkRanges(Sub);
	}

	static void AppendCjkSubFontFromPath(FStandaloneCompositeFont& Font, const FString& CjkPath)
	{
		FCompositeSubFont& Sub = Font.SubTypefaces.AddDefaulted_GetRef();
		Sub.Typeface = FTypeface(FName(TEXT("Regular")), CjkPath, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		AppendCjkRanges(Sub);
	}

	static UFont* LoadKuaiLeUFont()
	{
		if (UFont* Font = LoadObj<UFont>(
				TEXT("/Game/UI/Fonts/ZCOOLKuaiLe-Regular_Font.ZCOOLKuaiLe-Regular_Font")))
		{
			return Font;
		}
		return LoadObj<UFont>(TEXT("/Game/UI/Fonts/Font_ZCOOLKuaiLe.Font_ZCOOLKuaiLe"));
	}

	static TSharedPtr<const FCompositeFont> GetOrCreateKuaiLeComposite()
	{
		static TSharedPtr<const FCompositeFont> Cached;
		if (Cached.IsValid())
		{
			return Cached;
		}

		if (UFont* KuaiLeAsset = LoadKuaiLeUFont())
		{
			if (const FCompositeFont* AssetComposite = KuaiLeAsset->GetCompositeFont())
			{
				TSharedRef<FStandaloneCompositeFont> Font = MakeShared<FStandaloneCompositeFont>();
				Font->DefaultTypeface = AssetComposite->DefaultTypeface;
				Font->FallbackTypeface = AssetComposite->FallbackTypeface;
				Cached = Font;
				return Cached;
			}
		}

		const FString KuaiLe = FindKuaiLePath();
		if (!KuaiLe.IsEmpty())
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("MenuUIStyle: using loose KuaiLe TTF (editor only; prefer cooked UFont for packages)"));
			Cached = MakeShared<FStandaloneCompositeFont>(
				FName(TEXT("Regular")),
				KuaiLe,
				EFontHinting::Default,
				EFontLoadingPolicy::LazyLoad);
			return Cached;
		}

		const FString YaHei = FindYaHeiPath();
		if (!YaHei.IsEmpty())
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("MenuUIStyle: ZCOOLKuaiLe UFont missing, falling back to system CJK font"));
			Cached = MakeShared<FStandaloneCompositeFont>(
				FName(TEXT("Regular")),
				YaHei,
				EFontHinting::Default,
				EFontLoadingPolicy::LazyLoad);
			return Cached;
		}
		return nullptr;
	}

	static TSharedPtr<const FCompositeFont> GetOrCreateMixedComposite()
	{
		static TSharedPtr<const FCompositeFont> Cached;
		if (Cached.IsValid())
		{
			return Cached;
		}

		UFont* Marker = LoadObj<UFont>(
			TEXT("/Game/UIMaterialLab/Fonts/PermanentMarker-Regular_Font.PermanentMarker-Regular_Font"));
		UFont* KuaiLeAsset = LoadKuaiLeUFont();

		if (Marker)
		{
			if (const FCompositeFont* MarkerComposite = Marker->GetCompositeFont())
			{
				TSharedRef<FStandaloneCompositeFont> Font = MakeShared<FStandaloneCompositeFont>();
				Font->DefaultTypeface = MarkerComposite->DefaultTypeface;
				Font->FallbackTypeface = MarkerComposite->FallbackTypeface;
				if (KuaiLeAsset)
				{
					AppendCjkSubFontFromUFont(*Font, KuaiLeAsset);
				}
				else
				{
					const FString CjkPath = FindKuaiLePath().IsEmpty() ? FindYaHeiPath() : FindKuaiLePath();
					if (!CjkPath.IsEmpty())
					{
						UE_LOG(LogSlimeFable, Warning, TEXT("MenuUIStyle: Mixed font CJK from disk path (not package-safe)"));
						AppendCjkSubFontFromPath(*Font, CjkPath);
					}
				}
				Cached = Font;
				return Cached;
			}
		}

		return GetOrCreateKuaiLeComposite();
	}
}

FLinearColor FMenuUIStyle::WarmTextColor()
{
	return FLinearColor(0.93f, 0.90f, 0.82f, 1.f);
}

FLinearColor FMenuUIStyle::WarmMutedTextColor()
{
	return FLinearColor(0.78f, 0.74f, 0.66f, 1.f);
}

FLinearColor FMenuUIStyle::WarmTitleColor()
{
	return FLinearColor(0.97f, 0.93f, 0.84f, 1.f);
}

FLinearColor FMenuUIStyle::TodayEdgeColor()
{
	return FLinearColor(0.92f, 0.72f, 0.32f, 1.f);
}

UTexture2D* FMenuUIStyle::LoadMenuBackgroundTexture()
{
	return MenuUIStylePrivate::LoadObj<UTexture2D>(
		TEXT("/Game/UI/Textures/T_MenuBackground.T_MenuBackground"));
}

UMaterialInterface* FMenuUIStyle::LoadButtonMaterial()
{
	return MenuUIStylePrivate::LoadObj<UMaterialInterface>(
		TEXT("/Game/UIMaterialLab/Widgets/ComponentMaterials/MaterialInstances/MI_UI_Button.MI_UI_Button"));
}

UFont* FMenuUIStyle::LoadTitleFont()
{
	if (UFont* Font = MenuUIStylePrivate::LoadObj<UFont>(
			TEXT("/Game/UIMaterialLab/Fonts/PermanentMarker-Regular_Font.PermanentMarker-Regular_Font")))
	{
		return Font;
	}
	return MenuUIStylePrivate::LoadObj<UFont>(TEXT("/Game/UIMaterialLab/Fonts/Roboto.Roboto"));
}

UFont* FMenuUIStyle::LoadBrushCJKFontAsset()
{
	return MenuUIStylePrivate::LoadKuaiLeUFont();
}

FSlateFontInfo FMenuUIStyle::MakeMarkerFont(float Size)
{
	if (UFont* Marker = LoadTitleFont())
	{
		return FSlateFontInfo(Marker, Size);
	}
	return MakeBrushCJKFont(Size);
}

FSlateFontInfo FMenuUIStyle::MakeBrushCJKFont(float Size)
{
	if (UFont* Asset = LoadBrushCJKFontAsset())
	{
		return FSlateFontInfo(Asset, Size);
	}
	if (TSharedPtr<const FCompositeFont> Composite = MenuUIStylePrivate::GetOrCreateKuaiLeComposite())
	{
		return FSlateFontInfo(Composite, Size, FName(TEXT("Regular")));
	}
	return FSlateFontInfo();
}

FSlateFontInfo FMenuUIStyle::MakeMixedMenuFont(float Size)
{
	if (TSharedPtr<const FCompositeFont> Composite = MenuUIStylePrivate::GetOrCreateMixedComposite())
	{
		return FSlateFontInfo(Composite, Size, FName(TEXT("Regular")));
	}
	return MakeBrushCJKFont(Size);
}

FSlateBrush FMenuUIStyle::MakeMaterialBrush(UMaterialInterface* Material, FVector2D ImageSize)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.Tiling = ESlateBrushTileType::NoTile;
	Brush.ImageSize = ImageSize;
	if (Material)
	{
		Brush.SetResourceObject(Material);
	}
	return Brush;
}

FSlateBrush FMenuUIStyle::MakeTextureBrush(UTexture2D* Texture, FVector2D ImageSize)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.Tiling = ESlateBrushTileType::NoTile;
	Brush.ImageSize = ImageSize;
	if (Texture)
	{
		Brush.SetResourceObject(Texture);
	}
	return Brush;
}

void FMenuUIStyle::ApplyMarkerFont(UTextBlock* Text, float Size, FLinearColor Color)
{
	if (!Text)
	{
		return;
	}
	Text->SetFont(MakeMarkerFont(Size));
	Text->SetColorAndOpacity(FSlateColor(Color));
}

void FMenuUIStyle::ApplyBrushCJKFont(UTextBlock* Text, float Size, FLinearColor Color)
{
	if (!Text)
	{
		return;
	}
	Text->SetFont(MakeBrushCJKFont(Size));
	Text->SetColorAndOpacity(FSlateColor(Color));
}

void FMenuUIStyle::ApplyMixedMenuFont(UTextBlock* Text, float Size, FLinearColor Color)
{
	if (!Text)
	{
		return;
	}
	Text->SetFont(MakeMixedMenuFont(Size));
	Text->SetColorAndOpacity(FSlateColor(Color));
}

void FMenuUIStyle::ApplyTitleFont(UTextBlock* Text, float Size, FLinearColor Color)
{
	ApplyMarkerFont(Text, Size, Color);
}

void FMenuUIStyle::ApplyMaterialButtonStyle(UButton* Button, UMaterialInterface* Material, FVector2D Size)
{
	if (!Button)
	{
		return;
	}

	FButtonStyle Style = Button->GetStyle();
	const FSlateBrush Normal = MakeMaterialBrush(Material, Size);

	FSlateBrush Hovered = Normal;
	Hovered.TintColor = FSlateColor(FLinearColor(1.08f, 1.02f, 0.88f, 1.f));
	Hovered.DrawAs = ESlateBrushDrawType::RoundedBox;
	Hovered.OutlineSettings.CornerRadii = FVector4(10.f, 10.f, 10.f, 10.f);
	Hovered.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Hovered.OutlineSettings.Color = FSlateColor(TodayEdgeColor());
	Hovered.OutlineSettings.Width = 2.25f;
	if (Material)
	{
		Hovered.SetResourceObject(Material);
		Hovered.DrawAs = ESlateBrushDrawType::Image;
		// Keep material look but add a warm outline via a companion rounded overlay is hard in one brush;
		// tint + foreground change carry most of the feedback with BindInkButtonHover.
	}

	FSlateBrush Pressed = Normal;
	Pressed.TintColor = FSlateColor(FLinearColor(0.82f, 0.78f, 0.7f, 1.f));

	Style.SetNormal(Normal);
	Style.SetHovered(Hovered);
	Style.SetPressed(Pressed);
	Style.SetDisabled(Normal);
	Style.SetNormalPadding(FMargin(18.f, 10.f));
	Style.SetPressedPadding(FMargin(18.f, 12.f, 18.f, 8.f));
	Style.NormalForeground = FSlateColor(WarmTextColor());
	Style.HoveredForeground = FSlateColor(TodayEdgeColor());
	Style.PressedForeground = FSlateColor(WarmMutedTextColor());
	Button->SetStyle(Style);
}

void FMenuUIStyle::ApplyFlatButtonStyle(UButton* Button, FLinearColor Fill, FVector2D Size, FMargin Padding)
{
	if (!Button)
	{
		return;
	}

	auto MakeBrush = [&](FLinearColor Color, float OutlineWidth, FLinearColor OutlineColor) -> FSlateBrush
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.ImageSize = Size;
		Brush.OutlineSettings.CornerRadii = FVector4(8.f, 8.f, 8.f, 8.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Width = OutlineWidth;
		Brush.OutlineSettings.Color = FSlateColor(OutlineColor);
		return Brush;
	};

	const FSlateBrush Normal = MakeBrush(Fill, 0.f, FLinearColor::Transparent);
	const FSlateBrush Hovered = MakeBrush(
		FLinearColor(Fill.R + 0.08f, Fill.G + 0.06f, Fill.B + 0.03f, FMath::Min(Fill.A + 0.1f, 1.f)),
		2.f,
		TodayEdgeColor());
	const FSlateBrush Pressed = MakeBrush(
		FLinearColor(Fill.R * 0.85f, Fill.G * 0.85f, Fill.B * 0.85f, Fill.A),
		1.5f,
		TodayEdgeColor() * 0.8f);

	FButtonStyle Style = Button->GetStyle();
	Style.SetNormal(Normal);
	Style.SetHovered(Hovered);
	Style.SetPressed(Pressed);
	Style.SetDisabled(Normal);
	Style.SetNormalPadding(Padding);
	Style.SetPressedPadding(Padding);
	Button->SetStyle(Style);
}

void FMenuUIStyle::BindInkButtonHover(UButton* Button, UTextBlock* Label)
{
	if (!Button)
	{
		return;
	}

	UMenuButtonHoverHelper* Helper = NewObject<UMenuButtonHoverHelper>(Button);
	Helper->Button = Button;
	Helper->Label = Label;
	Helper->NormalLabelColor = Label ? Label->GetColorAndOpacity().GetSpecifiedColor() : WarmTextColor();
	Button->OnHovered.AddUniqueDynamic(Helper, &UMenuButtonHoverHelper::HandleHovered);
	Button->OnUnhovered.AddUniqueDynamic(Helper, &UMenuButtonHoverHelper::HandleUnhovered);
}

void FMenuUIStyle::ApplyMenuBackground(UImage* Image)
{
	if (!Image)
	{
		return;
	}

	if (UTexture2D* Texture = LoadMenuBackgroundTexture())
	{
		Image->SetBrush(MakeTextureBrush(Texture, FVector2D(1920.f, 1080.f)));
		Image->SetColorAndOpacity(FLinearColor(0.62f, 0.58f, 0.52f, 1.f));
		return;
	}

	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(FLinearColor(0.12f, 0.1f, 0.09f, 1.f));
	Brush.ImageSize = FVector2D(64.f, 64.f);
	Image->SetBrush(Brush);
	Image->SetColorAndOpacity(FLinearColor::White);
}

void FMenuUIStyle::ApplyImageMaterial(UImage* Image, UMaterialInterface* Material)
{
	if (!Image || !Material)
	{
		return;
	}
	Image->SetBrush(MakeMaterialBrush(Material, FVector2D(64.f, 64.f)));
}
