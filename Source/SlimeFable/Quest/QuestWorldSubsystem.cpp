#include "Quest/QuestWorldSubsystem.h"
#include "Quest/QuestSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

bool UQuestWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UQuestWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	UGameInstance* GI = InWorld.GetGameInstance();
	UQuestSubsystem* Quests = GI ? GI->GetSubsystem<UQuestSubsystem>() : nullptr;
	if (Quests)
	{
		Quests->BeginForWorld(&InWorld);
	}
}
