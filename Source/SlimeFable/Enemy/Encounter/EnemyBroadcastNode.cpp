#include "EnemyBroadcastNode.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

AEnemyBroadcastNode::AEnemyBroadcastNode()
{
	PrimaryActorTick.bCanEverTick = true;
	NodeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NodeMesh"));
	NodeMesh->SetMobility(EComponentMobility::Movable);
	RootComponent = NodeMesh;
}

void AEnemyBroadcastNode::BeginPlay()
{
	Super::BeginPlay();
	PulseRemaining = FMath::Max(PulseInterval, 0.25f);
}

void AEnemyBroadcastNode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsNodeActive())
	{
		return;
	}
	PulseRemaining -= DeltaSeconds;
	if (PulseRemaining <= 0.f)
	{
		PulseRemaining = FMath::Max(PulseInterval, 0.25f);
		Pulse();
	}
}

void AEnemyBroadcastNode::Pulse()
{
	OnPulse.Broadcast();
	if (UNiagaraSystem* System = PulseNiagara.LoadSynchronous())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, GetActorLocation());
	}
	if (PulseDamage <= 0.f || !GetWorld())
	{
		return;
	}
	for (TObjectIterator<APlayerController> It; It; ++It)
	{
		APlayerController* PC = *It;
		if (!PC || PC->GetWorld() != GetWorld() || !PC->GetPawn())
		{
			continue;
		}
		if (FVector::DistSquared(PC->GetPawn()->GetActorLocation(), GetActorLocation()) <= FMath::Square(PulseRadius))
		{
			PC->GetPawn()->TakeDamage(PulseDamage, FDamageEvent(), PC, this);
		}
	}
}

float AEnemyBroadcastNode::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!bActive || bDestroyed || NodeHealth <= 0.f || DamageAmount <= 0.f)
	{
		return 0.f;
	}
	NodeHealth = FMath::Max(NodeHealth - DamageAmount, 0.f);
	if (NodeHealth <= 0.f)
	{
		bDestroyed = true;
		bActive = false;
		SetActorTickEnabled(false);
	}
	return DamageAmount;
}

void AEnemyBroadcastNode::ActivateNode()
{
	if (!bDestroyed)
	{
		bActive = true;
		SetActorTickEnabled(true);
	}
}

void AEnemyBroadcastNode::DeactivateNode()
{
	bActive = false;
}
