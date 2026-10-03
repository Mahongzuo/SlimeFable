// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeHomeFluidPad.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UPostProcessComponent;
class UStaticMeshComponent;

/** Places a FluidNinja preset unchanged. AimQuery stays out of FluidTrace so the sim rays are not blocked. */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeHomeFluidPad : public AActor
{
	GENERATED_BODY()

public:
	ASlimeHomeFluidPad();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void Configure(TSubclassOf<AActor> InFluidClass, int32 InHomePieceId, float InPlanScale = 1.f, float InDepthCm = 0.f, bool bInSwapPlanAxes = false);
	int32 GetHomePieceId() const { return HomePieceId; }
	void SetHomePieceId(int32 InId) { HomePieceId = InId; }

	void SetHighlighted(bool bEnabled);
	AActor* GetFluidActor() const { return FluidActor; }

	/** 泳池水下后处理比水材质位移上限低多少。越大，没入水时水面上方的墙越不容易被染色。 */
	UPROPERTY(EditAnywhere, Category = "0_Config|Pool", meta = (ClampMin = "0", ClampMax = "80", UIMin = "0", UIMax = "80", ToolTip = "厘米。切割面 = 水材质位移上限减去这个值，默认 30。只影响泳池。调大：水面上方的墙不再被染色。调太大：入水后墙面露出一条原色。运行中修改会立刻生效。"))
	float UnderwaterMaskTrimCm = 30.f;

	UPROPERTY(VisibleAnywhere, Category = "Z_Components")
	TObjectPtr<USceneComponent> PadRoot;

	UPROPERTY(VisibleAnywhere, Category = "Z_Components")
	TObjectPtr<UBoxComponent> AimQuery;

	/** Kept hidden. Sand, snow, sea and river draw on the preset TraceMesh, which has the tessellation their displacement needs. */
	UPROPERTY(VisibleAnywhere, Category = "Z_Components")
	TObjectPtr<UStaticMeshComponent> Surface;

private:
	void AddUnderwaterPost();
	void UpdateUnderwaterCamera();
	void FitAimQuery();
	void LetPawnPassThroughFluid() const;
	void IncludeWorldDynamicBrush() const;
	void DropDemoBalls() const;
	void SetupSurface(const TCHAR* MaterialPath, float Lift);
	void ApplySize(float WidthCm, float LengthCm, float DepthCm);
	void SitOnFloor();
	void SyncSurface();
	void ApplyMaskTrim();

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SurfaceMID;

	TWeakObjectPtr<UMaterialInstanceDynamic> SimMID;
	FTimerHandle SurfaceTimer;

	UPROPERTY()
	TSubclassOf<AActor> FluidClass;

	UPROPERTY()
	TObjectPtr<AActor> FluidActor;

	UPROPERTY()
	TObjectPtr<UBoxComponent> UnderwaterBox;

	UPROPERTY()
	TObjectPtr<UPostProcessComponent> UnderwaterPost;

	/** Whole-screen overrides from the source volume. They cannot be masked per pixel, so they fade in with camera depth. */
	UPROPERTY()
	TObjectPtr<UPostProcessComponent> UnderwaterGlobalPost;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> UnderwaterMIDs;

	FTimerHandle CameraTimer;
	FVector2D PoolMinXY = FVector2D::ZeroVector;
	FVector2D PoolMaxXY = FVector2D::ZeroVector;
	float WaterSurfaceZ = 0.f;
	float UnderwaterMaskCeilingZ = 0.f;
	float UnderwaterMaskFloorZ = 0.f;
	float AppliedMaskTrimCm = -1.f;
	float GlobalWeight = 1.f;
	bool bCameraNear = false;
	bool bEyeWasUnder = false;

	int32 HomePieceId = INDEX_NONE;
	float PlanScale = 1.f;
	float RequestedDepthCm = 0.f;
	bool bSwapPlanAxes = false;
	bool bHighlighted = false;
};
