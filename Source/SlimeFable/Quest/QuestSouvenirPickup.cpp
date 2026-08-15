#include "Quest/QuestSouvenirPickup.h"
#include "Quest/QuestObjectiveComponent.h"
#include "Inventory/SlimeInventorySubsystem.h"
#include "Inventory/SlimeItemDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"

AQuestSouvenirPickup::AQuestSouvenirPickup()
{
	Objective = CreateDefaultSubobject<UQuestObjectiveComponent>(TEXT("Objective"));
	ItemIdOverride = NAME_None;
}

void AQuestSouvenirPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyWorldMesh();
}

void AQuestSouvenirPickup::BeginPlay()
{
	Super::BeginPlay();
	ApplyWorldMesh();
	if (USlimeSouvenirDefinition* Def = SouvenirDefinition.LoadSynchronous())
	{
		ItemDefinition = Def;
		ItemIdOverride = Def->ItemId;
		if (SouvenirImage.IsNull())
		{
			SouvenirImage = Def->StoryImage;
		}
		if (SouvenirMesh.IsNull())
		{
			SouvenirMesh = Def->StoryMesh;
		}
		if (SouvenirVideo.IsNull())
		{
			SouvenirVideo = Def->StoryVideo;
		}
		if (SouvenirDisplayName.IsEmpty())
		{
			SouvenirDisplayName = Def->DisplayName;
		}
		if (SouvenirStoryText.IsEmpty())
		{
			SouvenirStoryText = Def->StoryText;
		}
		ApplyWorldMesh();
	}
}

void AQuestSouvenirPickup::ApplyWorldMesh()
{
	if (!Mesh)
	{
		return;
	}
	if (UStaticMesh* Custom = SouvenirMesh.LoadSynchronous())
	{
		Mesh->SetStaticMesh(Custom);
		Mesh->SetWorldScale3D(FVector(1.f));
	}
}

void AQuestSouvenirPickup::PrepareDefinition(USlimeInventorySubsystem& Inventory)
{
	if (USlimeSouvenirDefinition* SoftDef = SouvenirDefinition.LoadSynchronous())
	{
		Inventory.RegisterItemDefinition(SoftDef);
		ItemDefinition = SoftDef;
		ItemIdOverride = SoftDef->ItemId;
		return;
	}

	if (SouvenirImage.IsNull() && SouvenirMesh.IsNull())
	{
		ASlimePickupSouvenir::PrepareDefinition(Inventory);
		return;
	}

	const FString AssetName = !SouvenirImage.IsNull()
		? SouvenirImage.GetAssetName()
		: SouvenirMesh.GetAssetName();
	const FName DerivedId = ItemIdOverride.IsNone()
		? FName(*FString::Printf(TEXT("Souvenir_%s"), *AssetName))
		: ItemIdOverride;
	ItemIdOverride = DerivedId;

	USlimeSouvenirDefinition* Def = Cast<USlimeSouvenirDefinition>(Inventory.FindDefinition(DerivedId));
	if (!Def)
	{
		Def = NewObject<USlimeSouvenirDefinition>(&Inventory, DerivedId);
		Def->ItemId = DerivedId;
		Inventory.RegisterItemDefinition(Def);
	}

	Def->DisplayName = SouvenirDisplayName.IsEmpty()
		? FText::FromString(AssetName)
		: SouvenirDisplayName;
	Def->Description = FText::FromString(TEXT("一段关于史莱姆旅途的回忆"));
	Def->StoryText = SouvenirStoryText.IsEmpty()
		? FText::FromString(TEXT("这件纪念品记下了一段尚未讲完的故事。"))
		: SouvenirStoryText;
	Def->StoryImage = SouvenirImage;
	Def->Icon = SouvenirImage;
	Def->StoryVideo = SouvenirVideo;
	Def->StoryMesh = SouvenirMesh;
}

void AQuestSouvenirPickup::OnPickedUp(APawn* Picker)
{
	(void)Picker;
	if (Objective)
	{
		Objective->TryContribute();
	}
}
