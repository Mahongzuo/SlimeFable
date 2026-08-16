// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeInventorySubsystem.h"

#include "SlimeItemDefinition.h"
#include "SlimePlacedActor.h"
#include "SlimePlacementComponent.h"
#include "Combat/SlimeCombatComponent.h"
#include "Combat/SlimeHealthComponent.h"
#include "SlimeFablePlayerController.h"
#include "UI/SlimeSouvenirViewerWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "FileMediaSource.h"
#include "Kismet/GameplayStatics.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "SlimeFable.h"

const TCHAR* USlimeInventorySubsystem::InventorySaveSlot = TEXT("Inventory");

void USlimeInventorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Hotbar.SetNum(SlimeHotbarSlotCount);
	for (int32 Index = 0; Index < SlimeHotbarSlotCount; ++Index)
	{
		Hotbar[Index] = NAME_None;
	}
	EnsureBuiltinDefinitions();
	ScanSouvenirAssets();
	RestoreInventory();
}

void USlimeInventorySubsystem::EnsureBuiltinDefinitions()
{
	auto MakeConsumable = [this](FName Id, const FString& Name, float Heal, float CdReduce, float DmgMul, float Duration)
	{
		if (Definitions.Contains(Id))
		{
			return;
		}
		USlimeConsumableDefinition* Def = NewObject<USlimeConsumableDefinition>(this, Id);
		Def->ItemId = Id;
		Def->DisplayName = FText::FromString(Name);
		Def->Description = FText::FromString(TEXT("测试消耗品"));
		Def->HealAmount = Heal;
		Def->CooldownReduceSeconds = CdReduce;
		Def->DamageBonusMul = DmgMul;
		Def->BuffDuration = Duration;
		Definitions.Add(Id, Def);
	};

	auto MakePlaceable = [this](FName Id, const FString& Name)
	{
		if (Definitions.Contains(Id))
		{
			return;
		}
		USlimePlaceableDefinition* Def = NewObject<USlimePlaceableDefinition>(this, Id);
		Def->ItemId = Id;
		Def->DisplayName = FText::FromString(Name);
		Def->Description = FText::FromString(TEXT("可在平坦地面放置的测试物品"));
		Def->PlacedActorClass = TSoftClassPtr<ASlimePlacedActor>(
			FSoftObjectPath(TEXT("/Game/Blueprints/Items/BP_PlacedProp.BP_PlacedProp_C")));
		Def->PreviewMesh = TSoftObjectPtr<UStaticMesh>(
			FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
		Def->PlacedMeshScale = FVector(0.55f, 0.55f, 0.28f);
		Definitions.Add(Id, Def);
	};

	auto MakeSouvenir = [this](FName Id, const FString& Name)
	{
		const FSoftObjectPath StoryImagePath(TEXT("/Game/Movies/T_ZzmxPostcard.T_ZzmxPostcard"));
		const FSoftObjectPath StoryVideoPath(
			TEXT("/Game/Movies/MS_H3_I2V_Turbo_00002.MS_H3_I2V_Turbo_00002"));

		if (USlimeItemDefinition* Existing = FindDefinition(Id))
		{
			if (USlimeSouvenirDefinition* ExistingSouvenir = Cast<USlimeSouvenirDefinition>(Existing))
			{
				// Always refresh media soft refs so path fixes apply without wiping inventory counts.
				ExistingSouvenir->StoryImage = TSoftObjectPtr<UTexture2D>(StoryImagePath);
				ExistingSouvenir->StoryVideo = TSoftObjectPtr<UFileMediaSource>(StoryVideoPath);
				ExistingSouvenir->Icon = TSoftObjectPtr<UTexture2D>(StoryImagePath);
			}
			return;
		}

		USlimeSouvenirDefinition* Def = NewObject<USlimeSouvenirDefinition>(this, Id);
		Def->ItemId = Id;
		Def->DisplayName = FText::FromString(Name);
		Def->Description = FText::FromString(TEXT("一段关于史莱姆旅途的回忆"));
		Def->StoryText = FText::FromString(
			TEXT("这是一张旧明信片。画面定格在洞穴深处的一束暖光，背后藏着一段尚未讲完的故事。"));
		Def->StoryImage = TSoftObjectPtr<UTexture2D>(StoryImagePath);
		Def->Icon = TSoftObjectPtr<UTexture2D>(StoryImagePath);
		Def->StoryVideo = TSoftObjectPtr<UFileMediaSource>(StoryVideoPath);
		Definitions.Add(Id, Def);
	};

	MakeConsumable(TEXT("HealJelly"), TEXT("回血果冻"), 30.f, 0.f, 1.f, 0.f);
	MakeConsumable(TEXT("CdTea"), TEXT("冷却茶"), 0.f, 5.f, 1.f, 0.f);
	MakeConsumable(TEXT("PowerCandy"), TEXT("增伤糖果"), 0.f, 0.f, 1.35f, 12.f);
	MakePlaceable(TEXT("FlatStone"), TEXT("平坦石"));
	MakeSouvenir(TEXT("OldPostcard"), TEXT("旧明信片"));
}

void USlimeInventorySubsystem::RegisterItemDefinition(USlimeItemDefinition* Definition)
{
	if (!Definition || Definition->ItemId.IsNone())
	{
		return;
	}
	Definitions.Add(Definition->ItemId, Definition);
}

USlimeItemDefinition* USlimeInventorySubsystem::FindDefinition(FName ItemId) const
{
	if (const TObjectPtr<USlimeItemDefinition>* Found = Definitions.Find(ItemId))
	{
		return *Found;
	}
	return nullptr;
}

void USlimeInventorySubsystem::NotifyChanged()
{
	OnInventoryChanged.Broadcast();
	PersistInventory();
}

void USlimeInventorySubsystem::ScanSouvenirAssets()
{
	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> Assets;
	ARM.Get().GetAssetsByClass(USlimeSouvenirDefinition::StaticClass()->GetClassPathName(), Assets, true);
	for (const FAssetData& Data : Assets)
	{
		if (USlimeSouvenirDefinition* Def = Cast<USlimeSouvenirDefinition>(Data.GetAsset()))
		{
			RegisterItemDefinition(Def);
		}
	}
}

void USlimeInventorySubsystem::PersistInventory() const
{
	USlimeInventorySaveGame* Save = Cast<USlimeInventorySaveGame>(
		UGameplayStatics::CreateSaveGameObject(USlimeInventorySaveGame::StaticClass()));
	if (!Save)
	{
		return;
	}
	Save->Entries = Entries;
	Save->Hotbar = Hotbar;
	for (const TPair<FName, TObjectPtr<USlimeItemDefinition>>& Pair : Definitions)
	{
		const USlimeSouvenirDefinition* Souvenir = Cast<USlimeSouvenirDefinition>(Pair.Value);
		if (!Souvenir)
		{
			continue;
		}
		FSlimeSavedSouvenirDef Packed;
		Packed.ItemId = Souvenir->ItemId;
		Packed.DisplayName = Souvenir->DisplayName.ToString();
		Packed.StoryText = Souvenir->StoryText.ToString();
		Packed.Icon = Souvenir->Icon.ToSoftObjectPath();
		Packed.StoryImage = Souvenir->StoryImage.ToSoftObjectPath();
		Packed.StoryMesh = Souvenir->StoryMesh.ToSoftObjectPath();
		Packed.StoryVideo = Souvenir->StoryVideo.ToSoftObjectPath();
		Save->Souvenirs.Add(Packed);
	}
	UGameplayStatics::SaveGameToSlot(Save, InventorySaveSlot, 0);
}

void USlimeInventorySubsystem::RestoreInventory()
{
	if (!UGameplayStatics::DoesSaveGameExist(InventorySaveSlot, 0))
	{
		return;
	}
	USlimeInventorySaveGame* Save = Cast<USlimeInventorySaveGame>(
		UGameplayStatics::LoadGameFromSlot(InventorySaveSlot, 0));
	if (!Save)
	{
		return;
	}
	for (const FSlimeSavedSouvenirDef& Packed : Save->Souvenirs)
	{
		if (Packed.ItemId.IsNone() || FindDefinition(Packed.ItemId))
		{
			continue;
		}
		USlimeSouvenirDefinition* Def = NewObject<USlimeSouvenirDefinition>(this, Packed.ItemId);
		Def->ItemId = Packed.ItemId;
		Def->DisplayName = FText::FromString(Packed.DisplayName);
		Def->StoryText = FText::FromString(Packed.StoryText);
		Def->Icon = TSoftObjectPtr<UTexture2D>(Packed.Icon);
		Def->StoryImage = TSoftObjectPtr<UTexture2D>(Packed.StoryImage);
		Def->StoryMesh = TSoftObjectPtr<UStaticMesh>(Packed.StoryMesh);
		Def->StoryVideo = TSoftObjectPtr<UFileMediaSource>(Packed.StoryVideo);
		RegisterItemDefinition(Def);
	}
	Entries = Save->Entries;
	if (Save->Hotbar.Num() == SlimeHotbarSlotCount)
	{
		Hotbar = Save->Hotbar;
	}
}

int32 USlimeInventorySubsystem::AddItem(FName ItemId, int32 Count)
{
	if (ItemId.IsNone() || Count <= 0)
	{
		return 0;
	}
	const USlimeItemDefinition* Def = FindDefinition(ItemId);
	if (!Def)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("AddItem: unknown item %s"), *ItemId.ToString());
		return 0;
	}

	int32 Remaining = Count;
	for (FSlimeInventoryEntry& Entry : Entries)
	{
		if (Entry.ItemId != ItemId)
		{
			continue;
		}
		const int32 Space = FMath::Max(Def->MaxStack - Entry.Count, 0);
		const int32 Add = FMath::Min(Space, Remaining);
		Entry.Count += Add;
		Remaining -= Add;
		if (Remaining <= 0)
		{
			break;
		}
	}

	while (Remaining > 0)
	{
		const int32 Add = FMath::Min(Def->MaxStack, Remaining);
		FSlimeInventoryEntry Entry;
		Entry.ItemId = ItemId;
		Entry.Count = Add;
		Entries.Add(Entry);
		Remaining -= Add;
	}

	NotifyChanged();
	return Count - Remaining;
}

