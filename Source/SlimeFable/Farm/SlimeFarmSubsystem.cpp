// Copyright Epic Games, Inc. All Rights Reserved.

#include "Farm/SlimeFarmSubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "Inventory/SlimeInventorySubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h"
#include "SlimeFable.h"
#include "TimerManager.h"

const TCHAR* USlimeFarmSubsystem::SaveSlot = TEXT("Farm");

void USlimeFarmSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	USlimeInventorySubsystem* Inventory = Collection.InitializeDependency<USlimeInventorySubsystem>();
	ScanHubAssets(Inventory);
	EnsureBuiltins(Inventory);
	Load();
	WorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddLambda(
		[this](UWorld* World, const UWorld::InitializationValues)
		{
			bool bHasDiskCrop = false;
			for (const TPair<FName, TObjectPtr<USlimeCropDefinition>>& Pair : Crops)
			{
				if (Pair.Value && Pair.Value->GetOuter() != this)
				{
					bHasDiskCrop = true;
					break;
				}
			}
			if (World && World->IsGameWorld() && !bHasDiskCrop)
			{
				UGameInstance* GI = GetGameInstance();
				ScanHubAssets(GI ? GI->GetSubsystem<USlimeInventorySubsystem>() : nullptr);
			}
			ArmFlushTimer(World);
		});
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddLambda([this](UWorld* World, bool, bool)
	{
		DisarmFlushTimer(World);
		FlushIfDirty();
	});
}

void USlimeFarmSubsystem::Deinitialize()
{
	FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitHandle);
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	if (UWorld* World = FlushWorld.Get())
	{
		World->GetTimerManager().ClearTimer(FlushTimer);
	}
	FlushIfDirty();
	Super::Deinitialize();
}

void USlimeFarmSubsystem::ArmFlushTimer(UWorld* World)
{
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	if (UWorld* Previous = FlushWorld.Get())
	{
		Previous->GetTimerManager().ClearTimer(FlushTimer);
	}
	FlushWorld = World;
	World->GetTimerManager().SetTimer(
		FlushTimer,
		FTimerDelegate::CreateUObject(this, &USlimeFarmSubsystem::FlushIfDirty),
		10.f,
		true);
}

void USlimeFarmSubsystem::DisarmFlushTimer(UWorld* World)
{
	if (!World || FlushWorld.Get() != World)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(FlushTimer);
	FlushWorld.Reset();
}

void USlimeFarmSubsystem::FlushIfDirty()
{
	Flush();
}

void USlimeFarmSubsystem::ScanHubAssets(USlimeInventorySubsystem* Inventory)
{
	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& Registry = AssetRegistry.Get();
	// Standalone does not scan /Game at startup, so WaitForCompletion returns immediately and GetAssets is empty.
	Registry.WaitForPremadeAssetRegistry();
	Registry.ScanPathsSynchronous({ TEXT("/Game/_Slime/Hub") }, /*bForceRescan*/ true);
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(TEXT("/Game/_Slime/Hub"));
	Filter.ClassPaths.Add(USlimeCropDefinition::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(USlimeSeedDefinition::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(USlimeConsumableDefinition::StaticClass()->GetClassPathName());

	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);
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
				const TObjectPtr<USlimeCropDefinition>* Existing = Crops.Find(Crop->CropId);
				const USlimeCropDefinition* Previous = Existing ? Existing->Get() : nullptr;
				const bool bReplaceBuiltin = Previous && Previous->GetOuter() == this;
				if (!Previous || bReplaceBuiltin)
				{
					Crops.Add(Crop->CropId, Crop);
				}
			}
		}
		if (USlimeSeedDefinition* Seed = Cast<USlimeSeedDefinition>(Loaded))
		{
			if (!Seeds.Contains(Seed))
			{
				Seeds.Add(Seed);
			}
		}
	}
	UE_LOG(LogSlimeFable, Log, TEXT("Farm hub scan found %d assets, %d crops."), Assets.Num(), Crops.Num());
}

