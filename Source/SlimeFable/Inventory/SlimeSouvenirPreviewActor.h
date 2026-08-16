#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeSouvenirPreviewActor.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UStaticMesh;

/** In-world inspect mesh shown through the souvenir viewer window. */
UCLASS()
class SLIMEFABLE_API ASlimeSouvenirPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	ASlimeSouvenirPreviewActor();

	void SetupPreview(UStaticMesh* Mesh);
	void FitToWorldRect(const FVector& Center, const FRotator& ViewRotation, float Width, float Height);
	void AddOrbit(float YawDelta, float PitchDelta);
	void AddZoom(float WheelDelta);

protected:
	void ApplyMeshTransform();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PreviewMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> FillLight;

	FBoxSphereBounds MeshBounds;
	float OrbitYaw = 25.f;
	float OrbitPitch = -18.f;
	float ZoomScale = 1.f;
	float FitScale = 1.f;
};
