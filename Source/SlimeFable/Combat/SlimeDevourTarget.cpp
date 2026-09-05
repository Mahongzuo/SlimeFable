// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeDevourTarget.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

USkeletalMeshComponent* SlimeDevourUtil::GetPrimaryMesh(AActor* Actor)
{
	if (ISlimeDevourTarget* Target = As(Actor))
	{
		if (USkeletalMeshComponent* Mesh = Target->GetPrimarySkeletalMesh())
		{
			return Mesh;
		}
	}
	if (ACharacter* Character = Cast<ACharacter>(Actor))
	{
		return Character->GetMesh();
	}
	return Actor ? Actor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

USkeletalMeshComponent* SlimeDevourUtil::GetPreviewMesh(AActor* Actor)
{
	if (ISlimeDevourTarget* Target = As(Actor))
	{
		if (USkeletalMeshComponent* Mesh = Target->GetDevourPreviewMesh())
		{
			return Mesh;
		}
	}
	return GetPrimaryMesh(Actor);
}

UCapsuleComponent* SlimeDevourUtil::GetCapsule(AActor* Actor)
{
	if (ISlimeDevourTarget* Target = As(Actor))
	{
		if (UCapsuleComponent* Capsule = Target->GetDevourCapsule())
		{
			return Capsule;
		}
	}
	if (ACharacter* Character = Cast<ACharacter>(Actor))
	{
		return Character->GetCapsuleComponent();
	}
	return Actor ? Actor->FindComponentByClass<UCapsuleComponent>() : nullptr;
}
