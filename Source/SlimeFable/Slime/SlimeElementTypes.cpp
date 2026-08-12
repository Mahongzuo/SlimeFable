// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeElementTypes.h"

#define LOCTEXT_NAMESPACE "SlimeElements"

USlimeElementDataAsset::USlimeElementDataAsset()
{
	Profiles.Reserve(SlimeElement::Count);
	for (int32 Index = 0; Index < SlimeElement::Count; ++Index)
	{
		Profiles.Add(MakeDefaultProfile(SlimeElement::FromIndex(Index)));
	}
}

FSlimeElementProfile USlimeElementDataAsset::GetProfile(ESlimeElement Element) const
{
	for (const FSlimeElementProfile& Profile : Profiles)
	{
		if (Profile.Element == Element)
		{
			return Profile;
		}
	}
	return MakeDefaultProfile(Element);
}

FSlimeElementProfile USlimeElementDataAsset::MakeDefaultProfile(ESlimeElement Element)
{
	FSlimeElementProfile Profile;
	Profile.Element = Element;

	switch (Element)
	{
	case ESlimeElement::Water:
		// Saturated face colour; rim / subsurface only lift the silhouette so the blob does
		// not wash out to white under translucency.
		Profile.DisplayName = LOCTEXT("Water", "水");
		Profile.Tag = TEXT("WATER");
		Profile.BaseColor = FLinearColor(FColor::FromHex(TEXT("5B86D9")));
		Profile.SubsurfaceColor = FLinearColor(FColor::FromHex(TEXT("8FB0E8")));
		Profile.RimColor = FLinearColor(FColor::FromHex(TEXT("A8C4F0")));
		Profile.EmissiveColor = FLinearColor::Black;
		Profile.EmissiveIntensity = 0.f;
		Profile.Opacity = 0.62f;
		Profile.Roughness = 0.18f;
		Profile.Refraction = 1.08f;
		Profile.FlowSpeed = 0.35f;
		Profile.NoiseScale = 3.f;
		Profile.RimPower = 2.6f;
		break;

	case ESlimeElement::Wind:
		Profile.DisplayName = LOCTEXT("Wind", "风");
		Profile.Tag = TEXT("WIND");
		Profile.BaseColor = FLinearColor(FColor::FromHex(TEXT("3D9B5C")));
		Profile.SubsurfaceColor = FLinearColor(FColor::FromHex(TEXT("6BCF84")));
		Profile.RimColor = FLinearColor(FColor::FromHex(TEXT("A8E6B8")));
		Profile.EmissiveColor = FLinearColor::Black;
		Profile.EmissiveIntensity = 0.f;
		Profile.Opacity = 0.6f;
		Profile.Roughness = 0.15f;
		Profile.Refraction = 1.04f;
		Profile.FlowSpeed = 1.1f;
		Profile.NoiseScale = 2.2f;
		Profile.RimPower = 1.8f;
		break;

	case ESlimeElement::Fire:
		Profile.DisplayName = LOCTEXT("Fire", "火");
		Profile.Tag = TEXT("FIRE");
		Profile.BaseColor = FLinearColor(FColor::FromHex(TEXT("E67A1A")));
		Profile.SubsurfaceColor = FLinearColor(FColor::FromHex(TEXT("F0A83A")));
		Profile.RimColor = FLinearColor(FColor::FromHex(TEXT("FFC857")));
		Profile.EmissiveColor = FLinearColor(FColor::FromHex(TEXT("C45A10")));
		Profile.EmissiveIntensity = 1.6f;
		Profile.Opacity = 0.58f;
		Profile.Roughness = 0.3f;
		Profile.Refraction = 1.06f;
		Profile.FlowSpeed = 0.7f;
		Profile.NoiseScale = 3.4f;
		Profile.RimPower = 2.2f;
		break;

	case ESlimeElement::Lightning:
		Profile.DisplayName = LOCTEXT("Lightning", "雷");
		Profile.Tag = TEXT("VOLT");
		Profile.BaseColor = FLinearColor(FColor::FromHex(TEXT("7B4DDB")));
		Profile.SubsurfaceColor = FLinearColor(FColor::FromHex(TEXT("A78BFA")));
		Profile.RimColor = FLinearColor(FColor::FromHex(TEXT("C4B5FD")));
		Profile.EmissiveColor = FLinearColor(FColor::FromHex(TEXT("9F7AEA")));
		Profile.EmissiveIntensity = 2.2f;
		Profile.Opacity = 0.58f;
		Profile.Roughness = 0.22f;
		Profile.Refraction = 1.07f;
		Profile.FlowSpeed = 1.4f;
		Profile.NoiseScale = 5.5f;
		Profile.RimPower = 2.f;
		break;

	case ESlimeElement::Dark:
		Profile.DisplayName = LOCTEXT("Dark", "暗");
		Profile.Tag = TEXT("DARK");
		Profile.BaseColor = FLinearColor(FColor::FromHex(TEXT("2A2A2E")));
		Profile.SubsurfaceColor = FLinearColor(FColor::FromHex(TEXT("3A3A40")));
		Profile.RimColor = FLinearColor(FColor::FromHex(TEXT("5A5A62")));
		Profile.EmissiveColor = FLinearColor(FColor::FromHex(TEXT("5A5A62")));
		Profile.EmissiveIntensity = 0.25f;
		Profile.Opacity = 0.55f;
		Profile.Roughness = 0.35f;
		Profile.Refraction = 1.1f;
		Profile.FlowSpeed = 0.25f;
		Profile.NoiseScale = 2.6f;
		// High power so the rim stays a thin edge and the body keeps swallowing light.
		Profile.RimPower = 4.f;
		break;

	case ESlimeElement::Physical:
	default:
		Profile.DisplayName = LOCTEXT("Physical", "物");
		Profile.Tag = TEXT("STONE");
		Profile.BaseColor = FLinearColor(FColor::FromHex(TEXT("8A7351")));
		Profile.SubsurfaceColor = FLinearColor(FColor::FromHex(TEXT("A08A66")));
		Profile.RimColor = FLinearColor(FColor::FromHex(TEXT("A08A66")));
		Profile.EmissiveColor = FLinearColor::Black;
		Profile.EmissiveIntensity = 0.f;
		Profile.Opacity = 0.6f;
		Profile.Roughness = 0.55f;
		Profile.Refraction = 1.02f;
		Profile.FlowSpeed = 0.08f;
		Profile.NoiseScale = 8.f;
		Profile.RimPower = 3.4f;
		break;
	}

	return Profile;
}

#undef LOCTEXT_NAMESPACE