bool USlimeInventorySubsystem::RemoveItem(FName ItemId, int32 Count)
{
	if (ItemId.IsNone() || Count <= 0 || GetItemCount(ItemId) < Count)
	{
		return false;
	}

	int32 Remaining = Count;
	for (int32 Index = Entries.Num() - 1; Index >= 0 && Remaining > 0; --Index)
	{
		FSlimeInventoryEntry& Entry = Entries[Index];
		if (Entry.ItemId != ItemId)
		{
			continue;
		}
		const int32 Take = FMath::Min(Entry.Count, Remaining);
		Entry.Count -= Take;
		Remaining -= Take;
		if (Entry.Count <= 0)
		{
			Entries.RemoveAt(Index);
		}
	}

	for (int32 Slot = 0; Slot < Hotbar.Num(); ++Slot)
	{
		if (Hotbar[Slot] == ItemId && GetItemCount(ItemId) <= 0)
		{
			Hotbar[Slot] = NAME_None;
		}
	}

	NotifyChanged();
	return true;
}

int32 USlimeInventorySubsystem::GetItemCount(FName ItemId) const
{
	int32 Total = 0;
	for (const FSlimeInventoryEntry& Entry : Entries)
	{
		if (Entry.ItemId == ItemId)
		{
			Total += Entry.Count;
		}
	}
	return Total;
}

