#include "Quest/SlimeReactionHearth.h"
#include "Quest/QuestObjectiveComponent.h"
#include "Slime/SlimeElementComponent.h"
#include "Combat/SlimeCombatTypes.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

ASlimeReactionHearth::ASlimeReactionHearth()
{
	PrimaryActorTick.bCanEverTick = true;

	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	SetRootComponent(Volume);
	Volume->SetMobility(EComponentMobility::Movable);
	Volume->SetBoxExtent(FVector(90.f, 90.f, 70.f));
	Volume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Volume->SetGenerateOverlapEvents(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Volume);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
		Mesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.8f));
	}

	Objective = CreateDefaultSubobject<UQuestObjectiveComponent>(TEXT("Objective"));
}

bool ASlimeReactionHearth::MatchesReactionTable() const
{
	TArray<FSlimeReactionRow> Rows;
	SlimeCombat::FillDefaultReactions(Rows);
	for (const FSlimeReactionRow& Row : Rows)
	{
		const bool bDirect = Row.First == FirstElement && Row.Second == SecondElement;
		const bool bSwap = Row.First == SecondElement && Row.Second == FirstElement;
		if (bDirect || bSwap)
		{
			return true;
		}
	}
	return false;
}

void ASlimeReactionHearth::TryOpen()
{
	if (bOpen || !bLatchedFirst || !bLatchedSecond || !MatchesReactionTable())
	{
		return;
	}
	bOpen = true;
	if (DoorActor)
	{
		DoorActor->SetActorHiddenInGame(true);
		DoorActor->SetActorEnableCollision(false);
	}
	if (Mesh)
	{
		Mesh->SetVisibility(false);
	}
	if (Objective && !Objective->ChapterId.IsNone())
	{
		Objective->TryContribute();
	}
}

void ASlimeReactionHearth::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bOpen)
	{
		return;
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player || !Volume || !Volume->IsOverlappingActor(Player))
	{
		return;
	}

	const USlimeElementComponent* Element = Player->FindComponentByClass<USlimeElementComponent>();
	if (!Element)
	{
		return;
	}
	if (Element->CurrentElement == FirstElement)
	{
		bLatchedFirst = true;
	}
	if (Element->CurrentElement == SecondElement)
	{
		bLatchedSecond = true;
	}
	TryOpen();
}
