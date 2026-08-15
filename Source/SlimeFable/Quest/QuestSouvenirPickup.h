#pragma once

#include "CoreMinimal.h"
#include "Inventory/SlimeTestPickups.h"
#include "QuestSouvenirPickup.generated.h"

class UQuestObjectiveComponent;
class USlimeSouvenirDefinition;

/** Placeable souvenir: inventory pickup + optional quest contribution. */
UCLASS(Blueprintable, meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API AQuestSouvenirPickup : public ASlimePickupSouvenir
{
	GENERATED_BODY()

public:
	AQuestSouvenirPickup();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void OnPickedUp(APawn* Picker) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UQuestObjectiveComponent> Objective;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Souvenir")
	TSoftObjectPtr<USlimeSouvenirDefinition> SouvenirDefinition;

protected:
	virtual void PrepareDefinition(USlimeInventorySubsystem& Inventory) override;
	void ApplyWorldMesh();
};
