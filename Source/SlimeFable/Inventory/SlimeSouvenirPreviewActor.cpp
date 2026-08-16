#include "SlimeSouvenirPreviewActor.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

ASlimeSouvenirPreviewActor::ASlimeSouvenirPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetTickableWhenPaused(true);
	bReplicates = false;

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	SetRootComponent(Pivot);
	Pivot->SetMobility(EComponentMobility::Movable);

	PreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	PreviewMesh->SetupAttachment(Pivot);
	PreviewMesh->SetMobility(EComponentMobility::Movable);
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetCastShadow(false);
	PreviewMesh->SetCanEverAffectNavigation(false);
	PreviewMesh->LightingChannels.bChannel0 = false;
	PreviewMesh->LightingChannels.bChannel1 = true;

	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(Pivot);
	FillLight->SetMobility(EComponentMobility::Movable);
	FillLight->SetRelativeLocation(FVector(-40.f, 35.f, 30.f));
	FillLight->SetIntensity(2500.f);
	FillLight->SetAttenuationRadius(280.f);
	FillLight->SetSourceRadius(12.f);
	FillLight->SetLightColor(FLinearColor(1.f, 0.93f, 0.82f));
	FillLight->SetCastShadows(false);
	FillLight->LightingChannels.bChannel0 = false;
	FillLight->LightingChannels.bChannel1 = true;
}

void ASlimeSouvenirPreviewActor::SetupPreview(UStaticMesh* Mesh)
{
	if (!PreviewMesh || !Mesh)
	{
		return;
	}

	PreviewMesh->SetStaticMesh(Mesh);
	PreviewMesh->UpdateBounds();
	MeshBounds = Mesh->GetBounds();
	OrbitYaw = 25.f;
	OrbitPitch = -18.f;
	ZoomScale = 1.f;
	ApplyMeshTransform();
}

void ASlimeSouvenirPreviewActor::FitToWorldRect(
	const FVector& Center, const FRotator& ViewRotation, float Width, float Height)
{
	SetActorLocation(Center);
	SetActorRotation(ViewRotation);

	const float Radius = FMath::Max(MeshBounds.SphereRadius, 1.f);
	const float MaxDim = FMath::Min(Width, Height) * 0.85f;
	FitScale = MaxDim / (Radius * 2.f);
	ApplyMeshTransform();
}

void ASlimeSouvenirPreviewActor::AddOrbit(float YawDelta, float PitchDelta)
{
	OrbitYaw += YawDelta;
	OrbitPitch = FMath::Clamp(OrbitPitch + PitchDelta, -70.f, 70.f);
	ApplyMeshTransform();
}

void ASlimeSouvenirPreviewActor::AddZoom(float WheelDelta)
{
	ZoomScale = FMath::Clamp(ZoomScale + WheelDelta * 0.08f, 0.35f, 1.f);
	ApplyMeshTransform();
}

void ASlimeSouvenirPreviewActor::ApplyMeshTransform()
{
	if (!PreviewMesh)
	{
		return;
	}

	const float Scale = FMath::Max(FitScale * ZoomScale, 0.001f);
	const FRotator Orbit(OrbitPitch, OrbitYaw, 0.f);
	PreviewMesh->SetRelativeScale3D(FVector(Scale));
	PreviewMesh->SetRelativeRotation(Orbit);
	PreviewMesh->SetRelativeLocation(Orbit.RotateVector(-MeshBounds.Origin * Scale));
}
