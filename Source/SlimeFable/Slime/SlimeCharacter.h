// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatDamageable.h"
#include "FoliageInteractVolume.h"
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
class USlimeCheatComponent;
class USlimeDodgeComponent;
class USlimeVehicleComponent;
class USlimeDevourComponent;
class USlimeMorphComponent;
class USlimePathSwordComponent;
class USlimeFluidNinjaContactComponent;
class USlimeFoliageInteractComponent;
class UStaticMeshComponent;
class USoundBase;
class UAudioComponent;

/**
 *  The slime pawn.
 *
 *  Intentionally thin: movement stays on the stock CharacterMovementComponent and everything
 *  slime specific lives in components, so inventory, skills and animation can be added later
 *  by dropping in more components rather than editing this class.
 */
UCLASS()
class SLIMEFABLE_API ASlimeCharacter : public ASlimeFableCharacter, public ICombatDamageable,
	public IFoliageInteractVolume
{
	GENERATED_BODY()

public:
	ASlimeCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool GetFoliageInteractVolume(FVector& OutLocation, float& OutRadius) const override;
	virtual bool ShouldSuppressFoliageInteract() const override;

	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Camera",
		meta = (ClampMin = "-89.0", ClampMax = "0.0", ToolTip = "自由视角俯视下限。默认 -32。"))
	float ViewPitchMin = -32.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Camera",
		meta = (ClampMin = "0.0", ClampMax = "89.0", ToolTip = "自由视角仰视上限。默认 10。"))
	float ViewPitchMax = 10.f;

	/** Farthest zoom (longest arm), in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Camera", meta = (ClampMin = "100.0"))
	float CameraArmLengthMax = 520.f;

	/** Arm-length change per mouse-wheel notch, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Camera", meta = (ClampMin = "5.0"))
	float CameraZoomStep = 40.f;

	/** How quickly the boom eases toward the desired length (higher = snappier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Camera", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float CameraZoomInterpSpeed = 8.f;

	/** Hold Sprint key multiplier on MaxWalkSpeed. Default 1.5. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Move", meta = (ClampMin = "1.0", ClampMax = "3.0",
		ToolTip = "按住冲刺键时 MaxWalkSpeed 倍率。默认 1.5。"))
	float SprintSpeedMul = 1.5f;

	/** Base walk speed before sprint / status multipliers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Move", meta = (ClampMin = "50.0", Units = "cm/s",
		ToolTip = "基础走路速度（冲刺前）。默认 420。"))
	float BaseWalkSpeed = 420.f;

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

	UFUNCTION(BlueprintPure, Category = "Combat")
	USlimePathSwordComponent* GetPathSword() const { return PathSword; }

	UFUNCTION(BlueprintPure, Category = "Slime|Vehicle")
	UStaticMeshComponent* GetVehicleMesh() const { return VehicleMesh; }

	/** Soft-body crawl while moving. Empty → /Game/Audio/SFX/Movement/sfx_crawl_01. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Audio",
		meta = (ToolTip = "地面蠕动/黏软移动。空则 /Game/Audio/SFX/Movement/sfx_crawl_01。"))
	TSoftObjectPtr<USoundBase> FootstepSound;

	/** Jump launch SFX. Empty → /Game/Audio/SFX/Movement/sfx_jump_01. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Audio",
		meta = (ToolTip = "跳跃起跳。空则 /Game/Audio/SFX/Movement/sfx_jump_01。"))
	TSoftObjectPtr<USoundBase> JumpSound;

	/** Player hit-taken SFX. Empty → /Game/Audio/SFX/Combat/sfx_hit_01. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Audio",
		meta = (ToolTip = "被敌人打中。空则 /Game/Audio/SFX/Combat/sfx_hit_01。"))
	TSoftObjectPtr<USoundBase> HitTakenSound;

	/** Seconds between crawl one-shots while moving on ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Audio",
		meta = (ClampMin = "0.1", ClampMax = "1.5", ToolTip = "地面蠕动音间隔（秒）。默认 0.45。"))
	float FootstepInterval = 0.45f;

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

	/** Lock-on framing drives DesiredCameraArmLength; skips free-look socket lerp while active. */
	void SetLockOnFramingActive(bool bActive);
	bool IsLockOnFramingActive() const { return bLockOnFramingActive; }
	void SetLockOnFramingArm(float FramingFloorArm, float FramingMaxArm);
	void SetDesiredCameraArmLengthClamped(float Length);

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
	void ApplyCameraViewLimits();

	/** When move/jump keys are customized, drive CMC from SlimeInputSettings. */
	void PollCustomMoveKeys(float DeltaSeconds);
	void UpdateSprintSpeed();

	void TickFootsteps(float DeltaSeconds);
	void PlayJumpSound();
	void StopFootstepAudio(bool bImmediate = false);
	void PlayFootstepAudio();

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cheat")
	TObjectPtr<USlimeCheatComponent> SlimeCheat;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<USlimePathSwordComponent> PathSword;

	/** FluidNinja LIVE footprint / body contact proxies (WorldDynamic overlap spheres). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|FluidNinja")
	TObjectPtr<USlimeFluidNinjaContactComponent> SlimeFluidNinjaContact;

	/** Writes position/velocity into MPC_SlimeFoliage for interactive grass WPO. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Foliage")
	TObjectPtr<USlimeFoliageInteractComponent> SlimeFoliageInteract;

	/** Cached before CMC clears vertical speed on Landed. */
	FVector LastVelocity = FVector::ZeroVector;

	/** Desired spring-arm length the boom smoothly follows. */
	float DesiredCameraArmLength = 260.f;

	bool bLockOnFramingActive = false;
	float LockOnFramingFloorArm = 120.f;
	float LockOnFramingMaxArm = 420.f;

	/** Accumulator for ground footstep cadence. */
	float FootstepTimer = 0.f;
	bool bWasMovingForFootstep = false;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> FootstepAudio;

	/** World transform at BeginPlay; Unstuck returns here. */
	FTransform SpawnTransform = FTransform::Identity;

	bool bPlayerDead = false;
	FTimerHandle PlayerDeathReloadTimer;
};
