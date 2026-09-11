// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeWatermelon.h"
#include "SlimeFallingWatermelon.generated.h"

/** Physics watermelon dropped by Xigua T. Shatters on impact, then despawns. */
UCLASS()
class SLIMEFABLE_API ASlimeFallingWatermelon : public ASlimeWatermelon
{
	GENERATED_BODY()

public:
	ASlimeFallingWatermelon();

	virtual void BeginPlay() override;

	void InitFalling(AActor* InSource, float InDamage, float InLifeAfterBreak);

protected:
	UFUNCTION()
	void HandleMeshHit(
		UPrimitiveComponent* HitComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

	UFUNCTION()
	void HandleMeshOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	void TryImpact(AActor* OtherActor);
	void Shatter(AActor* HitActor);
	void ApplyImpactDamage(AActor* Target);
	bool ShouldIgnoreActor(const AActor* Other) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Rain", meta = (ClampMin = "0.0",
		ToolTip = "砸中敌对单位的伤害。默认 8。"))
	float ImpactDamage = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Rain", meta = (ClampMin = "0.5", Units = "s",
		ToolTip = "碎开后销毁秒数。默认 5。"))
	float LifeAfterBreak = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Rain", meta = (ClampMin = "0.1",
		ToolTip = "相对源网格的缩放。默认 2。"))
	float RainScale = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Rain", meta = (ClampMin = "2",
		ToolTip = "落地最少碎成几块。默认 3。"))
	int32 ShatterPieceMin = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Rain", meta = (ClampMin = "2",
		ToolTip = "落地最多碎成几块。默认 5。"))
	int32 ShatterPieceMax = 5;

	TWeakObjectPtr<AActor> SourceActor;
	bool bShattered = false;
};
