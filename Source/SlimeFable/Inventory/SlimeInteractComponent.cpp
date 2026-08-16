// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeInteractComponent.h"

#include "SlimeFablePlayerController.h"
#include "SlimeWorldPickup.h"
#include "SlimePlacedActor.h"
#include "Quest/QuestInteractActor.h"
#include "Quest/QuestSubsystem.h"
#include "SlimeVehiclePickup.h"
#include "SlimeInventorySubsystem.h"
#include "SlimePlacementComponent.h"
#include "UI/SlimeInventoryWidget.h"
#include "Settings/SlimeInputSettings.h"
#include "Settings/SlimeInputTypes.h"
#include "SlimeAbilityComponent.h"
#include "SlimeVehicleComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"

USlimeInteractComponent::USlimeInteractComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USlimeInteractComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool USlimeInteractComponent::CanInteractNow() const
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || PC->IsPaused())
	{
		return false;
	}
	if (IsInventoryOpen())
	{
		return false;
	}
	if (const UGameInstance* Game = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (const UQuestSubsystem* Quests = Game->GetSubsystem<UQuestSubsystem>())
		{
			if (Quests->IsQuestLogOpen() || Quests->IsWeekSelectOpen())
			{
				return false;
			}
		}
	}
	if (const USlimeAbilityComponent* Abilities = Pawn->FindComponentByClass<USlimeAbilityComponent>())
	{
		if (Abilities->IsWheelOpen())
		{
			return false;
		}
	}
	if (const USlimePlacementComponent* Placement = Pawn->FindComponentByClass<USlimePlacementComponent>())
	{
		if (Placement->IsPlacing())
		{
			return false;
		}
	}
	return true;
}

void USlimeInteractComponent::RefreshFocusedTarget()
{
	FocusedPickup.Reset();
	FocusedPlaced.Reset();
	FocusedQuest.Reset();

	APawn* Pawn = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		return;
	}

	if (const USlimeVehicleComponent* Vehicle = Pawn->FindComponentByClass<USlimeVehicleComponent>())
	{
		if (Vehicle->IsUsingVehicle())
		{
			return;
		}
	}

	const FVector Loc = Pawn->GetActorLocation();
	const float RadiusSq = FMath::Square(InteractRadius);
	float BestDistSq = TNumericLimits<float>::Max();
	ASlimeWorldPickup* BestPickup = nullptr;
	ASlimePlacedActor* BestPlaced = nullptr;
	AQuestInteractActor* BestQuest = nullptr;

	for (TActorIterator<ASlimeWorldPickup> It(World); It; ++It)
	{
		ASlimeWorldPickup* Pickup = *It;
		if (!IsValid(Pickup))
		{
			continue;
		}
		if (const ASlimeVehiclePickup* VehiclePickup = Cast<ASlimeVehiclePickup>(Pickup))
		{
			if (!VehiclePickup->CanBeUsedBy(Pawn))
			{
				continue;
			}
		}
		const float DistSq = FVector::DistSquared(Loc, Pickup->GetActorLocation());
		if (DistSq <= RadiusSq && DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestPickup = Pickup;
			BestPlaced = nullptr;
			BestQuest = nullptr;
		}
	}

	for (TActorIterator<ASlimePlacedActor> It(World); It; ++It)
	{
		ASlimePlacedActor* Placed = *It;
		if (!IsValid(Placed) || Placed->GetItemId().IsNone())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Loc, Placed->GetActorLocation());
		if (DistSq <= RadiusSq && DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestPlaced = Placed;
			BestPickup = nullptr;
			BestQuest = nullptr;
		}
	}

	for (TActorIterator<AQuestInteractActor> It(World); It; ++It)
	{
		AQuestInteractActor* QuestActor = *It;
		if (!IsValid(QuestActor) || !QuestActor->CanBeFocused())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Loc, QuestActor->GetActorLocation());
		if (DistSq <= RadiusSq && DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestQuest = QuestActor;
			BestPickup = nullptr;
			BestPlaced = nullptr;
		}
	}

	FocusedPickup = BestPickup;
	FocusedPlaced = BestPlaced;
	FocusedQuest = BestQuest;
}

bool USlimeInteractComponent::GetFocusedPromptWorldLocation(FVector& OutLocation) const
{
	if (ASlimeWorldPickup* Pickup = FocusedPickup.Get())
	{
		OutLocation = Pickup->GetPromptWorldLocation();
		return true;
	}
	if (ASlimePlacedActor* Placed = FocusedPlaced.Get())
	{
		OutLocation = Placed->GetPromptWorldLocation();
		return true;
	}
	if (AQuestInteractActor* QuestActor = FocusedQuest.Get())
	{
		OutLocation = QuestActor->GetPromptWorldLocation();
		return true;
	}
	return false;
}

