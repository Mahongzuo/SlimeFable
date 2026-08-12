// Copyright Epic Games, Inc. All Rights Reserved.


#include "SideScrollingSoftPlatform.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "SideScrollingCharacter.h"

ASideScrollingSoftPlatform::ASideScrollingSoftPlatform()
{
 	PrimaryActorTick.bCanEverTick = true;

	// create the root component
	RootComponent = Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the mesh
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);

	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);

	// create the collision check box
	CollisionCheckBox = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision Check Box"));
	CollisionCheckBox->SetupAttachment(Mesh);

	CollisionCheckBox->SetRelativeLocation(FVector(0.0f, 0.0f, -40.0f));
	CollisionCheckBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionCheckBox->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionCheckBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionCheckBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// subscribe to the overlap events
	CollisionCheckBox->OnComponentBeginOverlap.AddDynamic(this, &ASideScrollingSoftPlatform::OnSoftCollisionOverlap);
}

void ASideScrollingSoftPlatform::OnSoftCollisionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// have we overlapped a character?
	if (ASideScrollingCharacter* Char = Cast<ASideScrollingCharacter>(OtherActor))
	{
		// disable the soft collision channel
		Char->SetSoftCollision(true);
	}
}

void ASideScrollingSoftPlatform::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	// have we overlapped a character?
	if (ASideScrollingCharacter* Char = Cast<ASideScrollingCharacter>(OtherActor))
	{
		// enable the soft collision channel
		Char->SetSoftCollision(false);
	}
}
