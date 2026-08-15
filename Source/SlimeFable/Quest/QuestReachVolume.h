#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QuestReachVolume.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UQuestObjectiveComponent;

UCLASS(Blueprintable)
class SLIMEFABLE_API AQuestReachVolume : public AActor
{
	GENERATED_BODY()

public:
	AQuestReachVolume();

	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

	UFUNCTION(BlueprintCallable, Category = "Quest")
	void Configure(FName ChapterId, FName QuestId, FName BranchId, const FVector& BoxExtent);

	UFUNCTION(BlueprintPure, Category = "Quest")
	UQuestObjectiveComponent* GetObjective() const { return Objective; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	TObjectPtr<UStaticMeshComponent> Marker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	TObjectPtr<UQuestObjectiveComponent> Objective;

protected:
	void TryCompleteFromPawn(AActor* OtherActor);
};
