#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeRunnerActors.generated.h"

class UBoxComponent;
class USphereComponent;
class UStaticMeshComponent;
class UNiagaraSystem;
class USoundBase;
class ASlimeRunnerDirector;

/** Base for runner course pieces: finds the director and filters overlaps to the player. */
UCLASS(Abstract, meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeRunnerPiece : public AActor
{
	GENERATED_BODY()

protected:
	ASlimeRunnerDirector* GetDirector();
	bool IsRunnerPlayer(AActor* Other);

	TWeakObjectPtr<ASlimeRunnerDirector> CachedDirector;
};

/** Collectible (red flag in 1949). */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeRunnerPickup : public ASlimeRunnerPiece
{
	GENERATED_BODY()

public:
	ASlimeRunnerPickup();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Pickup",
		meta = (ClampMin = "1", ToolTip = "拾取一次算几面红旗，默认 1。"))
	int32 Value = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Pickup",
		meta = (ToolTip = "上下浮动幅度（厘米），默认 12。"))
	float BobHeight = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Pickup",
		meta = (ToolTip = "每秒转多少度，默认 90。0 = 不转。"))
	float SpinDegreesPerSecond = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Pickup",
		meta = (ToolTip = "拾取爆点特效，可空。"))
	TObjectPtr<UNiagaraSystem> CollectFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Pickup",
		meta = (ToolTip = "拾取音效，可空。"))
	TObjectPtr<USoundBase> CollectSound;

	UPROPERTY(VisibleAnywhere, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UStaticMeshComponent> Mesh;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);

	FVector MeshHome = FVector::ZeroVector;
	float Clock = 0.f;
	bool bTaken = false;
};

/** Ink blob / falling tile / spikes. Side touch hurts, landing on top stomps it. */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeRunnerHazard : public ASlimeRunnerPiece
{
	GENERATED_BODY()

public:
	ASlimeRunnerHazard();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Hazard",
		meta = (ToolTip = "勾上：从上方落下能踩扁（墨团）。不勾：任何方向都扣血（尖刺、落瓦）。"))
	bool bStompable = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Hazard",
		meta = (ToolTip = "勾上：只在二、三周目出现。用来给高周目加敌人。"))
	bool bHardWeeksOnly = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Patrol",
		meta = (MakeEditWidget, ToolTip = "巡逻终点（相对本 Actor）。全 0 = 不巡逻。在视口拖菱形调整。"))
	FVector PatrolOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Patrol",
		meta = (ClampMin = "0.2", ToolTip = "单程巡逻秒数，默认 2.5。二三周目按导演的倍率加快。"))
	float PatrolSeconds = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Fall",
		meta = (ToolTip = "勾上：玩家走近后掉下来（落瓦）。"))
	bool bFallWhenNear = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Fall",
		meta = (ClampMin = "0", ToolTip = "玩家与本 Actor 水平距离小于多少时开始掉，默认 260。"))
	float FallTriggerDistance = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Fall",
		meta = (ClampMin = "0", ToolTip = "触发后先抖多久再掉（秒），默认 0.45。"))
	float FallDelay = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Hazard",
		meta = (ToolTip = "被踩扁时的特效，可空。"))
	TObjectPtr<UNiagaraSystem> StompFX;

	UPROPERTY(VisibleAnywhere, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(VisibleAnywhere, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UStaticMeshComponent> Mesh;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);

	void Stomp(class ACharacter* Character);

	FVector Home = FVector::ZeroVector;
	FVector MeshHome = FVector::ZeroVector;
	float Phase = 0.f;
	float Clock = 0.f;
	float FallTimer = -1.f;
	float FallSpeed = 0.f;
	bool bDead = false;
};

/** Touching it moves the respawn point here. */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeRunnerCheckpoint : public ASlimeRunnerPiece
{
	GENERATED_BODY()

public:
	ASlimeRunnerCheckpoint();

	UPROPERTY(VisibleAnywhere, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(VisibleAnywhere, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UStaticMeshComponent> Mesh;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);
};

/** Pit volume: back to the last checkpoint. */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeRunnerKillZone : public ASlimeRunnerPiece
{
	GENERATED_BODY()

public:
	ASlimeRunnerKillZone();

	UPROPERTY(VisibleAnywhere, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UBoxComponent> Box;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);
};

/** Reaching it starts the director's finale. */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeRunnerFinishZone : public ASlimeRunnerPiece
{
	GENERATED_BODY()

public:
	ASlimeRunnerFinishZone();

	UPROPERTY(VisibleAnywhere, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UBoxComponent> Box;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);
};

/** Bounce pad (clothesline, cart awning). */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeRunnerJumpPad : public ASlimeRunnerPiece
{
	GENERATED_BODY()

public:
	ASlimeRunnerJumpPad();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|JumpPad",
		meta = (ClampMin = "100", ToolTip = "弹起的竖直速度（厘米/秒），默认 1250。普通跳约 620。"))
	float LaunchZ = 1250.f;

	UPROPERTY(VisibleAnywhere, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(VisibleAnywhere, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UStaticMeshComponent> Mesh;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);
};

/** Platform that eases back and forth between its start and an offset. */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeRunnerMover : public ASlimeRunnerPiece
{
	GENERATED_BODY()

public:
	ASlimeRunnerMover();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Mover",
		meta = (MakeEditWidget, ToolTip = "移动终点（相对本 Actor）。在视口拖菱形调整。"))
	FVector MoveOffset = FVector(400.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Mover",
		meta = (ClampMin = "0.2", ToolTip = "单程秒数，默认 3。"))
	float MoveSeconds = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Mover",
		meta = (ClampMin = "0", ClampMax = "1", ToolTip = "起始相位 0–1，让相邻平台错开。"))
	float StartPhase = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UStaticMeshComponent> Mesh;

protected:
	virtual void BeginPlay() override;

	FVector Home = FVector::ZeroVector;
	float Phase = 0.f;
};
