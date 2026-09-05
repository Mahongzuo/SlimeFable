// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeDodgeComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "PhoebeClimbComponent.h"
#include "PhoebeEnemy.h"
#include "EnemyCharacter.h"
#include "EnemyFighter.h"
#include "EnemyTower.h"
#include "GaspSandboxPawn.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "MoverComponent.h"
#include "SlimeAIController.h"
#include "SlimeBodyComponent.h"
#include "SlimeDevourComponent.h"
#include "SlimeDevourTarget.h"
#include "SlimeDodgeAfterimage.h"
#include "SlimeEnemyCharacter.h"
#include "SlimeHealthComponent.h"
#include "Settings/SlimeInputSettings.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"
#include "Sound/SoundBase.h"
#include "Settings/SlimeAudioPlay.h"

namespace SlimeDodgeAudio
{
	static const TCHAR* DefaultBlink = TEXT("/Game/Audio/SFX/Movement/sfx_blink_01.sfx_blink_01");
	static const TCHAR* DefaultPerfect = TEXT("/Game/Audio/SFX/Combat/sfx_perfect_dodge_01.sfx_perfect_dodge_01");

	USoundBase* Resolve(const TSoftObjectPtr<USoundBase>& Soft, const TCHAR* Fallback)
	{
		if (!Soft.IsNull())
		{
			if (USoundBase* Loaded = Soft.LoadSynchronous())
			{
				return Loaded;
			}
		}
		return LoadObject<USoundBase>(nullptr, Fallback);
	}

	void PlayAtOwner(const UActorComponent* Comp, const TSoftObjectPtr<USoundBase>& Soft, const TCHAR* Fallback)
	{
		AActor* Owner = Comp ? Comp->GetOwner() : nullptr;
		if (!Owner)
		{
			return;
		}
		if (USoundBase* Sfx = Resolve(Soft, Fallback))
		{
			SlimeAudioPlay::PlaySfxAt(Owner, Sfx, Owner->GetActorLocation());
		}
	}
}

USlimeDodgeComponent::USlimeDodgeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	BlinkDashSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(SlimeDodgeAudio::DefaultBlink));
	PerfectDodgeSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(SlimeDodgeAudio::DefaultPerfect));
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

	// GASP Sandbox pawns (not AEnemyCharacter): use DetectRange so combat roll wins over blink.
	for (TActorIterator<AGaspSandboxPawn> It(World); It; ++It)
	{
		AGaspSandboxPawn* Gasp = *It;
		if (!Gasp || Gasp == Owner || Gasp->IsMorphTarget() || Gasp->IsInDeathSequence())
		{
			continue;
		}
		if (const USlimeHealthComponent* EnemyHealth = Gasp->GetEnemyHealth())
		{
			if (!EnemyHealth->IsAlive())
			{
				continue;
			}
		}

		if (FVector::DistSquared(Loc, Gasp->GetActorLocation()) <= FMath::Square(Gasp->DetectRange))
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
	if (const USlimeDevourComponent* Devour = GetOwner() ? GetOwner()->FindComponentByClass<USlimeDevourComponent>() : nullptr)
	{
		if (Devour->IsCombatLocked() || Devour->IsPhantomWheelOpen())
		{
			return;
		}
	}

	// Morphed Phoebe: RMB is sprint/climb-dash unless an attack is actually incoming.
	if (APhoebeEnemy* Phoebe = Cast<APhoebeEnemy>(GetOwner()))
	{
		if (!Phoebe->WantsCombatDodge())
		{
			return;
		}
	}

	if (!IsInEnemyThreatRange())
	{
		SlimeDodgeAudio::PlayAtOwner(this, BlinkDashSound, SlimeDodgeAudio::DefaultBlink);
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
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return FVector::ForwardVector;
	}

	FVector Dir = Pawn->GetLastMovementInputVector();
	Dir.Z = 0.f;
	if (Dir.IsNearlyZero())
	{
		Dir = Pawn->GetPendingMovementInputVector();
		Dir.Z = 0.f;
	}
	if (Dir.IsNearlyZero())
	{
		Dir = Pawn->GetVelocity();
		Dir.Z = 0.f;
	}
	if (Dir.IsNearlyZero())
	{
		if (const AGaspSandboxPawn* Gasp = Cast<AGaspSandboxPawn>(Pawn))
		{
			Dir = Gasp->GetAiMoveIntent();
			Dir.Z = 0.f;
		}
	}
	if (Dir.IsNearlyZero())
	{
		if (const ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			if (const UCharacterMovementComponent* Move = Character->GetCharacterMovement())
			{
				Dir = Move->Velocity;
				Dir.Z = 0.f;
			}
		}
	}
	if (Dir.IsNearlyZero())
	{
		Dir = Pawn->GetActorForwardVector();
		Dir.Z = 0.f;
	}
	return Dir.GetSafeNormal();
}

