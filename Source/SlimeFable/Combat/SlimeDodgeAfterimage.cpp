// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeDodgeAfterimage.h"

#include "Components/MeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "SlimeBodyComponent.h"
#include "SlimeElementComponent.h"
#include "SlimeElementTypes.h"

ASlimeDodgeAfterimage::ASlimeDodgeAfterimage()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("AfterimageMesh"));
	SetRootComponent(Mesh);
	Mesh->SetUsingAbsoluteLocation(true);
	Mesh->SetUsingAbsoluteRotation(true);
	Mesh->SetUsingAbsoluteScale(true);
	Mesh->SetWorldTransform(FTransform::Identity);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(false);
	Mesh->bCastDynamicShadow = false;
	Mesh->bCastContactShadow = false;
	Mesh->SetCanEverAffectNavigation(false);

	PoseableMesh = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("AfterimagePoseable"));
	PoseableMesh->SetupAttachment(RootComponent);
	PoseableMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PoseableMesh->SetGenerateOverlapEvents(false);
	PoseableMesh->SetCastShadow(false);
	PoseableMesh->bCastDynamicShadow = false;
	PoseableMesh->bCastContactShadow = false;
	PoseableMesh->SetCanEverAffectNavigation(false);
	PoseableMesh->SetHiddenInGame(true);
	PoseableMesh->SetVisibility(false);
}

void ASlimeDodgeAfterimage::CaptureFromSlime(USlimeBodyComponent* Body, float LifeSeconds)
{
	if (!Mesh || !Body)
	{
		Destroy();
		return;
	}

	UProceduralMeshComponent* Source = Body->GetSurfaceMesh();
	if (!Source || Source->GetNumSections() <= 0)
	{
		Source = Body->GetXRayMesh();
	}
	if (!Source || Source->GetNumSections() <= 0)
	{
		Destroy();
		return;
	}

	FProcMeshSection* Section = Source->GetProcMeshSection(0);
	if (!Section || Section->ProcVertexBuffer.Num() == 0 || Section->ProcIndexBuffer.Num() == 0)
	{
		Destroy();
		return;
	}

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	Vertices.Reserve(Section->ProcVertexBuffer.Num());
	Normals.Reserve(Section->ProcVertexBuffer.Num());

	for (const FProcMeshVertex& V : Section->ProcVertexBuffer)
	{
		Vertices.Add(FVector(V.Position));
		Normals.Add(FVector(V.Normal));
		UV0.Add(V.UV0);
		Colors.Add(FLinearColor(V.Color));
		Tangents.Add(V.Tangent);
	}

	TArray<int32> Triangles;
	Triangles.Reserve(Section->ProcIndexBuffer.Num());
	for (uint32 Index : Section->ProcIndexBuffer)
	{
		Triangles.Add(static_cast<int32>(Index));
	}

	Mesh->ClearAllMeshSections();
	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, Colors, Tangents, false);
	Mesh->SetWorldTransform(FTransform::Identity);

	UMaterialInterface* LiveMat = Source->GetMaterial(0);
	UMaterialInterface* BaseMat = Body->GetResolvedBodyMaterial();
	if (!BaseMat)
	{
		BaseMat = LiveMat;
	}
	if (!BaseMat)
	{
		BaseMat = BodyMaterialPath.LoadSynchronous();
	}
	if (!BaseMat)
	{
		BaseMat = Cast<UMaterialInterface>(
			StaticLoadObject(UMaterialInterface::StaticClass(), nullptr,
				TEXT("/Game/Characters/Slime/Materials/M_SlimeBody.M_SlimeBody")));
	}
	if (!BaseMat)
	{
		Destroy();
		return;
	}

	if (UMaterial* Root = BaseMat->GetBaseMaterial())
	{
		BaseMat = Root;
	}

	// Snapshot live look (element MID / profile), then fade slightly so it reads as a ghost.
	FLinearColor BaseColor = FLinearColor(0.35f, 0.75f, 1.f);
	FLinearColor SubsurfaceColor = BaseColor * 0.7f;
	FLinearColor EmissiveColor = FLinearColor::Black;
	FLinearColor RimColor = BaseColor;
	float Opacity = 0.6f;
	float EmissiveIntensity = 0.f;
	float Roughness = 0.35f;
	float Refraction = 1.05f;

	if (UMaterialInstanceDynamic* LiveMID = Cast<UMaterialInstanceDynamic>(LiveMat))
	{
		LiveMID->GetVectorParameterValue(TEXT("BaseColor"), BaseColor);
		LiveMID->GetVectorParameterValue(TEXT("SubsurfaceColor"), SubsurfaceColor);
		LiveMID->GetVectorParameterValue(TEXT("EmissiveColor"), EmissiveColor);
		LiveMID->GetVectorParameterValue(TEXT("RimColor"), RimColor);
		LiveMID->GetScalarParameterValue(TEXT("Opacity"), Opacity);
		LiveMID->GetScalarParameterValue(TEXT("EmissiveIntensity"), EmissiveIntensity);
		LiveMID->GetScalarParameterValue(TEXT("Roughness"), Roughness);
		LiveMID->GetScalarParameterValue(TEXT("Refraction"), Refraction);
	}
	else if (AActor* OwnerActor = Body->GetOwner())
	{
		if (USlimeElementComponent* Element = OwnerActor->FindComponentByClass<USlimeElementComponent>())
		{
			const FSlimeElementProfile Profile = Element->GetCurrentProfile();
			BaseColor = Profile.BaseColor;
			SubsurfaceColor = Profile.SubsurfaceColor;
			EmissiveColor = Profile.EmissiveColor;
			RimColor = Profile.RimColor;
			Opacity = Profile.Opacity;
			EmissiveIntensity = Profile.EmissiveIntensity;
			Roughness = Profile.Roughness;
			Refraction = Profile.Refraction;
		}
	}

	BaseColor = FMath::Lerp(BaseColor, FLinearColor::White, GhostLighten);
	SubsurfaceColor = FMath::Lerp(SubsurfaceColor, FLinearColor::White, GhostLighten * 0.5f);
	RimColor = FMath::Lerp(RimColor, FLinearColor::White, GhostLighten * 0.5f);
	Opacity = FMath::Clamp(Opacity * GhostOpacityScale, 0.05f, 1.f);
	EmissiveIntensity *= GhostEmissiveScale;
	// No PNO offset on ghosts — Distortion stacks badly with multiple afterimages.
	Refraction = 1.f;

	Mesh->SetMaterial(0, BaseMat);
	if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("BaseColor"), BaseColor);
		MID->SetVectorParameterValue(TEXT("SubsurfaceColor"), SubsurfaceColor);
		MID->SetVectorParameterValue(TEXT("EmissiveColor"), EmissiveColor);
		MID->SetVectorParameterValue(TEXT("RimColor"), RimColor);
		MID->SetScalarParameterValue(TEXT("Opacity"), Opacity);
		MID->SetScalarParameterValue(TEXT("EmissiveIntensity"), EmissiveIntensity);
		MID->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		MID->SetScalarParameterValue(TEXT("Refraction"), Refraction);
		MID->SetScalarParameterValue(TEXT("EnableRefraction"), 0.f);
	}

	Mesh->SetVisibility(true);
	Mesh->SetHiddenInGame(false);
	SetActorHiddenInGame(false);
	SetLifeSpan(FMath::Max(LifeSeconds, 0.05f));
}

