// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeDodgeComponent.h"

#include "EnemyCharacter.h"
#include "EnemyFighter.h"
#include "EnemyTower.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "SlimeAIController.h"
#include "SlimeBodyComponent.h"
#include "SlimeDodgeAfterimage.h"
#include "SlimeEnemyCharacter.h"
#include "SlimeHealthComponent.h"
#include "Settings/SlimeInputSettings.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"

USlimeDodgeComponent::USlimeDodgeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USlimeDodgeComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		Health = Owner->FindComponentByClass<USlimeHealthComponent>();
	}
}

void USlimeDodgeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	CooldownRemaining = FMath::Max(CooldownRemaining - DeltaTime, 0.f);
	if (bPollRightMouse)
	{
		PollRightMouse();
	}
}

void USlimeDodgeComponent::NotifyPlayerIncomingAttack(UObject* WorldContext, AActor* Attacker)
{
	if (!WorldContext)
	{
		return;
	}
	APawn* Player = UGameplayStatics::GetPlayerPawn(WorldContext, 0);
	if (!Player)
	{
		return;
	}
	if (USlimeDodgeComponent* Dodge = Player->FindComponentByClass<USlimeDodgeComponent>())
	{
		Dodge->NotifyIncomingAttack(Attacker);
	}
}

void USlimeDodgeComponent::NotifyIncomingAttack(AActor* Attacker)
{
	if (UWorld* World = GetWorld())
	{
		LastIncomingAttackTime = World->GetTimeSeconds();
	}
}

bool USlimeDodgeComponent::IsInPerfectWindow() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	return (World->GetTimeSeconds() - LastIncomingAttackTime) <= PerfectWindow;
}

bool USlimeDodgeComponent::IsInEnemyThreatRange() const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return false;
	}

	const FVector Loc = Owner->GetActorLocation();
	for (TActorIterator<AEnemyCharacter> It(World); It; ++It)
	{
		AEnemyCharacter* Enemy = *It;
		if (!Enemy || Enemy == Owner)
		{
			continue;
		}
		if (Enemy->GetEnemyPresence() == EEnemyPresence::Sleep
			|| Enemy->GetEnemyPresence() == EEnemyPresence::Despawned)
		{
			continue;
		}
		if (const USlimeHealthComponent* EnemyHealth = Enemy->GetEnemyHealth())
		{
			if (!EnemyHealth->IsAlive())
			{
				continue;
			}
		}

		float Range = 0.f;
		if (const AEnemyTower* Tower = Cast<AEnemyTower>(Enemy))
		{
			Range = Tower->AttackRange;
		}
		else if (const AEnemyFighter* Fighter = Cast<AEnemyFighter>(Enemy))
		{
			Range = Fighter->DetectRange;
		}
		else
		{
			continue;
		}

		if (FVector::DistSquared(Loc, Enemy->GetActorLocation()) <= FMath::Square(Range))
		{
			return true;
		}
	}

	// AI slime enemies: block Blink Dash inside their AttackRange.
	for (TActorIterator<ASlimeEnemyCharacter> It(World); It; ++It)
	{
		ASlimeEnemyCharacter* EnemySlime = *It;
		if (!EnemySlime || EnemySlime == Owner)
		{
			continue;
		}
		if (const USlimeHealthComponent* EnemyHealth = EnemySlime->GetSlimeHealth())
		{
			if (!EnemyHealth->IsAlive())
			{
				continue;
			}
		}

		float Range = 150.f;
		if (const ASlimeAIController* AIC = Cast<ASlimeAIController>(EnemySlime->GetController()))
		{
			Range = AIC->AttackRange;
		}

		if (FVector::DistSquared(Loc, EnemySlime->GetActorLocation()) <= FMath::Square(Range))
		{
			return true;
		}
	}

	return false;
}

void USlimeDodgeComponent::PollRightMouse()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !Pawn->IsPlayerControlled())
	{
		return;
	}

	bool bDown = false;
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (const USlimeInputSettings* InputSettings = GI ? GI->GetSubsystem<USlimeInputSettings>() : nullptr)
	{
		bDown = InputSettings->IsKeyDown(PC, ESlimeInputAction::Dodge);
	}
	else
	{
		bDown = PC->IsInputKeyDown(EKeys::RightMouseButton);
	}
	if (bDown && !bRightMouseDown)
	{
		TryHandleRightClick();
	}
	bRightMouseDown = bDown;
}

void USlimeDodgeComponent::TryHandleRightClick()
{
	if (!IsInEnemyThreatRange())
	{
		OnBlinkDashRequested.Broadcast();
		return;
	}

	if (CooldownRemaining > 0.f)
	{
		return;
	}

	CooldownRemaining = RollCooldown;

	if (IsInPerfectWindow())
	{
		PerformPerfectDodge();
	}
	else
	{
		PerformCombatRoll();
	}
}

FVector USlimeDodgeComponent::ResolveRollDirection() const
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return FVector::ForwardVector;
	}

	FVector Dir = Character->GetLastMovementInputVector();
	Dir.Z = 0.f;
	if (Dir.IsNearlyZero())
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Dir = Move->Velocity;
			Dir.Z = 0.f;
		}
	}
	if (Dir.IsNearlyZero())
	{
		Dir = Character->GetActorForwardVector();
		Dir.Z = 0.f;
	}
	return Dir.GetSafeNormal();
}

void USlimeDodgeComponent::PerformCombatRoll()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}
	const FVector Dir = ResolveRollDirection();
	const float Duration = FMath::Max(RollDuration, 0.05f);
	const float Speed = RollDistance / Duration;
	Character->LaunchCharacter(Dir * Speed + FVector(0.f, 0.f, 40.f), true, true);
}

void USlimeDodgeComponent::PerformPerfectDodge()
{
	SpawnAfterimage();

	if (Health)
	{
		Health->SetInvulnerableFor(PerfectInvulnDuration);
	}

	PerformCombatRoll();
	OnPerfectDodge.Broadcast();
}

void USlimeDodgeComponent::SpawnAfterimage()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}

	USlimeBodyComponent* Body = Owner->FindComponentByClass<USlimeBodyComponent>();
	if (!Body)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ASlimeDodgeAfterimage* Ghost = World->SpawnActor<ASlimeDodgeAfterimage>(
			ASlimeDodgeAfterimage::StaticClass(), FTransform::Identity, Params))
	{
		Ghost->CaptureFromSlime(Body, AfterimageLife);
	}
}
