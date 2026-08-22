// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatDamageable.h"
#include "SlimeFableCharacter.h"
#include "SlimeCombatTypes.h"
#include "SlimeCharacter.generated.h"

class UProceduralMeshComponent;
class USlimeAbilityComponent;
class USlimeBodyComponent;
class USlimeClingComponent;
class USlimeCombatComponent;
class USlimeElementComponent;
class USlimeHealthComponent;
class USlimeLockOnComponent;
class USlimeStatusComponent;
class USlimeTrailComponent;
class USlimePlacementComponent;
class USlimeInteractComponent;
class USlimeDodgeComponent;
class USlimeVehicleComponent;
class USlimeDevourComponent;
class USlimeMorphComponent;
class UStaticMeshComponent;

/**
 *  The slime pawn.
 *
 *  Intentionally thin: movement stays on the stock CharacterMovementComponent and everything
 *  slime specific lives in components, so inventory, skills and animation can be added later
 *  by dropping in more components rather than editing this class.
 */
UCLASS()
class SLIMEFABLE_API ASlimeCharacter : public ASlimeFableCharacter, public ICombatDamageable
{
	GENERATED_BODY()

public:
	ASlimeCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyControllerChanged() override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnJumped_Implementation() override;
	virtual void Jump() override;
	virtual void DoMove(float Right, float Forward) override;

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
	USlimeHealthComponent* GetSlimeHealth() const { return SlimeHealth; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	USlimeStatusComponent* GetSlimeStatus() const { return SlimeStatus; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	USlimeCombatComponent* GetSlimeCombat() const { return SlimeCombat; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	USlimeLockOnComponent* GetSlimeLockOn() const { return SlimeLockOn; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	USlimeClingComponent* GetSlimeCling() const { return SlimeCling; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	USlimePlacementComponent* GetSlimePlacement() const { return SlimePlacement; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	USlimeInteractComponent* GetSlimeInteract() const { return SlimeInteract; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	USlimeDodgeComponent* GetSlimeDodge() const { return SlimeDodge; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	USlimeDevourComponent* GetSlimeDevour() const { return SlimeDevour; }

	UFUNCTION(BlueprintPure, Category = "Slime|Vehicle")
	USlimeVehicleComponent* GetSlimeVehicle() const { return SlimeVehicle; }

	UFUNCTION(BlueprintPure, Category = "Slime|Morph")
	USlimeMorphComponent* GetSlimeMorph() const { return SlimeMorph; }

	UFUNCTION(BlueprintPure, Category = "Slime|Vehicle")
	UStaticMeshComponent* GetVehicleMesh() const { return VehicleMesh; }

	/** Teleport back to this pawn's spawn and reset cling / body. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void Unstuck();

	UFUNCTION(BlueprintPure, Category = "Slime")
	UProceduralMeshComponent* GetSurfaceMesh() const { return SurfaceMesh; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	UProceduralMeshComponent* GetShadowMesh() const { return ShadowMesh; }

	UFUNCTION(BlueprintPure, Category = "Slime")
	UProceduralMeshComponent* GetXRayMesh() const { return XRayMesh; }

	/**
	 *  Parks the slime out of the world while the player drives a morph body: hides it, kills
	 *  its collision and movement, and — critically — stops the shadow proxy from casting.
	 *  ShadowMesh sets bCastHiddenShadow, so SetActorHiddenInGame alone still leaves a dark
	 *  puddle stamped on the ground where the morph started.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|Morph")
	void SetMorphParked(bool bParked);

	/** Applies one or more mouse-wheel zoom steps and returns the new target arm length. */
	float AdjustCameraZoom(int32 WheelSteps);
	float GetDesiredCameraArmLength() const { return DesiredCameraArmLength; }

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;
	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void HandleDeath() override;

	void FinishPlayerDeathReload();
	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void ApplyHealing(float Healing, AActor* Healer) override;
	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override;

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void UpdateCameraZoom(float DeltaSeconds);

	/** When move/jump keys are customized, drive CMC from SlimeInputSettings. */
	void PollCustomMoveKeys(float DeltaSeconds);

	/** Surface mesh. Vertices are world space, so this component sits at the world origin. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<UProceduralMeshComponent> SurfaceMesh;

	/**
	 *  Hidden opaque copy that only casts shadows — avoids translucent self-shadow speckles
	 *  while keeping a readable ground blob.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<UProceduralMeshComponent> ShadowMesh;

	/**
	 * Visible translucent copy with depth-test disabled; material shows warm gold only where
	 * the slime is occluded by world geometry (PixelDepth vs SceneDepth).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<UProceduralMeshComponent> XRayMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<USlimeBodyComponent> SlimeBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<USlimeAbilityComponent> SlimeAbilities;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<USlimeElementComponent> SlimeElement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<USlimeTrailComponent> SlimeTrail;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<USlimeHealthComponent> SlimeHealth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<USlimeStatusComponent> SlimeStatus;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<USlimeCombatComponent> SlimeCombat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<USlimeLockOnComponent> SlimeLockOn;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<USlimeClingComponent> SlimeCling;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<USlimePlacementComponent> SlimePlacement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<USlimeInteractComponent> SlimeInteract;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<USlimeDodgeComponent> SlimeDodge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<USlimeDevourComponent> SlimeDevour;

	/** Flyer mesh under the slime; hidden until mounted. Adjust transform in BP. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Vehicle")
	TObjectPtr<UStaticMeshComponent> VehicleMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Vehicle")
	TObjectPtr<USlimeVehicleComponent> SlimeVehicle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Morph")
	TObjectPtr<USlimeMorphComponent> SlimeMorph;

	/** Cached before CMC clears vertical speed on Landed. */
	FVector LastVelocity = FVector::ZeroVector;

	/** Desired spring-arm length the boom smoothly follows. */
	float DesiredCameraArmLength = 260.f;

	/** World transform at BeginPlay; Unstuck returns here. */
	FTransform SpawnTransform = FTransform::Identity;

	bool bPlayerDead = false;
	FTimerHandle PlayerDeathReloadTimer;
};
