// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeHomeFluidPad.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/** Places a FluidNinja preset unchanged. AimQuery stays out of FluidTrace so the sim rays are not blocked. */
UCLASS()
class SLIMEFABLE_API ASlimeHomeFluidPad : public AActor
{
	GENERATED_BODY()

public:
	ASlimeHomeFluidPad();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void Configure(TSubclassOf<AActor> InFluidClass, int32 InHomePieceId);
	int32 GetHomePieceId() const { return HomePieceId; }
	void SetHomePieceId(int32 InId) { HomePieceId = InId; }

	void SetHighlighted(bool bEnabled);
	AActor* GetFluidActor() const { return FluidActor; }

	UPROPERTY(VisibleAnywhere, Category = "Z_Components")
	TObjectPtr<USceneComponent> PadRoot;

	UPROPERTY(VisibleAnywhere, Category = "Z_Components")
	TObjectPtr<UBoxComponent> AimQuery;

	/** Kept hidden. Sand, snow, sea and river draw on the preset TraceMesh, which has the tessellation their displacement needs. */
	UPROPERTY(VisibleAnywhere, Category = "Z_Components")
	TObjectPtr<UStaticMeshComponent> Surface;

private:
	void FitAimQuery();
	void LetPawnPassThroughFluid() const;
	void IncludeWorldDynamicBrush() const;
	void DropDemoBalls() const;
	void SetupSurface(const TCHAR* MaterialPath, float Lift);
	void ApplySize(float WidthCm, float LengthCm, float DepthCm);
	void SitOnFloor();
	void SyncSurface();

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SurfaceMID;

	TWeakObjectPtr<UMaterialInstanceDynamic> SimMID;
	FTimerHandle SurfaceTimer;

	UPROPERTY()
	TSubclassOf<AActor> FluidClass;

	UPROPERTY()
	TObjectPtr<AActor> FluidActor;

	int32 HomePieceId = INDEX_NONE;
	bool bHighlighted = false;
};
