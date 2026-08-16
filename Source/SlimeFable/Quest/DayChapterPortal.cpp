#include "Quest/DayChapterPortal.h"
#include "SlimeFable.h"
#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "GameFramework/Pawn.h"
#include "AssetRegistry/AssetRegistryModule.h"

ADayChapterPortal::ADayChapterPortal()
{
	if (Mesh)
	{
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetRelativeScale3D(FVector::OneVector);
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
	VisualPortal->SetMobility(EComponentMobility::Movable);

	PortalFx = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PortalFx"));
	PortalFx->SetupAttachment(RootComponent);
	PortalFx->SetAutoActivate(false);

	PortalStyleClasses.SetNum(10);
}

void ADayChapterPortal::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (Mesh)
	{
		const FVector Scale = Mesh->GetRelativeScale3D();
		if (!Scale.GetAbs().Equals(FVector(Scale.GetAbs().X), 0.01f))
		{
			const float Uniform = FMath::Pow(FMath::Abs(Scale.X * Scale.Y * Scale.Z), 1.f / 3.f);
			Mesh->SetRelativeScale3D(FVector(FMath::IsFinite(Uniform) && Uniform > KINDA_SMALL_NUMBER ? Uniform : 1.f));
		}
	}
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
		UE_LOG(LogSlimeFable, Warning, TEXT("DayChapterPortal: no BP_Portal_%d style; showing placeholder mesh."),
			FMath::Clamp(PortalStyle, 1, 10));
		if (Mesh)
		{
			Mesh->SetVisibility(true);
			Mesh->SetHiddenInGame(false);
		}
		return;
	}

	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetHiddenInGame(true);
	}

	if (VisualPortal->GetChildActorClass() != StyleClass)
	{
		VisualPortal->SetChildActorClass(StyleClass);
	}
	if (AActor* Child = VisualPortal->GetChildActor())
	{
		DisableChildGameplay(Child);
		SnapChildToPortal(Child);
	}
}

void ADayChapterPortal::SnapChildToPortal(AActor* Child) const
{
	if (!Child || !VisualPortal)
	{
		return;
	}

	TArray<USceneComponent*> Scenes;
	Child->GetComponents<USceneComponent>(Scenes);
	for (USceneComponent* Scene : Scenes)
	{
		if (Scene)
		{
			Scene->SetMobility(EComponentMobility::Movable);
		}
	}

	Child->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Child->AttachToComponent(VisualPortal, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Child->SetActorRelativeLocation(FVector::ZeroVector);
	Child->SetActorRelativeRotation(FRotator::ZeroRotator);
	Child->SetActorRelativeScale3D(FVector::OneVector);
}

void ADayChapterPortal::DisableChildGameplay(AActor* Child) const
{
	if (!Child)
	{
		return;
	}

	Child->SetActorEnableCollision(false);
	if (USceneComponent* Root = Child->GetRootComponent())
	{
		Root->SetMobility(EComponentMobility::Movable);
	}
	TArray<UActorComponent*> Components;
	Child->GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (USceneComponent* Scene = Cast<USceneComponent>(Component))
		{
			Scene->SetMobility(EComponentMobility::Movable);
		}
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Component))
		{
			Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Prim->SetGenerateOverlapEvents(false);
		}
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
