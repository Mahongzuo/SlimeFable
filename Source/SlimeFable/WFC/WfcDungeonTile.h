#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "WFC/WfcTypes.h"
#include "WfcDungeonTile.generated.h"

class UDecalComponent;
class UMaterialInterface;
class URectLightComponent;
class UStaticMeshComponent;
class UWfcTileSet;

UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API AWFCDungeonTile : public AActor
{
	GENERATED_BODY()

public:
	AWFCDungeonTile();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", meta = (AdvancedDisplay))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", meta = (AdvancedDisplay))
	TObjectPtr<URectLightComponent> LampLight;

	void Configure(
		const FWfcCellResult& Cell,
		const FWfcShellChoice& InShell,
		UWfcTileSet* InTileSet,
		int32 InWorldSeed,
		int32 InCellX,
		int32 InCellY,
		float InCellSize,
		float InDoorWidth,
		float InStandardHeight,
		float InTallHeight,
		float InDecalDensity,
		int32 InMaxFloorDecals,
		int32 InMaxWallDecals,
		bool bInBuildDecals,
		float InLightIntensityScale,
		const TArray<TSoftObjectPtr<UMaterialInterface>>& InFloorDecals,
		const TArray<TSoftObjectPtr<UMaterialInterface>>& InWallDecals);

	void UpdateLamp(int32 PlayerCellX, int32 PlayerCellY, int32 LightManhattan, int32 ShadowRank, int32 ShadowBudget);
	void ApplyLampSettings(float Scale, bool bForceOn);

	int32 GetCellX() const { return CellX; }
	int32 GetCellY() const { return CellY; }
	EWfcTopology GetTopology() const { return Result.Topology; }
	float DistanceToCell(int32 OtherX, int32 OtherY) const;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Z_Components", meta = (AdvancedDisplay))
	TArray<TObjectPtr<UStaticMeshComponent>> Pieces;

	UPROPERTY(VisibleAnywhere, Category = "Z_Components", meta = (AdvancedDisplay))
	TArray<TObjectPtr<UDecalComponent>> Decals;

	UPROPERTY(VisibleAnywhere, Category = "0_Config|Debug")
	FWfcCellResult Result;

	UPROPERTY(VisibleAnywhere, Category = "0_Config|Debug")
	FWfcShellChoice Shell;

	UPROPERTY(VisibleAnywhere, Category = "0_Config|Debug")
	TSoftObjectPtr<UWfcTileSet> SourceTileSet;

	UPROPERTY(VisibleAnywhere, Category = "0_Config|Debug")
	int32 CellX = 0;

	UPROPERTY(VisibleAnywhere, Category = "0_Config|Debug")
	int32 CellY = 0;

	UPROPERTY(VisibleAnywhere, Category = "0_Config|Debug")
	int32 WorldSeed = 1;

	UPROPERTY(VisibleAnywhere, Category = "0_Config|Debug")
	float CellSize = 800.f;

	UPROPERTY(VisibleAnywhere, Category = "0_Config|Debug")
	float DoorWidth = 240.f;

	UPROPERTY(VisibleAnywhere, Category = "0_Config|Debug")
	float StandardHeight = 400.f;

	UPROPERTY(VisibleAnywhere, Category = "0_Config|Debug")
	float TallHeight = 520.f;

	UPROPERTY(VisibleAnywhere, Category = "0_Config|Debug")
	float DecalDensity = 1.f;

	UPROPERTY(VisibleAnywhere, Category = "0_Config|Debug")
	int32 MaxFloorDecals = 1;

	UPROPERTY(VisibleAnywhere, Category = "0_Config|Debug")
	int32 MaxWallDecals = 2;

	UPROPERTY(VisibleAnywhere, Category = "0_Config|Debug")
	bool bBuildDecals = true;

	UPROPERTY(VisibleAnywhere, Category = "0_Config|Debug")
	float LightIntensityScale = 1.5f;

	UPROPERTY(VisibleAnywhere, Category = "0_Config|Debug")
	TArray<TSoftObjectPtr<UMaterialInterface>> FloorDecalMats;

	UPROPERTY(VisibleAnywhere, Category = "0_Config|Debug")
	TArray<TSoftObjectPtr<UMaterialInterface>> WallDecalMats;

	float RoomHeight = 400.f;

	void RebuildVisuals();
	void RefreshLamp();
	void ClearVisuals();
	void BuildShell(UWfcTileSet* TileSet);
	void BuildDecals(UWfcTileSet* TileSet);
	void PlaceDecal(UMaterialInterface* Mat, bool bWall, int32 Index, FRandomStream& Stream);
	UStaticMeshComponent* AddBox(
		const FName& Name,
		const FVector& Center,
		const FVector& Size,
		UMaterialInterface* Material,
		bool bBlockCamera,
		bool bAffectNav);
	void AddClosedWall(int32 Dir, float Thickness, UMaterialInterface* WallMat, bool bAlcove);
	void AddOpenDoor(int32 Dir, UMaterialInterface* WallMat, UMaterialInterface* TrimMat);
	void AddPillars(UMaterialInterface* WallMat);
	void AddInnerCornerFill(UMaterialInterface* WallMat, bool bColumn);
	void AddApse(UMaterialInterface* WallMat);
	FVector DirNormal(int32 Dir) const;
	FVector DirTangent(int32 Dir) const;
	float ChannelWidth() const;
};
