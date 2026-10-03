#include "MiniGame/SlimeMiniGameDirector.h"
#include "Quest/QuestSubsystem.h"
#include "SlimeFable.h"
#include "Components/ActorComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ASlimeMiniGameDirector::ASlimeMiniGameDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	DisabledComponentClasses = {
		TEXT("SlimeAbilityComponent"),
		TEXT("SlimeClingComponent"),
		TEXT("SlimeDevourComponent"),
		TEXT("SlimeBuildModeComponent"),
		TEXT("SlimeLockOnComponent"),
	};
}

void ASlimeMiniGameDirector::BeginPlay()
{
	Super::BeginPlay();
	GetWorldTimerManager().SetTimer(EnterHandle, this, &ASlimeMiniGameDirector::TryEnter, 0.1f, true, 0.05f);
}

void ASlimeMiniGameDirector::TryEnter()
{
	APlayerController* PC = GetPlayerController();
	ACharacter* Character = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!Character)
	{
		if (++EnterAttempts > 100)
		{
			GetWorldTimerManager().ClearTimer(EnterHandle);
			UE_LOG(LogSlimeFable, Error, TEXT("MiniGame %s: no player character to enter mode"), *GetName());
		}
		return;
	}
	GetWorldTimerManager().ClearTimer(EnterHandle);
	Player = Character;
	for (UActorComponent* Component : Character->GetComponents())
	{
		if (Component && Component->IsComponentTickEnabled()
			&& DisabledComponentClasses.Contains(Component->GetClass()->GetFName()))
		{
			Component->SetComponentTickEnabled(false);
			DisabledComponents.Add(Component);
		}
	}
	bModeActive = true;
	EnterMode(Character, PC);
}

void ASlimeMiniGameDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(EnterHandle);
	if (bModeActive)
	{
		ExitMode(Player.Get(), GetPlayerController());
		for (const TWeakObjectPtr<UActorComponent>& Component : DisabledComponents)
		{
			if (Component.IsValid()) Component->SetComponentTickEnabled(true);
		}
		DisabledComponents.Reset();
		bModeActive = false;
	}
	Super::EndPlay(EndPlayReason);
}

void ASlimeMiniGameDirector::EnterMode(ACharacter* Character, APlayerController* PC)
{
}

void ASlimeMiniGameDirector::ExitMode(ACharacter* Character, APlayerController* PC)
{
}

APlayerController* ASlimeMiniGameDirector::GetPlayerController() const
{
	return UGameplayStatics::GetPlayerController(this, 0);
}

int32 ASlimeMiniGameDirector::GetWeekIndex() const
{
	const UGameInstance* GI = GetGameInstance();
	const UQuestSubsystem* Quests = GI ? GI->GetSubsystem<UQuestSubsystem>() : nullptr;
	return Quests ? FMath::Clamp(Quests->GetWeekIndex(), 1, 3) : 1;
}

void ASlimeMiniGameDirector::FinishMiniGame()
{
	if (bFinished) return;
	bFinished = true;
	UGameInstance* GI = GetGameInstance();
	UQuestSubsystem* Quests = GI ? GI->GetSubsystem<UQuestSubsystem>() : nullptr;
	if (!Quests || !Quests->NotifyProgress(ChapterId, QuestId, FinishBranchId, 1))
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("MiniGame %s: quest %s/%s/%s did not accept finish"),
			*GetName(), *ChapterId.ToString(), *QuestId.ToString(), *FinishBranchId.ToString());
	}
}

void ASlimeMiniGameDirector::FailMiniGame()
{
	if (bFinished) return;
	bFinished = true;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UQuestSubsystem* Quests = GI->GetSubsystem<UQuestSubsystem>())
		{
			Quests->RequestPlayerSessionRestart(this);
		}
	}
}
