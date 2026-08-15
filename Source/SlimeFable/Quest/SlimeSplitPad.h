#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeSplitPad.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class ASlimeSplitPad;

UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeSplitPad : public AActor
{
	GENERATED_BODY()

public:
	ASlimeSplitPad();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "0_Config|Pad")
	bool IsPressed() const { return bPressed; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Pad")
	bool bLatchFragment = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Pad")
	TObjectPtr<ASlimeSplitPad> PartnerPad;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Pad")
	TObjectPtr<AActor> DoorActor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UBoxComponent> Volume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UStaticMeshComponent> Mesh;

protected:
	void RefreshPressed();
	void ApplyDoor();
	bool PointInside(const FVector& Point) const;

	bool bPressed = false;
	bool bLatched = false;
	bool bDoorOpen = false;
};
