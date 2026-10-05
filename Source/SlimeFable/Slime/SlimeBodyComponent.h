// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeSolver.h"
#include "SlimeSurfaceBuilder.h"
#include "SlimeTypes.h"
#include "Settings/SlimeGraphicsTypes.h"
#include "RHITypes.h"
#include "SlimeBodyComponent.generated.h"

class ACharacter;
class UCapsuleComponent;
class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlimeSqueezeChanged, float, SqueezeAmount);

/**
 *  Drives the semi fluid body: fixed step simulation, world collider gathering, surface
 *  rebuild, and the adaptive capsule that lets the character actually enter a gap narrow
 *  enough to deform in.
 *
 *  Squeeze probing lives here rather than in its own component so it can reuse the collider
 *  set and floor trace this component already pays for.
 */
UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent, PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API USlimeBodyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeBodyComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---- Configuration ---------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Solver", meta = (ShowOnlyInnerProperties))
	FSlimeSolverParams SolverParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Surface", meta = (ShowOnlyInnerProperties))
	FSlimeSurfaceParams SurfaceParams;

	/** Material for the surface mesh. Falls back to BodyMaterialPath, then to a plain colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Surface")
	TObjectPtr<UMaterialInterface> BodyMaterial;

	UPROPERTY(EditAnywhere, Category = "Slime|Surface")
	TSoftObjectPtr<UMaterialInterface> BodyMaterialPath =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Characters/Slime/Materials/M_SlimeBody.M_SlimeBody")));

	/** Default body material (ESlimeBodySkin::Spectral). Classic falls back to BodyMaterialPath. */
	UPROPERTY(EditAnywhere, Category = "Slime|Surface")
	TSoftObjectPtr<UMaterialInterface> SpectralBodyMaterialPath =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Characters/Slime/Materials/M_SlimeBody_Spectral.M_SlimeBody_Spectral")));

	/**
	 *  Third skin (ESlimeBodySkin::Volumetric): ray marches the body density field uploaded to
	 *  DensityAtlas every rebuild. Missing asset falls back to the spectral skin.
	 */
	UPROPERTY(EditAnywhere, Category = "Slime|Surface")
	TSoftObjectPtr<UMaterialInterface> VolumetricBodyMaterialPath =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Characters/Slime/Materials/M_SlimeBody_Volumetric.M_SlimeBody_Volumetric")));

	/** Fourth skin: exposure-compensated readable jelly, no density atlas. */
	UPROPERTY(EditAnywhere, Category = "0_Config|Surface", meta = (ToolTip = "莹光果冻母材质或实例。默认 M_SlimeBody_Luminous；按 7 切换，缺失时回退光谱／经典。只影响第四种皮肤。"))
	TSoftObjectPtr<UMaterialInterface> LuminousBodyMaterialPath =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Characters/Slime/Materials/M_SlimeBody_Luminous.M_SlimeBody_Luminous")));

	/** Opaque material on the hidden shadow-proxy mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Surface")
	TObjectPtr<UMaterialInterface> ShadowCasterMaterial;

	UPROPERTY(EditAnywhere, Category = "Slime|Surface")
	TSoftObjectPtr<UMaterialInterface> ShadowCasterMaterialPath =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Characters/Slime/Materials/M_SlimeShadowCaster.M_SlimeShadowCaster")));

	/** Translucent X-ray silhouette when occluded by world geometry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Surface")
	TObjectPtr<UMaterialInterface> XRayMaterial;

	UPROPERTY(EditAnywhere, Category = "Slime|Surface")
	TSoftObjectPtr<UMaterialInterface> XRayMaterialPath =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_SlimeXRay.M_SlimeXRay")));

	UPROPERTY(EditAnywhere, Category = "0_Config|XRay", meta = (
		ToolTip = "穿墙描边用屏幕空间剪影：身体网格只写 CustomDepth，相机后处理按像素描边，挤压和摊开都跟着变形。关掉就回到旧的菲涅尔网格。默认开。"))
	bool bScreenSpaceXRay = true;

	UPROPERTY(EditAnywhere, Category = "0_Config|XRay", meta = (EditCondition = "bScreenSpaceXRay",
		ToolTip = "穿墙描边后处理材质。默认 M_SlimeXRayOutlinePP，挂在本地玩家相机上。XRayColor 跟着当前元素色。"))
	TSoftObjectPtr<UMaterialInterface> XRayOutlineMaterialPath =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_SlimeXRayOutlinePP.M_SlimeXRayOutlinePP")));

	UPROPERTY(EditAnywhere, Category = "0_Config|XRay", meta = (EditCondition = "bScreenSpaceXRay",
		ToolTip = "穿墙剪影网格的材质。半透明并关闭深度测试，史莱姆完全躲在墙后也不会被遮挡剔除，仍写入 CustomDepth。默认 M_SlimeXRayDepthProxy。加载失败时退回不透明默认材质，墙后描边会再次丢失。"))
	TSoftObjectPtr<UMaterialInterface> XRayDepthProxyMaterialPath =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_SlimeXRayDepthProxy.M_SlimeXRayDepthProxy")));

	UPROPERTY(EditAnywhere, Category = "0_Config|XRay", meta = (EditCondition = "bScreenSpaceXRay", ClampMin = "0.0", ClampMax = "200.0",
		ToolTip = "墙要比史莱姆近多少厘米才开始画描边。默认 3。调大则贴着薄墙时更不容易透出，调小则栅栏背面也会显示。传给后处理材质的 OcclusionBias。"))
	float XRayOcclusionBias = 3.f;

	UPROPERTY(EditAnywhere, Category = "0_Config|XRay", meta = (EditCondition = "bScreenSpaceXRay", ClampMin = "0.1", ClampMax = "200.0",
		ToolTip = "描边从刚出现到完全不透明要再近多少厘米。默认 6。和 XRayOcclusionBias 一起传给后处理材质的 OcclusionRamp。"))
	float XRayOcclusionRamp = 6.f;

	/** Solver steps per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Stepping", meta = (ClampMin = "20.0", ClampMax = "90.0"))
	float StepRate = 60.f;

	/** Surface rebuilds per second. Match StepRate so mesh resolution does not stutter against the solve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Stepping", meta = (ClampMin = "5.0", ClampMax = "90.0"))
	float SurfaceRate = 60.f;

	/** Guards against a spiral of death after a hitch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Stepping", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxStepsPerFrame = 2;

	// ---- World collision -------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Collision", meta = (ClampMin = "2", ClampMax = "48"))
	int32 MaxWorldColliders = 28;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Collision", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float ColliderRefreshInterval = 0.25f;

	/** Refresh early when the body has moved this far since the last gather, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Collision", meta = (ClampMin = "2.0"))
	float ColliderRefreshDistance = 35.f;

	/** Query box scale over the body bounds. Above 1 so walls enter the set before contact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Collision", meta = (ClampMin = "1.0", ClampMax = "3.0"))
	float ColliderQueryScale = 1.5f;

	/** Extra query padding around launched chunks so distant walls stay in the collider set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Collision", meta = (ClampMin = "4.0", ClampMax = "80.0"))
	float FragmentColliderRadius = 18.f;

	/** Analytic floor-proxy radius under the chunk COM (≈ RestRadius * 0.45 when left at default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Collision", meta = (ClampMin = "2.0", ClampMax = "40.0"))
	float FragmentProxyRadius = 12.f;

	// ---- Adaptive capsule and squeeze ------------------------------------------------

	/** Without this the movement component blocks the character out of any gap narrower than the capsule. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze")
	bool bAdaptiveCapsule = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "8.0"))
	float DefaultCapsuleRadius = 32.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "8.0"))
	float DefaultCapsuleHalfHeight = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "4.0"))
	float MinCapsuleRadius = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "4.0"))
	float MinCapsuleHalfHeight = 6.f;

	/** Time constant while shrinking. Fast, so a gap does not stop the character dead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float ShrinkTime = 0.08f;

	/** Time constant while growing back. Slow, so the mouth of a gap does not chatter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "0.02", ClampMax = "2.0"))
	float RecoverTime = 0.3f;

	/** Probe this far along the velocity. Reacting on contact is already too late. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "0.0", ClampMax = "150.0"))
	float LookAheadDistance = 40.f;

	/** Walk speed multiplier at full squeeze. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float SqueezeSpeedScale = 0.6f;

	/** When heavily squeezed with move input, crawl out of a pinch at this speed, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "20.0", ClampMax = "300.0"))
	float OozeSpeed = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "1", ClampMax = "6"))
	int32 RadiusProbeIterations = 3;

	/** Where the anchor sits above the feet, as a fraction of RestRadius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "0.1", ClampMax = "1.5"))
	float AnchorHeightFraction = 0.5f;

	/** Horizontal distance at which the capsule gets dragged back towards the blob. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Squeeze", meta = (ClampMin = "20.0"))
	float MaxAnchorDistance = 160.f;

	// ---- Pancake ---------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "1.0", ClampMax = "8.0"))
	float SpreadRadiusScale = 5.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.0"))
	float SpreadPush = 300.f;

	/** Half-thickness of the pancake disk, in cm. Thin sheet ~ SIM DomeHeight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.5", ClampMax = "12.0"))
	float SpreadHalfHeight = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float SpreadGravityScale = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.05", ClampMax = "3.0",
		ToolTip = "按下摊开后，从球体压扁成薄饼所需秒数。默认 0.45。越大压得越慢。"))
	float SpreadFlattenTime = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.05", ClampMax = "3.0",
		ToolTip = "松开摊开后，从薄饼平滑收回球体所需秒数。默认 0.7。松开立刻开始收，下垂的部分同步被吸回。越大吸回越慢。"))
	float SpreadRecoverDuration = 0.7f;

	/** Extra XY splat while pancaked so the puddle stays one visual sheet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float SpreadSplatMultiplier = 2.0f;

	/** Vertical splat scale while pancaked (keeps the pie thin). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.25", ClampMax = "1.0"))
	float SpreadSplatZScale = 0.55f;

	/** Concentration multiplier while spread (keeps the centre filled). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float SpreadConcentrationScale = 1.15f;

	UPROPERTY(EditAnywhere, Category = "0_Config|Spread", meta = (ClampMin = "1.0", ClampMax = "4.0",
		ToolTip = "摊开时粒子水平间距相对静止间距的倍率，默认2.2。越大摊得越开、面积越大；1 = 不放宽（老行为）。"))
	float SpreadLateralScale = 2.2f;

	UPROPERTY(EditAnywhere, Category = "0_Config|Spread", meta = (
		ToolTip = "摊开时用体积守恒的薄饼高度场出网格：中间厚、边缘薄。关掉回到粒子球拼接（老行为）。"))
	bool bSpreadSheet = true;

	UPROPERTY(EditAnywhere, Category = "0_Config|Spread", meta = (ClampMin = "0.2", ClampMax = "3.0", EditCondition = "bSpreadSheet",
		ToolTip = "薄饼总体积相对静止球体积的倍率，默认1.0；>1 更厚，<1 更薄。"))
	float SpreadSheetVolumeScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "0_Config|Spread", meta = (ClampMin = "0.5", ClampMax = "4.0", EditCondition = "bSpreadSheet",
		ToolTip = "薄饼高度场的水平平滑核半径，相对粒子间距（含摊开放宽）的倍率，默认1.6。越大越圆润平滑，越小越贴粒子起伏。"))
	float SpreadSheetKernelScale = 1.6f;

	UPROPERTY(EditAnywhere, Category = "0_Config|Spread", meta = (ClampMin = "0.0", ClampMax = "5.0", Units = "cm", EditCondition = "bSpreadSheet",
		ToolTip = "薄饼比这个厚度更薄的地方当作没有粘液，厘米，默认0.3；决定边缘在哪里收口。"))
	float SpreadSheetMinThickness = 0.3f;

	UPROPERTY(EditAnywhere, Category = "0_Config|Spread", meta = (ClampMin = "1.0", ClampMax = "50.0", Units = "cm", EditCondition = "bSpreadSheet",
		ToolTip = "粒子比所在位置薄饼底面低出这么多厘米就当作垂下边缘的粘液，改用普通粒子球画，默认6。"))
	float SheetDrapeDepth = 6.f;

	/** 越过边缘的粘液最多垂到边缘下方多少厘米（硬上限）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.0", ClampMax = "200.0",
		ToolTip = "越过柱子/台阶边缘的粘液，最多垂到边缘下方多少厘米。默认 35，是硬上限；平时垂多深主要由 SpreadDrapeTension 决定。下方地面更高时会先落到地面上。"))
	float SpreadDrapeDepth = 35.f;

	/** 下垂粘液被拉回边缘的张力（1/秒²）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.0", ClampMax = "2000.0",
		ToolTip = "越过边缘的粘液往上拉回的张力，垂得越深拉得越紧。默认 140：静止时大约垂 18 厘米。越大挂得越浅、越粘；0 = 只受重力。"))
	float SpreadDrapeTension = 140.f;

	/** 下垂粘液的粘滞（1/秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.0", ClampMax = "400.0",
		ToolTip = "越过边缘的粘液流动的粘稠程度。默认 90：约 0.6 秒慢慢垂到位。越大流得越慢，按住越久垂得越深。"))
	float SpreadDrapeViscosity = 90.f;

	/** 粘液最多伸出支撑边缘多少厘米（水平）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.0", ClampMax = "100.0",
		ToolTip = "摊开时粘液最多水平伸出站立面边缘多少厘米，之外会被拉回边缘，保持连成一片。默认 12。平地上没有边缘，不受影响。"))
	float SpreadMaxOverhang = 12.f;

	/** 相邻两格地面高差不超过这个值算同一块站立面（厘米）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "2.0", ClampMax = "40.0",
		ToolTip = "相邻两格地面落差不超过多少厘米，仍算同一块可铺开的面（缓坡、小台阶）。默认 8。更大的落差当作边缘，粘液从那里垂下；向上更高的台阶当作墙。"))
	float SpreadStepHeight = 8.f;

	/** 比脚下地面再高多少仍会被地面射线检测到（厘米）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.0", ClampMax = "80.0",
		ToolTip = "地面射线从脚下地面往上多少厘米开始打。默认 20。旁边矮于这个高度的物体会被当成墙或可铺开的面；更高的会被忽略。"))
	float SpreadClimbHeight = 20.f;

	/** 摊开地面高度场的格子边长（厘米）。默认 10。越小越贴边，射线越多。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "4.0", ClampMax = "40.0",
		ToolTip = "摊开地面高度场的格子边长（厘米）。默认 10。越小越贴柱子边缘，向下射线越多。格子数有上限，设得过小会自动加粗格子以盖住摊开半径。"))
	float SpreadGroundCellSize = 10.f;

	/** 高度场刷新间隔（秒）。默认 0.1。质心移动超过半格也会立刻刷新。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread", meta = (ClampMin = "0.02", ClampMax = "0.5",
		ToolTip = "摊开地面高度场的刷新间隔（秒）。默认 0.1。质心水平移动超过半格时会立刻重采。"))
	float SpreadGroundRefreshInterval = 0.1f;

	/** 摊开时按局部地面下垂。关掉则恢复以质心为中心的水平薄饼。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Spread",
		meta = (ToolTip = "摊开时按脚下局部地面铺开，越过边缘的部分像粘液一样垂下并被拉住。默认开。关掉则恢复整片水平薄饼（悬在碰撞高度上）。"))
	bool bSpreadFollowTerrain = true;

	// ---- Lighting --------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Lighting",
		meta = (ToolTip = "史莱姆材质里的常数颜色（本体填充色、边缘光、虹彩、气泡、脸）按所处环境光压暗，避免夜间、洞穴里被自动曝光放大而发白。关掉则系数恒为 1（旧效果）。"))
	bool bAmbientLightResponse = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Lighting", meta = (ClampMin = "0.001",
		ToolTip = "照度参考值。原始照度（方向光 + 天光，已乘遮挡）达到这个值时系数为 1。用 slime.AmbientDebug 1 在白天露天处读数后，设为读数的一半左右。"))
	float AmbientReference = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Lighting", meta = (ClampMin = "0.0", ClampMax = "1.0",
		ToolTip = "系数下限。夜间、洞穴里最暗也保留这么多常数颜色，防止史莱姆完全变黑。默认 0.08。"))
	float AmbientFloor = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Lighting", meta = (ClampMin = "0.0",
		ToolTip = "天光在原始照度里的权重（乘在天光 Intensity 上）。默认 1。"))
	float AmbientSkyWeight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Lighting", meta = (ClampMin = "100.0",
		ToolTip = "判断露天程度的向上射线长度（厘米）。默认 3000。头顶这么远内有遮挡就算不露天（洞穴、室内）。"))
	float AmbientSkyTraceLength = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Lighting", meta = (ClampMin = "100.0",
		ToolTip = "判断是否在太阳 / 月亮阴影里的射线长度（厘米）。默认 20000。"))
	float AmbientSunTraceLength = 20000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Lighting", meta = (ClampMin = "0.02", ClampMax = "2.0",
		ToolTip = "光照估算的间隔（秒）。默认 0.15。每次只打一部分射线，轮换完成一整轮。"))
	float AmbientTraceInterval = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Lighting", meta = (ClampMin = "0.1", ClampMax = "20.0",
		ToolTip = "系数追随目标的速度。默认 2，约半秒过渡；越大进出洞穴、阴影时变化越快。"))
	float AmbientSmoothSpeed = 2.f;

	// ---- Landing ---------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Landing", meta = (ClampMin = "100.0"))
	float LandingSquashMinSpeed = 280.f;

	// ---- Launch and recall -----------------------------------------------------------

	/** Clone particle count as a fraction of the body particle budget (~30% mini slime). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "0.05", ClampMax = "0.6"))
	float LaunchFraction = 0.3f;

	/** Max simultaneous unrecovered clone shots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "1", ClampMax = "12"))
	int32 MaxActiveShots = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "0.5"))
	float FragmentLifetime = 10.f;

 UPROPERTY(EditAnywhere, Category="0_Config|Fragments", meta=(ClampMin="0.02", ToolTip="G发射拉颈分离时间，默认0.22秒。"))
 float ShotSeparationSeconds = 0.22f;
 UPROPERTY(EditAnywhere, Category="0_Config|Fragments", meta=(ClampMin="0.1", ToolTip="G子球寿命末尾用于跳回融合的时间，默认3秒。"))
 float ShotReturnSeconds = 3.f;
 UPROPERTY(EditAnywhere, Category="0_Config|Fragments", meta=(ClampMin="0", ToolTip="回程小跳高度，单位厘米，默认20。"))
 float ShotReturnHopHeight = 20.f;
 UPROPERTY(EditAnywhere, Category="0_Config|Surface", meta=(ClampMin="0", ClampMax="1", ToolTip="从背面透过身体看到正面眼睛的强度，默认0.85（保留正面85%亮度）。"))
 float RearEyeStrength = 0.85f;
 UPROPERTY(EditAnywhere, Category="0_Config|Surface", meta=(ClampMin="0", ToolTip="眼睛独立自发光强度，默认2，四种皮肤共用并做曝光补偿。"))
 float EyeEmissionStrength = 2.f;
 UPROPERTY(EditAnywhere, Category="0_Config|Surface", meta=(ClampMin="0", ClampMax="1", ToolTip="眼睛补光的环境亮度下限，默认0.65，越高暗处眼睛越清楚。"))
 float EyeReadabilityFloor = 0.65f;
 UPROPERTY(EditAnywhere, Category="0_Config|Bubbles", meta=(ClampMin="1", ToolTip="气泡随机放大的最小倍率，默认2，与最大倍率联动。"))
 float BubbleSizeScaleMin = 2.f;
 UPROPERTY(EditAnywhere, Category="0_Config|Bubbles", meta=(ClampMin="1", ToolTip="气泡随机放大的最大倍率，默认5；常驻气泡每次重生重新取值。"))
 float BubbleSizeScaleMax = 5.f;
 UPROPERTY(EditAnywhere, Category="0_Config|Bubbles", meta=(ClampMin="0", ToolTip="气泡自身菲涅尔边缘高光倍率，默认4。"))
 float BubbleEdgeStrength = 4.f;
 UPROPERTY(EditAnywhere, Category="0_Config|Bubbles", meta=(ClampMin="0.1", ClampMax="2", ToolTip="常驻气泡在随机半径基础上的显示倍率，默认0.65；降低可减少对表情的遮挡。"))
 float BubbleVisualRadiusScale = 0.65f;
 UPROPERTY(EditAnywhere, Category="0_Config|Bubbles", meta=(ClampMin="0.1", ClampMax="2", ToolTip="吞噬气泡在常驻显示半径基础上的倍率，默认0.55；形成物品周围的细泡与上升气流。"))
 float DevourBubbleRadiusScale = 0.55f;
 UPROPERTY(EditAnywhere, Category="0_Config|Bubbles", meta=(ClampMin="0.02", ClampMax="0.4", ToolTip="吞噬气泡穿出身体上缘后继续上升的距离，相对该处身体高度，默认0.12；随后膨胀破裂。"))
 float DevourBubbleOverflowHeight = 0.12f;
 UPROPERTY(EditAnywhere, Category="0_Config|Bubbles", meta=(ClampMin="0.01", ToolTip="常驻气泡到顶部后停留并膨胀破裂的秒数，默认0.25；结束后从底部重生。"))
 float BubblePopSeconds = 0.25f;
 UPROPERTY(EditAnywhere, Category="0_Config|Bubbles", meta=(ClampMin="0.005", ClampMax="0.25", ToolTip="气泡外缘亮边宽度相对半径的比例，默认0.12；控制球面菲涅尔亮边范围。"))
 float BubbleRimWidth = 0.12f;
 UPROPERTY(EditAnywhere, Category="0_Config|Bubbles", meta=(ClampMin="0", ToolTip="气泡偏心高光倍率，默认0.12，同时用于常驻及吞噬爆发气泡。"))
 float BubbleCoreStrength = 0.12f;
 UPROPERTY(EditAnywhere, Category="0_Config|Bubbles", meta=(ClampMin="0", ClampMax="32", ToolTip="常驻细小气泡数量，默认32，从身体下部上升。"))
 int32 FineBubbleCount = 32;
 UPROPERTY(EditAnywhere, Category="0_Config|Bubbles", meta=(ClampMin="0.01", ToolTip="细小气泡最小半径，厘米，默认0.15。"))
 float FineBubbleMinRadius = 0.15f;
 UPROPERTY(EditAnywhere, Category="0_Config|Bubbles", meta=(ClampMin="0.01", ToolTip="细小气泡最大半径，厘米，默认0.45。"))
 float FineBubbleMaxRadius = 0.45f;
 UPROPERTY(EditAnywhere, Category="0_Config|Bubbles", meta=(ClampMin="0", ClampMax="64", ToolTip="吞噬爆发气泡总上限，默认64，连续两次吞噬共享上限。"))
 int32 DevourBubbleCount = 64;
 UPROPERTY(EditAnywhere, Category="0_Config|Bubbles", meta=(ClampMin="0.1", ToolTip="吞噬细泡爆发持续秒数，默认1.5。"))
 float DevourBubbleSeconds = 1.5f;
 UPROPERTY(EditAnywhere, Category="0_Config|Bubbles", meta=(ToolTip="莹光果冻皮肤下把常驻气泡换成汽水气泡柱：更多、更小、更快，聚在身体一侧上升，只画高光点和淡边。默认开；其它皮肤不受影响。"))
 bool bFizzBubbles = true;
 UPROPERTY(EditAnywhere, Category="0_Config|Bubbles", meta=(ClampMin="0", ClampMax="96", EditCondition="bFizzBubbles", ToolTip="汽水气泡数量，默认64，替代常驻气泡数量（FineBubbleCount）。"))
 int32 FizzBubbleCount = 64;
 UPROPERTY(EditAnywhere, Category="0_Config|Bubbles", meta=(ClampMin="0.05", ClampMax="2", EditCondition="bFizzBubbles", ToolTip="汽水气泡半径相对常驻气泡的倍率，默认0.5。"))
 float FizzRadiusScale = 0.5f;
 UPROPERTY(EditAnywhere, Category="0_Config|Bubbles", meta=(ClampMin="0", ClampMax="0.4", EditCondition="bFizzBubbles", ToolTip="气泡柱横向散开半径（相对身体半轴），默认0.12；越大气泡柱越粗。"))
 float FizzColumnSpread = 0.12f;

 UPROPERTY(EditAnywhere, Category="0_Config|Ground", meta=(ToolTip="站在地上时身体底部向外翻出一圈贴地裙边（弯月面）。只改显示网格，碰撞和体积不变。默认开。"))
 bool bGroundSkirt = true;
 UPROPERTY(EditAnywhere, Category="0_Config|Ground", meta=(ClampMin="0", ClampMax="30", Units="cm", EditCondition="bGroundSkirt", ToolTip="裙边影响的离地高度，厘米，默认5；至少会覆盖1.5个网格单元。"))
 float SkirtHeight = 5.f;
 UPROPERTY(EditAnywhere, Category="0_Config|Ground", meta=(ClampMin="0", ClampMax="30", Units="cm", EditCondition="bGroundSkirt", ToolTip="裙边贴地处向外扩出的距离，厘米，默认8；越大底部越像摊开的水滴。"))
 float SkirtSpread = 8.f;
 UPROPERTY(EditAnywhere, Category="0_Config|Ground", meta=(ToolTip="脚下接触贴花：中心接触暗部 + 边缘一圈元素色透射光斑 + 湿润反光。默认开。"))
 bool bContactDecal = true;
 UPROPERTY(EditAnywhere, Category="0_Config|Ground", meta=(EditCondition="bContactDecal", ToolTip="接触贴花材质，默认 M_SlimeContactDecal；颜色每帧从身体材质的 BaseColor 同步。"))
 TSoftObjectPtr<UMaterialInterface> ContactDecalMaterialPath =
  TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Characters/Slime/Materials/M_SlimeContactDecal.M_SlimeContactDecal")));
 UPROPERTY(EditAnywhere, Category="0_Config|Shape", meta=(ClampMin="0.8", ClampMax="2.5", ToolTip="扁圆顶形态（按 8 切换）下身体水平方向相对圆球的倍率，默认1.3；碰撞胶囊不变，宽出来的部分贴墙会被压扁。"))
 float DomeWidthScale = 1.3f;
 UPROPERTY(EditAnywhere, Category="0_Config|Shape", meta=(ClampMin="0.25", ClampMax="1.2", ToolTip="扁圆顶形态下身体高度相对圆球的倍率，默认0.72；越小越扁。"))
 float DomeHeightScale = 0.72f;
 UPROPERTY(EditAnywhere, Category="0_Config|Shape", meta=(ClampMin="0", ClampMax="1", ToolTip="扁圆顶形态下往上撑回原形的力相对圆球的倍率，默认0.5；越小越在重力下塌、晃得越软，太小会像一摊水。"))
 float DomeUpwardRestoreScale = 0.5f;
 UPROPERTY(EditAnywhere, Category="0_Config|Shape", meta=(ClampMin="0.2", ClampMax="1.2", ToolTip="扁圆顶形态下身体质心锚点高度相对圆球的倍率，默认0.78；让矮身体坐在地面上而不是悬着。"))
 float DomeAnchorHeightScale = 0.78f;
 UPROPERTY(EditAnywhere, Category="0_Config|Shape", meta=(ClampMin="0.01", ClampMax="3", Units="s", ToolTip="圆球与扁圆顶之间切换的过渡时间，秒，默认0.35。"))
 float ShapeBlendTime = 0.35f;
 UPROPERTY(EditAnywhere, Category="0_Config|Shape", meta=(ToolTip="扁圆顶贴地时，从身体中下部开始向外摆成钟形裙边。只改显示网格，碰撞不变。默认开。"))
 bool bDomeFlare = true;
 UPROPERTY(EditAnywhere, Category="0_Config|Shape", meta=(ClampMin="0.1", ClampMax="0.8", EditCondition="bDomeFlare", ToolTip="钟形外摆从离地多高开始，相对身体离地高度的比例，默认0.45；越大越从高处就开始变宽。"))
 float DomeFlareHeightFraction = 0.45f;
 UPROPERTY(EditAnywhere, Category="0_Config|Shape", meta=(ClampMin="0", ClampMax="40", Units="cm", EditCondition="bDomeFlare", ToolTip="贴地处比身体截面多宽出的距离，厘米，默认14。"))
 float DomeFlareReach = 14.f;
 UPROPERTY(EditAnywhere, Category="0_Config|Shape", meta=(ClampMin="1", ClampMax="4", EditCondition="bDomeFlare", ToolTip="外摆曲线，默认2；越大越内凹像裙摆，1 接近直线斜坡。"))
 float DomeFlareCurve = 2.f;
 UPROPERTY(EditAnywhere, Category="0_Config|Shape", meta=(ClampMin="0", ClampMax="6", Units="cm", EditCondition="bDomeFlare", ToolTip="贴地边缘圆头的半径，厘米，默认1.5；避免最外缘收成刀刃。"))
 float DomeFlareTip = 1.5f;
 UPROPERTY(EditAnywhere, Category="0_Config|Ground", meta=(ClampMin="1", ClampMax="3", EditCondition="bContactDecal", ToolTip="外圈柔和阴影相对真实接触面（贴地/贴墙粒子拟合的椭圆）的倍率，默认1.25；1 = 只在接触面内，不外扩。"))
 float ContactShadowExtent = 1.25f;
 UPROPERTY(EditAnywhere, Category="0_Config|Ground", meta=(ClampMin="0", ClampMax="20", Units="cm", EditCondition="bContactDecal", ToolTip="接触面椭圆每个半轴额外外扩的厘米数，默认2；补上粒子外侧果冻表面露出的那一圈。"))
 float ContactFootprintPadding = 2.f;
 UPROPERTY(EditAnywhere, Category="0_Config|Ground", meta=(ClampMin="1", ClampMax="30", Units="cm", EditCondition="bContactDecal", ToolTip="接触贴花的投影深度（半厚），厘米，默认6；越小越不会投到旁边的侧墙和台阶立面。"))
 float ContactDepth = 6.f;
 UPROPERTY(EditAnywhere, Category="0_Config|Ground", meta=(ClampMin="0", ClampMax="1", Units="s", EditCondition="bContactDecal", ToolTip="接触面尺寸、位置的平滑时间，秒，默认0.1；0 = 不平滑（可能抖动）。"))
 float ContactSmoothing = 0.1f;
 UPROPERTY(EditAnywhere, Category="0_Config|Ground", meta=(ClampMin="0", ClampMax="5", EditCondition="bContactDecal", ToolTip="接触面边缘元素色焦散光环亮度，默认0（关闭）；0.5~1.2 可看到脚边一圈光斑。"))
 float ContactCausticStrength = 0.f;
 UPROPERTY(EditAnywhere, Category="0_Config|Ground", meta=(ClampMin="1", ClampMax="100", Units="cm", EditCondition="bContactDecal", ToolTip="身体离地超过该高度时接触贴花和接触带完全淡出，厘米，默认25。"))
 float ContactFadeHeight = 25.f;
 void SetDigestBubbleSource(class UMeshComponent* Source);
 void TriggerDevourBubbleBurst(const FVector& WorldPosition);


	/** Vertex budget ceiling while shots are in flight; the body keeps its normal budget and shots use the extra ~35%. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "3000", ClampMax = "60000",
		ToolTip = "有子球在飞时表面顶点预算上限。发射后本体不再让出 35% 预算，子球用额外的部分。默认 20000；过低会让本体顶部缺片（日志出现 vertex budget 警告）。"))
	int32 FragmentVertexBudgetCap = 20000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "50.0", Units = "cm",
		ToolTip = "G 子球自动攻击半径。默认 500cm。"))
	float FragmentAttackRadius = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "0.01", ClampMax = "1.0",
		ToolTip = "G 子球伤害相对本体 Combo1 的比例。默认 0.1。"))
	float FragmentAttackDamageScale = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "0.1", Units = "s",
		ToolTip = "每颗 G 子球自动攻击间隔。默认 1s。"))
	float FragmentAttackInterval = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "50.0", Units = "cm/s",
		ToolTip = "G 子球追击敌人的牵引速度。默认 700。"))
	float FragmentChaseSpeed = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "10.0", Units = "cm",
		ToolTip = "G 子球进入此距离才结算伤害。默认 80。"))
	float FragmentMeleeRange = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "100.0"))
	float RecallPullSpeed = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "0.5"))
	float RecallTimeout = 5.f;

	/**
	 *  Distance from body COM at which flying chunks start soft-merge (metaball absorb).
	 *  0 = RestRadius * 1.6.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "0.0", ClampMax = "120.0"))
	float AbsorbMergeRadius = 0.f;

	/** Inside this radius (or after hold), clones finally destroy. 0 = RestRadius * 0.7. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float AbsorbCommitRadius = 0.f;

	/** Soft-merge duang window before clones are destroyed, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Chunk", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float MergeHoldDuration = 0.7f;

	// ---- Quality ---------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Quality")
	bool bAutoQuality = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Quality", meta = (ClampMin = "200.0"))
	float MediumQualityDistance = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Quality", meta = (ClampMin = "400.0"))
	float LowQualityDistance = 3500.f;

	/** Rebuild the particle set on tier changes. Only ever kicks in far from the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Quality")
	bool bQualityScalesParticleCount = true;

	// ---- Events ----------------------------------------------------------------------

	/** Fires when the squeeze amount moves meaningfully. Hook level hazards up here. */
	UPROPERTY(BlueprintAssignable, Category = "Slime|Squeeze")
	FOnSlimeSqueezeChanged OnSqueezeChanged;

	// ---- API -------------------------------------------------------------------------

	/** Assigned by the owning character in its constructor. */
	void SetSurfaceMesh(UProceduralMeshComponent* InMesh) { SurfaceMesh = InMesh; }

	void SetShadowMesh(UProceduralMeshComponent* InMesh) { ShadowMesh = InMesh; }

	void SetXRayMesh(UProceduralMeshComponent* InMesh) { XRayMesh = InMesh; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	UProceduralMeshComponent* GetSurfaceMesh() const { return SurfaceMesh; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	UProceduralMeshComponent* GetShadowMesh() const { return ShadowMesh; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	UProceduralMeshComponent* GetXRayMesh() const { return XRayMesh; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	UMaterialInterface* GetResolvedBodyMaterial() const { return ResolvedMaterial; }

	/** Swaps classic / spectral / volumetric / luminous skins. Element MID is rebuilt by USlimeElementComponent. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void ApplyBodySkin(ESlimeBodySkin Skin);

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool IsVolumetricSkinActive() const { return bVolumetricActive; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	UMaterialInterface* GetResolvedXRayMaterial() const { return ResolvedXRayMaterial; }

	/** Pancake mode. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetSpread(bool bInSpread);

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool IsSpreading() const { return bSpread; }
	/** 0 = gathered, 1 = fully spread. */
	float GetSpreadBlend() const { return SpreadBlend; }

	/** Temporary MaxStepHeight used while walking onto short props. 0 restores the default. */
	void SetStepHeightBoost(float BoostedMaxStep);

	float GetDefaultStepHeight() const { return DefaultStepHeight; }

	/** Rebuilds the dome and clears every transient state. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void ResetBody();

	/** Spawns a cloned mini-slime shot without shrinking the body. Returns clone particle count. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	int32 LaunchChunk(const FVector& LaunchVelocity);

 int32 LaunchChunkAlongPath(const FSlimeLaunchPath& Path);
 int32 LaunchCannonShot(const FSlimeCannonLaunch& Cannon);
 bool CanLaunchCannon() const;
 float GetCannonRadius() const { return Solver.GetScaledRestRadius() * FMath::Pow(FMath::Clamp(LaunchFraction, 0.05f, 0.6f), 1.f / 3.f); }
 bool TraceShotWorld(FHitResult& Hit, const FVector& Start, const FVector& End, float Radius = 0.f, bool bIgnorePawns = false) const;
 bool PrepareCannonLaunch(FSlimeCannonLaunch& Aim) const;
 bool QueryShotGround(const FVector& Center, FHitResult& Hit) const;
 void ConfigureShotUmbrellas(float Height,float Follow,float Fall,float MaxFall);
 UPROPERTY(EditAnywhere, Category="0_Config|Launch", meta=(ClampMin="0", ToolTip="子球出射点在体表外额外预留的间隙，默认3厘米；与子球半径共同决定分离终点"))
 float CannonClearance = 3.f;
 UPROPERTY(EditAnywhere, Category="0_Config|Ground", meta=(ToolTip="子史莱姆复用本体接触阴影材质，随离地高度淡出；默认开启"))
 bool bShotContactDecals = true;
 FVector GetCannonMuzzle(const FSlimeCannonLaunch& Aim) const;

	/** Combat tendrils: short-lived clone blobs that peel then get recalled. */
	int32 LaunchTendril(const FVector& LaunchVelocity, float Fraction, float Life);

	/** Devour latch: clone a mini-slime that ignores the G-key shot cap. */
	int32 LaunchDevourShot(const FVector& LaunchVelocity, float Fraction, float Life, uint8& OutShotId);

	void SetShotTarget(uint8 ShotId, const FVector& Target, float PullSpeed);

	void ClearShotTarget(uint8 ShotId);

	void ClearShotTargets();

	void AddIgnoreWorldShot(uint8 ShotId);
	void ClearIgnoreWorldShots();

	void SetRecallPullSpeedOverride(float Speed);
	void ClearRecallPullSpeedOverride();

	float GetEffectiveRecallPullSpeed() const;

	/** Instantly destroy every flying clone (used before a devour latch). */
	void ClearFragments();

	/** While > 0, FixedStep uses this instead of LaunchFraction (devour half-volume minis). 0 = off. */
	void SetLaunchFractionOverride(float Fraction);

	void ClearLaunchFractionOverride() { LaunchFractionOverride = 0.f; }

	FVector GetShotCenter(uint8 ShotId) const;

	UFUNCTION(BlueprintPure, Category = "Slime")
	float GetRecallPullSpeed() const { return RecallPullSpeed; }

	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetRecalling(bool bInRecalling);

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool IsRecalling() const { return bRecalling; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool HasFragments() const { return Solver.HasFragments(); }

	UFUNCTION(BlueprintPure, Category = "Slime")
	int32 GetActiveShotCount() const { return Solver.GetActiveShotCount(); }

	UFUNCTION(BlueprintPure, Category = "Slime")
	int32 GetMaxActiveShots() const { return MaxActiveShots; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	FVector GetBlobCenter() const { return Solver.GetBodyCenter(); }

	/** Particle COM plus visual-only Z lift so devour inner mesh tracks the inflated ball. */
	UFUNCTION(BlueprintPure, Category = "Slime")
	FVector GetVisualBlobCenter() const { return GetBlobCenter() + FVector(0.f, 0.f, VisualZLift); }

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool GetFragmentCenter(FVector& OutCenter) const { return Solver.GetFragmentCenter(OutCenter); }

	/** COM of each active ballistic mini-slime shot (refreshes shot cache). */
	UFUNCTION(BlueprintPure, Category = "Slime")
	void GetActiveShotCenters(TArray<FVector>& OutCenters) const;

	void RefreshShotStates() { Solver.RefreshShotStates(); }
	const TArray<FSlimeSolver::FShotState>& GetShotStates() const { return Solver.GetShotStates(); }

	/** Shader shot slots (ShotCenter0..4). Matches the body material Custom node. */
	static constexpr int32 MaxShotSlots = 5;
	/** Slot -> ShotId assigned at the last surface rebuild; slot i drives ShotCenter{i} and vertex colour R = (i + 1) / 255. */
	const TArray<uint8>& GetShotSlotIds() const { return ShotSlotIds; }
	float GetMiniMembraneRadius() const { return Solver.GetMiniMembraneRadius(); }

	UFUNCTION(BlueprintPure, Category = "Slime")
	float GetSqueezeAmount() const { return SqueezeAmount; }

	// ---- Shell readback (face / bubbles) ----------------------------------------------

	/** Hard-shell half-axes (Forward, Right, Up) in cm, already scaled by body scale / squeeze / spread. */
	UFUNCTION(BlueprintPure, Category = "Slime|Shell")
	FVector GetShellAxes() const { return FVector(Solver.GetShellAxes()); }

	/** Unit horizontal move direction the shell is stretched along. */
	UFUNCTION(BlueprintPure, Category = "Slime|Shell")
	FVector GetInertiaForward() const { return FVector(Solver.GetInertiaForward()); }

	UFUNCTION(BlueprintPure, Category = "Slime|Shell")
	float GetInertiaAmount() const { return Solver.GetInertiaAmount(); }

	/** World centre of the hard shell (COM shifted back along the inertia trail) plus visual Z lift. */
	UFUNCTION(BlueprintPure, Category = "Slime|Shell")
	FVector GetShellCenter() const { return Solver.GetShellCenter() + FVector(0.f, 0.f, VisualZLift); }

	/** World AABB of the attached body particles. */
	UFUNCTION(BlueprintPure, Category = "Slime|Shell")
	FBox GetBodyBounds() const { return Solver.GetBodyBounds(); }

	/**
	 *  Ellipse fitted to the body particles touching the floor (or the wall while clinging).
	 *  HalfAxes are centimetres along MajorDir and the in-plane perpendicular. Confidence is 0..1.
	 */
	bool GetContactFootprint(FVector& OutCenter, FVector& OutNormal, FVector& OutMajorDir, FVector2D& OutHalfAxes, float& OutConfidence) const
	{
		if (!bContactValid)
		{
			return false;
		}
		OutCenter = ContactCenter;
		OutNormal = ContactNormal;
		OutMajorDir = ContactMajorDir;
		OutHalfAxes = ContactHalfAxes;
		OutConfidence = ContactFadeSmoothed;
		return true;
	}

	/**
	 *  Half-axes (cm) of the floor columns the mesh actually drew, projected onto MajorDir.
	 *  A filled disc of radius R has variance R^2/4, so 2*sqrt(variance) is the radius; half a cell is added
	 *  because the samples sit on grid points.
	 */
	static FVector2D FootprintHalfAxesFromMoments(const FSlimeFloorFootprint& Footprint, const FVector2D& MajorDir);

	/** Smoothed mesh footprint. False until a floor clip has produced one. */
	bool GetVisualContactHalfAxes(FVector2D& OutHalfAxes) const
	{
		if (!bVisualContactValid)
		{
			return false;
		}
		OutHalfAxes = VisualContactHalfAxes;
		return true;
	}

	/** 1 while a dome is showing its bell, 0 for a ball and while spread (the sheet puddle stays as tuned). */
	float GetDomeFootprintWeight() const;

	bool UsesScreenSpaceXRay() const { return bScreenSpaceXRay; }
	UMaterialInstanceDynamic* GetXRayOutlineMID() const { return XRayOutlineMID; }
	/** Luminous morph skin clips against CustomDepth; stop the slime writing it for the transition. */
	void SetXRayDepthSuppressed(bool bSuppressed);

	/** Rest-shape scale the dome preset applies on top of the ball: (wide, wide, tall). (1,1,1) while a ball. */
	FVector GetDomeAxisScale() const
	{
		const float Blend = FMath::SmoothStep(0.f, 1.f, ShapeBlend);
		const float Wide = FMath::Lerp(1.f, DomeWidthScale, Blend);
		const float Tall = FMath::Lerp(1.f, DomeHeightScale, Blend);
		return FVector(Wide, Wide, Tall);
	}

	/** Shader slot count. Custom HLSL is unrolled; runtime uses BubbleCount. */
	static constexpr int32 MaxBubbles = 10;

	/** World position of one lagging inner bubble (0..MaxBubbles-1). */
	FVector GetBubbleWorldPosition(int32 Index) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Surface",
		meta = (ClampMin = "0", ClampMax = "10",
		ToolTip = "同时可见的体内气泡个数（0–10）。默认 10。多出的槽半径写 0。"))
	int32 BubbleCount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Surface",
		meta = (ClampMin = "0.005", ClampMax = "0.15",
		ToolTip = "气泡刚从底部冒出时的半径，相对椭球最短半轴。默认 0.014（27cm 身体约 0.4cm）。"))
	float BubbleMinR = 0.014f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Surface",
		meta = (ClampMin = "0.01", ClampMax = "0.2",
		ToolTip = "气泡到顶破裂前的半径，相对椭球最短半轴。默认 0.035（27cm 身体约 0.9cm）。"))
	float BubbleMaxR = 0.035f;

	/** Drives the capsule towards its minimum regardless of what the probes found. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetForcedSqueeze(float Amount) { ForcedSqueeze = FMath::Clamp(Amount, 0.f, 1.f); }

	/** Called from the character Landed event — squash + settle, never fragment. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void ApplyLandingSquash(float ImpactSpeed);

	/** Light double-jump "duang"; far milder than a landing squash. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void ApplyAirBounce();

	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetCombatPose(const FSlimeCombatPoseState& Pose);

	UFUNCTION(BlueprintCallable, Category = "Slime")
	void ClearCombatPose();

	UFUNCTION(BlueprintCallable, Category = "Slime")
	void ApplyHitJolt();

	/**
	 *  Uniform blob scale (devour gulp). Capsule is left alone.
	 *  @param bIgnoreSqueeze 吞噬时勾上：不要被间隙挤压把 3x 压回 1x。
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetBodyScale(float NewScale, bool bIgnoreSqueeze = false);

	UFUNCTION(BlueprintPure, Category = "Slime")
	float GetBodyScale() const { return RequestedBodyScale; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	float GetAppliedBodyScale() const { return Solver.GetSizeScale(); }

	/** Extra walk-speed multiplier applied after squeeze. 1 = unchanged. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetExternalMoveSpeedScale(float Scale) { ExternalMoveSpeedScale = FMath::Clamp(Scale, 0.1f, 1.5f); }

	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetExternalJumpScale(float Scale) { ExternalJumpScale = FMath::Clamp(Scale, 0.1f, 1.5f); }

	/**
	 *  If true (or slime.BodyVisualScaleOnly=1), SetBodyScale only inflates the isosurface
	 *  and leaves particle positions at 1x.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Scale")
	bool bVisualOnlyBodyScale = false;

	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetQuality(ESlimeSimQuality InQuality);

	UFUNCTION(BlueprintPure, Category = "Slime")
	ESlimeSimQuality GetQuality() const { return Quality; }

	/** Re-applies params to the solver and surface builder after an edit. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void ApplyParams();

	/** Wall-cling hemisphere: Point/Normal of the stuck surface. Clears when bInCling is false. */
	void SetClingVisual(bool bInCling, const FVector& Point, const FVector& Normal);

	/** Skip ceiling height-squeeze for a short time after mantling onto a lip. */
	void SuppressHeightSqueeze(float Duration);

	/**
	 *  Stops the shadow proxy from casting. The proxy is hidden-in-game but casts anyway
	 *  (bCastHiddenShadow), so hiding the owning actor is not enough — and the surface rebuild
	 *  re-asserts the cast flags every frame, so the suppression has to live here.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void SetShadowCastSuppressed(bool bSuppressed);

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool IsShadowCastSuppressed() const { return bShadowCastSuppressed; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool IsClingingVisual() const { return bClingVisual; }

	/** 0 = blocked, 1 = open sky. The luminous skins use this to ramp night readability. */
	UFUNCTION(BlueprintPure, Category = "Slime")
	float GetAmbientScale() const { return AmbientScale; }

	/** xyz points at the brightest directional light, w is visibility times elevation. */
	UFUNCTION(BlueprintPure, Category = "Slime")
	FVector4 GetKeyLightDir() const { return FVector4(KeyLightDir); }

private:
	void FixedStep(float StepDelta);
	void ApplyCannonImpact(uint8 ShotId, AActor* Target, const FVector& Location);
	void SweepKinematicShots();
	void TickFragmentAttacks(float DeltaTime);
	void RefreshColliders();
	void UpdateFloor();
	void UpdateGroundField(float StepDelta);
	void ProbeSqueeze(float DeltaTime);

	/** FluidNinja TraceMesh / InteractionVolume / ActivationVolume — keep Overlap, skip soft-body squeeze & floor. */
	static bool ShouldIgnoreFluidNinjaCollider(const UPrimitiveComponent* Component);
	void TryOozeEscape(float DeltaTime);
	void ApplyCapsuleSize(float NewRadius, float NewHalfHeight);
	void UpdateAnchor();
	void RebuildSurface();
	void PushMeshSection();
	void UpdateMeshFollow();
	/** Lag-spring the bubble centres toward their rest spots and push shell / bubble params to the body MID. */
	void UpdateBubblesAndShellParams(float DeltaTime);
	/** Sky visibility + sun/moon shadow traces -> AmbientScale that dims the unlit colour terms of the skins. */
	void UpdateAmbientLight(float DeltaTime);
	float ComputeAmbientTarget(bool bLog) const;
	void UpdateQuality();
	void ResolveMaterial();
	class USlimeGraphicsSettings* GetGraphicsSettings() const;
	FVector GetFootLocation() const;

	// ---- Volumetric skin: density atlas ---------------------------------------------
	/** (Re)creates DensityAtlas for the current SurfaceParams.MaxGridDim. No-op when the skin is not volumetric. */
	void EnsureDensityAtlas();
	void ReleaseDensityAtlas();
	/** Copies the body field into the atlas (float -> half, Z slices tiled in 2D) and pushes grid params. */
	void UploadBodyField();
	/** Writes DensityAtlas / GridOrigin / GridDims / GridInfo on the surface MID. MeshOffset shifts the grid with the follow slide. */
	void PushFieldParams(const FVector& MeshOffset);

	FSlimeSolver Solver;
	FSlimeSurfaceBuilder Surface;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> SurfaceMesh;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> ShadowMesh;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> XRayMesh;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UCapsuleComponent> OwnerCapsule;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ResolvedMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ResolvedClassicMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ResolvedSpectralMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ResolvedVolumetricMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ResolvedLuminousMaterial;

	/** R16F 2D atlas of the body density grid: TilesX x TilesY tiles of AtlasTileDim^2, one per Z slice. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DensityAtlas;

	FDelegateHandle BodySkinChangedHandle;

	/** Ring of half-float staging buffers so the render thread never reads a buffer being rewritten. */
	TArray<uint16> AtlasStaging[3];
	int32 AtlasStagingIndex = 0;
	FUpdateTextureRegion2D AtlasRegion;
	int32 AtlasTileDim = 0;
	int32 AtlasTilesX = 0;
	int32 AtlasTilesY = 0;
	bool bVolumetricActive = false;
	bool bFieldParamsValid = false;
	TWeakObjectPtr<UMaterialInstanceDynamic> AtlasBoundMid;
	FVector FieldOrigin = FVector::ZeroVector;
	FIntVector FieldDims = FIntVector::ZeroValue;
	float FieldCellSize = 1.f;
	float FieldIso = 0.2f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ResolvedShadowMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ResolvedXRayMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ResolvedXRayDepthProxy;

	bool bLoggedMissingXRayDepthProxy = false;

	float StepAccumulator = 0.f;
	float SurfaceAccumulator = 0.f;
	float ColliderTimer = 0.f;
	FVector LastColliderGatherCenter = FVector::ZeroVector;

	/** Body COM at the last surface rebuild; mesh slides by (CurrentCOM - this) between rebuilds. */
	FVector RebuildBodyCOM = FVector::ZeroVector;
	bool bHaveRebuildBodyCOM = false;

	float FloorZ = -1.e9f;
	float FragmentFloorZ = -1.e9f;
	float CeilingZ = 1.e9f;
	float SqueezeAmount = 0.f;
	float ReportedSqueeze = 0.f;
	float ForcedSqueeze = 0.f;
	/** 0 = dome, 1 = fully spread. Rises over SpreadFlattenTime while held, falls over SpreadRecoverDuration. */
	float SpreadBlend = 0.f;
	float GroundFieldTimer = 0.f;
	FVector LastGroundFieldCenter = FVector::ZeroVector;

	static constexpr int32 AmbientSkyRays = 8;
	/** 1 = open sky, 0 = blocked; one entry per sky ray, refreshed round-robin. */
	float SkyRayOpen[AmbientSkyRays] = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
	int32 SkyRayCursor = 0;
	float AmbientTimer = 0.f;
	float AmbientLogTimer = 0.f;
	float AmbientTarget = 1.f;
	float AmbientScale = 1.f;
	struct FBubbleBurst { FVector Local = FVector::ZeroVector; float Started = -100.f; float Seed = 0.f; };
	FBubbleBurst DevourBursts[2];
	int32 NextDevourBurst = 0;
 TWeakObjectPtr<class UMeshComponent> DigestBubbleSource;
 UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> BubbleVisualMesh;
 UPROPERTY(Transient) TObjectPtr<UDecalComponent> ContactDecal;
 void UpdateContactFootprint();
 void UpdateVisualContactFootprint();
 void UpdateContactDecal();
 void EnsureXRayOutlineMaterial();
 void ApplyXRayRenderMode();
 void UpdateShotContactDecals();
 UPROPERTY(Transient)
 TMap<uint8,TObjectPtr<UDecalComponent>> ShotContactDecals;
 /** Smoothed contact ellipse: centre on the contact plane, plane normal, major axis direction, half axes (cm). */
 FVector ContactCenter = FVector::ZeroVector;
 FVector ContactNormal = FVector::UpVector;
 FVector ContactMajorDir = FVector::ForwardVector;
 FVector2D ContactHalfAxes = FVector2D::ZeroVector;
 /** Mesh floor-crossing ellipse, smoothed like ContactHalfAxes. Only used for the dome puddle. */
 FVector2D VisualContactHalfAxes = FVector2D::ZeroVector;
 bool bVisualContactValid = false;
 UPROPERTY(Transient)
 TObjectPtr<UMaterialInstanceDynamic> XRayOutlineMID;
 bool bXRayDepthSuppressed = false;
 /** 0 = ball, 1 = dome; eases toward the setting over ShapeBlendTime. */
 float ShapeBlend = 0.f;
 float ShapeTarget = 0.f;
 FDelegateHandle BodyShapeChangedHandle;
 void ApplyBodyShape(ESlimeBodyShape Shape);
 float ContactFadeSmoothed = 0.f;
 bool bContactValid = false;
 bool bLuminousSkinActive = false;
 /** Direction to the brightest visible directional light (w = visibility * elevation), refreshed with AmbientTarget. */
 mutable FVector4f KeyLightDir = FVector4f(0.35f, 0.2f, 0.91f, 1.f);
 mutable float KeyLightContribution = 0.f;
 struct FVisibleBubble { FVector Local = FVector::ZeroVector; float Radius = 1.f; float Speed = 0.15f; float PopAge = -1.f; float Seed = 0.f; bool bValid = false; };
 TArray<FVisibleBubble> VisibleBubbles;
 void UpdateBubbleVisuals(float DeltaTime);

	bool bAmbientPrimed = false;
	float RecallElapsed = 0.f;
	FVector SqueezeFreeDirection = FVector::UpVector;

	float DefaultStepHeight = 45.f;
	float DefaultWalkSpeed = 500.f;
	float DefaultJumpZ = 620.f;
	float StepHeightBoost = 0.f;
	float HeightSqueezeSuppressRemaining = 0.f;
	float ExternalMoveSpeedScale = 1.f;
	float ExternalJumpScale = 1.f;
	float RequestedBodyScale = 1.f;
	float VisualZLift = 0.f;
	float LaunchFractionOverride = 0.f;
	float RecallPullSpeedOverride = 0.f;
	int32 SavedSurfaceMaxVertices = 9000;
	int32 SavedSurfaceMaxGridDim = 36;
	bool bEnlargedSurfaceBudget = false;
	/** World time fragments were last seen; the enlarged shot budget is held ~0.5 s past that to avoid section churn. */
	float LastFragmentSeenTime = -1.e9f;
	/** Vertex count the render sections were created with; a budget change forces a section rebuild. */
	int32 SectionVertexCount = 0;
	/** Shader slot (0..MaxShotSlots-1) -> ShotId, shared by the surface vertex colours and the ShotCenter params. */
	TArray<uint8> ShotSlotIds;
	bool bFreezeQualityLod = false;

	bool bSpread = false;
	bool bRecalling = false;
	bool bClingVisual = false;
	bool bShadowCastSuppressed = false;
	bool bMeshSectionCreated = false;
	bool bShadowMeshSectionCreated = false;
	bool bXRayMeshSectionCreated = false;
	bool bWarnedTruncation = false;

	FVector ClingPoint = FVector::ZeroVector;
	FVector ClingNormal = FVector::ForwardVector;

	/** Inner bubble state: rest offset in shell-normalised space, current world offset from shell centre, velocity. */
	FVector BubbleRestNorm[MaxBubbles];
	FVector BubbleOffset[MaxBubbles];
	FVector BubbleVelocity[MaxBubbles];
	FVector2D BubbleLateral[MaxBubbles];
	float BubblePhase[MaxBubbles];
	float BubbleSpeed[MaxBubbles];
	float BubbleBurst[MaxBubbles];
	bool bBubblesInitialised = false;

	void RespawnBubble(int32 Index, bool bStagger);

	TMap<uint8, float> FragmentAttackCooldownRemaining;

	ESlimeSimQuality Quality = ESlimeSimQuality::High;
};
