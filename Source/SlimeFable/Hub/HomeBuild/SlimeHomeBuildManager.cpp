// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hub/HomeBuild/SlimeHomeBuildManager.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Hub/HomeBuild/SlimeHomeBuildCatalog.h"
#include "Hub/HomeBuild/SlimeHomeFluidPad.h"
#include "Inventory/SlimeItemDefinition.h"
#include "Inventory/SlimeInventorySubsystem.h"
#include "Inventory/SlimePlacedActor.h"
#include "Materials/MaterialInterface.h"
#include "SlimeFable.h"

namespace
{
	FRotator HomePieceRotation(const FSlimeHomeBuildRecord& Record)
	{
		return FRotator(Record.PitchDegrees, Record.YawSteps * 90.f, 0.f);
	}

	float HomeUserScale(const FSlimeHomeBuildRecord& Record)
	{
		const float Raw = Record.UserScale > KINDA_SMALL_NUMBER ? Record.UserScale : 1.f;
		return FMath::Clamp(Raw, 0.25f, 4.f);
	}
}

ASlimeHomeBuildManager::ASlimeHomeBuildManager()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);

	HighlightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Highlight"));
	HighlightMesh->SetupAttachment(Root);
	HighlightMesh->SetMobility(EComponentMobility::Movable);
	HighlightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HighlightMesh->SetCastShadow(false);
	HighlightMesh->SetHiddenInGame(true);
}

bool ASlimeHomeBuildManager::AddRecord(const FSlimeHomeBuildRecord& Record)
{
	if (!Catalog)
	{
		return false;
	}
	const FSlimeHomeBuildEntry* Entry = Record.bFromBag ? nullptr : Catalog->FindEntry(Record.EntryId);
	if (!Record.bFromBag && !Entry)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("[HomeBuild] missing catalog entry %s"), *Record.EntryId.ToString());
		return false;
	}

	FTransform Transform = FTransform::Identity;
	if (Entry)
	{
		if (!ComputeTransform(*Entry, Record, Transform))
		{
			return false;
		}
	}
	else if (!GetRecordTransform(Record, Transform))
	{
		return false;
	}

	Records.Add(Record);
	if (Record.bFluid && Entry)
	{
		SpawnFluid(Record, *Entry, Transform);
	}
	else if (Record.bFromBag)
	{
		SpawnBag(Record, Transform);
	}
	else if (Entry)
	{
		if (UStaticMesh* Mesh = Entry->Mesh.LoadSynchronous())
		{
			if (UInstancedStaticMeshComponent* Bucket = GetBucket(Entry->EntryId, Mesh))
			{
				FInstancePiece Piece;
				Piece.RecordId = Record.Id;
				Piece.MeshKey = Entry->EntryId;
				Piece.Transform = Transform;
				Piece.InstanceIndex = Bucket->AddInstance(Transform, true);
				Instances.Add(Piece);
			}
		}
	}
	return true;
}

void ASlimeHomeBuildManager::RemoveRecord(int32 RecordId)
{
	FName MeshKey = NAME_None;
	int32 InstanceIndex = INDEX_NONE;
	for (const FInstancePiece& Piece : Instances)
	{
		if (Piece.RecordId == RecordId)
		{
			MeshKey = Piece.MeshKey;
			InstanceIndex = Piece.InstanceIndex;
			break;
		}
	}
	Instances.RemoveAll([RecordId](const FInstancePiece& Piece) { return Piece.RecordId == RecordId; });
	if (!MeshKey.IsNone() && InstanceIndex != INDEX_NONE)
	{
		RemoveInstanceAt(MeshKey, InstanceIndex);
	}
	if (TObjectPtr<AActor>* Found = SpawnedActors.Find(RecordId))
	{
		if (AActor* Actor = Found->Get())
		{
			Actor->Destroy();
		}
		SpawnedActors.Remove(RecordId);
	}
	Records.RemoveAll([RecordId](const FSlimeHomeBuildRecord& Record) { return Record.Id == RecordId; });
	if (HighlightMesh)
	{
		HighlightMesh->SetHiddenInGame(true);
	}
}

