// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimePoopActor.generated.h"

class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimePoopActor : public AActor
{
	GENERATED_BODY()

public:
	ASlimePoopActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "0_Config|Poop")
	void Configure(const FLinearColor& InColor, const FVector& InForward);

	/** Digest / cheat spawn. Caps concurrent piles and traces the floor. */
	static ASlimePoopActor* SpawnFromProducer(AActor* Producer);

	/** Wicked face + poop SFX. Digest plays this 2s before spawn. */
	static void PlayProducerReaction(AActor* Producer);

	static bool IsPoopActor(const AActor* Actor);

	/** Seconds before the pile appears to play face + SFX. Default 1. */
	static constexpr float ReactLeadSeconds = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Poop",
		meta = (ClampMin = "4.0", ToolTip = "存活秒数。默认 20。"))
	float LifetimeSeconds = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Poop",
		meta = (ClampMin = "1", ClampMax = "8", ToolTip = "同时最多几坨。超出销毁最老的。默认 4。"))
	int32 MaxConcurrent = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Poop",
		meta = (ClampMin = "4.0", ClampMax = "80.0", ToolTip = "世界高度（厘米）。默认 14，大约一坨正常便便。"))
	float TargetHeightCm = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Poop",
		meta = (ToolTip = "Meshy 导入的静态网格。空则加载 /Game/Characters/Slime/Meshes/SM_SlimePoop。"))
	TSoftObjectPtr<UStaticMesh> PoopMeshPath =
		TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Characters/Slime/Meshes/SM_SlimePoop/StaticMeshes/SM_SlimePoop.SM_SlimePoop")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Audio",
		meta = (ToolTip = "拉屎音效。空则 /Game/Audio/SFX/Combat/sfx_poop_01。"))
	TSoftObjectPtr<USoundBase> PoopSound =
		TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/SFX/Combat/sfx_poop_01.sfx_poop_01")));

protected:
	UPROPERTY(VisibleAnywhere, Category = "Z_Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Z_Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

private:
	void RegisterLive();
	void UnregisterLive();
	void EnforceBudget();
	void ApplyMeshAndScale();
	static FVector ResolveBehindDirection(const AActor* Producer);
	static FVector ResolveSpawnOrigin(const AActor* Producer, const FVector& Face);

	static TArray<TWeakObjectPtr<ASlimePoopActor>> LivePiles;

	float Age = 0.f;
};
