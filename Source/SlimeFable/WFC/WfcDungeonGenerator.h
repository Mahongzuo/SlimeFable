#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WFC/WfcSolver.h"
#include "WFC/WfcTypes.h"
#include "WfcDungeonGenerator.generated.h"

class AActor;
class AWFCDungeonTile;
class UAudioComponent;
class UMaterialInterface;
class USoundBase;
class UWfcTileSet;

UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API AWFCDungeonGenerator : public AActor
{
	GENERATED_BODY()

public:
	AWFCDungeonGenerator();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Generation",
		meta = (ToolTip = "世界种子。同一种子再点生成，路网/壳体/Decal/Prop 一致。换种子全变。"))
	int32 WorldSeed = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Generation",
		meta = (ClampMin = "4", ClampMax = "32",
			ToolTip = "固定图 +X 方向格子数。默认 16。点「生成」后外沿封死，不再长新块。"))
	int32 MapCellsX = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Generation",
		meta = (ClampMin = "4", ClampMax = "32",
			ToolTip = "固定图 +Y 方向格子数。默认 16。约 16×16×8m。"))
	int32 MapCellsY = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Generation",
		meta = (ClampMin = "0.0", ClampMax = "1.0",
			ToolTip = "随机度。0=更规整（直廊/转角多），1=更碎（死胡同/十字多）。默认 0.5。只影响下一次生成。"))
	float LayoutChaos = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Generation",
		meta = (ClampMin = "200.0", ClampMax = "2000.0", ToolTip = "单格边长，厘米。默认 800。邻格门洞对在格边上。"))
	float CellSize = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Assets",
		meta = (ToolTip = "瓦片/材质/Decal 表。空则用 /Game/_Slime/WFC/Data/DA_WfcDungeonTiles。"))
	TSoftObjectPtr<UWfcTileSet> TileSet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Shell",
		meta = (ToolTip = "十字间约 25% 抽挑高天花。关掉则全是标准层高。"))
	bool bAllowTallRooms = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Shell",
		meta = (ClampMin = "200.0", ClampMax = "800.0", ToolTip = "标准净高，厘米。默认 400。"))
	float StandardHeight = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Shell",
		meta = (ClampMin = "300.0", ClampMax = "900.0", ToolTip = "Tall 十字间净高。默认 520。"))
	float TallHeight = 520.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Shell",
		meta = (ClampMin = "120.0", ClampMax = "400.0", ToolTip = "门洞宽，必须全图一致才能对齐。默认 240。"))
	float DoorWidth = 240.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Dressing",
		meta = (ClampMin = "0.0", ClampMax = "3.0",
			ToolTip = "Decal 密度倍率。0=不贴。1=按地上/墙上每格上限。改完点「生成」。"))
	float DecalDensity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Dressing",
		meta = (ToolTip = "地上 Decal 池。默认 ResearchMegaPack 的 MI_Decal_1。空则回退 TileSet。拖材质到此数组即可。"))
	TArray<TSoftObjectPtr<UMaterialInterface>> FloorDecals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Dressing",
		meta = (ToolTip = "墙上 Decal 池。默认 ResearchMegaPack 的 MI_Decal_3。空则回退 TileSet。"))
	TArray<TSoftObjectPtr<UMaterialInterface>> WallDecals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Dressing",
		meta = (ClampMin = "0", ClampMax = "8", ToolTip = "每格地上 Decal 上限。再乘 DecalDensity。默认 1。"))
	int32 MaxFloorDecalsPerCell = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Dressing",
		meta = (ClampMin = "0", ClampMax = "8", ToolTip = "每格墙上 Decal 上限。再乘 DecalDensity。默认 2。"))
	int32 MaxWallDecalsPerCell = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Dressing",
		meta = (ToolTip = "打开则按下面 Props 在地板刷 Actor。空数组不刷（不再出灰盒方块）。再生成只清 WfcGenerated。"))
	bool bSpawnProps = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Dressing",
		meta = (ToolTip = "全局 Actor/Mesh 池。填任意蓝图类或 StaticMesh。空=不刷。不必点某个格子。"))
	TArray<FWfcPropEntry> Props;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Dressing",
		meta = (ClampMin = "0", ClampMax = "8", ToolTip = "每格最少刷几个 Props。Props 为空则无效。默认 0。"))
	int32 PropCountMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Dressing",
		meta = (ClampMin = "0", ClampMax = "8", ToolTip = "每格最多刷几个 Props。默认 2。"))
	int32 PropCountMax = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Light",
		meta = (ClampMin = "0.2", ClampMax = "4.0",
			ToolTip = "全局顶灯亮度倍率。1=原亮度（房间 22 / 走廊 14），1.5=略亮。改完点「应用灯光」或「生成」。"))
	float LightIntensityScale = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Light",
		meta = (ClampMin = "0", ClampMax = "8", ToolTip = "PIE 里曼哈顿距离 ≤ 此值的格子开顶灯。编辑器视口全开。默认 2。"))
	int32 LightManhattan = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Light",
		meta = (ClampMin = "0", ClampMax = "16", ToolTip = "PIE 最近若干盏才投影。默认 8。"))
	int32 ShadowCastingLights = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Audio",
		meta = (ToolTip = "恐怖氛围 BGM 列表。PIE 从第一首播，播完切下一首并循环。空则静音。"))
	TArray<TSoftObjectPtr<USoundBase>> BgmPlaylist;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "0_Config|Generation",
		meta = (DisplayName = "生成"))
	void GenerateDungeon();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "0_Config|Generation",
		meta = (DisplayName = "清除生成物"))
	void ClearGenerated();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "0_Config|Generation",
		meta = (DisplayName = "随机种子"))
	void RandomizeSeed();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "0_Config|Light",
		meta = (DisplayName = "应用灯光"))
	void ApplyLights();

	UFUNCTION(BlueprintCallable, Category = "Audio",
		meta = (ToolTip = "战斗时淡出探索 BGM；脱战后淡回。暂停时不切下一首。"))
	void SetExploreBgmDucked(bool bDucked);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", meta = (AdvancedDisplay))
	TObjectPtr<UAudioComponent> BgmComponent;

	UPROPERTY()
	TObjectPtr<UWfcTileSet> ResolvedTileSet;

	UPROPERTY(VisibleAnywhere, Category = "0_Config|Debug")
	TArray<TObjectPtr<AActor>> GeneratedActors;

	TMap<FIntPoint, TObjectPtr<AWFCDungeonTile>> LoadedTiles;

	FWfcSolver Solver;
	int32 BgmIndex = 0;
	bool bExploreBgmDucked = false;

	UWfcTileSet* ResolveTileSet();
	FIntPoint WorldToCell(const FVector& Location) const;
	FVector CellWorldLocation(int32 InCellX, int32 InCellY) const;
	void CollectBakedTiles();
	void DestroyGeneratedActors();
	void SpawnRectTiles(const TArray<FWfcCellResult>& Cells, int32 Width, int32 Height, UWfcTileSet* Set);
	void SpawnCellProps(const FWfcCellResult& Cell, int32 InCellX, int32 InCellY, UWfcTileSet* Set);
	void TagGenerated(AActor* Actor, const FString& Label) const;
	void UpdateLights(const FIntPoint& PlayerCell);
	APawn* FindFollowPawn() const;
	void StartBgm();
	void PlayBgmAt(int32 Index);

	UFUNCTION()
	void HandleBgmFinished();
};