void ASlimeHomeBuildManager::ClearAll()
{
	for (const TPair<FName, TObjectPtr<UInstancedStaticMeshComponent>>& Pair : Buckets)
	{
		if (Pair.Value)
		{
			Pair.Value->ClearInstances();
		}
	}
	for (const TPair<int32, TObjectPtr<AActor>>& Pair : SpawnedActors)
	{
		if (Pair.Value)
		{
			Pair.Value->Destroy();
		}
	}
	SpawnedActors.Reset();
	Instances.Reset();
	Records.Reset();
}

int32 ASlimeHomeBuildManager::NumFluidPads() const
{
	int32 Count = 0;
	for (const FSlimeHomeBuildRecord& Record : Records)
	{
		if (Record.bFluid)
		{
			++Count;
		}
	}
	return Count;
}

bool ASlimeHomeBuildManager::IsBlocked(int32 AnchorX, int32 AnchorY, int32 FootX, int32 FootY, float BaseZ, float HeightCm, int32 IgnoreId, bool bIgnoreNonFluid) const
{
	if (!Catalog)
	{
		return false;
	}
	const float Cell = Catalog->CellSize;
	const float MinX = AnchorX * Cell;
	const float MinY = AnchorY * Cell;
	const float MaxX = MinX + FootX * Cell;
	const float MaxY = MinY + FootY * Cell;
	const float Top = BaseZ + HeightCm;

	for (const FSlimeHomeBuildRecord& Other : Records)
	{
		if (Other.Id == IgnoreId || (bIgnoreNonFluid && !Other.bFluid))
		{
			continue;
		}
		int32 OtherFootX = 1;
		int32 OtherFootY = 1;
		float OtherHeight = Cell;
		if (!Other.bFromBag)
		{
			if (const FSlimeHomeBuildEntry* Entry = Catalog->FindEntry(Other.EntryId))
			{
				OtherFootX = FMath::Max(Entry->FootprintX, 1);
				OtherFootY = FMath::Max(Entry->FootprintY, 1);
				if ((Other.YawSteps & 1) != 0)
				{
					Swap(OtherFootX, OtherFootY);
				}
				OtherHeight = FMath::Max(Entry->HeightCm, 1.f);
			}
		}
		const float OMinX = Other.AnchorX * Cell;
		const float OMinY = Other.AnchorY * Cell;
		const bool bOverlapXY = MinX < OMinX + OtherFootX * Cell && MaxX > OMinX
			&& MinY < OMinY + OtherFootY * Cell && MaxY > OMinY;
		const bool bOverlapZ = BaseZ < Other.BaseZ + OtherHeight - 1.f && Top > Other.BaseZ + 1.f;
		if (bOverlapXY && bOverlapZ)
		{
			return true;
		}
	}
	return false;
}

bool ASlimeHomeBuildManager::GetRecordStack(int32 RecordId, float& OutBaseZ, float& OutHeight) const
{
	const FSlimeHomeBuildRecord* Found = Records.FindByPredicate(
		[RecordId](const FSlimeHomeBuildRecord& Record) { return Record.Id == RecordId; });
	if (!Found)
	{
		return false;
	}
	OutBaseZ = Found->BaseZ;
	const float Cell = Catalog ? Catalog->CellSize : 50.f;
	OutHeight = Cell;
	if (!Found->bFromBag)
	{
		if (const FSlimeHomeBuildEntry* Entry = Catalog ? Catalog->FindEntry(Found->EntryId) : nullptr)
		{
			OutHeight = FMath::Max(Entry->HeightCm, 1.f);
		}
		return true;
	}
	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	USlimeInventorySubsystem* Inv = GI ? GI->GetSubsystem<USlimeInventorySubsystem>() : nullptr;
	const USlimePlaceableDefinition* Def = Inv
		? Cast<USlimePlaceableDefinition>(Inv->FindDefinition(Found->EntryId))
		: nullptr;
	if (Def)
	{
		if (UStaticMesh* Mesh = Def->PreviewMesh.LoadSynchronous())
		{
			const FBox Box = Mesh->GetBoundingBox();
			OutHeight = FMath::Max((Box.Max.Z - Box.Min.Z) * Def->PlacedMeshScale.Z, 1.f);
		}
	}
	return true;
}

