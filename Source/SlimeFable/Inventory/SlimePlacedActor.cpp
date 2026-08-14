// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimePlacedActor.h"

#include "SlimeItemDefinition.h"
#include "SlimeInventorySubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "SlimeFable.h"

ASlimePlacedActor::ASlimePlacedActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
		Mesh->SetWorldScale3D(FVector(0.5f, 0.5f, 0.35f));
	}

	OutlineOverlayMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/Materials/M_PickupOutline.M_PickupOutline")));
}

void ASlimePlacedActor::BeginPlay()
{
	Super::BeginPlay();
	if (Mesh)
	{
		RestRelativeScale = Mesh->GetRelativeScale3D();
	}
}

void ASlimePlacedActor::ConfigureFromItem(FName InItemId, USlimeItemDefinition* InDefinition)
{
	SourceItemId = InItemId;
	ItemDefinition = InDefinition;

	if (USlimePlaceableDefinition* Placeable = Cast<USlimePlaceableDefinition>(InDefinition))
	{
		if (Mesh)
		{
			if (UStaticMesh* Preview = Placeable->PreviewMesh.LoadSynchronous())
			{
				Mesh->SetStaticMesh(Preview);
			}
			Mesh->SetRelativeScale3D(Placeable->PlacedMeshScale);
			RestRelativeScale = Mesh->GetRelativeScale3D();
		}
	}
}

FVector ASlimePlacedActor::GetPromptWorldLocation() const
{
	if (Mesh)
	{
		const FBoxSphereBounds& B = Mesh->Bounds;
		return FVector(B.Origin.X, B.Origin.Y, B.Origin.Z + B.BoxExtent.Z + PromptHeightOffset);
	}
	return GetActorLocation() + FVector(0.f, 0.f, 80.f + PromptHeightOffset);
}

UMaterialInterface* ASlimePlacedActor::ResolveOutlineMaterial()
{
	if (CachedOutlineMaterial)
	{
		return CachedOutlineMaterial;
	}
	CachedOutlineMaterial = OutlineOverlayMaterial.LoadSynchronous();
	if (!CachedOutlineMaterial)
	{
		CachedOutlineMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Game/NiagaraExamples/Materials/MI_Mesh_Overlay_TeslaCoil_Player.MI_Mesh_Overlay_TeslaCoil_Player"));
	}
	return CachedOutlineMaterial;
}

void ASlimePlacedActor::SetHighlight(bool bEnabled)
{
	if (bHighlighted == bEnabled || !Mesh)
	{
		return;
	}
	bHighlighted = bEnabled;
	if (bEnabled)
	{
		// Edge-only Fresnel overlay — do not tint / emissive-boost base materials.
		if (UMaterialInterface* Overlay = ResolveOutlineMaterial())
		{
			Mesh->SetOverlayMaterial(Overlay);
		}
	}
	else
	{
		Mesh->SetOverlayMaterial(nullptr);
		Mesh->SetRelativeScale3D(RestRelativeScale);
	}
}

void ASlimePlacedActor::RefreshHighlightFromNearbyPlayers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bool bAnyNear = false;
	const FVector Loc = GetActorLocation();
	const float RadiusSq = FMath::Square(HighlightRadius);
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}
		if (FVector::DistSquared(Pawn->GetActorLocation(), Loc) <= RadiusSq)
		{
			bAnyNear = true;
			break;
		}
	}
	SetHighlight(bAnyNear);
}

void ASlimePlacedActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshHighlightFromNearbyPlayers();
}

bool ASlimePlacedActor::TryPickup(APawn* Picker)
{
	if (!Picker || SourceItemId.IsNone())
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("PlacedActor %s has no SourceItemId"), *GetName());
		return false;
	}

	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	USlimeInventorySubsystem* Inv = GI ? GI->GetSubsystem<USlimeInventorySubsystem>() : nullptr;
	if (!Inv)
	{
		return false;
	}

	if (ItemDefinition)
	{
		Inv->RegisterItemDefinition(ItemDefinition);
	}

	const float DistSq = FVector::DistSquared(Picker->GetActorLocation(), GetActorLocation());
	if (DistSq > FMath::Square(HighlightRadius))
	{
		return false;
	}

	if (Inv->AddItem(SourceItemId, 1) <= 0)
	{
		return false;
	}

	Destroy();
	return true;
}
