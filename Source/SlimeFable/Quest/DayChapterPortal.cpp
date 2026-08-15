#include "Quest/DayChapterPortal.h"
#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "GameFramework/Pawn.h"
#include "AssetRegistry/AssetRegistryModule.h"

ADayChapterPortal::ADayChapterPortal()
{
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetHiddenInGame(true);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
	}

	TravelVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TravelVolume"));
	TravelVolume->SetupAttachment(RootComponent);
	TravelVolume->SetBoxExtent(OverlapExtent);
	TravelVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	TravelVolume->SetGenerateOverlapEvents(true);
	TravelVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	VisualPortal = CreateDefaultSubobject<UChildActorComponent>(TEXT("VisualPortal"));
	VisualPortal->SetupAttachment(RootComponent);

	PortalFx = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PortalFx"));
	PortalFx->SetupAttachment(RootComponent);
	PortalFx->SetAutoActivate(false);

	PortalStyleClasses.SetNum(10);
}

void ADayChapterPortal::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (TravelVolume)
	{
		TravelVolume->SetBoxExtent(OverlapExtent);
	}
	ApplyPortalStyle();
	ApplyPortalVisuals();
}

void ADayChapterPortal::BeginPlay()
{
	Super::BeginPlay();
	if (TravelVolume)
	{
		TravelVolume->SetBoxExtent(OverlapExtent);
		TravelVolume->OnComponentBeginOverlap.AddDynamic(this, &ADayChapterPortal::HandleBeginOverlap);
	}
	ApplyPortalStyle();
	ApplyPortalVisuals();
}

void ADayChapterPortal::ApplyPortalVisuals()
{
	if (!PortalFx)
	{
		return;
	}
	if (UNiagaraSystem* System = PortalVfx.LoadSynchronous())
	{
		PortalFx->SetAsset(System);
		PortalFx->Activate(true);
	}
}

void ADayChapterPortal::ApplyPortalStyle()
{
	if (!VisualPortal)
	{
		return;
	}

	UClass* StyleClass = ResolveStyleClass();
	if (!StyleClass)
	{
		return;
	}

	if (VisualPortal->GetChildActorClass() != StyleClass)
	{
		VisualPortal->SetChildActorClass(StyleClass);
	}
	DisableChildGameplay(VisualPortal->GetChildActor());
}

void ADayChapterPortal::DisableChildGameplay(AActor* Child) const
{
	if (!Child)
	{
		return;
	}

	Child->SetActorEnableCollision(false);
	Child->SetActorTickEnabled(false);
	TArray<UActorComponent*> Components;
	Child->GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Component))
		{
			Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Prim->SetGenerateOverlapEvents(false);
		}
		Component->SetComponentTickEnabled(false);
	}
}

UClass* ADayChapterPortal::ResolveStyleClass() const
{
	const int32 Index = FMath::Clamp(PortalStyle, 1, 10) - 1;
	if (PortalStyleClasses.IsValidIndex(Index) && !PortalStyleClasses[Index].IsNull())
	{
		if (UClass* Loaded = PortalStyleClasses[Index].LoadSynchronous())
		{
			return Loaded;
		}
	}
	return FindPortalStyleClass(FMath::Clamp(PortalStyle, 1, 10));
}

UClass* ADayChapterPortal::FindPortalStyleClass(int32 Style)
{
	const FName Wanted(*FString::Printf(TEXT("BP_Portal_%d"), Style));
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> Assets;
	AssetRegistryModule.Get().GetAssetsByPath(FName(TEXT("/Game")), Assets, true);

	for (const FAssetData& Asset : Assets)
	{
		if (Asset.AssetName != Wanted)
		{
			continue;
		}

		const FString ClassPath = Asset.GetObjectPathString() + TEXT("_C");
		if (UClass* Loaded = LoadObject<UClass>(nullptr, *ClassPath))
		{
			return Loaded;
		}
	}
	return nullptr;
}

void ADayChapterPortal::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!bEnterOnOverlap)
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}
	RequestEnter(Pawn);
}
