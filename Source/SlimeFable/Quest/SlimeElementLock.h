#pragma once

#include "CoreMinimal.h"
#include "Quest/QuestInteractActor.h"
#include "SlimeElementTypes.h"
#include "SlimeElementLock.generated.h"

UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeElementLock : public AQuestInteractActor
{
	GENERATED_BODY()

public:
	ASlimeElementLock();

	virtual bool TryInteract(APawn* Interactor) override;
	virtual FText GetInteractPromptVerb() const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Lock")
	ESlimeElement RequiredElement = ESlimeElement::Water;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Lock")
	bool bUnlocked = false;

protected:
	bool HasRequiredElement(const APawn* Interactor) const;
	FText MakeSwapHint() const;
};
