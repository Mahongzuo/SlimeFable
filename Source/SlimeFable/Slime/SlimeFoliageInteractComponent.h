// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeFoliageInteractComponent.generated.h"

class ACharacter;

/** One frame of foliage interaction data reported to the world subsystem / MPC. */
USTRUCT(BlueprintType)
struct FSlimeFoliageInteractSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Foliage")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Foliage")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Foliage")
	float Radius = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Foliage")
	float Strength = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Foliage")
	bool bPlayerControlled = false;
};

/**
 * Reports owner position / velocity / radius for interactive foliage WPO
 * (MPC_SlimeFoliage). Mounted on slime and enemies; morph-parked slime reports strength 0.
 */
UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent, PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API USlimeFoliageInteractComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeFoliageInteractComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** 关掉后本组件不再向 MPC 上报，草不受此角色影响。默认开。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Foliage",
		meta = (ToolTip = "开启后向 MPC_SlimeFoliage 上报位置/速度，驱动草 WPO 拨开。关掉等于此角色不压草。默认开。"))
	bool bEnableFoliageInteract = true;

	/** 拨开半径 = 胶囊半径 × 本倍率。略大于身体，走过两侧才有缝。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Foliage",
		meta = (ClampMin = "0.5", ClampMax = "4.0",
			ToolTip = "拨开半径 = max(胶囊, 史莱姆膜半径) × 倍率。默认 1.15，贴着身体；太大整片倒。材质按三维球体衰减，跳起后草会回弹。"))
	float RadiusScale = 1.15f;

	/** 材质侧梢端最大水平位移（厘米）。子系统会取玩家槽写入 MPC。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Foliage",
		meta = (ClampMin = "5.0", ClampMax = "160.0", Units = "cm",
			ToolTip = "草梢最大水平拨开距离（厘米）。默认 36。写入 MPC MaxBend，改完即时生效。"))
	float MaxBend = 36.f;

	/** 草高（厘米），用于梢端 HeightMask。太小则整株几乎不弯。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Foliage",
		meta = (ClampMin = "20.0", ClampMax = "200.0", Units = "cm",
			ToolTip = "草高（厘米）。默认 80。HeightMask = 梢端相对根的 Z / 本值。写入 MPC GrassHeight。"))
	float GrassHeight = 80.f;

	/** 梢端向下压倒距离（厘米）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Foliage",
		meta = (ClampMin = "0.0", ClampMax = "120.0", Units = "cm",
			ToolTip = "梢端最大下压（厘米）。默认 20。写入 MPC Flatten。站住也会压；跳起后随球体离开回弹。"))
	float Flatten = 20.f;

	/** 尾迹滞后秒数；材质用当前点→滞后点线段距离形成走过的缝。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Foliage",
		meta = (ClampMin = "0.05", ClampMax = "1.0", Units = "s",
			ToolTip = "尾迹滞后（秒）。默认 0.2。材质用 Pos−Vel×本值近似走过的缝。"))
	float TrailSeconds = 0.2f;

	/** 站住时径向推开强度倍率（相对全速）。避免站在草里穿模。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Foliage",
		meta = (ClampMin = "0.0", ClampMax = "1.0",
			ToolTip = "低速/站住时的径向推开强度倍率。默认 0.45；0=站住完全不拨。写入 MPC IdlePartStrength。"))
	float IdlePartStrength = 0.45f;

	/** 水平速度达到此值视为“全速”拨开（cm/s）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Foliage",
		meta = (ClampMin = "50.0", ClampMax = "1200.0", Units = "cm/s",
			ToolTip = "水平速度达到该值时强度视为 1。默认 400。低于此值按比例插值到 IdlePartStrength。"))
	float FullSpeedForStrength = 400.f;

	UFUNCTION(BlueprintPure, Category = "Foliage")
	FSlimeFoliageInteractSample GetLatestSample() const { return LatestSample; }

protected:
	void RebuildSample();

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter = nullptr;

	UPROPERTY(Transient)
	FSlimeFoliageInteractSample LatestSample;
};
