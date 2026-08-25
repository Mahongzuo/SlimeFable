// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeElementTypes.h"
#include "SlimePathSwordComponent.generated.h"

class UParticleSystem;
class UParticleSystemComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Path-driven Cascade AnimTrail + matching P_Particle debris for slime combos.
 * Hidden SM_SlimeTrailBlade provides TrailStart/TrailEnd; TrailMaster packs per element.
 */
UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent, PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API USlimePathSwordComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimePathSwordComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "PathSword")
	void PlaySwing(ESlimeElement Element, int32 ComboIndex, FVector Forward, float DurationSeconds);

	UFUNCTION(BlueprintCallable, Category = "PathSword")
	void AbortSwing();

	UFUNCTION(BlueprintPure, Category = "PathSword")
	bool IsSwinging() const { return bSwinging; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|PathSword",
		meta = (ToolTip = "Hidden blade driver mesh with TrailStart/TrailEnd sockets."))
	TSoftObjectPtr<UStaticMesh> BladeMeshAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|PathSword",
		meta = (ClampMin = "10.0", Units = "cm",
			ToolTip = "刀柄 TrailStart 距挥砍轴心的距离（沿攻击前方）。两 socket 都必须离开轴心，否则会扫成楔形。默认 36。"))
	float HiltRadius = 36.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|PathSword",
		meta = (Units = "cm", ToolTip = "挥砍轴心相对史莱姆质心的高度。默认 18。"))
	float HeightOffset = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|PathSword",
		meta = (ClampMin = "40.0", Units = "cm",
			ToolTip = "刃长：TrailEnd 比 TrailStart 再往前多少，对应 TrailMaster 大剑刃长。默认 110。"))
	float BladeLength = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|PathSword",
		meta = (Units = "cm",
			ToolTip = "刀刃微仰：Start 偏低、End 偏高，避免完全共面时从身后看起来像一条线。默认 16。"))
	float BladeTiltZ = 16.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|PathSword",
		meta = (ClampMin = "10.0", ToolTip = "水平横扫半角（度）。第 3、4 击斜砍也用这个水平分量。默认 100。"))
	float SwingHalfAngleDeg = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|PathSword",
		meta = (ClampMin = "10.0", ToolTip = "第 3、4 击斜砍的俯仰半角（度）。右上↔左下。默认 42。"))
	float DiagonalPitchDeg = 42.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|PathSword",
		meta = (ClampMin = "0.1",
			ToolTip = "第 1～3 击刀光尺寸相对基准的倍率。默认 0.75。"))
	float NormalTrailScale = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|PathSword",
		meta = (ClampMin = "0.1",
			ToolTip = "第 4 击终结刀光尺寸相对基准的倍率。默认 1.25。"))
	float FinisherTrailScale = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|PathSword",
		meta = (ClampMin = "0.05", ToolTip = "BeginTrails 宽度倍率，1 表示完全用两 socket 间距。"))
	float TrailWidth = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|PathSword",
		meta = (ClampMin = "0.0", Units = "s",
			ToolTip = "挥砍结束后刀光/碎屑再留多久再关掉。默认 0.35。"))
	float TrailLinger = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|PathSword",
		meta = (ClampMin = "1", ClampMax = "8",
			ToolTip = "挥砍时 AnimTrail 每帧子步采样次数（引擎每 tick 只采 1 点，子步让弧更圆）。默认 4。"))
	int32 TrailSubsteps = 4;

protected:
	void EnsureVisuals();
	void EnsureTrailSockets();
	void HideBladeMesh();
	void SetTrailVisible(bool bVisible);
	void ApplySwingPose(float YawDeg, float PitchDeg);
	void SampleTrailAtPose(float YawDeg, float PitchDeg, float SubstepDelta);
	void SetTrailManualTick(bool bManual);
	void EndTrailInternal();
	void FinishTrailVisuals();
	UParticleSystem* ResolveTrailForElement(ESlimeElement Element) const;
	UParticleSystem* ResolveParticleForElement(ESlimeElement Element) const;
	FVector GetBlobCenter() const;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> SwingRoot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BladeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UParticleSystemComponent> TrailPSC;

	UPROPERTY(Transient)
	TObjectPtr<UParticleSystemComponent> DebrisPSC;

	bool bSwinging = false;
	bool bTrailActive = false;
	bool bVisualsReady = false;
	bool bTrailManualTick = false;
	float SwingElapsed = 0.f;
	float SwingDuration = 0.2f;
	float SwingStartYaw = 80.f;
	float SwingEndYaw = -80.f;
	float SwingStartPitch = 0.f;
	float SwingEndPitch = 0.f;
	float SwingScale = 1.f;
	float LingerRemaining = 0.f;
	FVector SwingForward = FVector::ForwardVector;
};
