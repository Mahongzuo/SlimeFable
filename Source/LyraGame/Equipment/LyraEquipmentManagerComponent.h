// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystem/LyraAbilitySet.h"
#include "Components/PawnComponent.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "LyraEquipmentManagerComponent.generated.h"

#define UE_API LYRAGAME_API

class UActorComponent;
class ULyraAbilitySystemComponent;
class ULyraEquipmentDefinition;
class ULyraEquipmentInstance;
class ULyraEquipmentManagerComponent;
class UObject;
struct FFrame;
struct FLyraEquipmentList;
struct FNetDeltaSerializeInfo;
struct FReplicationFlags;

/** A single piece of applied equipment */
USTRUCT(BlueprintType)
struct FLyraAppliedEquipmentEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FLyraAppliedEquipmentEntry()
	{}

	FString GetDebugString() const;

private:
	friend FLyraEquipmentList;
	friend ULyraEquipmentManagerComponent;

	// The equipment class that got equipped
	UPROPERTY()
	TSubclassOf<ULyraEquipmentDefinition> EquipmentDefinition;

	UPROPERTY()
	TObjectPtr<ULyraEquipmentInstance> Instance = nullptr;

	// Authority-only list of granted handles
	UPROPERTY(NotReplicated)
	FLyraAbilitySet_GrantedHandles GrantedHandles;
};

/** List of applied equipment */
USTRUCT(BlueprintType)
struct FLyraEquipmentList : public FFastArraySerializer
{
	GENERATED_BODY()

	FLyraEquipmentList()
		: OwnerComponent(nullptr)
	{
	}

	FLyraEquipmentList(UActorComponent* InOwnerComponent)
		: OwnerComponent(InOwnerComponent)
	{
	}

public:
	//~FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	//~End of FFastArraySerializer contract

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FLyraAppliedEquipmentEntry, FLyraEquipmentList>(Entries, DeltaParms, *this);
	}

	ULyraEquipmentInstance* AddEntry(TSubclassOf<ULyraEquipmentDefinition> EquipmentDefinition);
	void RemoveEntry(ULyraEquipmentInstance* Instance);

private:
	ULyraAbilitySystemComponent* GetAbilitySystemComponent() const;

	friend ULyraEquipmentManagerComponent;

private:
	// Replicated list of equipment entries
	UPROPERTY()
	TArray<FLyraAppliedEquipmentEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UActorComponent> OwnerComponent;
};

template<>
struct TStructOpsTypeTraits<FLyraEquipmentList> : public TStructOpsTypeTraitsBase2<FLyraEquipmentList>
{
	enum { WithNetDeltaSerializer = true };
};










/**
 * Manages equipment applied to a pawn
 */
UCLASS(MinimalAPI, BlueprintType, Const)
class ULyraEquipmentManagerComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	UE_API ULyraEquipmentManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	UE_API ULyraEquipmentInstance* EquipItem(TSubclassOf<ULyraEquipmentDefinition> EquipmentDefinition);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	UE_API void UnequipItem(ULyraEquipmentInstance* ItemInstance);

	//~UObject interface
	UE_API virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	//~End of UObject interface

	//~UActorComponent interface
	//virtual void EndPlay() override;
	UE_API virtual void InitializeComponent() override;
	UE_API virtual void UninitializeComponent() override;
	UE_API virtual void ReadyForReplication() override;
	//~End of UActorComponent interface

	/** Returns the first equipped instance of a given type, or nullptr if none are found */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	UE_API ULyraEquipmentInstance* GetFirstInstanceOfType(TSubclassOf<ULyraEquipmentInstance> InstanceType);

 	/** Returns all equipped instances of a given type, or an empty array if none are found */
 	UFUNCTION(BlueprintCallable, BlueprintPure)
	UE_API TArray<ULyraEquipmentInstance*> GetEquipmentInstancesOfType(TSubclassOf<ULyraEquipmentInstance> InstanceType) const;

	template <typename T>
	T* GetFirstInstanceOfType()
	{
		return (T*)GetFirstInstanceOfType(T::StaticClass());
	}

private:
	UPROPERTY(Replicated)
	FLyraEquipmentList EquipmentList;
};

#undef UE_API
