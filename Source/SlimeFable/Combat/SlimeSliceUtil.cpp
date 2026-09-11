// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeSliceUtil.h"

#include "Kismet/GameplayStatics.h"
#include "KismetProceduralMeshLibrary.h"
#include "ProceduralMeshComponent.h"

bool USlimeSliceUtil::IsMeshLargeEnoughToSlice(const UProceduralMeshComponent* Mesh, float MinSliceExtent)
{
	if (!Mesh)
	{
		return false;
	}
	const FBoxSphereBounds Bounds = Mesh->CalcLocalBounds();
	const FVector Extent = Bounds.BoxExtent;
	const float Shortest = FMath::Min3(Extent.X, Extent.Y, Extent.Z) * 2.f;
	return Shortest >= MinSliceExtent;
}

void USlimeSliceUtil::EnableSimpleConvexCollision(UProceduralMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return;
	}

	// Chaos cannot simulate ComplexAsSimple (the PMC default after mesh copy).
	Mesh->bUseComplexAsSimpleCollision = false;

	const FBox Box = Mesh->CalcLocalBounds().GetBox();
	const FVector Min = Box.Min;
	const FVector Max = Box.Max;
	TArray<FVector> ConvexVerts = {
		FVector(Min.X, Min.Y, Min.Z),
		FVector(Min.X, Min.Y, Max.Z),
		FVector(Min.X, Max.Y, Min.Z),
		FVector(Min.X, Max.Y, Max.Z),
		FVector(Max.X, Min.Y, Min.Z),
		FVector(Max.X, Min.Y, Max.Z),
		FVector(Max.X, Max.Y, Min.Z),
		FVector(Max.X, Max.Y, Max.Z),
	};

	Mesh->ClearCollisionConvexMeshes();
	Mesh->AddCollisionConvexMesh(MoveTemp(ConvexVerts));
}

void USlimeSliceUtil::PrepareSlicedMeshPhysics(UProceduralMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return;
	}
	EnableSimpleConvexCollision(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECC_PhysicsBody);
	Mesh->SetGenerateOverlapEvents(true);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetEnableGravity(true);
}

bool USlimeSliceUtil::SliceProceduralMeshAt(
	UProceduralMeshComponent* TargetMesh,
	FVector PlanePosition,
	FVector PlaneNormal,
	UMaterialInterface* CapMaterial,
	USoundBase* SliceSound,
	float MinSliceExtent,
	float SliceImpulse)
{
	if (!TargetMesh || !IsMeshLargeEnoughToSlice(TargetMesh, MinSliceExtent))
	{
		return false;
	}

	FVector Normal = PlaneNormal.GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		AActor* Owner = TargetMesh->GetOwner();
		Normal = Owner ? Owner->GetActorForwardVector() : FVector::ForwardVector;
	}

	UProceduralMeshComponent* OtherHalf = nullptr;
	UKismetProceduralMeshLibrary::SliceProceduralMesh(
		TargetMesh,
		PlanePosition,
		Normal,
		true,
		OtherHalf,
		EProcMeshSliceCapOption::CreateNewSectionForCap,
		CapMaterial);

	PrepareSlicedMeshPhysics(TargetMesh);
	if (OtherHalf)
	{
		PrepareSlicedMeshPhysics(OtherHalf);
		const FVector Impulse = Normal * SliceImpulse + FVector::UpVector * (SliceImpulse * 0.35f);
		OtherHalf->AddImpulseAtLocation(Impulse, PlanePosition);
		TargetMesh->AddImpulseAtLocation(-Impulse * 0.6f, PlanePosition);
	}

	if (SliceSound)
	{
		if (AActor* Owner = TargetMesh->GetOwner())
		{
			UGameplayStatics::PlaySoundAtLocation(Owner, SliceSound, Owner->GetActorLocation());
		}
	}
	return true;
}

