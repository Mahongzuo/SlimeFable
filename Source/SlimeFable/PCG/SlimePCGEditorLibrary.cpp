#include "PCG/SlimePCGEditorLibrary.h"

#include "SlimeFable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"

#if WITH_EDITOR
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeDataAccess.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#endif

#if WITH_EDITOR
namespace SlimePCGLandscape
{
	static const TCHAR* DefaultMaterialPath = TEXT("/Game/_Slime/SlimePCG/Materials/M_SlimePCG_Landscape.M_SlimePCG_Landscape");
	static const TCHAR* FallbackMaterialPath = TEXT("/Game/CityPark/Materials/Ground/MI_Landscape.MI_Landscape");

	static const TCHAR* LayerInfoPaths[] = {
		TEXT("/Game/CityPark/Maps/Showcase_sharedassets/1_LayerInfo.1_LayerInfo"),
		TEXT("/Game/CityPark/Maps/Showcase_sharedassets/2_LayerInfo.2_LayerInfo"),
		TEXT("/Game/CityPark/Maps/Showcase_sharedassets/3_LayerInfo.3_LayerInfo"),
		TEXT("/Game/CityPark/Maps/Showcase_sharedassets/4_LayerInfo.4_LayerInfo"),
		TEXT("/Game/CityPark/Maps/Showcase_sharedassets/5_LayerInfo.5_LayerInfo"),
		TEXT("/Game/CityPark/Maps/Showcase_sharedassets/6_LayerInfo.6_LayerInfo"),
		TEXT("/Game/CityPark/Maps/Showcase_sharedassets/7_LayerInfo.7_LayerInfo"),
	};

	static float LayerNoise(int32 X, int32 Y, float Frequency, float Offset)
	{
		return FMath::PerlinNoise2D(FVector2D(
			static_cast<float>(X) * Frequency + Offset,
			static_cast<float>(Y) * Frequency + Offset * 0.73f));
	}

	/** Additive weights per vertex, summing to 255. Grass (index 1) is the base. */
	static void FillRandomWeightmaps(
		int32 SizeX,
		int32 SizeY,
		int32 Seed,
		TArray<TArray<uint8>>& OutLayers)
	{
		const int32 NumLayers = OutLayers.Num();
		const int32 NumVerts = SizeX * SizeY;
		for (TArray<uint8>& Layer : OutLayers)
		{
			Layer.SetNumZeroed(NumVerts);
		}

		const float SeedOff = static_cast<float>(Seed) * 17.3f;
		for (int32 Y = 0; Y < SizeY; ++Y)
		{
			for (int32 X = 0; X < SizeX; ++X)
			{
				float W[8] = {};
				// Layer 2 in the CityPark stack is grass (index 1 here). Keep it as the floor.
				W[1] = 0.45f + 0.35f * LayerNoise(X, Y, 0.012f, SeedOff);
				W[0] = FMath::Max(0.f, LayerNoise(X, Y, 0.018f, SeedOff + 20.f) - 0.12f); // dirt
				W[2] = FMath::Max(0.f, LayerNoise(X, Y, 0.016f, SeedOff + 40.f) - 0.22f); // sand
				W[3] = FMath::Max(0.f, LayerNoise(X, Y, 0.022f, SeedOff + 60.f) - 0.18f); // dry grass
				W[4] = FMath::Max(0.f, LayerNoise(X, Y, 0.035f, SeedOff + 80.f) - 0.32f); // stone / paving
				W[5] = FMath::Max(0.f, LayerNoise(X, Y, 0.055f, SeedOff + 100.f) - 0.38f); // rock
				if (NumLayers > 6)
				{
					W[6] = FMath::Max(0.f, LayerNoise(X, Y, 0.028f, SeedOff + 120.f) - 0.42f);
				}

				float Sum = 0.f;
				for (int32 L = 0; L < NumLayers; ++L)
				{
					Sum += W[L];
				}
				if (Sum <= KINDA_SMALL_NUMBER)
				{
					W[1] = 1.f;
					Sum = 1.f;
				}

				const int32 Index = Y * SizeX + X;
				int32 Acc = 0;
				for (int32 L = 0; L < NumLayers; ++L)
				{
					const uint8 Byte = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(W[L] / Sum * 255.f), 0, 255));
					OutLayers[L][Index] = Byte;
					Acc += Byte;
				}
				if (Acc != 255 && NumLayers > 1)
				{
					const int32 Delta = 255 - Acc;
					int32 Grass = static_cast<int32>(OutLayers[1][Index]) + Delta;
					OutLayers[1][Index] = static_cast<uint8>(FMath::Clamp(Grass, 0, 255));
				}
			}
		}
	}
}
#endif

