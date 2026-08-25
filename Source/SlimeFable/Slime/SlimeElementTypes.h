// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SlimeElementTypes.generated.h"

/**
 *  The six slime elements. Declaration order is also the wheel order, clockwise from noon,
 *  and Water being first makes it the default.
 */
UENUM(BlueprintType)
enum class ESlimeElement : uint8
{
	Water,
	Wind,
	Fire,
	Lightning,
	Dark,
	Physical
};

namespace SlimeElement
{
	constexpr int32 Count = 6;

	/** Wraps an index into the wheel. */
	FORCEINLINE ESlimeElement FromIndex(int32 Index)
	{
		const int32 Wrapped = ((Index % Count) + Count) % Count;
		return static_cast<ESlimeElement>(Wrapped);
	}

	FORCEINLINE int32 ToIndex(ESlimeElement Element)
	{
		return static_cast<int32>(Element);
	}
}

/** Material parameter set for one element. Names match the M_SlimeBody parameters. */
USTRUCT(BlueprintType)
struct SLIMEFABLE_API FSlimeElementProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element")
	ESlimeElement Element = ESlimeElement::Water;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element")
	FText DisplayName;

	/** Short latin tag drawn under the wheel label. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element")
	FString Tag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element|Colour")
	FLinearColor BaseColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element|Colour")
	FLinearColor SubsurfaceColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element|Colour")
	FLinearColor EmissiveColor = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element|Colour")
	FLinearColor RimColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element|Scalar", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float EmissiveIntensity = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element|Scalar", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Opacity = 0.62f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element|Scalar", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Roughness = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element|Scalar", meta = (ClampMin = "1.0", ClampMax = "1.5"))
	float Refraction = 1.16f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element|Scalar", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float FlowSpeed = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element|Scalar", meta = (ClampMin = "0.1", ClampMax = "20.0"))
	float NoiseScale = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element|Scalar", meta = (ClampMin = "0.5", ClampMax = "8.0"))
	float RimPower = 2.6f;
};

/**
 *  The six element profiles.
 *
 *  Values live in the class defaults so the palette is authored in exactly one place: the
 *  DA_SlimeElements asset just serialises them, and a project missing that asset still gets
 *  the intended look instead of white slime.
 */
UCLASS(BlueprintType)
class SLIMEFABLE_API USlimeElementDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USlimeElementDataAsset();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elements")
	TArray<FSlimeElementProfile> Profiles;

	/** Returns the authored profile, or the built in default when the asset is incomplete. */
	UFUNCTION(BlueprintPure, Category = "Elements")
	FSlimeElementProfile GetProfile(ESlimeElement Element) const;

	/** The palette as designed, independent of any asset. */
	static FSlimeElementProfile MakeDefaultProfile(ESlimeElement Element);
};
