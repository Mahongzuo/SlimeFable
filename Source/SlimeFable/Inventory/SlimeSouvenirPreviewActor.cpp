#include "SlimeSouvenirPreviewActor.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"

ASlimeSouvenirPreviewActor::ASlimeSouvenirPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	SetRootComponent(Pivot);

	PreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	PreviewMesh->SetupAttachment(Pivot);
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetCastShadow(true);

	KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Pivot);
	KeyLight->SetRelativeRotation(FRotator(-35.f, 40.f, 0.f));
	KeyLight->SetIntensity(6.f);
	KeyLight->SetLightColor(FLinearColor(1.f, 0.92f, 0.78f));

	CaptureComp = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	CaptureComp->SetupAttachment(Pivot);
	CaptureComp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureComp->bCaptureEveryFrame = true;
	CaptureComp->bCaptureOnMovement = true;
	CaptureComp->ShowFlags.SetAtmosphere(false);
	CaptureComp->ShowFlags.SetFog(false);
	CaptureComp->ShowFlags.SetDynamicShadows(true);
	CaptureComp->CaptureSource = SCS_FinalColorLDR;
}

void ASlimeSouvenirPreviewActor::SetupPreview(UStaticMesh* Mesh, UTextureRenderTarget2D* Target)
{
	if (PreviewMesh)
	{
		PreviewMesh->SetStaticMesh(Mesh);
		CaptureComp->ShowOnlyComponent(PreviewMesh);
	}
	if (CaptureComp)
	{
		CaptureComp->TextureTarget = Target;
	}
	OrbitYaw = 25.f;
	OrbitPitch = -18.f;
	FrameCamera();
	Capture();
}

void ASlimeSouvenirPreviewActor::AddOrbit(float YawDelta, float PitchDelta)
{
	OrbitYaw += YawDelta;
	OrbitPitch = FMath::Clamp(OrbitPitch + PitchDelta, -70.f, 70.f);
	FrameCamera();
	Capture();
}

void ASlimeSouvenirPreviewActor::FrameCamera()
{
	if (!CaptureComp || !PreviewMesh)
	{
		return;
	}

	const FBoxSphereBounds Bounds = PreviewMesh->Bounds;
	const float Radius = FMath::Max(Bounds.SphereRadius, 20.f);
	const FVector Center = Bounds.Origin;
	const float Dist = Radius * 2.6f;
	const FRotator Rot(OrbitPitch, OrbitYaw, 0.f);
	const FVector Offset = Rot.Vector() * Dist;
	CaptureComp->SetWorldLocation(Center - Offset);
	CaptureComp->SetWorldRotation((-Offset).Rotation());
	if (CaptureComp->ProjectionType == ECameraProjectionMode::Perspective)
	{
		CaptureComp->FOVAngle = 35.f;
	}
}

void ASlimeSouvenirPreviewActor::Capture()
{
	if (CaptureComp)
	{
		CaptureComp->CaptureScene();
	}
}
