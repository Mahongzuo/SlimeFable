// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeFableGameInstance.h"
#include "UI/SlimeLoadingGateWidget.h"
#include "Quest/QuestTypes.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "RHI.h"
#include "SlimeFable.h"
#include "SlimeFablePlayerController.h"

void USlimeFableGameInstance::Init()
{
	Super::Init();

	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &USlimeFableGameInstance::BeginLoadingScreen);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &USlimeFableGameInstance::EndLoadingScreen);
}

void USlimeFableGameInstance::Shutdown()
{
	if (PreLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
		PreLoadMapHandle.Reset();
	}
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	if (ActiveLoadingGate)
	{
		ActiveLoadingGate->RemoveFromParent();
		ActiveLoadingGate = nullptr;
	}

	Super::Shutdown();
}

void USlimeFableGameInstance::BeginLoadingScreen(const FString& MapName)
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	// No MoviePlayer overlay — it conflicted with LoadingGate's menu background.
	// Only suppress "Preparing Shaders" chrome; the gate restores it when finished.
	bPrevScreenMessagesEnabled = GAreScreenMessagesEnabled;
	GAreScreenMessagesEnabled = false;
	UE_LOG(LogSlimeFable, Log, TEXT("Map travel started for '%s' (LoadingGate after load)"), *MapName);
}

void USlimeFableGameInstance::EndLoadingScreen(UWorld* LoadedWorld)
{
	ShowLoadingGate(LoadedWorld);
}

static bool ParseYearStoryMap(const UWorld* World, FString& OutDay, FString& OutChapter)
{
	if (!World)
	{
		return false;
	}
	TArray<FString> Names;
	Names.Add(UWorld::RemovePIEPrefix(World->GetMapName()));
	if (const UPackage* Package = World->GetOutermost())
	{
		Names.Add(FPackageName::GetShortName(UWorld::RemovePIEPrefix(Package->GetName())));
	}
	for (const FString& Name : Names)
	{
		if (!Name.StartsWith(TEXT("SL_")) || Name.Len() <= 8)
		{
			continue;
		}
		const FString Day = Name.Mid(3, 4);
		const FString Chapter = Name.Mid(8);
		if (Day.Len() == 4 && Day.IsNumeric() && Chapter.Len() == 4 && Chapter.IsNumeric())
		{
			OutDay = Day;
			OutChapter = Chapter;
			return true;
		}
	}
	return false;
}

static void ResolveYearStory(const UWorld* World, FText& OutTitle, FText& OutBody)
{
	OutTitle = FText::GetEmpty();
	OutBody = FText::GetEmpty();
	FString Day;
	FString Chapter;
	if (!ParseYearStoryMap(World, Day, Chapter))
	{
		return;
	}
	const FString Month = Day.Left(2);
	const FString ObjectPath = FString::Printf(
		TEXT("/Game/_Slime/Days/%s/%s/Quests/DA_Quest_%s.DA_Quest_%s"),
		*Month, *Day, *Day, *Day);
	const UDayQuestBook* Book = LoadObject<UDayQuestBook>(nullptr, *ObjectPath);
	const FQuestChapter* Story = Book ? Book->FindChapter(FName(*Chapter)) : nullptr;
	if (!Story || Story->Summary.IsEmpty())
	{
		return;
	}
	OutTitle = FText::FromString(FString::Printf(TEXT("穿越到%s年"), *Chapter));
	OutBody = Story->Summary;
}

void USlimeFableGameInstance::ShowLoadingGate(UWorld* LoadedWorld)
{
	// -nullrhi (headless verification): Slate never ticks the widget, so the gate would pause the world forever.
	if (IsRunningDedicatedServer() || !LoadedWorld || GUsingNullRHI)
	{
		GAreScreenMessagesEnabled = bPrevScreenMessagesEnabled;
		return;
	}

	if (ActiveLoadingGate)
	{
		ActiveLoadingGate->RemoveFromParent();
		ActiveLoadingGate = nullptr;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(LoadedWorld, 0);
	if (!PC)
	{
		GAreScreenMessagesEnabled = bPrevScreenMessagesEnabled;
		return;
	}

	ActiveLoadingGate = CreateWidget<USlimeLoadingGateWidget>(PC, USlimeLoadingGateWidget::StaticClass());
	if (!ActiveLoadingGate)
	{
		GAreScreenMessagesEnabled = bPrevScreenMessagesEnabled;
		return;
	}

	FText StoryTitle;
	FText StoryBody;
	ResolveYearStory(LoadedWorld, StoryTitle, StoryBody);
	ActiveLoadingGate->SetStory(StoryTitle, StoryBody);
	ActiveLoadingGate->OnGateFinished.AddDynamic(this, &USlimeFableGameInstance::HandleLoadingGateFinished);
	ActiveLoadingGate->AddToViewport(100);

	if (ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(PC))
	{
		SlimePC->PushUIInput(ESlimeUIInputReason::LoadingGate, ActiveLoadingGate);
	}
	else
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(ActiveLoadingGate->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = false;
	}

	UE_LOG(LogSlimeFable, Log, TEXT("Loading gate shown for world '%s'"), *LoadedWorld->GetMapName());
}

void USlimeFableGameInstance::HandleLoadingGateFinished()
{
	GAreScreenMessagesEnabled = bPrevScreenMessagesEnabled;
	ActiveLoadingGate = nullptr;

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			if (ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(PC))
			{
				SlimePC->PopUIInput(ESlimeUIInputReason::LoadingGate);
			}
			else
			{
				// Main menu stays UI-only.
				FInputModeUIOnly InputMode;
				InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				PC->SetInputMode(InputMode);
				PC->bShowMouseCursor = true;
			}
		}
	}
}

void USlimeFableGameInstance::SlimeHostListen(const FString& MapPackage)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeHostListen: no world"));
		return;
	}
	FString URL = MapPackage;
	if (!URL.Contains(TEXT("?listen")))
	{
		URL += TEXT("?listen");
	}
	World->ServerTravel(URL, true);
}

void USlimeFableGameInstance::SlimeJoin(const FString& Address)
{
	if (Address.IsEmpty())
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeJoin: empty address"));
		return;
	}
	if (APlayerController* PC = GetFirstLocalPlayerController())
	{
		PC->ClientTravel(Address, TRAVEL_Absolute);
	}
}