TArray<FSlimeInventoryEntry> USlimeInventorySubsystem::GetEntriesByCategory(ESlimeItemCategory Category) const
{
	TArray<FSlimeInventoryEntry> Result;
	for (const FSlimeInventoryEntry& Entry : Entries)
	{
		if (const USlimeItemDefinition* Def = FindDefinition(Entry.ItemId))
		{
			if (Def->Category == Category)
			{
				Result.Add(Entry);
			}
		}
	}
	return Result;
}

bool USlimeInventorySubsystem::UseConsumable(FName ItemId, APawn* User)
{
	USlimeConsumableDefinition* Def = Cast<USlimeConsumableDefinition>(FindDefinition(ItemId));
	if (!Def || !User || GetItemCount(ItemId) <= 0)
	{
		return false;
	}

	if (USlimeHealthComponent* Health = User->FindComponentByClass<USlimeHealthComponent>())
	{
		if (Def->HealAmount > 0.f)
		{
			Health->ApplyHealing(Def->HealAmount);
		}
	}

	if (USlimeCombatComponent* Combat = User->FindComponentByClass<USlimeCombatComponent>())
	{
		if (Def->CooldownReduceSeconds > 0.f)
		{
			Combat->ReduceSkillCooldowns(Def->CooldownReduceSeconds);
		}
		if (Def->BuffDuration > 0.f && !FMath::IsNearlyEqual(Def->DamageBonusMul, 1.f))
		{
			Combat->ApplyOutgoingDamageMul(Def->DamageBonusMul, Def->BuffDuration);
		}
	}

	return RemoveItem(ItemId, 1);
}

