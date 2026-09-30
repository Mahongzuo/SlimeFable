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

	/** 解开后隐藏并关掉碰撞。用来挡住楼梯，检查完成才放行。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Lock")
	TObjectPtr<AActor> DoorActor;

protected:
	bool HasRequiredElement(const APawn* Interactor) const;
	FText MakeSwapHint() const;
};
