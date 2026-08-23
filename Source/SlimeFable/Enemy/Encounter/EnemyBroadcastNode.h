#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyBroadcastNode.generated.h"

class UStaticMeshComponent;
class UNiagaraSystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEnemyBroadcastNodePulse);

/** Periodic 1945 arena hazard. It is intentionally mesh/VFX agnostic so the level can skin it. */
UCLASS(Blueprintable, meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API AEnemyBroadcastNode : public AActor
{
	GENERATED_BODY()

public:
	AEnemyBroadcastNode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<UStaticMeshComponent> NodeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Broadcast",
		meta = (ToolTip = "勾选后节点开始周期广播；阶段 Director 可在切换时调用 ActivateNode/DeactivateNode。"))
	bool bActive = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Broadcast",
		meta = (ClampMin = "0.25", Units = "s", ToolTip = "危险脉冲间隔，默认 3 秒。"))
	float PulseInterval = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Broadcast",
		meta = (ClampMin = "50.0", Units = "cm", ToolTip = "脉冲影响半径，默认 700 厘米。"))
	float PulseRadius = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Broadcast",
		meta = (ClampMin = "0.0", ToolTip = "每次脉冲对半径内玩家施加的伤害；0 表示只做预警。"))
	float PulseDamage = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Broadcast",
		meta = (ClampMin = "0.0", ToolTip = "节点可承受的伤害；0 表示不可破坏。"))
	float NodeHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Broadcast",
		meta = (ToolTip = "脉冲时播放的 Niagara；空则由蓝图或材质表现。"))
	TSoftObjectPtr<UNiagaraSystem> PulseNiagara;

	UPROPERTY(BlueprintAssignable, Category = "Broadcast")
	FEnemyBroadcastNodePulse OnPulse;

	UFUNCTION(BlueprintCallable, Category = "Broadcast")
	void ActivateNode();

	UFUNCTION(BlueprintCallable, Category = "Broadcast")
	void DeactivateNode();

	UFUNCTION(BlueprintPure, Category = "Broadcast")
	bool IsNodeActive() const { return bActive && !bDestroyed; }

protected:
	void Pulse();
	float PulseRemaining = 0.f;
	bool bDestroyed = false;
};
