// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeTestPickups.h"
#include "SlimeInventorySubsystem.h"
#include "SlimeItemDefinition.h"
#include "SlimePlacedActor.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "SlimeFable.h"

ASlimePickupConsumable::ASlimePickupConsumable()
{
	ItemIdOverride = TEXT("HealJelly");
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (MeshAsset.Succeeded() && Mesh)
	{
		Mesh->SetStaticMesh(MeshAsset.Object);
		Mesh->SetWorldScale3D(FVector(0.4f));
	}
}

ASlimePickupPlaceable::ASlimePickupPlaceable()
{
	ItemIdOverride = TEXT("FlatStone");
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (MeshAsset.Succeeded() && Mesh)
	{
		Mesh->SetStaticMesh(MeshAsset.Object);
		Mesh->SetWorldScale3D(FVector(0.55f, 0.55f, 0.28f));
	}
}

void ASlimePickupPlaceable::PrepareDefinition(USlimeInventorySubsystem& Inventory)
{
	const FName Id = GetItemId();
	if (Id.IsNone())
	{
		return;
	}

	USlimePlaceableDefinition* Def = Cast<USlimePlaceableDefinition>(Inventory.FindDefinition(Id));
	if (!Def)
	{
		Def = NewObject<USlimePlaceableDefinition>(&Inventory, Id);
		Def->ItemId = Id;
		Def->DisplayName = FText::FromName(Id);
		Def->PlacedActorClass = TSoftClassPtr<ASlimePlacedActor>(
			FSoftObjectPath(TEXT("/Game/Blueprints/Items/BP_PlacedProp.BP_PlacedProp_C")));
		Inventory.RegisterItemDefinition(Def);
	}

	if (Mesh)
	{
		if (UStaticMesh* StaticMesh = Mesh->GetStaticMesh())
		{
			Def->PreviewMesh = StaticMesh;
		}
		Def->PlacedMeshScale = Mesh->GetRelativeScale3D();
	}
}

ASlimePickupSouvenir::ASlimePickupSouvenir()
{
	ItemIdOverride = TEXT("OldPostcard");
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (MeshAsset.Succeeded() && Mesh)
	{
		Mesh->SetStaticMesh(MeshAsset.Object);
		Mesh->SetWorldScale3D(FVector(0.35f, 0.35f, 0.08f));
	}
}

void ASlimePickupSouvenir::PrepareDefinition(USlimeInventorySubsystem& Inventory)
{
	if (SouvenirImage.IsNull())
	{
		return;
	}

	const FString AssetName = SouvenirImage.GetAssetName();
	const FName DerivedId(*FString::Printf(TEXT("Souvenir_%s"), *AssetName));
	ItemIdOverride = DerivedId;

	if (USlimeItemDefinition* Existing = Inventory.FindDefinition(DerivedId))
	{
		if (USlimeSouvenirDefinition* ExistingSouvenir = Cast<USlimeSouvenirDefinition>(Existing))
		{
			ExistingSouvenir->StoryImage = SouvenirImage;
			ExistingSouvenir->Icon = SouvenirImage;
			ExistingSouvenir->StoryVideo = SouvenirVideo;
			if (!SouvenirDisplayName.IsEmpty())
			{
				ExistingSouvenir->DisplayName = SouvenirDisplayName;
			}
			if (!SouvenirStoryText.IsEmpty())
			{
				ExistingSouvenir->StoryText = SouvenirStoryText;
			}
		}
		return;
	}

	USlimeSouvenirDefinition* Def = NewObject<USlimeSouvenirDefinition>(&Inventory, DerivedId);
	Def->ItemId = DerivedId;
	Def->DisplayName = SouvenirDisplayName.IsEmpty()
		? FText::FromString(AssetName)
		: SouvenirDisplayName;
	Def->Description = FText::FromString(TEXT("一段关于史莱姆旅途的回忆"));
	Def->StoryText = SouvenirStoryText.IsEmpty()
		? FText::FromString(TEXT("这张画面背后，藏着一段尚未讲完的故事。"))
		: SouvenirStoryText;
	Def->StoryImage = SouvenirImage;
	Def->Icon = SouvenirImage;
	Def->StoryVideo = SouvenirVideo;
	Inventory.RegisterItemDefinition(Def);

	if (UTexture2D* Tex = SouvenirImage.LoadSynchronous())
	{
		if (Tex->VirtualTextureStreaming)
		{
			UE_LOG(LogSlimeFable, Warning,
				TEXT("Souvenir image %s has VirtualTextureStreaming enabled; it will render as a white square in UMG. Disable VT on the texture."),
				*Tex->GetPathName());
		}
	}
}
