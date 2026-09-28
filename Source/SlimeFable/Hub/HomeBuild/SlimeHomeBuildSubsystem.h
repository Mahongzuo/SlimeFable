// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Hub/HomeBuild/SlimeHomeBuildTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "SlimeHomeBuildSubsystem.generated.h"

class ASlimeHomeBuildManager;
class USlimeHomeBuildCatalog;

UCLASS()
class SLIMEFABLE_API USlimeHomeBuildSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FSlimeHomeBuildRecord> Records;

	UPROPERTY()
	int32 NextId = 1;

	/** Fluid pads saved before version 2 used footprints and blueprints that no longer match. */
	UPROPERTY()
	int32 FluidVersion = 0;
};

UCLASS()
class SLIMEFABLE_API USlimeHomeBuildSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	bool IsMuseum() const { return bMuseum; }
	USlimeHomeBuildCatalog* GetCatalog() const { return Catalog; }
	ASlimeHomeBuildManager* GetManager() const { return Manager; }

	/** Append a record, spawn it, and save. Returns the new id, or INDEX_NONE. */
	int32 CommitRecord(FSlimeHomeBuildRecord Record);

	/** Drop a saved piece. Refunds a bag item when bRefundBag is set. */
	bool RemoveRecord(int32 RecordId, bool bRefundBag);

	/** Save-only removal. The caller already destroyed the actor or refunded the item. */
	void ForgetRecord(int32 RecordId);

	int32 CountFluid() const;

	void ScreenMessage(const FString& Text) const;

private:
	void Load();
	void Save() const;
	bool IsMuseumWorld(const UWorld& World) const;

	UPROPERTY()
	TObjectPtr<USlimeHomeBuildSaveGame> SaveGame;

	UPROPERTY()
	TObjectPtr<USlimeHomeBuildCatalog> Catalog;

	UPROPERTY()
	TObjectPtr<ASlimeHomeBuildManager> Manager;

	bool bMuseum = false;

	static const TCHAR* SaveSlot;
};
