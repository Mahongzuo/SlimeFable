#include "Quest/QuestInteractActor.h"
#include "Quest/QuestObjectiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

AQuestInteractActor::AQuestInteractActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Mesh->SetGenerateOverlapEvents(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
		Mesh->SetWorldScale3D(FVector(0.5f));
	}

	Objective = CreateDefaultSubobject<UQuestObjectiveComponent>(TEXT("Objective"));
}

void AQuestInteractActor::Configure(FName ChapterId, FName QuestId, FName BranchId, const FText& PromptVerb, const FLinearColor& Color, float Scale)
{
	if (Objective)
	{
		Objective->ChapterId = ChapterId;
		Objective->QuestId = QuestId;
		Objective->BranchId = BranchId;
		Objective->PromptVerb = PromptVerb;
	}
	if (Mesh)
	{
		Mesh->SetWorldScale3D(FVector(Scale));
		if (UMaterial* Base = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this))
			{
				MID->SetVectorParameterValue(TEXT("Color"), Color);
				Mesh->SetMaterial(0, MID);
			}
		}
	}
}

bool AQuestInteractActor::CanBeFocused() const
{
	return Objective && !Objective->IsConsumed();
}

FVector AQuestInteractActor::GetPromptWorldLocation() const
{
	return Objective ? Objective->GetPromptWorldLocation() : GetActorLocation() + FVector(0.f, 0.f, 80.f);
}

FText AQuestInteractActor::GetInteractPromptVerb() const
{
	return Objective ? Objective->GetPromptVerb() : FText::FromString(TEXT("交谈"));
}

bool AQuestInteractActor::TryInteract(APawn* Interactor)
{
	if (!Interactor || !Objective)
	{
		return false;
	}
	if (!Objective->TryContribute())
	{
		return false;
	}
	ApplyConsumedVisual();
	return true;
}

void AQuestInteractActor::ApplyConsumedVisual()
{
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}
