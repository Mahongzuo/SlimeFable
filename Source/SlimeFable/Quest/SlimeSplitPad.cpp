#include "Quest/SlimeSplitPad.h"
#include "Slime/SlimeBodyComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

ASlimeSplitPad::ASlimeSplitPad()
{
	PrimaryActorTick.bCanEverTick = true;

	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	SetRootComponent(Volume);
	Volume->SetBoxExtent(FVector(80.f, 80.f, 30.f));
	Volume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Volume->SetGenerateOverlapEvents(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Volume);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
		Mesh->SetRelativeScale3D(FVector(1.6f, 1.6f, 0.12f));
	}
}

bool ASlimeSplitPad::PointInside(const FVector& Point) const
{
	if (!Volume)
	{
		return false;
	}
	const FVector Local = Volume->GetComponentTransform().InverseTransformPosition(Point);
	const FVector Extent = Volume->GetScaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X
		&& FMath::Abs(Local.Y) <= Extent.Y
		&& FMath::Abs(Local.Z) <= Extent.Z + 40.f;
}

void ASlimeSplitPad::RefreshPressed()
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const USlimeBodyComponent* Body = Player ? Player->FindComponentByClass<USlimeBodyComponent>() : nullptr;

	bool bBodyOn = false;
	bool bFragOn = false;
	if (Player)
	{
		const FVector BodyCenter = Body ? Body->GetBlobCenter() : Player->GetActorLocation();
		bBodyOn = PointInside(BodyCenter);
		FVector FragCenter;
		if (Body && Body->GetFragmentCenter(FragCenter))
		{
			bFragOn = PointInside(FragCenter);
		}
	}

	if (bLatchFragment && bFragOn)
	{
		bLatched = true;
	}
	if (bLatchFragment && Body && !Body->HasFragments())
	{
		bLatched = false;
	}

	bPressed = bBodyOn || bFragOn || (bLatchFragment && bLatched);
	ApplyDoor();
}

void ASlimeSplitPad::ApplyDoor()
{
	const bool bPartnerOk = !PartnerPad || PartnerPad->IsPressed();
	const bool bShouldOpen = bPressed && bPartnerOk;
	if (bShouldOpen == bDoorOpen)
	{
		return;
	}
	bDoorOpen = bShouldOpen;
	if (DoorActor)
	{
		DoorActor->SetActorHiddenInGame(bDoorOpen);
		DoorActor->SetActorEnableCollision(!bDoorOpen);
	}
}

void ASlimeSplitPad::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshPressed();
}
