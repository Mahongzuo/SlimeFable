#pragma once

#include "CoreMinimal.h"
#include "Quest/QuestInteractActor.h"
#include "QuestChapterGate.generated.h"

UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API AQuestChapterGate : public AQuestInteractActor
{
	GENERATED_BODY()

public:
	AQuestChapterGate();

	virtual bool TryInteract(APawn* Interactor) override;
	virtual FText GetInteractPromptVerb() const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Gate")
	FName TargetChapterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Gate")
	FName DayId = FName(TEXT("0815"));

protected:
	bool IsUnlocked() const;
};
