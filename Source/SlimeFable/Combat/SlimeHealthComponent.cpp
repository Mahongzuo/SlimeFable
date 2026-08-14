// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeHealthComponent.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "ProceduralMeshComponent.h"
#include "SlimeBodyComponent.h"
#include "SlimeCombatComponent.h"
#include "SlimeElementComponent.h"
#include "SlimeFable.h"
#include "TimerManager.h"

USlimeHealthComponent::USlimeHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USlimeHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	ResetHP();
}

void USlimeHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DissolveTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void USlimeHealthComponent::ResetHP()
{
	CurrentHP = MaxHP;
	OnHealthChanged.Broadcast(CurrentHP, MaxHP);
}

void USlimeHealthComponent::ApplyHealing(float Healing)
{
	if (!IsAlive() || Healing <= 0.f)
	{
		return;
	}
	CurrentHP = FMath::Min(CurrentHP + Healing, MaxHP);
	OnHealthChanged.Broadcast(CurrentHP, MaxHP);
}

void USlimeHealthComponent::SetInvulnerableFor(float Seconds)
{
	if (UWorld* World = GetWorld())
	{
		InvulnerableUntil = FMath::Max(InvulnerableUntil, World->GetTimeSeconds() + FMath::Max(Seconds, 0.f));
	}
}

bool USlimeHealthComponent::IsInvulnerable() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < InvulnerableUntil;
}

float USlimeHealthComponent::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	if (!IsAlive() || Damage <= 0.f || bDissolving || IsInvulnerable())
	{
		return 0.f;
	}

	CurrentHP = FMath::Max(CurrentHP - Damage, 0.f);
	OnHealthChanged.Broadcast(CurrentHP, MaxHP);

	if (AActor* Owner = GetOwner())
	{
		if (USlimeElementComponent* Element = Owner->FindComponentByClass<USlimeElementComponent>())
		{
			Element->PlayHitFlash();
		}
		if (USlimeBodyComponent* Body = Owner->FindComponentByClass<USlimeBodyComponent>())
		{
			Body->ApplyHitJolt();
		}
		if (USlimeCombatComponent* Combat = Owner->FindComponentByClass<USlimeCombatComponent>())
		{
			Combat->InterruptCombat();
		}
		if (ACharacter* Character = Cast<ACharacter>(Owner))
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				Movement->AddImpulse(DamageImpulse, true);
			}
		}
	}

	if (CurrentHP <= 0.f)
	{
		HandleDeath(DamageCauser);
	}

	return Damage;
}

void USlimeHealthComponent::HandleDeath(AActor* DamageCauser)
{
	OnDied.Broadcast();
	UE_LOG(LogSlimeFable, Log, TEXT("Slime '%s' died (caused by '%s')"), *GetNameSafe(GetOwner()), *GetNameSafe(DamageCauser));

	if (bDestroyOnDeath)
	{
		BeginDeathDissolve();
	}
	else if (bRegenOnDeath)
	{
		ResetHP();
	}
}

void USlimeHealthComponent::BeginDeathDissolve()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || bDissolving)
	{
		return;
	}

	bDissolving = true;
	DissolveElapsed = 0.f;

	if (ACharacter* Character = Cast<ACharacter>(Owner))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->GravityScale = 0.f;
			Movement->DisableMovement();
			Movement->Velocity = FVector::ZeroVector;
		}
		if (AController* Controller = Character->GetController())
		{
			Controller->StopMovement();
			if (AAIController* AI = Cast<AAIController>(Controller))
			{
				if (UBrainComponent* Brain = AI->GetBrainComponent())
				{
					Brain->StopLogic(TEXT("Death"));
				}
			}
			Character->DetachFromControllerPendingDestroy();
		}
	}

	if (UWidgetComponent* HealthBar = Owner->FindComponentByClass<UWidgetComponent>())
	{
		HealthBar->SetVisibility(false);
		HealthBar->SetHiddenInGame(true);
	}

	// Keep collision so the corpse does not fall through; freeze in place.
	Owner->SetActorEnableCollision(true);
	ApplyDeathVisual(1.f);

	World->GetTimerManager().SetTimer(
		DissolveTimerHandle,
		this,
		&USlimeHealthComponent::TickDeathDissolve,
		0.05f,
		true);
}

void USlimeHealthComponent::TickDeathDissolve()
{
	if (!bDissolving)
	{
		return;
	}

	DissolveElapsed += 0.05f;
	const float Alpha = 1.f - FMath::Clamp(DissolveElapsed / FMath::Max(DissolveDuration, 0.05f), 0.f, 1.f);
	ApplyDeathVisual(Alpha);

	if (DissolveElapsed >= DissolveDuration)
	{
		FinishDeathDissolve();
	}
}

void USlimeHealthComponent::ApplyDeathVisual(float Alpha) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (USlimeElementComponent* Element = Owner->FindComponentByClass<USlimeElementComponent>())
	{
		Element->SetOpacityScale(Alpha);
	}

	// Surface verts are world-space; hide meshes when nearly gone instead of scaling.
	const bool bShow = Alpha > 0.08f;
	if (USlimeBodyComponent* Body = Owner->FindComponentByClass<USlimeBodyComponent>())
	{
		if (UProceduralMeshComponent* Surface = Body->GetSurfaceMesh())
		{
			Surface->SetVisibility(bShow);
		}
		if (UProceduralMeshComponent* Shadow = Body->GetShadowMesh())
		{
			Shadow->SetVisibility(bShow);
		}
	}
	if (!bShow)
	{
		Owner->SetActorHiddenInGame(true);
	}
}

void USlimeHealthComponent::FinishDeathDissolve()
{
	bDissolving = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DissolveTimerHandle);
	}
	if (AActor* Owner = GetOwner())
	{
		Owner->Destroy();
	}
}