bool USlimeInventorySubsystem::IsHotbarSlotValidForItem(int32 SlotIndex, const USlimeItemDefinition* Def) const
{
	if (!Def || SlotIndex < 0 || SlotIndex >= SlimeHotbarSlotCount)
	{
		return false;
	}
	if (SlotIndex <= 2)
	{
		return Def->Category == ESlimeItemCategory::Consumable;
	}
	return Def->Category == ESlimeItemCategory::Placeable;
}

bool USlimeInventorySubsystem::AssignHotbar(int32 SlotIndex, FName ItemId)
{
	const USlimeItemDefinition* Def = FindDefinition(ItemId);
	if (!IsHotbarSlotValidForItem(SlotIndex, Def) || GetItemCount(ItemId) <= 0)
	{
		return false;
	}
	Hotbar[SlotIndex] = ItemId;
	NotifyChanged();
	return true;
}

void USlimeInventorySubsystem::ClearHotbar(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= Hotbar.Num())
	{
		return;
	}
	Hotbar[SlotIndex] = NAME_None;
	NotifyChanged();
}

FName USlimeInventorySubsystem::GetHotbarItem(int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= Hotbar.Num())
	{
		return NAME_None;
	}
	return Hotbar[SlotIndex];
}

bool USlimeInventorySubsystem::ActivateHotbar(int32 SlotIndex, APawn* User)
{
	const FName ItemId = GetHotbarItem(SlotIndex);
	if (ItemId.IsNone() || !User)
	{
		return false;
	}
	const USlimeItemDefinition* Def = FindDefinition(ItemId);
	if (!Def)
	{
		return false;
	}
	if (Def->Category == ESlimeItemCategory::Consumable)
	{
		return UseConsumable(ItemId, User);
	}
	if (Def->Category == ESlimeItemCategory::Placeable)
	{
		return BeginPlaceItem(ItemId, User);
	}
	return false;
}

bool USlimeInventorySubsystem::BeginPlaceItem(FName ItemId, APawn* User)
{
	USlimePlaceableDefinition* Def = Cast<USlimePlaceableDefinition>(FindDefinition(ItemId));
	if (!Def || !User || GetItemCount(ItemId) <= 0)
	{
		return false;
	}
	if (USlimePlacementComponent* Placement = User->FindComponentByClass<USlimePlacementComponent>())
	{
		return Placement->BeginPlacement(Def);
	}
	return false;
}

bool USlimeInventorySubsystem::OpenSouvenir(FName ItemId, APlayerController* PC)
{
	USlimeSouvenirDefinition* Def = Cast<USlimeSouvenirDefinition>(FindDefinition(ItemId));
	if (!Def || !PC)
	{
		return false;
	}

	if (SouvenirWidget)
	{
		CloseSouvenir();
	}

	USlimeSouvenirViewerWidget* Viewer = CreateWidget<USlimeSouvenirViewerWidget>(PC, USlimeSouvenirViewerWidget::StaticClass());
	if (!Viewer)
	{
		return false;
	}
	// AddToViewport must run first: the pure C++ widget only builds its child
	// widgets (StoryImage etc.) inside RebuildWidget, so setting data earlier
	// would silently hit null BindWidgetOptional pointers.
	Viewer->AddToViewport(40);
	Viewer->SetSouvenir(Def);
	SouvenirWidget = Viewer;
	if (ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(PC))
	{
		SlimePC->PushUIInput(ESlimeUIInputReason::Souvenir, Viewer);
	}
	else
	{
		UGameplayStatics::SetGamePaused(PC, true);
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Viewer->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}
	return true;
}

void USlimeInventorySubsystem::CloseSouvenir()
{
	APlayerController* PC = nullptr;
	if (SouvenirWidget)
	{
		PC = SouvenirWidget->GetOwningPlayer();
		SouvenirWidget->Dismiss();
		SouvenirWidget = nullptr;
	}
	if (ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(PC))
	{
		SlimePC->PopUIInput(ESlimeUIInputReason::Souvenir);
	}
}
