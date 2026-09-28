// Copyright Epic Games, Inc. All Rights Reserved.

#include "Farm/SlimeFarmSubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Inventory/SlimeInventorySubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h"
#include "SlimeFable.h"

const TCHAR* USlimeFarmSubsystem::SaveSlot = TEXT("Farm");

void USlimeFarmSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	USlimeInventorySubsystem* Inventory = Collection.InitializeDependency<USlimeInventorySubsystem>();
	ScanHubAssets(Inventory);
	EnsureBuiltins(Inventory);
	Load();
}

void USlimeFarmSubsystem::Deinitialize()
{
	Flush();
	Super::Deinitialize();
}

void USlimeFarmSubsystem::ScanHubAssets(USlimeInventorySubsystem* Inventory)
{
	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(TEXT("/Game/_Slime/Hub"));
	Filter.ClassPaths.Add(USlimeCropDefinition::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(USlimeSeedDefinition::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(USlimeConsumableDefinition::StaticClass()->GetClassPathName());

	TArray<FAssetData> Assets;
	AssetRegistry.Get().GetAssets(Filter, Assets);
	for (const FAssetData& Asset : Assets)
	{
		UObject* Loaded = Asset.GetAsset();
		if (USlimeItemDefinition* Item = Cast<USlimeItemDefinition>(Loaded))
		{
			if (Inventory)
			{
				Inventory->RegisterItemDefinition(Item);
			}
		}
		if (USlimeCropDefinition* Crop = Cast<USlimeCropDefinition>(Loaded))
		{
			if (!Crop->CropId.IsNone())
			{
				Crops.Add(Crop->CropId, Crop);
			}
		}
		if (USlimeSeedDefinition* Seed = Cast<USlimeSeedDefinition>(Loaded))
		{
			Seeds.Add(Seed);
		}
	}
}

void USlimeFarmSubsystem::EnsureBuiltins(USlimeInventorySubsystem* Inventory)
{
	auto IconPath = [](const TCHAR* Name)
	{
		return FSoftObjectPath(FString::Printf(TEXT("/Game/_Slime/Hub/Icons/T_Icon_%s.T_Icon_%s"), Name, Name));
	};

	auto Consumable = [this, Inventory](FName Id, const FString& Name, const FString& Desc, float Heal, float Cd, float Dmg, float Duration, const FSoftObjectPath& Icon)
	{
		if (!Inventory)
		{
			return;
		}
		if (USlimeConsumableDefinition* Existing = Cast<USlimeConsumableDefinition>(Inventory->FindDefinition(Id)))
		{
			if (Existing->GetOuter() != this && Existing->GetOuter() != Inventory)
			{
				return;
			}
		}
		USlimeConsumableDefinition* Def = NewObject<USlimeConsumableDefinition>(this, Id);
		Def->ItemId = Id;
		Def->DisplayName = FText::FromString(Name);
		Def->Description = FText::FromString(Desc);
		Def->HealAmount = Heal;
		Def->CooldownReduceSeconds = Cd;
		Def->DamageBonusMul = Dmg;
		Def->BuffDuration = Duration;
		Def->Icon = TSoftObjectPtr<UTexture2D>(Icon);
		Inventory->RegisterItemDefinition(Def);
	};

	Consumable(TEXT("JellyTomato"), TEXT("果冻番茄"), TEXT("立即恢复 40 点生命。"), 40.f, 0.f, 1.f, 0.f, IconPath(TEXT("JellyTomato")));
	Consumable(TEXT("PowerPepper"), TEXT("力量辣椒"), TEXT("12 秒内攻击伤害 ×1.45。"), 0.f, 0.f, 1.45f, 12.f, IconPath(TEXT("PowerPepper")));
	Consumable(TEXT("CoolMint"), TEXT("冷却薄荷"), TEXT("缩短技能冷却 6 秒。"), 0.f, 6.f, 1.f, 0.f, IconPath(TEXT("CoolMint")));
	Consumable(TEXT("SunDrop"), TEXT("日光露"), TEXT("恢复 18 点生命，并在 10 秒内伤害 ×1.25。"), 18.f, 0.f, 1.25f, 10.f, IconPath(TEXT("SunDrop")));
	Consumable(TEXT("NightNectar"), TEXT("夜露花蜜"), TEXT("立即恢复 45 点生命。夜间种出来的花更甜。"), 45.f, 0.f, 1.f, 0.f, IconPath(TEXT("NightNectar")));
	Consumable(TEXT("DewTonic"), TEXT("露水药剂"), TEXT("恢复 22 点生命，并缩短冷却 3 秒。"), 22.f, 3.f, 1.f, 0.f, IconPath(TEXT("DewTonic")));

	struct FBuiltinCrop
	{
		const TCHAR* CropId;
		const TCHAR* Name;
		const TCHAR* SeedName;
		bool bFlower;
		bool bNight;
		float GrowSeconds;
		ESlimeElement Element;
		const TCHAR* YieldId;
		int32 YieldMin;
		int32 YieldMax;
		FSlimePlantShape Shape;
	};

	FSlimePlantShape Tomato;
	Tomato.StemHeight = 70.f;
	Tomato.LeafCount = 5;
	Tomato.FruitScale = 1.15f;
	Tomato.FruitColor = FLinearColor(0.85f, 0.12f, 0.08f);
	Tomato.BloomColor = FLinearColor(0.95f, 0.85f, 0.2f);

	FSlimePlantShape Chili;
	Chili.StemHeight = 62.f;
	Chili.StemBend = 18.f;
	Chili.LeafCount = 4;
	Chili.FruitScale = 0.7f;
	Chili.FruitColor = FLinearColor(0.9f, 0.15f, 0.05f);
	Chili.StemColor = FLinearColor(0.2f, 0.42f, 0.12f);

	FSlimePlantShape Mint;
	Mint.StemHeight = 36.f;
	Mint.LeafCount = 8;
	Mint.LeafLength = 16.f;
	Mint.PetalLayers = 1;
	Mint.PetalsPerLayer = 5;
	Mint.LeafColor = FLinearColor(0.25f, 0.75f, 0.45f);
	Mint.BloomColor = FLinearColor(0.75f, 0.9f, 0.8f);

	FSlimePlantShape Sun;
	Sun.StemHeight = 110.f;
	Sun.StemRadius = 5.f;
	Sun.LeafCount = 4;
	Sun.LeafLength = 34.f;
	Sun.PetalLayers = 1;
	Sun.PetalsPerLayer = 10;
	Sun.BloomColor = FLinearColor(0.98f, 0.78f, 0.12f);
	Sun.FruitColor = FLinearColor(0.35f, 0.18f, 0.05f);

	FSlimePlantShape Night;
	Night.StemHeight = 78.f;
	Night.StemBend = 16.f;
	Night.LeafCount = 3;
	Night.PetalLayers = 2;
	Night.PetalsPerLayer = 6;
	Night.BloomColor = FLinearColor(0.45f, 0.25f, 0.75f);
	Night.LeafColor = FLinearColor(0.12f, 0.28f, 0.22f);

	FSlimePlantShape Dew;
	Dew.StemHeight = 48.f;
	Dew.LeafCount = 6;
	Dew.LeafLength = 22.f;
	Dew.FruitScale = 0.85f;
	Dew.FruitColor = FLinearColor(0.55f, 0.85f, 0.7f);
	Dew.LeafColor = FLinearColor(0.2f, 0.62f, 0.4f);

	const FBuiltinCrop Table[] = {
		{TEXT("Tomato"), TEXT("番茄"), TEXT("番茄种子"), false, false, 540.f, ESlimeElement::Water, TEXT("JellyTomato"), 1, 2, Tomato},
		{TEXT("Chili"), TEXT("辣椒"), TEXT("辣椒种子"), false, false, 600.f, ESlimeElement::Fire, TEXT("PowerPepper"), 1, 1, Chili},
		{TEXT("Mint"), TEXT("薄荷"), TEXT("薄荷种子"), true, false, 420.f, ESlimeElement::Wind, TEXT("CoolMint"), 1, 2, Mint},
		{TEXT("Sunflower"), TEXT("向日葵"), TEXT("向日葵种子"), true, false, 660.f, ESlimeElement::Lightning, TEXT("SunDrop"), 1, 1, Sun},
		{TEXT("Nightbloom"), TEXT("夜来香"), TEXT("夜来香种子"), true, true, 600.f, ESlimeElement::Dark, TEXT("NightNectar"), 1, 1, Night},
		{TEXT("Dewleaf"), TEXT("露叶菜"), TEXT("露叶菜种子"), false, false, 480.f, ESlimeElement::Water, TEXT("DewTonic"), 1, 2, Dew},
	};

	for (const FBuiltinCrop& Row : Table)
	{
		const FName CropId(Row.CropId);
		if (!Crops.Contains(CropId))
		{
			USlimeCropDefinition* Crop = NewObject<USlimeCropDefinition>(this, CropId);
			Crop->CropId = CropId;
			Crop->DisplayName = FText::FromString(Row.Name);
			Crop->bFlower = Row.bFlower;
			Crop->bPrefersNight = Row.bNight;
			Crop->GrowSeconds = Row.GrowSeconds;
			Crop->PreferredElement = Row.Element;
			Crop->YieldItemId = FName(Row.YieldId);
			Crop->YieldMin = Row.YieldMin;
			Crop->YieldMax = Row.YieldMax;
			Crop->Shape = Row.Shape;
			Crops.Add(CropId, Crop);
		}

		const FName SeedId(*FString::Printf(TEXT("Seed_%s"), Row.CropId));
		if (Inventory)
		{
			USlimeSeedDefinition* Seed = Cast<USlimeSeedDefinition>(Inventory->FindDefinition(SeedId));
			const bool bNewSeed = Seed == nullptr;
			if (bNewSeed)
			{
				Seed = NewObject<USlimeSeedDefinition>(this, SeedId);
				Seed->ItemId = SeedId;
				Seed->CropId = CropId;
				Seed->DisplayName = FText::FromString(Row.SeedName);
				Seed->Description = FText::FromString(FString::Printf(TEXT("种在博物馆农田里，长成%s。"), Row.Name));
				Inventory->RegisterItemDefinition(Seed);
				Seeds.Add(Seed);
			}
			Seed->Icon = TSoftObjectPtr<UTexture2D>(IconPath(*FString::Printf(TEXT("Seed_%s"), Row.CropId)));
		}
	}
}

void USlimeFarmSubsystem::Load()
{
	Save = Cast<USlimeFarmSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
	if (!Save)
	{
		Save = NewObject<USlimeFarmSaveGame>(this);
	}
}

USlimeCropDefinition* USlimeFarmSubsystem::FindCrop(FName CropId) const
{
	if (const TObjectPtr<USlimeCropDefinition>* Found = Crops.Find(CropId))
	{
		return Found->Get();
	}
	return nullptr;
}

USlimeSeedDefinition* USlimeFarmSubsystem::FindSeedForCrop(FName CropId) const
{
	for (USlimeSeedDefinition* Seed : Seeds)
	{
		if (Seed && Seed->CropId == CropId)
		{
			return Seed;
		}
	}
	return nullptr;
}

void USlimeFarmSubsystem::GetSeedsInBag(const USlimeInventorySubsystem* Inventory, TArray<USlimeSeedDefinition*>& OutSeeds) const
{
	OutSeeds.Reset();
	if (!Inventory)
	{
		return;
	}
	for (const FSlimeInventoryEntry& Entry : Inventory->GetEntries())
	{
		if (Entry.Count <= 0)
		{
			continue;
		}
		if (USlimeSeedDefinition* Seed = Cast<USlimeSeedDefinition>(Inventory->FindDefinition(Entry.ItemId)))
		{
			OutSeeds.Add(Seed);
		}
	}
}

bool USlimeFarmSubsystem::GetPlotRecord(FName PlotId, FSlimeFarmPlotRecord& OutRecord) const
{
	if (!Save || PlotId.IsNone())
	{
		return false;
	}
	for (const FSlimeFarmPlotRecord& Record : Save->Plots)
	{
		if (Record.PlotId == PlotId)
		{
			OutRecord = Record;
			return true;
		}
	}
	return false;
}

void USlimeFarmSubsystem::WritePlotRecord(const FSlimeFarmPlotRecord& Record)
{
	if (!Save || Record.PlotId.IsNone())
	{
		return;
	}
	for (FSlimeFarmPlotRecord& Existing : Save->Plots)
	{
		if (Existing.PlotId == Record.PlotId)
		{
			Existing = Record;
			return;
		}
	}
	Save->Plots.Add(Record);
}

void USlimeFarmSubsystem::Flush()
{
	if (Save)
	{
		UGameplayStatics::SaveGameToSlot(Save, SaveSlot, 0);
	}
}

int32 USlimeFarmSubsystem::GetLastSeedCrateDayKey() const
{
	return Save ? Save->LastSeedCrateDayKey : 0;
}

void USlimeFarmSubsystem::SetLastSeedCrateDayKey(int32 DayKey)
{
	if (Save)
	{
		Save->LastSeedCrateDayKey = DayKey;
		Flush();
	}
}

int32 USlimeFarmSubsystem::MakeTodayKey()
{
	const FDateTime Now = FDateTime::Now();
	return Now.GetYear() * 10000 + Now.GetMonth() * 100 + Now.GetDay();
}