int32 ASlimeHomeBuildManager::FindRecordAtHit(const FHitResult& Hit) const
{
	const AActor* HitActor = Hit.GetActor();
	const ASlimeHomeFluidPad* Pad = Cast<ASlimeHomeFluidPad>(HitActor);
	if (!Pad && HitActor)
	{
		Pad = Cast<ASlimeHomeFluidPad>(HitActor->GetOwner());
		if (!Pad)
		{
			Pad = Cast<ASlimeHomeFluidPad>(HitActor->GetAttachParentActor());
		}
	}
	if (Pad)
	{
		return Pad->GetHomePieceId();
	}
	if (const ASlimePlacedActor* Placed = Cast<ASlimePlacedActor>(Hit.GetActor()))
	{
		return Placed->GetHomePieceId();
	}
	const UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(Hit.GetComponent());
	if (!ISM)
	{
		return INDEX_NONE;
	}
	for (const FInstancePiece& Piece : Instances)
	{
		if (Piece.InstanceIndex == Hit.Item)
		{
			const UInstancedStaticMeshComponent* Bucket = Buckets.FindRef(Piece.MeshKey);
			if (Bucket == ISM)
			{
				return Piece.RecordId;
			}
		}
	}
	return INDEX_NONE;
}

bool ASlimeHomeBuildManager::GetRecordTransform(const FSlimeHomeBuildRecord& Record, FTransform& OutTransform) const
{
	if (!Catalog)
	{
		return false;
	}
	if (!Record.bFromBag)
	{
		if (const FSlimeHomeBuildEntry* Entry = Catalog->FindEntry(Record.EntryId))
		{
			return ComputeTransform(*Entry, Record, OutTransform);
		}
		return false;
	}

	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	USlimeInventorySubsystem* Inv = GI ? GI->GetSubsystem<USlimeInventorySubsystem>() : nullptr;
	const USlimePlaceableDefinition* Def = Inv
		? Cast<USlimePlaceableDefinition>(Inv->FindDefinition(Record.EntryId))
		: nullptr;
	UStaticMesh* Mesh = Def ? Def->PreviewMesh.LoadSynchronous() : nullptr;
	if (!Mesh)
	{
		return false;
	}
	const float User = HomeUserScale(Record);
	const FVector Scale = Def->PlacedMeshScale * User;
	const float Cell = Catalog->CellSize;
	const FVector Center((Record.AnchorX + 0.5f) * Cell, (Record.AnchorY + 0.5f) * Cell, Record.BaseZ);
	const FBox Box = Mesh->GetBoundingBox();
	const FRotator Rotation = HomePieceRotation(Record);
	const FTransform Local(Rotation, FVector::ZeroVector, Scale);
	FVector Min(FLT_MAX);
	FVector Max(-FLT_MAX);
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector Point = Local.TransformPosition(FVector(
			(Corner & 1) ? Box.Max.X : Box.Min.X,
			(Corner & 2) ? Box.Max.Y : Box.Min.Y,
			(Corner & 4) ? Box.Max.Z : Box.Min.Z));
		Min = Min.ComponentMin(Point);
		Max = Max.ComponentMax(Point);
	}
	const FVector BoundsCenter = (Min + Max) * 0.5f;
	const FVector Origin(Center.X - BoundsCenter.X, Center.Y - BoundsCenter.Y, Record.BaseZ - Min.Z);
	OutTransform = FTransform(Rotation, Origin, Scale);
	return true;
}

