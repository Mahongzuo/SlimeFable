#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QuestObjectiveComponent.generated.h"

UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API UQuestObjectiveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQuestObjectiveComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName ChapterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName BranchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText PromptVerb;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	float PromptHeightOffset = 80.f;

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool IsConsumed() const { return bConsumed; }

	UFUNCTION(BlueprintCallable, Category = "Quest")
	void SetConsumed(bool bInConsumed) { bConsumed = bInConsumed; }

	UFUNCTION(BlueprintPure, Category = "Quest")
	FVector GetPromptWorldLocation() const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	FText GetPromptVerb() const;

	UFUNCTION(BlueprintCallable, Category = "Quest")
	bool TryContribute();

protected:
	bool bConsumed = false;
};
