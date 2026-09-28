// Copyright Epic Games, Inc. All Rights Reserved.

#include "Farm/SlimeProceduralPlantComponent.h"

#include "KismetProceduralMeshLibrary.h"
#include "Materials/MaterialInterface.h"
#include "SlimeFable.h"

namespace
{
	const TCHAR* PlantVertexColorMaterialPath = TEXT("/Game/_Slime/Hub/Materials/M_HubPlantVertexColor.M_HubPlantVertexColor");
}

USlimeProceduralPlantComponent::USlimeProceduralPlantComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetMobility(EComponentMobility::Movable);
	bUseAsyncCooking = true;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCastShadow(true);
	SetGenerateOverlapEvents(false);
	ApplyPlantMaterial();
}

void USlimeProceduralPlantComponent::ApplyPlantMaterial()
{
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, PlantVertexColorMaterialPath))
	{
		SetMaterial(0, Mat);
		return;
	}
	if (!bLoggedMissingMaterial)
	{
		bLoggedMissingMaterial = true;
		UE_LOG(LogSlimeFable, Warning, TEXT("Hub plant: vertex-color material missing (%s)"), PlantVertexColorMaterialPath);
	}
	if (UMaterialInterface* Fallback = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		SetMaterial(0, Fallback);
	}
}

void USlimeProceduralPlantComponent::UpdatePlant(
	const USlimeCropDefinition* Crop,
	FName VariationId,
	float Growth01,
	float HueShift,
	ESlimeCropQuality Quality,
	bool bWithered)
{
	if (!Crop || Growth01 <= 0.01f)
	{
		if (GetNumSections() > 0)
		{
			ClearAllMeshSections();
		}
		BuiltBucket = INDEX_NONE;
		SetRelativeScale3D(FVector::OneVector);
		return;
	}

	const int32 Bucket = FMath::Clamp(FMath::FloorToInt(Growth01 * 20.f), 0, 19);
	const bool bSame = Bucket == BuiltBucket
		&& BuiltCropId == Crop->CropId
		&& bBuiltWithered == bWithered
		&& BuiltQuality == Quality
		&& FMath::IsNearlyEqual(BuiltHue, HueShift, 0.02f);
	if (!bSame)
	{
		const float MeshGrowth = FMath::Max(Bucket / 20.f, 0.05f);
		Rebuild(Crop, VariationId, MeshGrowth, HueShift, Quality, bWithered);
		BuiltBucket = Bucket;
		BuiltCropId = Crop->CropId;
		BuiltHue = HueShift;
		BuiltQuality = Quality;
		bBuiltWithered = bWithered;
	}

	const float Frac = FMath::Frac(Growth01 * 20.f);
	SetRelativeScale3D(FVector(FMath::Lerp(1.f, 1.045f, Frac)));
}

FLinearColor USlimeProceduralPlantComponent::ShiftHue(FLinearColor Color, float HueShift)
{
	FLinearColor Hsv = Color.LinearRGBToHSV();
	Hsv.R = FMath::Fmod(Hsv.R + HueShift * 360.f + 360.f, 360.f);
	return Hsv.HSVToLinearRGB();
}

void USlimeProceduralPlantComponent::AddTri(
	TArray<FVector>& Verts,
	TArray<int32>& Tris,
	TArray<FVector2D>& UV,
	TArray<FLinearColor>& Colors,
	const FVector& A,
	const FVector& B,
	const FVector& C,
	const FLinearColor& Color)
{
	const int32 Base = Verts.Num();
	Verts.Add(A);
	Verts.Add(B);
	Verts.Add(C);
	UV.Add(FVector2D(0.f, 0.f));
	UV.Add(FVector2D(1.f, 0.f));
	UV.Add(FVector2D(0.5f, 1.f));
	Colors.Add(Color);
	Colors.Add(Color);
	Colors.Add(Color);
	Tris.Add(Base);
	Tris.Add(Base + 1);
	Tris.Add(Base + 2);
	Tris.Add(Base);
	Tris.Add(Base + 2);
	Tris.Add(Base + 1);
}

