// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeFluidNinjaContactComponent.generated.h"

class USphereComponent;
class ACharacter;

/**
 * FluidNinja LIVE area sims (Pool / water) track causers via InteractionVolume Overlap:
 * - Mannequin: Pawn overlap → skeletal foot bones → line-trace onto TraceMesh → paint.
 * - Slime: procedural body, mesh hidden / no bones ticking → that path never paints.
 *
 * This component adds small WorldDynamic query-only spheres along the capsule height so
 * thin water InteractionVolumes still see contact points (foot + body), without feeding
 * TraceMesh back into soft-body squeeze.
 */
UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent, PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API USlimeFluidNinjaContactComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeFluidNinjaContactComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** 关掉后销毁接触球，史莱姆不再搅动 FluidNinja 水域。默认开。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|FluidNinja",
		meta = (ToolTip = "开启后在胶囊高度上挂小接触球，供 FluidNinja InteractionVolume 采样（脚印/身体）。关掉则完全不交互。默认开。"))
	bool bEnableFluidContacts = true;

	/** 每个接触球半径（cm）。决定脚印宽度主因之一；宜小以免水迹比身体宽。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|FluidNinja",
		meta = (ClampMin = "2.0", ClampMax = "40.0",
			ToolTip = "接触球半径（厘米）。痕迹宽度主要由本值与腰部环决定，不是 TraceMesh。默认 5。太大水迹偏宽；太小 overlap 不稳。"))
	float ContactRadius = 5.f;

	/** 沿竖直方向采样点数（脚底→头顶，必要时略高出胶囊）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|FluidNinja",
		meta = (ClampMin = "1", ClampMax = "12",
			ToolTip = "竖直采样点数。水面 InteractionVolume 很薄时，多高度才能命中；默认 3。"))
	int32 VerticalSampleCount = 3;

	/** 在胶囊顶之上再延伸多少 cm，覆盖略高于史莱姆的薄水面。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|FluidNinja",
		meta = (ClampMin = "0.0", ClampMax = "200.0",
			ToolTip = "相对胶囊顶再向上延伸的高度（厘米），用来碰到略高于史莱姆的薄 InteractionVolume。默认 40。"))
	float ExtraHeightAboveCapsule = 40.f;

	/** 身体水平环上额外点数（不含中轴）。0=只有中轴竖列（窄脚印）；2~4 更宽的身体搅水。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|FluidNinja",
		meta = (ClampMin = "0", ClampMax = "8",
			ToolTip = "腰部高度额外水平采样数（不含中轴）。0 只有竖列（默认，痕迹最窄）；加大环会明显加宽水迹。"))
	int32 BodyRingSampleCount = 0;

	/** 腰部环相对胶囊半径的比例（0~1）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|FluidNinja",
		meta = (ClampMin = "0.1", ClampMax = "1.0",
			ToolTip = "腰部环半径 = 胶囊半径 × 该比例。仅 BodyRingSampleCount>0 时生效。默认 0.65。"))
	float BodyRingRadiusScale = 0.65f;

	/**
	 * 让史莱姆胶囊忽略 TraceMesh / InteractionVolume 的 Block，避免被挤出水面盒子。
	 * 不会忽略 ActivationVolume：Ninja 靠 Pawn 进入 ActivationVolume 才把 TraceMesh 从 Inactive 灰/白盒切到流体材质。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|FluidNinja",
		meta = (ToolTip = "开启后史莱姆胶囊 IgnoreComponentWhenMoving TraceMesh/InteractionVolume（防挤出）。不含 ActivationVolume，否则流体永远停在 Inactive 白盒。默认开。"))
	bool bPawnPassthroughNinjaSimGeom = true;

	/** 重新扫描世界中 NinjaLive 仿真几何的间隔（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|FluidNinja",
		meta = (ClampMin = "0.1", ClampMax = "5.0",
			ToolTip = "多久重扫一次关卡里的 NinjaLive TraceMesh。默认 0.5 秒。"))
	float NinjaPassthroughRescanInterval = 0.5f;

protected:
	void RebuildContacts();
	void DestroyContacts();
	void SyncContactTransforms();
	USphereComponent* CreateContactSphere(const FName& Name);
	void ApplyFluidNinjaPawnPassthrough();

	/** TraceMesh / InteractionVolume — move-ignore only (do NOT include ActivationVolume). */
	static bool IsFluidNinjaBlockingSimGeom(const UPrimitiveComponent* Component);

	UPROPERTY(Transient)
	TArray<TObjectPtr<USphereComponent>> ContactSpheres;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter = nullptr;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UPrimitiveComponent>> PassthroughIgnoredComponents;

	float PassthroughRescanTimer = 0.f;
};
