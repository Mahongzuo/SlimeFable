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
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
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

ALandscape* USlimePCGEditorLibrary::CreateHomeLandscape(
	UObject* WorldContextObject,
	UMaterialInterface* Material,
	const TArray<ULandscapeLayerInfoObject*>& LayerInfos,
	FVector2D HoleMin,
	FVector2D HoleMax)
{
#if WITH_EDITOR
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (World == nullptr || Material == nullptr)
	{
		UE_LOG(LogSlimeFable, Error, TEXT("CreateHomeLandscape: missing world or material"));
		return nullptr;
	}

	TArray<ALandscape*> Stale;
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		if (It->GetActorLabel() == TEXT("HomeLandscape"))
		{
			Stale.Add(*It);
		}
	}
	for (ALandscape* Old : Stale)
	{
		Old->Destroy();
	}

	constexpr int32 ComponentCount = 16;
	constexpr int32 QuadsPerSection = 63;
	constexpr int32 SectionsPerComponent = 1;
	const int32 QuadsPerComponent = SectionsPerComponent * QuadsPerSection;
	const int32 SizeX = ComponentCount * QuadsPerComponent + 1;
	const int32 SizeY = SizeX;
	const FVector Scale(100.f, 100.f, 100.f);
	const FVector Offset(
		-0.5f * ComponentCount * QuadsPerComponent * Scale.X,
		-0.5f * ComponentCount * QuadsPerComponent * Scale.Y,
		0.f);

	TArray<uint16> Heights;
	Heights.SetNumUninitialized(SizeX * SizeY);
	for (int32 Y = 0; Y < SizeY; ++Y)
	{
		for (int32 X = 0; X < SizeX; ++X)
		{
			const float WorldX = Offset.X + static_cast<float>(X) * Scale.X;
			const float WorldY = Offset.Y + static_cast<float>(Y) * Scale.Y;
			const float Dist = FVector2D(WorldX, WorldY).Size();
			// Core sits 5cm under the courtyard floor so the overlap ring does not z-fight.
			float WorldZ = -5.f;
			if (Dist > 15000.f)
			{
				const float T = FMath::Clamp((Dist - 15000.f) / 33000.f, 0.f, 1.f);
				const float Smooth = T * T * (3.f - 2.f * T);
				WorldZ = -5.f + Smooth * 800.f;
				WorldZ += FMath::PerlinNoise2D(FVector2D(WorldX, WorldY) * 0.00008f) * 60.f * Smooth;
			}
			const int32 Delta = FMath::RoundToInt(WorldZ / Scale.Z * 128.f);
			Heights[Y * SizeX + X] = static_cast<uint16>(FMath::Clamp(LandscapeDataAccess::MidValue + Delta, 0, 65535));
		}
	}

	TArray<TArray<uint8>> WeightLayers;
	WeightLayers.SetNum(LayerInfos.Num());
	for (TArray<uint8>& Layer : WeightLayers)
	{
		Layer.SetNumZeroed(SizeX * SizeY);
	}
	if (LayerInfos.Num() > 0)
	{
		for (int32 Y = 0; Y < SizeY; ++Y)
		{
			for (int32 X = 0; X < SizeX; ++X)
			{
				const float WorldX = Offset.X + static_cast<float>(X) * Scale.X;
				const float WorldY = Offset.Y + static_cast<float>(Y) * Scale.Y;
				const float Dist = FVector2D(WorldX, WorldY).Size();
				float Weights[3] = {1.f, 0.f, 0.f};
				if (Dist < 8000.f)
				{
					Weights[1] = 0.75f;
					Weights[0] = 0.25f;
				}
				else if (Dist > 40000.f && LayerInfos.Num() > 2)
				{
					const float Rock = FMath::Clamp((Dist - 40000.f) / 8000.f, 0.f, 1.f);
					Weights[2] = Rock;
					Weights[0] = 1.f - Rock;
				}
				float Sum = 0.f;
				const int32 Used = FMath::Min(LayerInfos.Num(), 3);
				for (int32 Layer = 0; Layer < Used; ++Layer)
				{
					Sum += Weights[Layer];
				}
				int32 Acc = 0;
				const int32 Index = Y * SizeX + X;
				for (int32 Layer = 0; Layer < Used; ++Layer)
				{
					const uint8 Byte = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Weights[Layer] / Sum * 255.f), 0, 255));
					WeightLayers[Layer][Index] = Byte;
					Acc += Byte;
				}
				if (Acc != 255)
				{
					const int32 Fix = FMath::Clamp(static_cast<int32>(WeightLayers[0][Index]) + (255 - Acc), 0, 255);
					WeightLayers[0][Index] = static_cast<uint8>(Fix);
				}
			}
		}
	}

	ALandscape* Landscape = World->SpawnActor<ALandscape>(Offset, FRotator::ZeroRotator);
	if (!Landscape)
	{
		UE_LOG(LogSlimeFable, Error, TEXT("CreateHomeLandscape: spawn failed"));
		return nullptr;
	}
	Landscape->LandscapeMaterial = Material;
	Landscape->SetActorRelativeScale3D(Scale);
	Landscape->SetActorLabel(TEXT("HomeLandscape"));

	TArray<FLandscapeImportLayerInfo> ImportLayers;
	for (int32 Index = 0; Index < LayerInfos.Num(); ++Index)
	{
		if (!LayerInfos[Index])
		{
			continue;
		}
		FLandscapeImportLayerInfo ImportLayer(LayerInfos[Index]->GetLayerName());
		ImportLayer.LayerInfo = LayerInfos[Index];
		ImportLayer.LayerData = WeightLayers[Index];
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
		TEXT("HomeLandscape"),
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
		if (ALandscapeProxy::VisibilityLayer)
		{
			Landscape->AddTargetLayer(
				ALandscapeProxy::VisibilityLayer->GetLayerName(),
				FLandscapeTargetLayerSettings(ALandscapeProxy::VisibilityLayer),
				false);
			const bool bHole = HoleMax.X > HoleMin.X && HoleMax.Y > HoleMin.Y;
			if (bHole)
			{
				TArray<uint8> Visibility;
				Visibility.Init(0, SizeX * SizeY);
				for (int32 Y = 0; Y < SizeY; ++Y)
				{
					for (int32 X = 0; X < SizeX; ++X)
					{
						const float WorldX = Offset.X + static_cast<float>(X) * Scale.X;
						const float WorldY = Offset.Y + static_cast<float>(Y) * Scale.Y;
						if (WorldX >= HoleMin.X && WorldX <= HoleMax.X && WorldY >= HoleMin.Y && WorldY <= HoleMax.Y)
						{
							Visibility[Y * SizeX + X] = 255;
						}
					}
				}
				FLandscapeEditDataInterface Edit(Info);
				Edit.SetAlphaData(ALandscapeProxy::VisibilityLayer, 0, 0, SizeX - 1, SizeY - 1, Visibility.GetData(), 0);
			}
		}
		else
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("CreateHomeLandscape: visibility layer missing, courtyard is not holed"));
		}
		Info->UpdateLayerInfoMap(Landscape);
	}

	Landscape->UpdateAllComponentMaterialInstances();
	Landscape->RegisterAllComponents();
	Landscape->PostEditChange();
	Landscape->MarkPackageDirty();
	UE_LOG(LogSlimeFable, Log, TEXT("CreateHomeLandscape: %dx%d hole (%.0f,%.0f)-(%.0f,%.0f) layers=%d"),
		SizeX, SizeY, HoleMin.X, HoleMin.Y, HoleMax.X, HoleMax.Y, LayerInfos.Num());
	return Landscape;