void USlimeProceduralPlantComponent::Rebuild(
	const USlimeCropDefinition* Crop,
	FName VariationId,
	float Growth01,
	float HueShift,
	ESlimeCropQuality Quality,
	bool bWithered)
{
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector2D> UV;
	TArray<FLinearColor> Colors;

	constexpr int32 Side = 4;
	constexpr float Spacing = 46.f;
	const float GridOrigin = -0.5f * (Side - 1) * Spacing;
	const bool bTall = Crop->CropId == TEXT("Sunflower");
	const int32 BaseSeed = static_cast<int32>(GetTypeHash(Crop->CropId) ^ GetTypeHash(VariationId));

	for (int32 Row = 0; Row < Side; ++Row)
	{
		for (int32 Col = 0; Col < Side; ++Col)
		{
			const int32 Cell = Row * Side + Col;
			FRandomStream CellStream(BaseSeed ^ (Cell * 196613));
			const float Yaw = CellStream.FRandRange(0.f, 360.f);
			const float Scale = (bTall ? 0.5f : 0.4f) * CellStream.FRandRange(0.88f, 1.12f);
			const FVector Jitter(CellStream.FRandRange(-4.f, 4.f), CellStream.FRandRange(-4.f, 4.f), 0.f);
			const FVector Origin(GridOrigin + Col * Spacing + Jitter.X, GridOrigin + Row * Spacing + Jitter.Y, 0.f);
			const float CellHue = HueShift + CellStream.FRandRange(-0.04f, 0.04f);
			AppendPlant(Verts, Tris, UV, Colors, Crop, CellStream, Growth01, CellHue, Quality, bWithered, Origin, Yaw, Scale);
		}
	}

	if (Verts.Num() == 0 || Tris.Num() == 0)
	{
		ClearAllMeshSections();
		return;
	}

	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Verts, Tris, UV, Normals, Tangents);
	CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UV, Colors, Tangents, false);
	ApplyPlantMaterial();
}

