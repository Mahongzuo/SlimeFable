// Copyright Epic Games, Inc. All Rights Reserved.

#include "Farm/SlimeFarmField.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Farm/SlimeFarmPlot.h"
#include "Farm/SlimeSeedCrate.h"
#include "Hub/SlimeMuseumDayGate.h"
#include "Materials/MaterialInterface.h"
#include "SlimeFable.h"

namespace
{
	template <typename ActorType>
	bool WorldHasActor(const UWorld* World)
	{
		if (!World)
		{
			return false;
		}
		for (TActorIterator<ActorType> It(World); It; ++It)
		{
			return true;
		}
		return false;
	}
}

ASlimeFarmField::ASlimeFarmField()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);

	PreviewSoil = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PreviewSoil"));
	PreviewSoil->SetupAttachment(Root);
	PreviewSoil->SetMobility(EComponentMobility::Movable);
	PreviewSoil->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	PreviewSoil->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewSoil->SetGenerateOverlapEvents(false);
	PreviewSoil->SetCastShadow(false);
	PreviewSoil->SetHiddenInGame(true);
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		PreviewSoil->SetMaterial(0, Base);
	}
}

void ASlimeFarmField::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildPreview();
}

void ASlimeFarmField::RebuildPreview()
{
	if (!PreviewSoil)
	{
		return;
	}

	PreviewSoil->ClearInstances();
	const int32 RowCount = FMath::Clamp(Rows, 1, 8);
	const int32 ColCount = FMath::Clamp(Cols, 1, 12);
	for (int32 Row = 0; Row < RowCount; ++Row)
	{
		for (int32 Col = 0; Col < ColCount; ++Col)
		{
			const FTransform InstanceTransform(
				FRotator::ZeroRotator,
				PlotLocalOffset(Row, Col) + FVector(0.f, 0.f, 6.f),
				FVector(2.2f, 2.2f, 0.12f));
			PreviewSoil->AddInstance(InstanceTransform);
		}
	}
}

FVector ASlimeFarmField::PlotLocalOffset(int32 Row, int32 Col) const
{
	const int32 RowCount = FMath::Clamp(Rows, 1, 8);
	const int32 ColCount = FMath::Clamp(Cols, 1, 12);
	const float SpanX = (RowCount - 1) * Spacing;
	const float SpanY = (ColCount - 1) * Spacing;
	return FVector(Row * Spacing - SpanX * 0.5f, Col * Spacing - SpanY * 0.5f, 0.f);
}

FVector ASlimeFarmField::ResolveWorldLocation(const FVector& LocalOffset) const
{
	FVector WorldLocation = GetActorTransform().TransformPosition(LocalOffset);
	UWorld* World = GetWorld();
	if (!bSnapToGround || !World)
	{
		return WorldLocation;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FarmFieldSnap), false, this);
	const FVector Start = WorldLocation + FVector(0.f, 0.f, 400.f);
	const FVector End = WorldLocation - FVector(0.f, 0.f, 2000.f);
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		WorldLocation = Hit.ImpactPoint;
	}
	return WorldLocation;
}

void ASlimeFarmField::BeginPlay()
{
	Super::BeginPlay();
	if (bDidSpawn || !GetWorld())
	{
		return;
	}
	bDidSpawn = true;

	const int32 RowCount = FMath::Clamp(Rows, 1, 8);
	const int32 ColCount = FMath::Clamp(Cols, 1, 12);
	const FString Prefix = PlotIdPrefix.IsEmpty() ? TEXT("MuseumPlot") : PlotIdPrefix;
	const FRotator Rotation = GetActorRotation();

	for (int32 Row = 0; Row < RowCount; ++Row)
	{
		for (int32 Col = 0; Col < ColCount; ++Col)
		{
			const int32 Index = Row * ColCount + Col;
			const FVector WorldLocation = ResolveWorldLocation(PlotLocalOffset(Row, Col));
			const FTransform Transform(Rotation, WorldLocation);

			ASlimeFarmPlot* Plot = GetWorld()->SpawnActorDeferred<ASlimeFarmPlot>(
				ASlimeFarmPlot::StaticClass(),
				Transform,
				this,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Plot)
			{
				continue;
			}
			Plot->PlotId = FName(*FString::Printf(TEXT("%s_%02d"), *Prefix, Index));
			Plot->FinishSpawning(Transform);
			SpawnedActors.Add(Plot);
		}
	}

	if (bSpawnSeedCrate && !WorldHasActor<ASlimeSeedCrate>(GetWorld()))
	{
		const FVector WorldLocation = ResolveWorldLocation(SeedCrateOffset);
		if (ASlimeSeedCrate* Crate = GetWorld()->SpawnActor<ASlimeSeedCrate>(
			ASlimeSeedCrate::StaticClass(), WorldLocation, Rotation))
		{
			SpawnedActors.Add(Crate);
		}
	}

	if (bSpawnDayGate && !WorldHasActor<ASlimeMuseumDayGate>(GetWorld()))
	{
		const FVector WorldLocation = ResolveWorldLocation(DayGateOffset);
		if (ASlimeMuseumDayGate* Gate = GetWorld()->SpawnActor<ASlimeMuseumDayGate>(
			ASlimeMuseumDayGate::StaticClass(), WorldLocation, Rotation))
		{
			SpawnedActors.Add(Gate);
		}
	}

	UE_LOG(LogSlimeFable, Log, TEXT("[MuseumHub] farm field spawned %d actors (plots use prefix %s)"),
		SpawnedActors.Num(), *Prefix);
}

void ASlimeFarmField::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		for (AActor* Actor : SpawnedActors)
		{
			if (IsValid(Actor))
			{
				Actor->Destroy();
			}
		}
	}
	SpawnedActors.Reset();
	Super::EndPlay(EndPlayReason);
}
