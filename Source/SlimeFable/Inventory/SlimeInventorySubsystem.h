// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SlimeItemTypes.h"
#include "SlimeInventorySubsystem.generated.h"

class USlimeItemDefinition;
class USlimeConsumableDefinition;
class USlimePlaceableDefinition;
class USlimeSouvenirDefinition;
class APawn;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSlimeInventoryChanged);

UCLASS()
class SLIMEFABLE_API USlimeInventorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnSlimeInventoryChanged OnInventoryChanged;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RegisterItemDefinition(USlimeItemDefinition* Definition);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	USlimeItemDefinition* FindDefinition(FName ItemId) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 AddItem(FName ItemId, int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItem(FName ItemId, int32 Count = 1);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FSlimeInventoryEntry> GetEntries() const { return Entries; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FSlimeInventoryEntry> GetEntriesByCategory(ESlimeItemCategory Category) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool UseConsumable(FName ItemId, APawn* User);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AssignHotbar(int32 SlotIndex, FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ClearHotbar(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FName GetHotbarItem(int32 SlotIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool ActivateHotbar(int32 SlotIndex, APawn* User);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool BeginPlaceItem(FName ItemId, APawn* User);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool OpenSouvenir(FName ItemId, APlayerController* PC);

	/** Built-in test definitions used when Content DataAssets are not cooked yet. */
	void EnsureBuiltinDefinitions();

protected:
	UPROPERTY()
	TArray<FSlimeInventoryEntry> Entries;

	UPROPERTY()
	TArray<FName> Hotbar;

	UPROPERTY()
	TMap<FName, TObjectPtr<USlimeItemDefinition>> Definitions;

	void NotifyChanged();
	bool IsHotbarSlotValidForItem(int32 SlotIndex, const USlimeItemDefinition* Def) const;
};
