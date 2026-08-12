// Copyright Epic Games, Inc. All Rights Reserved.


#include "CombatDummy.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

ACombatDummy::ACombatDummy()
{
 	PrimaryActorTick.bCanEverTick = true;

	// create the root
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// create the base plate
	BasePlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base Plate"));
	BasePlate->SetupAttachment(RootComponent);

	// create the dummy
	Dummy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Dummy"));
	Dummy->SetupAttachment(RootComponent);

	Dummy->SetSimulatePhysics(true);

	// create the physics constraint
	PhysicsConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("Physics Constraint"));
	PhysicsConstraint->SetupAttachment(RootComponent);

	PhysicsConstraint->SetConstrainedComponents(BasePlate, NAME_None, Dummy, NAME_None);
}

void ACombatDummy::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	// apply impulse to the dummy
	Dummy->AddImpulseAtLocation(DamageImpulse, DamageLocation);

	// call the BP handler
	BP_OnDummyDamaged(DamageLocation, DamageImpulse.GetSafeNormal());
}

void ACombatDummy::HandleDeath()
{
	// unused
}

void ACombatDummy::ApplyHealing(float Healing, AActor* Healer)
{
	// unused
}

void ACombatDummy::NotifyDanger(const FVector& DangerLocation, AActor* DangerSource)
{
	// unused
}
