// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slime/SlimeElementProgressSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

const TCHAR* USlimeElementProgressSubsystem::SaveSlot = TEXT("SlimeElement");

void USlimeElementProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	EnsureDefaultOrder();
	Load();
}

USlimeElementProgressSubsystem* USlimeElementProgressSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}
	const UWorld* World = WorldContext->GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : Cast<UGameInstance>(WorldContext);
	return GI ? GI->GetSubsystem<USlimeElementProgressSubsystem>() : nullptr;
}

void USlimeElementProgressSubsystem::EnsureDefaultOrder()
{
	const int32 Need = SlimeElement::Count;
	if (ElementOrder.Num() == Need)
	{
		TSet<ESlimeElement> Seen;
		bool bValid = true;
		for (const ESlimeElement El : ElementOrder)
		{
			if (Seen.Contains(El))
			{
				bValid = false;
				break;
			}
			Seen.Add(El);
		}
		if (bValid && Seen.Num() == Need)
		{
			return;
		}
	}
	ElementOrder.Reset(Need);
	for (int32 Index = 0; Index < Need; ++Index)
	{
		ElementOrder.Add(SlimeElement::FromIndex(Index));
	}
}

ESlimeElement USlimeElementProgressSubsystem::GetOrderedElement(int32 SlotIndex) const
{
	if (ElementOrder.IsValidIndex(SlotIndex))
	{
		return ElementOrder[SlotIndex];
	}
	return SlimeElement::FromIndex(SlotIndex);
}

void USlimeElementProgressSubsystem::SetSavedElement(ESlimeElement Element, bool bWriteSave)
{
	CurrentElement = Element;
	if (bWriteSave)
	{
		Save();
	}
}

void USlimeElementProgressSubsystem::SetElementOrder(const TArray<ESlimeElement>& NewOrder, bool bWriteSave)
{
	ElementOrder = NewOrder;
	EnsureDefaultOrder();
	if (bWriteSave)
	{
		Save();
	}
}

void USlimeElementProgressSubsystem::MoveOrder(int32 FromIndex, int32 ToIndex)
{
	EnsureDefaultOrder();
	if (!ElementOrder.IsValidIndex(FromIndex) || !ElementOrder.IsValidIndex(ToIndex) || FromIndex == ToIndex)
	{
		return;
	}
	const ESlimeElement Moving = ElementOrder[FromIndex];
	ElementOrder.RemoveAt(FromIndex);
	ElementOrder.Insert(Moving, ToIndex);
	Save();
}

void USlimeElementProgressSubsystem::Save()
{
	EnsureDefaultOrder();
	USlimeElementSaveGame* SaveObj = Cast<USlimeElementSaveGame>(
		UGameplayStatics::CreateSaveGameObject(USlimeElementSaveGame::StaticClass()));
	if (!SaveObj)
	{
		return;
	}
	SaveObj->CurrentElement = CurrentElement;
	SaveObj->ElementOrder = ElementOrder;
	UGameplayStatics::SaveGameToSlot(SaveObj, SaveSlot, 0);
}

void USlimeElementProgressSubsystem::Load()
{
	EnsureDefaultOrder();
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlot, 0))
	{
		return;
	}
	USlimeElementSaveGame* SaveObj = Cast<USlimeElementSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
	if (!SaveObj)
	{
		return;
	}
	CurrentElement = SaveObj->CurrentElement;
	if (SaveObj->ElementOrder.Num() > 0)
	{
		ElementOrder = SaveObj->ElementOrder;
	}
	EnsureDefaultOrder();
}