#else
	return nullptr;
#endif
}

int32 USlimePCGEditorLibrary::PaintHomeLandscapeLayersEven(UObject* WorldContextObject)
{
#if WITH_EDITOR
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (World == nullptr)
	{
		return 0;
	}

	ALandscape* Landscape = nullptr;
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		if (It->GetActorLabel() == TEXT("HomeLandscape"))
		{
			Landscape = *It;
			break;
		}
	}
	if (Landscape == nullptr)
	{
		UE_LOG(LogSlimeFable, Error, TEXT("PaintHomeLandscapeLayersEven: HomeLandscape missing"));
		return 0;
	}

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (Info == nullptr)
	{
		Info = Landscape->CreateLandscapeInfo();
	}
	if (Info == nullptr)
	{
		return 0;
	}
	Info->UpdateLayerInfoMap(Landscape);

	TArray<ULandscapeLayerInfoObject*> Layers;
	for (const FLandscapeInfoLayerSettings& Layer : Info->Layers)
	{
		ULandscapeLayerInfoObject* LayerInfo = Layer.LayerInfoObj;
		if (LayerInfo == nullptr || LayerInfo == ALandscapeProxy::VisibilityLayer)
		{
			continue;
		}
		Layers.Add(LayerInfo);
	}
	Layers.Sort([](const ULandscapeLayerInfoObject& A, const ULandscapeLayerInfoObject& B)
	{
		return A.GetLayerName().LexicalLess(B.GetLayerName());
	});
	if (Layers.Num() == 0)
	{
		UE_LOG(LogSlimeFable, Error, TEXT("PaintHomeLandscapeLayersEven: no paint layers"));
		return 0;
	}

	int32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
	if (!Info->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
	{
		UE_LOG(LogSlimeFable, Error, TEXT("PaintHomeLandscapeLayersEven: no extent"));
		return 0;
	}
	const int32 SizeX = MaxX - MinX + 1;
	const int32 SizeY = MaxY - MinY + 1;
	const int32 Count = SizeX * SizeY;
	const int32 NumLayers = Layers.Num();

	TArray<TArray<uint8>> Weights;
	Weights.SetNum(NumLayers);
	TArray<int32> Wins;
	Wins.SetNumZeroed(NumLayers);
	for (TArray<uint8>& Layer : Weights)
	{
		Layer.SetNumUninitialized(Count);
	}

	for (int32 Y = 0; Y < SizeY; ++Y)
	{
		for (int32 X = 0; X < SizeX; ++X)
		{
			// ~90m cells, edges nudged by noise so the split is not a hard grid.
			// A hash of the cell index keeps the three layers near equal area.
			const float JitterX = FMath::PerlinNoise2D(FVector2D(X * 0.01f, Y * 0.01f));
			const float JitterY = FMath::PerlinNoise2D(FVector2D(X * 0.01f + 8.f, Y * 0.01f));
			const int32 CellX = FMath::FloorToInt(static_cast<float>(X) / 90.f + JitterX * 0.35f);
			const int32 CellY = FMath::FloorToInt(static_cast<float>(Y) / 90.f + JitterY * 0.35f);
			const uint32 Hash = HashCombine(GetTypeHash(CellX), GetTypeHash(CellY));
			const int32 Winner = static_cast<int32>(Hash % static_cast<uint32>(NumLayers));
			Wins[Winner] += 1;
			const int32 Index = Y * SizeX + X;
			for (int32 Layer = 0; Layer < NumLayers; ++Layer)
			{
				Weights[Layer][Index] = Layer == Winner ? 255 : 0;
			}
		}
	}

	FLandscapeEditDataInterface Edit(Info);
	const TArray<ULandscapeEditLayerBase*> EditLayers = Landscape->GetEditLayers();
	if (EditLayers.Num() > 0 && EditLayers[0] != nullptr)
	{
		Edit.SetEditLayer(EditLayers[0]->GetGuid());
	}
	for (int32 Layer = 0; Layer < NumLayers; ++Layer)
	{
		Edit.SetAlphaData(Layers[Layer], MinX, MinY, MaxX, MaxY, Weights[Layer].GetData(), 0);
		UE_LOG(LogSlimeFable, Log, TEXT("PaintHomeLandscapeLayersEven: %s covers %d / %d"),
			*Layers[Layer]->GetLayerName().ToString(), Wins[Layer], Count);
	}
	Edit.Flush();
	Landscape->RequestLayersContentUpdateForceAll(ELandscapeLayerUpdateMode::Update_All, true);
	Landscape->ForceUpdateLayersContent(false);
	Landscape->MarkPackageDirty();
	return NumLayers;
#else
	return 0;
#endif
}