void USlimeProceduralPlantComponent::AppendPlant(
	TArray<FVector>& Verts,
	TArray<int32>& Tris,
	TArray<FVector2D>& UV,
	TArray<FLinearColor>& Colors,
	const USlimeCropDefinition* Crop,
	FRandomStream& Stream,
	float Growth01,
	float HueShift,
	ESlimeCropQuality Quality,
	bool bWithered,
	const FVector& Origin,
	float YawDegrees,
	float Scale) const
{
	const FSlimePlantShape& Shape = Crop->Shape;

	FLinearColor Stem = ShiftHue(Shape.StemColor, HueShift * 0.25f);
	FLinearColor Leaf = ShiftHue(Shape.LeafColor, HueShift * 0.35f);
	FLinearColor Bloom = ShiftHue(Shape.BloomColor, HueShift);
	FLinearColor Fruit = ShiftHue(Shape.FruitColor, HueShift * 0.5f);
	if (bWithered)
	{
		const FLinearColor Brown(0.35f, 0.24f, 0.12f);
		Stem = FMath::Lerp(Stem, Brown, 0.75f);
		Leaf = FMath::Lerp(Leaf, Brown, 0.8f);
		Bloom = FMath::Lerp(Bloom, Brown, 0.7f);
		Fruit = FMath::Lerp(Fruit, Brown, 0.7f);
	}
	else if (Quality == ESlimeCropQuality::Mutated)
	{
		Bloom = ShiftHue(Bloom, 0.45f) * 1.25f;
		Fruit = ShiftHue(Fruit, 0.45f) * 1.2f;
	}
	else if (Quality == ESlimeCropQuality::Fine)
	{
		Fruit *= 1.1f;
		Bloom *= 1.08f;
	}

	const FTransform PlantXform(FRotator(0.f, YawDegrees, 0.f), Origin, FVector(Scale));
	auto Tri = [&](const FVector& A, const FVector& B, const FVector& C, const FLinearColor& Color)
	{
		AddTri(Verts, Tris, UV, Colors,
			PlantXform.TransformPosition(A),
			PlantXform.TransformPosition(B),
			PlantXform.TransformPosition(C),
			Color);
	};

	const int32 Sides = 6;
	const int32 Rings = 7;
	const float BendSign = Stream.FRandRange(-1.f, 1.f);
	TArray<FVector> RingCenters;
	RingCenters.Reserve(Rings);
	for (int32 Ring = 0; Ring < Rings; ++Ring)
	{
		const float T = static_cast<float>(Ring) / static_cast<float>(Rings - 1);
		const float Height = Shape.StemHeight * Growth01 * T;
		const float Radius = FMath::Lerp(Shape.StemRadius, Shape.StemRadius * 0.35f, T) * FMath::Lerp(0.45f, 1.f, Growth01);
		const float Bend = FMath::Sin(T * PI) * Shape.StemBend * BendSign;
		const FVector Center(Bend, Bend * 0.35f, Height);
		RingCenters.Add(Center);
		if (Ring == 0)
		{
			continue;
		}
		const FVector Prev = RingCenters[Ring - 1];
		const float PrevT = static_cast<float>(Ring - 1) / static_cast<float>(Rings - 1);
		const float PrevRadius = FMath::Lerp(Shape.StemRadius, Shape.StemRadius * 0.35f, PrevT) * FMath::Lerp(0.45f, 1.f, Growth01);
		for (int32 Side = 0; Side < Sides; ++Side)
		{
			const float A0 = (PI * 2.f * Side) / Sides;
			const float A1 = (PI * 2.f * (Side + 1)) / Sides;
			const FVector P0 = Prev + FVector(FMath::Cos(A0) * PrevRadius, FMath::Sin(A0) * PrevRadius, 0.f);
			const FVector P1 = Prev + FVector(FMath::Cos(A1) * PrevRadius, FMath::Sin(A1) * PrevRadius, 0.f);
			const FVector Q0 = Center + FVector(FMath::Cos(A0) * Radius, FMath::Sin(A0) * Radius, 0.f);
			const FVector Q1 = Center + FVector(FMath::Cos(A1) * Radius, FMath::Sin(A1) * Radius, 0.f);
			Tri(P0, P1, Q1, Stem);
			Tri(P0, Q1, Q0, Stem);
		}
	}

	const int32 Leaves = FMath::Max(Shape.LeafCount, 0);
	for (int32 LeafIndex = 0; LeafIndex < Leaves; ++LeafIndex)
	{
		const float Need = static_cast<float>(LeafIndex + 1) / static_cast<float>(Leaves + 1);
		if (Growth01 + 0.08f < Need)
		{
			continue;
		}
		const float Open = FMath::Clamp((Growth01 - Need + 0.2f) / 0.45f, 0.25f, 1.f);
		const float Yaw = (360.f / Leaves) * LeafIndex + Stream.FRandRange(-10.f, 10.f);
		const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
		const int32 RingIndex = FMath::Clamp(FMath::FloorToInt(Need * (Rings - 1)), 1, Rings - 2);
		const FVector Base = RingCenters[RingIndex];
		const float Length = Shape.LeafLength * Open;
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal() * Length * 0.28f;
		const FVector Tip = Base + Dir * Length + FVector(0.f, 0.f, Length * 0.22f);
		Tri(Base, Base + Right, Tip, Leaf);
		Tri(Base, Tip, Base - Right, Leaf);
	}

	const FVector Top = RingCenters.Last();
	if (Crop->bFlower && Growth01 > 0.5f)
	{
		const float Open = FMath::Clamp((Growth01 - 0.5f) / 0.4f, 0.15f, 1.f);
		const int32 Layers = FMath::Max(Shape.PetalLayers, 1);
		const int32 Petals = FMath::Max(Shape.PetalsPerLayer, 3);
		for (int32 Layer = 0; Layer < Layers; ++Layer)
		{
			const float LayerScale = 1.f - Layer * 0.22f;
			const float Pitch = FMath::Lerp(70.f, 18.f, Open) + Layer * 8.f;
			for (int32 Petal = 0; Petal < Petals; ++Petal)
			{
				const float Yaw = (360.f / Petals) * Petal + Layer * (180.f / Petals);
				const FVector Out = FRotator(Pitch, Yaw, 0.f).Vector();
				const float Length = Shape.LeafLength * 0.85f * LayerScale * FMath::Lerp(0.35f, 1.f, Open);
				const FVector Right = FVector::CrossProduct(FVector::UpVector, Out).GetSafeNormal() * Length * 0.32f;
				const FVector Tip = Top + Out * Length;
				const FLinearColor PetalColor = FMath::Lerp(Bloom, FLinearColor::White, Layer * 0.15f);
				Tri(Top, Top + Right, Tip, PetalColor);
				Tri(Top, Tip, Top - Right, PetalColor);
			}
		}
	}
	else if (!Crop->bFlower && Growth01 > 0.68f)
	{
		const float FruitRadius = Shape.FruitScale * FMath::Lerp(8.f, 16.f, FMath::Clamp((Growth01 - 0.68f) / 0.32f, 0.15f, 1.f));
		const FVector Center = Top + FVector(0.f, 0.f, FruitRadius * 0.35f);
		const FLinearColor Young = FMath::Lerp(Leaf, Fruit, FMath::Clamp((Growth01 - 0.68f) / 0.32f, 0.f, 1.f));
		const FVector Offsets[6] = {
			FVector(FruitRadius, 0, 0), FVector(-FruitRadius, 0, 0),
			FVector(0, FruitRadius, 0), FVector(0, -FruitRadius, 0),
			FVector(0, 0, FruitRadius), FVector(0, 0, -FruitRadius * 0.6f)
		};
		const int32 Faces[8][3] = {
			{4, 0, 2}, {4, 2, 1}, {4, 1, 3}, {4, 3, 0},
			{5, 2, 0}, {5, 1, 2}, {5, 3, 1}, {5, 0, 3}
		};
		for (int32 FaceIndex = 0; FaceIndex < 8; ++FaceIndex)
		{
			Tri(
				Center + Offsets[Faces[FaceIndex][0]],
				Center + Offsets[Faces[FaceIndex][1]],
				Center + Offsets[Faces[FaceIndex][2]],
				Young);
		}
	}
}
