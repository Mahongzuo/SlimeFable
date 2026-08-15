#include "Quest/QuestObjectiveComponent.h"
#include "Quest/QuestSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

UQuestObjectiveComponent::UQuestObjectiveComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FVector UQuestObjectiveComponent::GetPromptWorldLocation() const
{
	if (const AActor* Owner = GetOwner())
	{
		return Owner->GetActorLocation() + FVector(0.f, 0.f, PromptHeightOffset);
	}
	return FVector::ZeroVector;
}

FText UQuestObjectiveComponent::GetPromptVerb() const
{
	if (!PromptVerb.IsEmpty())
	{
		return PromptVerb;
	}
	return FText::FromString(TEXT("交谈"));
}

bool UQuestObjectiveComponent::TryContribute()
{
	if (bConsumed)
	{
		return false;
	}

	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UQuestSubsystem* Quests = GI ? GI->GetSubsystem<UQuestSubsystem>() : nullptr;
	if (!Quests || !Quests->CanContribute(ChapterId, QuestId, BranchId))
	{
		return false;
	}

	if (Quests->NotifyProgress(ChapterId, QuestId, BranchId, 1))
	{
		bConsumed = true;
		return true;
	}
	return false;
}