void ASlimeHomeBuildManager::SetHighlight(int32 RecordId)
{
	if (!HighlightMesh)
	{
		return;
	}
	if (RecordId == INDEX_NONE)
	{
		HighlightMesh->SetHiddenInGame(true);
		for (const TPair<int32, TObjectPtr<AActor>>& Pair : SpawnedActors)
		{
			if (ASlimeHomeFluidPad* Pad = Cast<ASlimeHomeFluidPad>(Pair.Value.Get()))
			{
				Pad->SetHighlighted(false);
			}
			else if (ASlimePlacedActor* Placed = Cast<ASlimePlacedActor>(Pair.Value.Get()))
			{
				Placed->SetHighlight(false);
			}
		}
		return;
	}

	for (const TPair<int32, TObjectPtr<AActor>>& Pair : SpawnedActors)
	{
		const bool bThis = Pair.Key == RecordId;
		if (ASlimeHomeFluidPad* Pad = Cast<ASlimeHomeFluidPad>(Pair.Value.Get()))
		{
			Pad->SetHighlighted(bThis);
		}
		else if (ASlimePlacedActor* Placed = Cast<ASlimePlacedActor>(Pair.Value.Get()))
		{
			Placed->SetHighlight(bThis);
		}
	}

	for (const FInstancePiece& Piece : Instances)
	{
		if (Piece.RecordId != RecordId)
		{
			continue;
		}
		UStaticMesh* Mesh = nullptr;
		if (const UInstancedStaticMeshComponent* Bucket = Buckets.FindRef(Piece.MeshKey))
		{
			Mesh = Bucket->GetStaticMesh();
		}
		if (Mesh)
		{
			HighlightMesh->SetStaticMesh(Mesh);
			HighlightMesh->SetWorldTransform(Piece.Transform);
			HighlightMesh->SetHiddenInGame(false);
			if (UMaterialInterface* Overlay = LoadObject<UMaterialInterface>(
					nullptr, TEXT("/Game/Materials/M_PickupOutline.M_PickupOutline")))
			{
				HighlightMesh->SetOverlayMaterial(Overlay);
			}
		}
		return;
	}
	HighlightMesh->SetHiddenInGame(true);
}

UInstancedStaticMeshComponent* ASlimeHomeBuildManager::GetBucket(FName MeshKey, UStaticMesh* Mesh)
{
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = Buckets.Find(MeshKey))
	{
		return Found->Get();
	}
	UInstancedStaticMeshComponent* Bucket = NewObject<UInstancedStaticMeshComponent>(this);
	Bucket->SetupAttachment(GetRootComponent());
	Bucket->SetMobility(EComponentMobility::Movable);
	Bucket->SetStaticMesh(Mesh);
	Bucket->SetCollisionProfileName(TEXT("BlockAll"));
	Bucket->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Bucket->RegisterComponent();
	Buckets.Add(MeshKey, Bucket);
	return Bucket;
}

void ASlimeHomeBuildManager::RemoveInstanceAt(FName MeshKey, int32 InstanceIndex)
{
	UInstancedStaticMeshComponent* Bucket = nullptr;
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = Buckets.Find(MeshKey))
	{
		Bucket = Found->Get();
	}
	if (!Bucket || InstanceIndex < 0 || InstanceIndex >= Bucket->GetInstanceCount())
	{
		return;
	}
	const int32 Last = Bucket->GetInstanceCount() - 1;
	Bucket->RemoveInstance(InstanceIndex);
	if (InstanceIndex >= Last)
	{
		return;
	}
	for (FInstancePiece& Piece : Instances)
	{
		if (Piece.MeshKey == MeshKey && Piece.InstanceIndex == Last)
		{
			Piece.InstanceIndex = InstanceIndex;
			break;
		}
	}
}

