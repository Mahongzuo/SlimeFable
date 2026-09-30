// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Hub/HomeBuild/SlimeHomeBuildTypes.h"
#include "SlimeHomeBuildManager.generated.h"

class UInstancedStaticMeshComponent;
class USlimeHomeBuildCatalog;
class UStaticMesh;
class UStaticMeshComponent;
class ASlimeHomeFluidPad;
class ASlimePlacedActor;

UCLASS()
class SLIMEFABLE_API ASlimeHomeBuildManager : public AActor
{
	GENERATED_BODY()

public:
	ASlimeHomeBuildManager();

	void SetCatalog(USlimeHomeBuildCatalog* InCatalog) { Catalog = InCatalog; }

	bool AddRecord(const FSlimeHomeBuildRecord& Record);
	void RemoveRecord(int32 RecordId);
	void ClearAll();

	bool IsBlocked(int32 AnchorX, int32 AnchorY, int32 FootX, int32 FootY, float BaseZ, float HeightCm, int32 IgnoreId, bool bIgnoreNonFluid = false, bool bIncomingPool = false, float IncomingPoolDepthCm = 0.f) const;
	int32 FindRecordAtHit(const FHitResult& Hit) const;
	bool GetRecordStack(int32 RecordId, float& OutBaseZ, float& OutHeight) const;
	bool GetRecordTransform(const FSlimeHomeBuildRecord& Record, FTransform& OutTransform) const;

	void SetHighlight(int32 RecordId);

	static constexpr int32 MaxRecords = 20000;
	static constexpr int32 MaxFluidPads = 6;
	int32 NumRecords() const { return Records.Num(); }
	int32 NumFluidPads() const;

	const TArray<FSlimeHomeBuildRecord>& GetRecords() const { return Records; }

private:
	struct FInstancePiece
	{
		int32 RecordId = 0;
		FName MeshKey;
		int32 InstanceIndex = INDEX_NONE;
		FTransform Transform = FTransform::Identity;
	};

	UInstancedStaticMeshComponent* GetBucket(FName MeshKey, UStaticMesh* Mesh);
	void RemoveInstanceAt(FName MeshKey, int32 InstanceIndex);
	bool ComputeTransform(const FSlimeHomeBuildEntry& Entry, const FSlimeHomeBuildRecord& Record, FTransform& OutTransform) const;
	void SpawnFluid(const FSlimeHomeBuildRecord& Record, const FSlimeHomeBuildEntry& Entry, const FTransform& Transform);
	void SpawnFarmPlot(const FSlimeHomeBuildRecord& Record, const FSlimeHomeBuildEntry& Entry);
	void ForgetFarmPlot(AActor* Actor);
	void SpawnBag(const FSlimeHomeBuildRecord& Record, const FTransform& Transform);

	UPROPERTY()
	TObjectPtr<USlimeHomeBuildCatalog> Catalog;

	UPROPERTY()
	TArray<FSlimeHomeBuildRecord> Records;

	UPROPERTY()
	TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> Buckets;

	TArray<FInstancePiece> Instances;

	UPROPERTY()
	TMap<int32, TObjectPtr<AActor>> SpawnedActors;

	UPROPERTY()
	TObjectPtr<AActor> HighlightActor;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HighlightMesh;
};
