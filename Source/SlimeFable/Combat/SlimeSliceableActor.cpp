// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeSliceableActor.h"

#include "Components/StaticMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "ProceduralMeshComponent.h"
#include "SlimeSliceUtil.h"

ASlimeSliceableActor::ASlimeSliceableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SourceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SourceMesh"));
	SourceMesh->SetupAttachment(SceneRoot);
	SourceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
	ProceduralMesh->SetupAttachment(SceneRoot);
	ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ProceduralMesh->SetCollisionObjectType(ECC_WorldDynamic);
	ProceduralMesh->SetGenerateOverlapEvents(true);
	ProceduralMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ProceduralMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ProceduralMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void ASlimeSliceableActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EnsureProceduralFromSource();
}

void ASlimeSliceableActor::BeginPlay()
{
	Super::BeginPlay();
	EnsureProceduralFromSource();
	if (ProceduralMesh)
	{
		ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		ProceduralMesh->SetCollisionObjectType(ECC_WorldDynamic);
		ProceduralMesh->SetGenerateOverlapEvents(true);
		ProceduralMesh->SetSimulatePhysics(false);
	}
}

void ASlimeSliceableActor::EnsureProceduralFromSource()
{
	if (bMeshCopied || !SourceMesh || !ProceduralMesh || !SourceMesh->GetStaticMesh())
	{
		return;
	}

	UKismetProceduralMeshLibrary::CopyProceduralMeshFromStaticMeshComponent(
		SourceMesh, 0, ProceduralMesh, true);

	SourceMesh->SetVisibility(false);
	SourceMesh->SetHiddenInGame(true);
	SourceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	bMeshCopied = true;
}

void ASlimeSliceableActor::SliceAt_Implementation(
	FVector PlanePosition,
	FVector PlaneNormal,
	UProceduralMeshComponent* MeshToSlice)
{
	UProceduralMeshComponent* TargetMesh = MeshToSlice;
	if (!TargetMesh || TargetMesh->GetOwner() != this)
	{
		TargetMesh = ProceduralMesh;
	}
	USlimeSliceUtil::SliceProceduralMeshAt(
		TargetMesh,
		PlanePosition,
		PlaneNormal,
		CapMaterial,
		SliceSound,
		MinSliceExtent,
		SliceImpulse);
}