void USlimeDodgeComponent::PerformCombatRoll(bool bSpawnRollAfterimage)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const FVector Dir = ResolveRollDirection();
	const float Duration = FMath::Max(RollDuration, 0.05f);
	const float Speed = RollDistance / Duration;
	const FVector Impulse = Dir * Speed + FVector(0.f, 0.f, 40.f);

	if (ACharacter* Character = Cast<ACharacter>(Owner))
	{
		Character->LaunchCharacter(Impulse, true, true);
	}
	else if (UMoverComponent* Mover = Owner->FindComponentByClass<UMoverComponent>())
	{
		TSharedPtr<FApplyVelocityEffect> Effect = MakeShared<FApplyVelocityEffect>();
		Effect->VelocityToApply = Impulse;
		Effect->bAdditiveVelocity = false;
		Mover->QueueInstantMovementEffect(Effect);
	}

	// Regular roll ghost: GASP / skeletal devour targets only. Slime body stays on perfect dodge.
	if (bSpawnRollAfterimage && !Owner->FindComponentByClass<USlimeBodyComponent>())
	{
		if (const ISlimeDevourTarget* Target = SlimeDevourUtil::As(Owner))
		{
			USkeletalMeshComponent* Mesh = Target->GetDevourPreviewMesh();
			if (!Mesh || !Mesh->GetSkeletalMeshAsset())
			{
				Mesh = Target->GetPrimarySkeletalMesh();
			}
			if (Mesh && Mesh->GetSkeletalMeshAsset())
			{
				SpawnAfterimage();
			}
		}
	}
}

void USlimeDodgeComponent::PerformPerfectDodge()
{
	SpawnAfterimage();

	if (Health)
	{
		Health->SetInvulnerableFor(PerfectInvulnDuration);
	}

	PerformCombatRoll(false);
	SlimeDodgeAudio::PlayAtOwner(this, PerfectDodgeSound, SlimeDodgeAudio::DefaultPerfect);
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

	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASlimeDodgeAfterimage* Ghost = World->SpawnActor<ASlimeDodgeAfterimage>(
		ASlimeDodgeAfterimage::StaticClass(), FTransform::Identity, Params);
	if (!Ghost)
	{
		return;
	}

	if (USlimeBodyComponent* Body = Owner->FindComponentByClass<USlimeBodyComponent>())
	{
		Ghost->CaptureFromSlime(Body, AfterimageLife);
		return;
	}

	USkeletalMeshComponent* Source = nullptr;
	if (const ISlimeDevourTarget* Target = SlimeDevourUtil::As(Owner))
	{
		Source = Target->GetDevourPreviewMesh();
		if (!Source || !Source->GetSkeletalMeshAsset())
		{
			Source = Target->GetPrimarySkeletalMesh();
		}
	}
	if (!Source || !Source->GetSkeletalMeshAsset())
	{
		if (const ACharacter* Character = Cast<ACharacter>(Owner))
		{
			Source = Character->GetMesh();
		}
	}
	if (!Source || !Source->GetSkeletalMeshAsset())
	{
		if (const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Owner))
		{
			for (USceneComponent* Part : Enemy->GetGeneratedParts())
			{
				if (USkeletalMeshComponent* PartSkel = Cast<USkeletalMeshComponent>(Part))
				{
					if (PartSkel->GetSkeletalMeshAsset())
					{
						Source = PartSkel;
						break;
					}
				}
			}
		}
	}

	if (Source && Source->GetSkeletalMeshAsset())
	{
		Ghost->CaptureFromSkeletalMesh(Source, AfterimageLife);
		return;
	}

	Ghost->Destroy();
}
