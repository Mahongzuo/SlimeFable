#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QuestInteractActor.generated.h"

class UStaticMeshComponent;
class UQuestObjectiveComponent;

UCLASS(Blueprintable)
class SLIMEFABLE_API AQuestInteractActor : public AActor
{
	GENERATED_BODY()

public:
	AQuestInteractActor();

	UFUNCTION(BlueprintCallable, Category = "Quest")
	virtual bool TryInteract(APawn* Interactor);

	UFUNCTION(BlueprintPure, Category = "Quest")
	FVector GetPromptWorldLocation() const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	virtual FText GetInteractPromptVerb() const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	virtual bool CanBeFocused() const;

	UFUNCTION(BlueprintCallable, Category = "Quest")
	void Configure(FName ChapterId, FName QuestId, FName BranchId, const FText& PromptVerb, const FLinearColor& Color, float Scale = 0.5f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	TObjectPtr<UQuestObjectiveComponent> Objective;

protected:
	void ApplyConsumedVisual();
};
