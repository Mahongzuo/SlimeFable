// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeDodgeAfterimage.generated.h"

class UProceduralMeshComponent;
class USlimeBodyComponent;
class UMaterialInterface;

UCLASS()
class SLIMEFABLE_API ASlimeDodgeAfterimage : public AActor
{
	GENERATED_BODY()

public:
	ASlimeDodgeAfterimage();

	/** Snapshot the slime surface; tint matches the live body, slightly faded. */
	void CaptureFromSlime(USlimeBodyComponent* Body, float LifeSeconds);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Dodge")
	TObjectPtr<UProceduralMeshComponent> Mesh;

	/** Fallback if the live surface has no material assigned. */
	UPROPERTY(EditAnywhere, Category = "Dodge")
	TSoftObjectPtr<UMaterialInterface> BodyMaterialPath =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
			TEXT("/Game/Characters/Slime/Materials/M_SlimeBody.M_SlimeBody")));

	/** Multiplier on the live Opacity so the ghost reads as a faint copy. */
	UPROPERTY(EditAnywhere, Category = "Dodge", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float GhostOpacityScale = 0.55f;

	/** Blend live BaseColor toward white (0 = exact match, 1 = pure white). */
	UPROPERTY(EditAnywhere, Category = "Dodge", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GhostLighten = 0.12f;

	/** Multiplier on live emissive intensity. */
	UPROPERTY(EditAnywhere, Category = "Dodge", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GhostEmissiveScale = 0.45f;
};