void ASlimeDodgeAfterimage::CaptureFromSkeletalMesh(USkeletalMeshComponent* Source, float LifeSeconds)
{
	if (!PoseableMesh || !Source || !Source->GetSkeletalMeshAsset())
	{
		Destroy();
		return;
	}

	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetHiddenInGame(true);
	}

	PoseableMesh->SetSkinnedAssetAndUpdate(Source->GetSkeletalMeshAsset());
	PoseableMesh->SetWorldTransform(Source->GetComponentTransform());
	PoseableMesh->CopyPoseFromSkeletalComponent(Source);

	const int32 NumMats = Source->GetNumMaterials();
	for (int32 Index = 0; Index < NumMats; ++Index)
	{
		if (UMaterialInterface* SrcMat = Source->GetMaterial(Index))
		{
			PoseableMesh->SetMaterial(Index, SrcMat);
		}
	}
	ApplyGhostOpacityToComponent(PoseableMesh);

	PoseableMesh->SetVisibility(true);
	PoseableMesh->SetHiddenInGame(false);
	SetActorHiddenInGame(false);
	SetLifeSpan(FMath::Max(LifeSeconds, 0.05f));
}

void ASlimeDodgeAfterimage::ApplyGhostOpacityToComponent(UMeshComponent* TargetMesh) const
{
	if (!TargetMesh)
	{
		return;
	}
	const int32 Num = TargetMesh->GetNumMaterials();
	for (int32 Index = 0; Index < Num; ++Index)
	{
		UMaterialInterface* Base = TargetMesh->GetMaterial(Index);
		if (!Base)
		{
			continue;
		}
		if (UMaterialInstanceDynamic* MID = TargetMesh->CreateAndSetMaterialInstanceDynamic(Index))
		{
			float Opacity = 1.f;
			MID->GetScalarParameterValue(TEXT("Opacity"), Opacity);
			MID->SetScalarParameterValue(TEXT("Opacity"), FMath::Clamp(Opacity * GhostOpacityScale, 0.05f, 1.f));
			float OpacityMask = 1.f;
			MID->GetScalarParameterValue(TEXT("OpacityMask"), OpacityMask);
			MID->SetScalarParameterValue(TEXT("OpacityMask"), FMath::Clamp(OpacityMask * GhostOpacityScale, 0.05f, 1.f));
		}
	}
}