bool USlimeSliceUtil::ShatterProceduralMesh(
	UProceduralMeshComponent* RootMesh,
	UMaterialInterface* CapMaterial,
	USoundBase* SliceSound,
	int32 PieceMin,
	int32 PieceMax,
	float MinSliceExtent,
	float SliceImpulse)
{
	if (!RootMesh)
	{
		return false;
	}

	const int32 Low = FMath::Max(2, FMath::Min(PieceMin, PieceMax));
	const int32 High = FMath::Max(Low, FMath::Max(PieceMin, PieceMax));
	const int32 Target = FMath::RandRange(Low, High);

	TArray<UProceduralMeshComponent*> Pieces;
	Pieces.Add(RootMesh);
	TArray<FVector> UsedNormals;

	auto PickLargest = [&]() -> UProceduralMeshComponent*
	{
		UProceduralMeshComponent* Best = nullptr;
		float BestSize = 0.f;
		for (UProceduralMeshComponent* Mesh : Pieces)
		{
			if (!Mesh || !IsMeshLargeEnoughToSlice(Mesh, MinSliceExtent))
			{
				continue;
			}
			const FVector Extent = Mesh->CalcLocalBounds().BoxExtent;
			const float Size = Extent.X * Extent.Y * Extent.Z;
			if (Size > BestSize)
			{
				BestSize = Size;
				Best = Mesh;
			}
		}
		return Best;
	};

	auto MakeNormal = [&]() -> FVector
	{
		for (int32 Attempt = 0; Attempt < 8; ++Attempt)
		{
			FVector Candidate = FMath::VRand();
			if (Candidate.Normalize())
			{
				bool bTooParallel = false;
				for (const FVector& Used : UsedNormals)
				{
					if (FMath::Abs(FVector::DotProduct(Candidate, Used)) > 0.86f)
					{
						bTooParallel = true;
						break;
					}
				}
				if (!bTooParallel)
				{
					return Candidate;
				}
			}
		}
		return FMath::VRand().GetSafeNormal();
	};

	int32 Guard = Target + 4;
	while (Pieces.Num() < Target && Guard-- > 0)
	{
		UProceduralMeshComponent* Victim = PickLargest();
		if (!Victim)
		{
			break;
		}

		const FVector Normal = MakeNormal();
		const FBoxSphereBounds LocalBounds = Victim->CalcLocalBounds();
		const FVector Extent = LocalBounds.BoxExtent;
		const FVector LocalOffset(
			FMath::FRandRange(-Extent.X * 0.35f, Extent.X * 0.35f),
			FMath::FRandRange(-Extent.Y * 0.35f, Extent.Y * 0.35f),
			FMath::FRandRange(-Extent.Z * 0.35f, Extent.Z * 0.35f));
		const FVector PlanePos = Victim->GetComponentTransform().TransformPosition(
			LocalBounds.Origin + LocalOffset);

		UProceduralMeshComponent* OtherHalf = nullptr;
		UKismetProceduralMeshLibrary::SliceProceduralMesh(
			Victim,
			PlanePos,
			Normal,
			true,
			OtherHalf,
			EProcMeshSliceCapOption::CreateNewSectionForCap,
			CapMaterial);

		UsedNormals.Add(Normal);
		PrepareSlicedMeshPhysics(Victim);
		if (OtherHalf)
		{
			PrepareSlicedMeshPhysics(OtherHalf);
			Pieces.Add(OtherHalf);
		}
	}

	FVector Center = FVector::ZeroVector;
	int32 Count = 0;
	for (UProceduralMeshComponent* Mesh : Pieces)
	{
		if (Mesh)
		{
			Center += Mesh->Bounds.Origin;
			++Count;
		}
	}
	if (Count > 0)
	{
		Center /= static_cast<float>(Count);
	}
	for (UProceduralMeshComponent* Mesh : Pieces)
	{
		if (!Mesh)
		{
			continue;
		}
		FVector Outward = Mesh->Bounds.Origin - Center;
		if (!Outward.Normalize())
		{
			Outward = FMath::VRand().GetSafeNormal();
		}
		Mesh->AddImpulseAtLocation(
			Outward * SliceImpulse + FVector::UpVector * (SliceImpulse * 0.25f),
			Mesh->Bounds.Origin);
	}

	if (SliceSound)
	{
		if (AActor* Owner = RootMesh->GetOwner())
		{
			UGameplayStatics::PlaySoundAtLocation(Owner, SliceSound, Owner->GetActorLocation());
		}
	}
	return Pieces.Num() > 1;
}
