#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeSouvenirPreviewActor.generated.h"

class UStaticMeshComponent;
class USceneCaptureComponent2D;
class UDirectionalLightComponent;
class UTextureRenderTarget2D;
class UStaticMesh;

/** Off-screen mesh + SceneCapture used by the souvenir viewer. */
UCLASS()
class SLIMEFABLE_API ASlimeSouvenirPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	ASlimeSouvenirPreviewActor();

	void SetupPreview(UStaticMesh* Mesh, UTextureRenderTarget2D* Target);
	void AddOrbit(float YawDelta, float PitchDelta);
	void Capture();

protected:
	void FrameCamera();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PreviewMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneCaptureComponent2D> CaptureComp;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDirectionalLightComponent> KeyLight;

	float OrbitYaw = 25.f;
	float OrbitPitch = -18.f;
};
