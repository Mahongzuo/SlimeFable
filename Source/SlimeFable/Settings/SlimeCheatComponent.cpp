// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/SlimeCheatComponent.h"

#include "Settings/SlimeCheatSubsystem.h"
#include "Settings/SlimeInputSettings.h"
#include "Settings/SlimeInputTypes.h"
#include "UI/SlimeCheatConsoleWidget.h"
#include "Inventory/SlimePlacementComponent.h"
#include "SlimeFablePlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

USlimeCheatComponent::USlimeCheatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void USlimeCheatComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(true);
}

void USlimeCheatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseConsole();
	if (ConsoleWidget)
	{
		ConsoleWidget->RemoveFromParent();
		ConsoleWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

bool USlimeCheatComponent::IsConsoleOpen() const
{
	return bInputOpen && ConsoleWidget && ConsoleWidget->IsInViewport();
}

void USlimeCheatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(PC);
	if (!PC)
	{
		return;
	}

	if (IsConsoleOpen())
	{
		return;
	}

	// Only real pause-class modals block opening; CheatConsole is overlay and must not lock combat.
	if (SlimePC && SlimePC->HasModalUI())
	{
		return;
	}

	if (const USlimePlacementComponent* Placement = Pawn->FindComponentByClass<USlimePlacementComponent>())
	{
		if (Placement->IsPlacing())
		{
			return;
		}
	}

	UGameInstance* GI = Pawn->GetGameInstance();
	const USlimeInputSettings* InputSettings = GI ? GI->GetSubsystem<USlimeInputSettings>() : nullptr;
	if (!InputSettings || !InputSettings->WasKeyPressed(PC, ESlimeInputAction::CheatConsole))
	{
		return;
	}

	OpenConsole();
}

void USlimeCheatComponent::OpenConsole()
{
	if (IsConsoleOpen())
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(PC);
	if (!PC || !SlimePC)
	{
		return;
	}

	if (!ConsoleWidget)
	{
		ConsoleWidget = CreateWidget<USlimeCheatConsoleWidget>(PC, USlimeCheatConsoleWidget::StaticClass());
	}
	if (!ConsoleWidget)
	{
		return;
	}

	ConsoleWidget->Setup(this);
	ConsoleWidget->ShowInputBar(true);
	if (!ConsoleWidget->IsInViewport())
	{
		ConsoleWidget->AddToViewport(50);
	}
	bInputOpen = true;
	SlimePC->PushUIInput(ESlimeUIInputReason::CheatConsole, ConsoleWidget);
	if (UWorld* World = GetWorld())
	{
		ConsoleWidget->SetIgnoreCommitUntil(World->GetTimeSeconds() + 0.2f);
	}
	ConsoleWidget->FocusInput();
}

void USlimeCheatComponent::CloseConsole()
{
	const bool bWasOpen = bInputOpen;
	bInputOpen = false;

	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(PC);
	if (SlimePC && (bWasOpen || SlimePC->HasUIInput(ESlimeUIInputReason::CheatConsole)))
	{
		SlimePC->PopUIInput(ESlimeUIInputReason::CheatConsole);
		SlimePC->RestoreGameplayInput();
	}

	if (ConsoleWidget)
	{
		if (ConsoleWidget->IsToastVisible())
		{
			ConsoleWidget->ShowInputBar(false);
			ConsoleWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ConsoleWidget->RemoveFromParent();
		}
	}
}

void USlimeCheatComponent::HandleCommand(const FString& RawCommand)
{
	AActor* Owner = GetOwner();
	UGameInstance* GI = Owner ? Owner->GetGameInstance() : nullptr;
	USlimeCheatSubsystem* Cheats = GI ? GI->GetSubsystem<USlimeCheatSubsystem>() : nullptr;
	if (!Cheats)
	{
		CloseConsole();
		return;
	}

	FString Message;
	Cheats->ExecuteCommand(RawCommand, Message);

	bInputOpen = false;
	APawn* Pawn = Cast<APawn>(Owner);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(PC))
	{
		SlimePC->PopUIInput(ESlimeUIInputReason::CheatConsole);
		SlimePC->RestoreGameplayInput();
	}

	if (ConsoleWidget)
	{
		ConsoleWidget->ShowInputBar(false);
		ConsoleWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (!Message.IsEmpty())
		{
			if (!ConsoleWidget->IsInViewport())
			{
				ConsoleWidget->AddToViewport(50);
			}
			ConsoleWidget->ShowToast(Message, 2.f);
		}
		else
		{
			ConsoleWidget->RemoveFromParent();
		}
	}
}
