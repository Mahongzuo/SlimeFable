#pragma once

#include "CoreMinimal.h"
#include "Quest/QuestChapterGate.h"
#include "DayChapterPortal.generated.h"

class UBoxComponent;
class UChildActorComponent;
class UNiagaraComponent;
class UNiagaraSystem;

UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ADayChapterPortal : public AQuestChapterGate
{
	GENERATED_BODY()

public:
	ADayChapterPortal();

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", meta = (AdvancedDisplay))
	TObjectPtr<UBoxComponent> TravelVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", meta = (AdvancedDisplay))
	TObjectPtr<UChildActorComponent> VisualPortal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", meta = (AdvancedDisplay))
	TObjectPtr<UNiagaraComponent> PortalFx;

	/** 1–10 maps to BP_Portal_1…10. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Portal",
		meta = (ClampMin = "1", ClampMax = "10",
			ToolTip = "1–10 对应 BP_Portal_1…10 的外观。只换皮，不换进哪一年。"))
	int32 PortalStyle = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Portal",
		meta = (ToolTip = "自动绑定的 10 个外观类，一般不用改。空了才用手填。"))
	TArray<TSoftClassPtr<AActor>> PortalStyleClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Portal",
		meta = (ToolTip = "额外 Niagara。空则只用 ChildActor 外观（BP_Portal_*）。"))
	TSoftObjectPtr<UNiagaraSystem> PortalVfx;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Portal",
		meta = (ToolTip = "走近也进。关掉则只靠按 F。"))
	bool bEnterOnOverlap = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Portal",
		meta = (ToolTip = "走近判定盒的半尺寸，单位厘米。默认 180×80×200。"))
	FVector OverlapExtent = FVector(180.f, 80.f, 200.f);

protected:
	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	void ApplyPortalVisuals();
	void ApplyPortalStyle();
	void DisableChildGameplay(AActor* Child) const;
	void SnapChildToPortal(AActor* Child) const;
	UClass* ResolveStyleClass() const;
	static UClass* FindPortalStyleClass(int32 Style);
};
