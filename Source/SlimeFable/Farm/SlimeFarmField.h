// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeFarmField.generated.h"

class UInstancedStaticMeshComponent;

/**
 * One placeable farm. Editor shows a soil grid; play spawns the real plots,
 * and optionally the seed crate and calendar gate if the level does not already have them.
 */
UCLASS(Blueprintable, meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeFarmField : public AActor
{
	GENERATED_BODY()

public:
	ASlimeFarmField();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Farm",
		meta = (ClampMin = "1", ClampMax = "8",
			ToolTip = "田地有几排。默认 4。改行列会换掉地块存档键，已经种下的进度会对不上。"))
	int32 Rows = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Farm",
		meta = (ClampMin = "1", ClampMax = "12",
			ToolTip = "每排几块地。默认 5。和 Rows、Spacing 一起决定整块田的占地。"))
	int32 Cols = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Farm",
		meta = (ClampMin = "80.0", Units = "cm",
			ToolTip = "沿田地长边的中心间距，厘米。默认 720，对应约 7 米的土垄再留一条缝，避免两块土面贴在一起。"))
	float Spacing = 720.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Farm",
		meta = (ClampMin = "50.0", Units = "cm",
			ToolTip = "沿田地短边的中心间距，厘米。默认 120，土垄窄边大约 1 米。"))
	float SpacingY = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Farm",
		meta = (ToolTip = "地块存档键前缀。实际键是 前缀_00、前缀_01。同一块田不要改这个名字，否则进度读不回来。默认 MuseumPlot。"))
	FString PlotIdPrefix = TEXT("MuseumPlot");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Farm",
		meta = (ToolTip = "勾选后，进游戏时如果关卡里还没有种子箱，就在这块田旁边生成一个。已经摆过种子箱就不会再生成。"))
	bool bSpawnSeedCrate = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Farm",
		meta = (ToolTip = "种子箱相对这块田原点的位置，厘米，跟随着田的朝向。默认在田的前方偏左。"))
	FVector SeedCrateOffset = FVector(-450.f, -800.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Farm",
		meta = (ToolTip = "勾选后，进游戏时如果关卡里还没有时光门，就在这块田旁边生成一个。已经摆过时光门就不会再生成。"))
	bool bSpawnDayGate = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Farm",
		meta = (ToolTip = "时光门相对这块田原点的位置，厘米，跟随着田的朝向。默认在田的前方偏右。"))
	FVector DayGateOffset = FVector(-450.f, 800.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Farm",
		meta = (ToolTip = "勾选后，地块、种子箱、时光门会向下射线贴到地面。射线打不中就用田本身的高度。"))
	bool bSnapToGround = true;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Z_Components", meta = (AdvancedDisplay))
	TObjectPtr<UInstancedStaticMeshComponent> PreviewSoil;

	void RebuildPreview();
	FVector PlotLocalOffset(int32 Row, int32 Col) const;
	FVector ResolveWorldLocation(const FVector& LocalOffset) const;

	bool bDidSpawn = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedActors;
};
