#include "Quest/SlimeElementLock.h"
#include "Quest/QuestObjectiveComponent.h"
#include "Slime/SlimeElementComponent.h"
#include "GameFramework/Pawn.h"

ASlimeElementLock::ASlimeElementLock()
{
	if (Mesh)
	{
		Mesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.1f));
	}
}

bool ASlimeElementLock::HasRequiredElement(const APawn* Interactor) const
{
	if (!Interactor)
	{
		return false;
	}
	const USlimeElementComponent* Element = Interactor->FindComponentByClass<USlimeElementComponent>();
	return Element && Element->CurrentElement == RequiredElement;
}

FText ASlimeElementLock::MakeSwapHint() const
{
	switch (RequiredElement)
	{
	case ESlimeElement::Water: return FText::FromString(TEXT("换水"));
	case ESlimeElement::Wind: return FText::FromString(TEXT("换风"));
	case ESlimeElement::Fire: return FText::FromString(TEXT("换火"));
	case ESlimeElement::Lightning: return FText::FromString(TEXT("换雷"));
	case ESlimeElement::Dark: return FText::FromString(TEXT("换暗"));
	case ESlimeElement::Physical: return FText::FromString(TEXT("换物"));
	default: return FText::FromString(TEXT("换元素"));
	}
}

FText ASlimeElementLock::GetInteractPromptVerb() const
{
	if (bUnlocked)
	{
		return FText::FromString(TEXT("已开"));
	}
	return MakeSwapHint();
}

bool ASlimeElementLock::TryInteract(APawn* Interactor)
{
	if (bUnlocked)
	{
		return false;
	}
	if (!HasRequiredElement(Interactor))
	{
		return false;
	}

	bUnlocked = true;
	ApplyConsumedVisual();
	if (Objective && !Objective->ChapterId.IsNone())
	{
		Objective->TryContribute();
	}
	return true;
}
