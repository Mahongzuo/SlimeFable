#include "Quest/QuestReachVolume.h"
#include "Quest/QuestObjectiveComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

AQuestReachVolume::AQuestReachVolume()
{
	PrimaryActorTick.bCanEverTick = true;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetMobility(EComponentMobility::Movable);
	Box->SetBoxExtent(FVector(180.f, 180.f, 120.f));
	Box->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Box->SetGenerateOverlapEvents(true);
	Box->SetHiddenInGame(false);
	Box->SetVisibility(true);
	Box->ShapeColor = FColor(235, 184, 82);

	Objective = CreateDefaultSubobject<UQuestObjectiveComponent>(TEXT("Objective"));
	Objective->PromptVerb = FText::FromString(TEXT("到达"));

	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	Marker->SetupAttachment(Box);
	Marker->SetMobility(EComponentMobility::Movable);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Marker->SetStaticMesh(CylinderMesh.Object);
		Marker->SetRelativeScale3D(FVector(3.2f, 3.2f, 0.12f));
	}
}

void AQuestReachVolume::Configure(FName ChapterId, FName QuestId, FName BranchId, const FVector& BoxExtent)
{
	if (Objective)
	{
		Objective->ChapterId = ChapterId;
		Objective->QuestId = QuestId;
		Objective->BranchId = BranchId;
		Objective->PromptHeightOffset = BoxExtent.Z + 40.f;
	}
	if (Box)
	{
		Box->SetBoxExtent(BoxExtent);
	}
	if (Marker)
	{
		if (UMaterial* Base = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this))
			{
				MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.92f, 0.72f, 0.32f, 0.55f));
				Marker->SetMaterial(0, MID);
			}
		}
	}
}

void AQuestReachVolume::TryCompleteFromPawn(AActor* OtherActor)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled() || !Objective || Objective->IsConsumed())
	{
		return;
	}
	if (Objective->TryContribute())
	{
		SetActorEnableCollision(false);
		SetActorHiddenInGame(true);
		SetActorTickEnabled(false);
	}
}

void AQuestReachVolume::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Box || !Objective || Objective->IsConsumed())
	{
		return;
	}

	TArray<AActor*> Overlapping;
	Box->GetOverlappingActors(Overlapping, APawn::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		TryCompleteFromPawn(Actor);
		if (Objective->IsConsumed())
		{
			break;
		}
	}
}

void AQuestReachVolume::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	TryCompleteFromPawn(OtherActor);
}
