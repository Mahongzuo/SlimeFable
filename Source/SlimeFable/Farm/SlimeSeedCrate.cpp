// Copyright Epic Games, Inc. All Rights Reserved.

#include "Farm/SlimeSeedCrate.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Farm/SlimeFarmSubsystem.h"
#include "Inventory/SlimeInventorySubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UI/SlimeFloatingTextWidget.h"

ASlimeSeedCrate::ASlimeSeedCrate()
{
	CrateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Crate"));
	SetRootComponent(CrateMesh);
	CrateMesh->SetMobility(EComponentMobility::Movable);
	CrateMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	CrateMesh->SetRelativeScale3D(FVector(0.8f, 0.55f, 0.45f));
	CrateMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CrateMesh->SetCollisionObjectType(ECC_WorldDynamic);
	CrateMesh->SetCollisionResponseToAllChannels(ECR_Block);
	CrateMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		CrateMesh->SetMaterial(0, Base);
	}
}

void ASlimeSeedCrate::BeginPlay()
{
	Super::BeginPlay();
	if (CrateMesh)
	{
		if (UMaterialInstanceDynamic* Mid = CrateMesh->CreateDynamicMaterialInstance(0))
		{
			Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.55f, 0.36f, 0.14f));
		}
	}
}

FText ASlimeSeedCrate::GetInteractPromptVerb() const
{
	const UGameInstance* GI = GetGameInstance();
	const USlimeFarmSubsystem* Farm = GI ? GI->GetSubsystem<USlimeFarmSubsystem>() : nullptr;
	if (Farm && Farm->GetLastSeedCrateDayKey() == USlimeFarmSubsystem::MakeTodayKey())
	{
		return FText::FromString(TEXT("明天再来"));
	}
	return FText::FromString(TEXT("领取种子"));
}

bool ASlimeSeedCrate::TryInteract(APawn* Interactor)
{
	if (!Interactor)
	{
		return false;
	}
	UGameInstance* GI = GetGameInstance();
	USlimeFarmSubsystem* Farm = GI ? GI->GetSubsystem<USlimeFarmSubsystem>() : nullptr;
	USlimeInventorySubsystem* Inv = GI ? GI->GetSubsystem<USlimeInventorySubsystem>() : nullptr;
	if (!Farm || !Inv)
	{
		return false;
	}

	const int32 Today = USlimeFarmSubsystem::MakeTodayKey();
	if (Farm->GetLastSeedCrateDayKey() == Today)
	{
		return false;
	}

	const TCHAR* CropIds[] = {TEXT("Tomato"), TEXT("Chili"), TEXT("Mint"), TEXT("Sunflower"), TEXT("Nightbloom"), TEXT("Dewleaf")};
	int32 Granted = 0;
	for (const TCHAR* CropId : CropIds)
	{
		if (USlimeSeedDefinition* Seed = Farm->FindSeedForCrop(FName(CropId)))
		{
			Inv->AddItem(Seed->ItemId, SeedsPerType);
			++Granted;
		}
	}
	if (Granted == 0)
	{
		return false;
	}

	Farm->SetLastSeedCrateDayKey(Today);
	USlimeFloatingTextWidget::Spawn(
		this,
		GetActorLocation() + FVector(0, 0, 80),
		FText::FromString(TEXT("领到种子了")),
		FLinearColor(0.85f, 0.7f, 0.25f),
		true);
	return true;
}