void USlimeFarmSubsystem::EnsureBuiltins(USlimeInventorySubsystem* Inventory)
{
	auto IconPath = [](const TCHAR* Name)
	{
		return FSoftObjectPath(FString::Printf(TEXT("/Game/_Slime/Hub/Icons/T_Icon_%s.T_Icon_%s"), Name, Name));
	};
	auto ThumbPath = [](const TCHAR* Name)
	{
		return FSoftObjectPath(FString::Printf(TEXT("/Game/UltimateFarming/Textures/Thumbnails/T_Thumb_%s.T_Thumb_%s"), Name, Name));
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

	Consumable(TEXT("JellyTomato"), TEXT("果冻番茄"), TEXT("吃掉恢复 24 点生命，10 秒内攻击伤害 ×1.15。"), 24.f, 0.f, 1.15f, 10.f, ThumbPath(TEXT("Tomatoes")));
	Consumable(TEXT("PowerPepper"), TEXT("力量辣椒"), TEXT("吃掉恢复 24 点生命，10 秒内攻击伤害 ×1.15。"), 24.f, 0.f, 1.15f, 10.f, ThumbPath(TEXT("Pepper")));
	Consumable(TEXT("CoolMint"), TEXT("冷却薄荷"), TEXT("吃掉恢复 8 点生命。"), 8.f, 0.f, 1.f, 0.f, ThumbPath(TEXT("Mint")));
	Consumable(TEXT("SunDrop"), TEXT("日光露"), TEXT("吃掉恢复 24 点生命，10 秒内攻击伤害 ×1.15。"), 24.f, 0.f, 1.15f, 10.f, ThumbPath(TEXT("Sunflower")));
	Consumable(TEXT("NightNectar"), TEXT("夜露花蜜"), TEXT("吃掉恢复 40 点生命，14 秒内攻击伤害 ×1.35。"), 40.f, 0.f, 1.35f, 14.f,
		FSoftObjectPath(TEXT("/Game/_Slime/Hub/Icons/T_Thumb_Jasmine.T_Thumb_Jasmine")));
	Consumable(TEXT("DewTonic"), TEXT("露水药剂"), TEXT("吃掉恢复 8 点生命。"), 8.f, 0.f, 1.f, 0.f, ThumbPath(TEXT("Lattuce")));

	struct FBuiltinCrop
	{
		const TCHAR* CropId;
		const TCHAR* Name;
		const TCHAR* SeedName;
		bool bFlower;
		bool bNight;
		float GrowSeconds;
		const TCHAR* RealSpan;
		ESlimeElement Element;
		const TCHAR* YieldId;
		int32 YieldMin;
		int32 YieldMax;
		FSlimePlantShape Shape;
		const TCHAR* Thumb;
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
		{TEXT("Tomato"), TEXT("番茄"), TEXT("番茄种子"), false, false, 720.f, TEXT("大约三个月"), ESlimeElement::Water, TEXT("JellyTomato"), 1, 2, Tomato, TEXT("Tomatoes")},
		{TEXT("Chili"), TEXT("辣椒"), TEXT("辣椒种子"), false, false, 720.f, TEXT("大约三个月"), ESlimeElement::Fire, TEXT("PowerPepper"), 1, 1, Chili, TEXT("Pepper")},
		{TEXT("Mint"), TEXT("薄荷"), TEXT("薄荷种子"), true, false, 240.f, TEXT("大约一个月"), ESlimeElement::Wind, TEXT("CoolMint"), 1, 2, Mint, TEXT("Mint")},
		{TEXT("Sunflower"), TEXT("向日葵"), TEXT("向日葵种子"), true, false, 720.f, TEXT("大约三个月"), ESlimeElement::Lightning, TEXT("SunDrop"), 1, 1, Sun, TEXT("Sunflower")},
		{TEXT("Nightbloom"), TEXT("夜来香"), TEXT("夜来香种子"), true, true, 1440.f, TEXT("大约半年到一年"), ESlimeElement::Dark, TEXT("NightNectar"), 1, 1, Night, nullptr},
		{TEXT("Dewleaf"), TEXT("露叶菜"), TEXT("露叶菜种子"), false, false, 240.f, TEXT("大约一个月"), ESlimeElement::Water, TEXT("DewTonic"), 1, 2, Dew, TEXT("Lattuce")},
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
			Crop->RealWorldSpan = FText::FromString(Row.RealSpan);
			Crop->PreferredElement = Row.Element;
			Crop->YieldItemId = FName(Row.YieldId);
			Crop->YieldMin = Row.YieldMin;
			Crop->YieldMax = Row.YieldMax;
			Crop->Shape = Row.Shape;
			const FSoftObjectPath Icon = Row.Thumb
				? ThumbPath(Row.Thumb)
				: FSoftObjectPath(TEXT("/Game/_Slime/Hub/Icons/T_Thumb_Jasmine.T_Thumb_Jasmine"));
			Crop->Icon = TSoftObjectPtr<UTexture2D>(Icon);
			Crops.Add(CropId, Crop);
		}

		(void)Row.SeedName;
	}
}