ALandscape* USlimePCGEditorLibrary::CreateFlatLandscape(
	UObject* WorldContextObject,
	int32 ComponentCountX,
	int32 ComponentCountY,
	UMaterialInterface* Material,
	int32 Seed)
{
#if WITH_EDITOR
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (World == nullptr)
	{
		UE_LOG(LogSlimeFable, Error, TEXT("CreateFlatLandscape: no world"));
		return nullptr;
	}

	ComponentCountX = FMath::Clamp(ComponentCountX, 1, 32);
	ComponentCountY = FMath::Clamp(ComponentCountY, 1, 32);

	constexpr int32 QuadsPerSection = 63;
	constexpr int32 SectionsPerComponent = 1;
	const int32 QuadsPerComponent = SectionsPerComponent * QuadsPerSection;
	const int32 SizeX = ComponentCountX * QuadsPerComponent + 1;
	const int32 SizeY = ComponentCountY * QuadsPerComponent + 1;

	UMaterialInterface* LandscapeMat = Material;
	if (LandscapeMat == nullptr)
	{
		LandscapeMat = LoadObject<UMaterialInterface>(nullptr, SlimePCGLandscape::DefaultMaterialPath);
	}
	if (LandscapeMat == nullptr)
	{
		LandscapeMat = LoadObject<UMaterialInterface>(nullptr, SlimePCGLandscape::FallbackMaterialPath);
	}

	TArray<ULandscapeLayerInfoObject*> LayerInfos;
	for (const TCHAR* Path : SlimePCGLandscape::LayerInfoPaths)
	{
		if (ULandscapeLayerInfoObject* Info = LoadObject<ULandscapeLayerInfoObject>(nullptr, Path))
		{
			LayerInfos.Add(Info);
		}
		else
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("CreateFlatLandscape: missing layer %s"), Path);
		}
	}

	TArray<TArray<uint8>> WeightLayers;
	WeightLayers.SetNum(LayerInfos.Num());
	if (LayerInfos.Num() > 0)
	{
		SlimePCGLandscape::FillRandomWeightmaps(SizeX, SizeY, Seed, WeightLayers);
	}

	const FVector Scale(100.f, 100.f, 100.f);
	const FVector Offset(
		-0.5f * ComponentCountX * QuadsPerComponent * Scale.X,
		-0.5f * ComponentCountY * QuadsPerComponent * Scale.Y,
		0.f);

	ALandscape* Landscape = World->SpawnActor<ALandscape>(Offset, FRotator::ZeroRotator);
	if (Landscape == nullptr)
	{
		UE_LOG(LogSlimeFable, Error, TEXT("CreateFlatLandscape: spawn failed"));
		return nullptr;
	}

	if (LandscapeMat)
	{
		Landscape->LandscapeMaterial = LandscapeMat;
	}
	Landscape->SetActorRelativeScale3D(Scale);
	Landscape->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((SizeX * SizeY) / (2048 * 2048) + 1), 2u);

	TArray<uint16> Heights;
	Heights.Init(LandscapeDataAccess::MidValue, SizeX * SizeY);

	TArray<FLandscapeImportLayerInfo> ImportLayers;
	ImportLayers.Reserve(LayerInfos.Num());
	for (int32 i = 0; i < LayerInfos.Num(); ++i)
	{
		FLandscapeImportLayerInfo ImportLayer(LayerInfos[i]->GetLayerName());
		ImportLayer.LayerInfo = LayerInfos[i];
		ImportLayer.LayerData = MoveTemp(WeightLayers[i]);
		ImportLayers.Add(MoveTemp(ImportLayer));
	}

	TMap<FGuid, TArray<uint16>> HeightDataPerLayers;
	HeightDataPerLayers.Add(FGuid(), MoveTemp(Heights));

	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayers;
	MaterialLayerDataPerLayers.Add(FGuid(), MoveTemp(ImportLayers));

	Landscape->Import(
		FGuid::NewGuid(),
		0, 0, SizeX - 1, SizeY - 1,
		SectionsPerComponent,
		QuadsPerSection,
		HeightDataPerLayers,
		TEXT(""),
		MaterialLayerDataPerLayers,
		ELandscapeImportAlphamapType::Additive,
		TArrayView<const FLandscapeLayer>());

	if (ULandscapeInfo* Info = Landscape->CreateLandscapeInfo())
	{
		Info->UpdateLayerInfoMap(Landscape);
		for (ULandscapeLayerInfoObject* LayerInfo : LayerInfos)
		{
			if (LayerInfo)
			{
				Landscape->AddTargetLayer(LayerInfo->GetLayerName(), FLandscapeTargetLayerSettings(LayerInfo), false);
			}
		}
		Info->UpdateLayerInfoMap(Landscape);
	}

	Landscape->UpdateAllComponentMaterialInstances();
	Landscape->RegisterAllComponents();
	Landscape->PostEditChange();
	Landscape->MarkPackageDirty();
	Landscape->SetActorLabel(TEXT("Landscape"));

	const int32 CompCount = Landscape->LandscapeComponents.Num();
	UE_LOG(LogSlimeFable, Log, TEXT("CreateFlatLandscape: %dx%d verts=%d layers=%d comps=%d seed=%d mat=%s"),
		ComponentCountX, ComponentCountY, SizeX, LayerInfos.Num(), CompCount, Seed,
		LandscapeMat ? *LandscapeMat->GetName() : TEXT("None"));
	if (CompCount <= 0)
	{
		UE_LOG(LogSlimeFable, Error, TEXT("CreateFlatLandscape: Import produced zero components"));
	}
	return Landscape;
#else
	return nullptr;
#endif
}
