// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeFableCharacter.h"
#include "SlimeCharacter.generated.h"

class UProceduralMeshComponent;
class USlimeAbilityComponent;
class USlimeBodyComponent;
class USlimeElementComponent;

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

	UFUNCTION(BlueprintPure, Category = "Slime")
	USlimeBodyComponent* GetSlimeBody() const { return SlimeBody; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	USlimeAbilityComponent* GetSlimeAbilities() const { return SlimeAbilities; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	USlimeElementComponent* GetSlimeElement() const { return SlimeElement; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	UProceduralMeshComponent* GetSurfaceMesh() const { return SurfaceMesh; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	UProceduralMeshComponent* GetShadowMesh() const { return ShadowMesh; }

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

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

	/** Cached before CMC clears vertical speed on Landed. */
	FVector LastVelocity = FVector::ZeroVector;
};