void USlimeFarmSubsystem::Load()
{
	Save = Cast<USlimeFarmSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
	if (!Save)
	{
		Save = NewObject<USlimeFarmSaveGame>(this);
	}
	RebuildPlotIndex();
}

void USlimeFarmSubsystem::RebuildPlotIndex()
{
	PlotIndex.Reset();
	if (!Save)
	{
		return;
	}
	for (int32 Index = 0; Index < Save->Plots.Num(); ++Index)
	{
		PlotIndex.Add(Save->Plots[Index].PlotId, Index);
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

void USlimeFarmSubsystem::GetAllCrops(TArray<USlimeCropDefinition*>& OutCrops) const
{
	OutCrops.Reset();
	for (const TPair<FName, TObjectPtr<USlimeCropDefinition>>& Pair : Crops)
	{
		if (Pair.Value)
		{
			OutCrops.Add(Pair.Value);
		}
	}
	OutCrops.Sort([](const USlimeCropDefinition& A, const USlimeCropDefinition& B)
	{
		if (A.Category != B.Category)
		{
			return static_cast<uint8>(A.Category) < static_cast<uint8>(B.Category);
		}
		return A.DisplayName.ToString() < B.DisplayName.ToString();
	});
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
	if (const int32* Index = PlotIndex.Find(PlotId))
	{
		if (Save->Plots.IsValidIndex(*Index))
		{
			OutRecord = Save->Plots[*Index];
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
	if (const int32* Index = PlotIndex.Find(Record.PlotId))
	{
		if (Save->Plots.IsValidIndex(*Index))
		{
			Save->Plots[*Index] = Record;
			bDirty = true;
			return;
		}
	}
	const int32 Index = Save->Plots.Add(Record);
	PlotIndex.Add(Record.PlotId, Index);
	bDirty = true;
}

void USlimeFarmSubsystem::RemovePlotRecord(FName PlotId)
{
	if (!Save || PlotId.IsNone())
	{
		return;
	}
	const int32 Removed = Save->Plots.RemoveAll([PlotId](const FSlimeFarmPlotRecord& Record)
	{
		return Record.PlotId == PlotId;
	});
	if (Removed > 0)
	{
		RebuildPlotIndex();
		bDirty = true;
	}
}

void USlimeFarmSubsystem::PurgeOrphanHomePlots(const TSet<FName>& LiveHomePlotIds)
{
	if (!Save)
	{
		return;
	}
	const int32 Removed = Save->Plots.RemoveAll([&LiveHomePlotIds](const FSlimeFarmPlotRecord& Record)
	{
		return Record.PlotId.ToString().StartsWith(TEXT("HomePlot_")) && !LiveHomePlotIds.Contains(Record.PlotId);
	});
	if (Removed > 0)
	{
		RebuildPlotIndex();
		bDirty = true;
		UE_LOG(LogSlimeFable, Log, TEXT("[Farm] purged %d orphan home plots"), Removed);
		RequestFlush();
	}
}

void USlimeFarmSubsystem::RequestFlush()
{
	Flush();
}

void USlimeFarmSubsystem::Flush()
{
	if (!Save || !bDirty)
	{
		return;
	}
	if (UGameplayStatics::SaveGameToSlot(Save, SaveSlot, 0))
	{
		bDirty = false;
	}
}

FName USlimeFarmSubsystem::GetLastPlantedCrop() const
{
	return Save ? Save->LastPlantedCropId : NAME_None;
}

void USlimeFarmSubsystem::SetLastPlantedCrop(FName CropId)
{
	if (!Save || Save->LastPlantedCropId == CropId)
	{
		return;
	}
	Save->LastPlantedCropId = CropId;
	bDirty = true;
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
		bDirty = true;
		RequestFlush();
	}
}

int32 USlimeFarmSubsystem::MakeTodayKey()
{
	const FDateTime Now = FDateTime::Now();
	return Now.GetYear() * 10000 + Now.GetMonth() * 100 + Now.GetDay();
}
