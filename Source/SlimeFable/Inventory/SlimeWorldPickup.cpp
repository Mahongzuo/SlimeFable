// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeWorldPickup.h"

#include "SlimeItemDefinition.h"
#include "SlimeInventorySubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "SlimeFable.h"

ASlimeWorldPickup::ASlimeWorldPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Mesh->SetGenerateOverlapEvents(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
		Mesh->SetRelativeScale3D(FVector(0.45f));
	}

	HighlightSphere = CreateDefaultSubobject<USphereComponent>(TEXT("HighlightSphere"));
	HighlightSphere->SetupAttachment(RootComponent);
	HighlightSphere->InitSphereRadius(HighlightRadius);
	HighlightSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	OutlineOverlayMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/Materials/M_PickupOutline.M_PickupOutline")));
}

void ASlimeWorldPickup::BeginPlay()
{
	Super::BeginPlay();
	if (HighlightSphere)
	{
		HighlightSphere->SetSphereRadius(HighlightRadius);
	}
	if (Mesh)
	{
		RestRelativeScale = Mesh->GetRelativeScale3D();
	}
}

FName ASlimeWorldPickup::GetItemId() const
{
	if (!ItemIdOverride.IsNone())
	{
		return ItemIdOverride;
	}
	return ItemDefinition ? ItemDefinition->ItemId : NAME_None;
}

FVector ASlimeWorldPickup::GetPromptWorldLocation() const
{
	if (Mesh)
	{
		const FBoxSphereBounds& B = Mesh->Bounds;
		return FVector(B.Origin.X, B.Origin.Y, B.Origin.Z + B.BoxExtent.Z + PromptHeightOffset);
	}
	return GetActorLocation() + FVector(0.f, 0.f, 80.f + PromptHeightOffset);
}

FText ASlimeWorldPickup::GetInteractPromptVerb() const
{
	return FText::FromString(TEXT("拾取"));
}

UMaterialInterface* ASlimeWorldPickup::ResolveOutlineMaterial()
{
	if (CachedOutlineMaterial)
	{
		return CachedOutlineMaterial;
	}

	CachedOutlineMaterial = OutlineOverlayMaterial.LoadSynchronous();
	if (!CachedOutlineMaterial)
	{
		// Known-working mesh overlay from the project as a last-resort rim look.
		CachedOutlineMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Game/NiagaraExamples/Materials/MI_Mesh_Overlay_TeslaCoil_Player.MI_Mesh_Overlay_TeslaCoil_Player"));
	}
	return CachedOutlineMaterial;
}

void ASlimeWorldPickup::SetHighlight(bool bEnabled)
{
	if (bHighlighted == bEnabled || !Mesh)
	{
		return;
	}
	bHighlighted = bEnabled;
	if (bEnabled)
	{
		// Edge-only Fresnel overlay — do not tint / emissive-boost base materials
		// (that washed out the whole mesh and hid the original look).
		if (UMaterialInterface* Overlay = ResolveOutlineMaterial())
		{
			Mesh->SetOverlayMaterial(Overlay);
		}
	}
	else
	{
		Mesh->SetOverlayMaterial(nullptr);
		Mesh->SetRenderCustomDepth(false);
		Mesh->SetRelativeScale3D(RestRelativeScale);
	}
}

void ASlimeWorldPickup::RefreshHighlightFromNearbyPlayers()
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

void ASlimeWorldPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshHighlightFromNearbyPlayers();
}

bool ASlimeWorldPickup::TryPickup(APawn* Picker)
{
	if (!Picker)
	{
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
	// May register a per-instance definition and redirect ItemIdOverride,
	// so it must run before the item id is resolved.
	PrepareDefinition(*Inv);

	const FName Id = GetItemId();
	if (Id.IsNone())
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("Pickup %s has no ItemId"), *GetName());
		return false;
	}

	const float DistSq = FVector::DistSquared(Picker->GetActorLocation(), GetActorLocation());
	if (DistSq > FMath::Square(HighlightRadius))
	{
		return false;
	}

	if (Inv->AddItem(Id, 1) <= 0)
	{
		return false;
	}

	OnPickedUp(Picker);
	Destroy();
	return true;
}