FText USlimeInteractComponent::GetFocusedPromptVerb() const
{
	if (ASlimeWorldPickup* Pickup = FocusedPickup.Get())
	{
		return Pickup->GetInteractPromptVerb();
	}
	if (FocusedPlaced.IsValid())
	{
		return FText::FromString(TEXT("拾取"));
	}
	if (AQuestInteractActor* QuestActor = FocusedQuest.Get())
	{
		return QuestActor->GetInteractPromptVerb();
	}
	return FText::GetEmpty();
}

bool USlimeInteractComponent::TryInteract()
{
	if (!CanInteractNow())
	{
		return false;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return false;
	}

	if (USlimeVehicleComponent* Vehicle = Pawn->FindComponentByClass<USlimeVehicleComponent>())
	{
		if (Vehicle->IsUsingVehicle())
		{
			Vehicle->ExitVehicle(true);
			return true;
		}
	}

	RefreshFocusedTarget();
	if (ASlimeWorldPickup* Pickup = FocusedPickup.Get())
	{
		return Pickup->TryPickup(Pawn);
	}
	if (ASlimePlacedActor* Placed = FocusedPlaced.Get())
	{
		return Placed->TryPickup(Pawn);
	}
	if (AQuestInteractActor* QuestActor = FocusedQuest.Get())
	{
		return QuestActor->TryInteract(Pawn);
	}
	return false;
}

void USlimeInteractComponent::ToggleInventory()
{
	if (IsInventoryOpen())
	{
		CloseInventory();
		return;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		return;
	}

	if (USlimePlacementComponent* Placement = Pawn->FindComponentByClass<USlimePlacementComponent>())
	{
		if (Placement->IsPlacing())
		{
			Placement->CancelPlacement();
		}
	}

	InventoryWidget = CreateWidget<USlimeInventoryWidget>(PC, USlimeInventoryWidget::StaticClass());
	if (!InventoryWidget)
	{
		return;
	}
	InventoryWidget->AddToViewport(30);
	if (ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(PC))
	{
		SlimePC->PushUIInput(ESlimeUIInputReason::Inventory, InventoryWidget);
	}
	else
	{
		UGameplayStatics::SetGamePaused(PC, true);
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(InventoryWidget->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}
}

void USlimeInteractComponent::CloseInventory()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (InventoryWidget)
	{
		InventoryWidget->RemoveFromParent();
		InventoryWidget = nullptr;
	}
	if (ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(PC))
	{
		SlimePC->PopUIInput(ESlimeUIInputReason::Inventory);
	}
	else if (PC)
	{
		UGameplayStatics::SetGamePaused(PC, false);
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
}

void USlimeInteractComponent::PollKeys()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !Pawn->IsPlayerControlled())
	{
		return;
	}

	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const USlimeInputSettings* InputSettings = GI ? GI->GetSubsystem<USlimeInputSettings>() : nullptr;

	auto WasPressed = [PC, InputSettings](ESlimeInputAction Action, const FKey& Fallback) -> bool
	{
		if (InputSettings)
		{
			return InputSettings->WasKeyPressed(PC, Action);
		}
		return PC->WasInputKeyJustPressed(Fallback);
	};

	if (WasPressed(ESlimeInputAction::Inventory, EKeys::B))
	{
		if (UQuestSubsystem* Quests = GI ? GI->GetSubsystem<UQuestSubsystem>() : nullptr)
		{
			if (Quests->IsQuestLogOpen())
			{
				Quests->CloseQuestLog();
			}
		}
		ToggleInventory();
	}

	if (WasPressed(ESlimeInputAction::QuestLog, EKeys::J))
	{
		if (!IsInventoryOpen())
		{
			if (UQuestSubsystem* Quests = GI ? GI->GetSubsystem<UQuestSubsystem>() : nullptr)
			{
				Quests->ToggleQuestLog();
			}
		}
	}

	if (WasPressed(ESlimeInputAction::Interact, EKeys::F))
	{
		TryInteract();
	}

	static const ESlimeInputAction HotbarActions[6] = {
		ESlimeInputAction::Hotbar1, ESlimeInputAction::Hotbar2, ESlimeInputAction::Hotbar3,
		ESlimeInputAction::Hotbar4, ESlimeInputAction::Hotbar5, ESlimeInputAction::Hotbar6
	};
	static const FKey HotbarFallback[6] = {
		EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six
	};

	if (!IsInventoryOpen() && CanInteractNow())
	{
		USlimeInventorySubsystem* Inv = GI ? GI->GetSubsystem<USlimeInventorySubsystem>() : nullptr;
		if (Inv)
		{
			for (int32 Index = 0; Index < 6; ++Index)
			{
				if (WasPressed(HotbarActions[Index], HotbarFallback[Index]))
				{
					if (Index >= 3)
					{
						CloseInventory();
					}
					Inv->ActivateHotbar(Index, Pawn);
				}
			}
		}
	}
}

void USlimeInteractComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshFocusedTarget();
	if (bPollKeys)
	{
		PollKeys();
	}
}
