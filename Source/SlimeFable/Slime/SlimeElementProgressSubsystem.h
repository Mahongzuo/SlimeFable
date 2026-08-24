// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/SaveGame.h"
#include "SlimeElementTypes.h"
#include "SlimeElementProgressSubsystem.generated.h"

UCLASS()
class SLIMEFABLE_API USlimeElementSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	ESlimeElement CurrentElement = ESlimeElement::Water;

	/** Preferred switch order for keys 1-6. */
	UPROPERTY()
	TArray<ESlimeElement> ElementOrder;
};

UCLASS()
class SLIMEFABLE_API USlimeElementProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Slime|Element")
	ESlimeElement GetSavedElement() const { return CurrentElement; }

	UFUNCTION(BlueprintPure, Category = "Slime|Element")
	TArray<ESlimeElement> GetElementOrder() const { return ElementOrder; }

	UFUNCTION(BlueprintPure, Category = "Slime|Element")
	ESlimeElement GetOrderedElement(int32 SlotIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Slime|Element")
	void SetSavedElement(ESlimeElement Element, bool bWriteSave = true);

	UFUNCTION(BlueprintCallable, Category = "Slime|Element")
	void SetElementOrder(const TArray<ESlimeElement>& NewOrder, bool bWriteSave = true);

	UFUNCTION(BlueprintCallable, Category = "Slime|Element")
	void MoveOrder(int32 FromIndex, int32 ToIndex);

	void Save();
	void Load();

	static USlimeElementProgressSubsystem* Get(const UObject* WorldContext);

protected:
	void EnsureDefaultOrder();

	ESlimeElement CurrentElement = ESlimeElement::Water;
	TArray<ESlimeElement> ElementOrder;

	static const TCHAR* SaveSlot;
};