bool ASlimeHomeBuildManager::ComputeTransform(const FSlimeHomeBuildEntry& Entry, const FSlimeHomeBuildRecord& Record, FTransform& OutTransform) const
{
	UStaticMesh* Mesh = Entry.Mesh.LoadSynchronous();
	if (!Mesh || !Catalog)
	{
		return false;
	}
	int32 FootX = FMath::Max(Entry.FootprintX, 1);
	int32 FootY = FMath::Max(Entry.FootprintY, 1);
	if ((Record.YawSteps & 1) != 0)
	{
		Swap(FootX, FootY);
	}
	const float User = Entry.bFluidPad ? 1.f : HomeUserScale(Record);
	const FVector Scale(Entry.UniformScale * User);
	const float Cell = Catalog->CellSize;
	const FVector Center((Record.AnchorX + FootX * 0.5f) * Cell, (Record.AnchorY + FootY * 0.5f) * Cell, Record.BaseZ);
	const FBox Box = Mesh->GetBoundingBox();
	const FRotator Rotation = HomePieceRotation(Record);
	const FTransform Local(Rotation, FVector::ZeroVector, Scale);
	FVector Min(FLT_MAX);
	FVector Max(-FLT_MAX);
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector Point = Local.TransformPosition(FVector(
			(Corner & 1) ? Box.Max.X : Box.Min.X,
			(Corner & 2) ? Box.Max.Y : Box.Min.Y,
			(Corner & 4) ? Box.Max.Z : Box.Min.Z));
		Min = Min.ComponentMin(Point);
		Max = Max.ComponentMax(Point);
	}
	const FVector BoundsCenter = (Min + Max) * 0.5f;
	const FVector Origin(Center.X - BoundsCenter.X, Center.Y - BoundsCenter.Y, Record.BaseZ - Min.Z);
	OutTransform = FTransform(Rotation, Origin, Scale);
	return true;
}

void ASlimeHomeBuildManager::SpawnFluid(const FSlimeHomeBuildRecord& Record, const FSlimeHomeBuildEntry& Entry, const FTransform& Transform)
{
	(void)Transform;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	UClass* FluidClass = Entry.FluidClass.LoadSynchronous();
	int32 FootX = FMath::Max(Entry.FootprintX, 1);
	int32 FootY = FMath::Max(Entry.FootprintY, 1);
	if ((Record.YawSteps & 1) != 0)
	{
		Swap(FootX, FootY);
	}
	const float Cell = Catalog ? Catalog->CellSize : 50.f;
	const FVector Center((Record.AnchorX + FootX * 0.5f) * Cell, (Record.AnchorY + FootY * 0.5f) * Cell, Record.BaseZ);
	const FTransform ActorXform(FRotator(Record.PitchDegrees, Record.YawSteps * 90.f, 0.f), Center);
	ASlimeHomeFluidPad* Pad = World->SpawnActorDeferred<ASlimeHomeFluidPad>(
		ASlimeHomeFluidPad::StaticClass(), ActorXform, this, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pad)
	{
		return;
	}
	Pad->Configure(FluidClass, Record.Id);
	Pad->FinishSpawning(ActorXform);
	SpawnedActors.Add(Record.Id, Pad);
}

void ASlimeHomeBuildManager::SpawnBag(const FSlimeHomeBuildRecord& Record, const FTransform& Transform)
{
	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	USlimeInventorySubsystem* Inv = GI ? GI->GetSubsystem<USlimeInventorySubsystem>() : nullptr;
	USlimePlaceableDefinition* Def = Inv ? Cast<USlimePlaceableDefinition>(Inv->FindDefinition(Record.EntryId)) : nullptr;
	if (!World || !Def)
	{
		return;
	}
	UClass* SpawnClass = Def->PlacedActorClass.LoadSynchronous();
	if (!SpawnClass)
	{
		SpawnClass = ASlimePlacedActor::StaticClass();
	}
	FTransform ActorXform = Transform;
	ActorXform.SetScale3D(FVector::OneVector);
	AActor* Spawned = World->SpawnActorDeferred<AActor>(
		SpawnClass, ActorXform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (ASlimePlacedActor* Placed = Cast<ASlimePlacedActor>(Spawned))
	{
		Placed->ConfigureFromItem(Record.EntryId, Def);
		if (Placed->Mesh)
		{
			Placed->Mesh->SetRelativeScale3D(Placed->Mesh->GetRelativeScale3D() * HomeUserScale(Record));
		}
		Placed->SetHomePieceId(Record.Id);
		Placed->FinishSpawning(ActorXform);
		SpawnedActors.Add(Record.Id, Placed);
	}
	else if (Spawned)
	{
		Spawned->FinishSpawning(ActorXform);
		Spawned->Destroy();
	}
}
