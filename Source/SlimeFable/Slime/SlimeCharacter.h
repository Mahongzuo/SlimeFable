// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeFableCharacter.h"
#include "SlimeCharacter.generated.h"

class UProceduralMeshComponent;
class USlimeAbilityComponent;
class USlimeBodyComponent;
class USlimeElementComponent;
class USlimeTrailComponent;

/**
 *  The slime pawn.
 *
 *  Intentionally thin: movement stays on the stock CharacterMovementComponent and everything
 *  slime specific lives in components, so inventory, skills and animation can be added later
 *  by dropping in more components rather than editing this class.
 */
UCLASS()
class SLIMEFABLE_API ASlimeCharacter : public ASlimeFableCharacter
{
	GENERATED_BODY()

public:
	ASlimeCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyControllerChanged() override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnJumped_Implementation() override;

	/** Ground / first jump vertical impulse written into CharacterMovement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Jump", meta = (ClampMin = "100.0"))
	float JumpZVelocity = 620.f;

	/** Second jump vertical speed (replaces CMC JumpZVelocity for air jumps). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Jump", meta = (ClampMin = "100.0"))
	float AirJumpZVelocity = 520.f;

	// ---- Camera zoom (mouse wheel) ---------------------------------------------------

	/** Default spring-arm length at spawn, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Camera", meta = (ClampMin = "50.0"))
	float CameraArmLengthDefault = 260.f;

	/** Closest zoom (shortest arm), in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Camera", meta = (ClampMin = "50.0"))
	float CameraArmLengthMin = 120.f;

	/** Farthest zoom (longest arm), in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Camera", meta = (ClampMin = "100.0"))
	float CameraArmLengthMax = 520.f;

	/** Arm-length change per mouse-wheel notch, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Camera", meta = (ClampMin = "5.0"))
	float CameraZoomStep = 40.f;

	/** How quickly the boom eases toward the desired length (higher = snappier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Camera", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float CameraZoomInterpSpeed = 8.f;

	UFUNCTION(BlueprintPure, Category = "Slime")
	USlimeBodyComponent* GetSlimeBody() const { return SlimeBody; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	USlimeAbilityComponent* GetSlimeAbilities() const { return SlimeAbilities; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	USlimeElementComponent* GetSlimeElement() const { return SlimeElement; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	USlimeTrailComponent* GetSlimeTrail() const { return SlimeTrail; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	UProceduralMeshComponent* GetSurfaceMesh() const { return SurfaceMesh; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	UProceduralMeshComponent* GetShadowMesh() const { return ShadowMesh; }

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void UpdateCameraZoom(float DeltaSeconds);

	/** Surface mesh. Vertices are world space, so this component sits at the world origin. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<UProceduralMeshComponent> SurfaceMesh;

	/**
	 *  Hidden opaque copy that only casts shadows — avoids translucent self-shadow speckles
	 *  while keeping a readable ground blob.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<UProceduralMeshComponent> ShadowMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<USlimeBodyComponent> SlimeBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<USlimeAbilityComponent> SlimeAbilities;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<USlimeElementComponent> SlimeElement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<USlimeTrailComponent> SlimeTrail;

	/** Cached before CMC clears vertical speed on Landed. */
	FVector LastVelocity = FVector::ZeroVector;

	/** Desired spring-arm length the boom smoothly follows. */
	float DesiredCameraArmLength = 260.f;
};
