#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeElementTypes.h"
#include "SlimeReactionHearth.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UQuestObjectiveComponent;

UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeReactionHearth : public AActor
{
	GENERATED_BODY()

public:
	ASlimeReactionHearth();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "0_Config|Hearth")
	bool IsOpen() const { return bOpen; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Hearth")
	ESlimeElement FirstElement = ESlimeElement::Water;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Hearth")
	ESlimeElement SecondElement = ESlimeElement::Lightning;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Hearth")
	TObjectPtr<AActor> DoorActor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UBoxComponent> Volume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UQuestObjectiveComponent> Objective;

protected:
	bool MatchesReactionTable() const;
	void TryOpen();

	bool bLatchedFirst = false;
	bool bLatchedSecond = false;
	bool bOpen = false;
};
