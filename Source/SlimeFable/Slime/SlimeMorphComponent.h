// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeMorphComponent.generated.h"

class AEnemyCharacter;
class USlimeBodyComponent;
class USlimeElementComponent;
class USlimeDevourComponent;
class USlimeDodgeComponent;
class USlimePhantomWheelWidget;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USkeletalMeshComponent;
class UMeshComponent;
class USpringArmComponent;

/** Per-mesh morph visual state (primary + GeneratedParts). */
USTRUCT()
struct FSlimeMorphMeshVisual
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> Mesh = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> SavedMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> MorphMIDs;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ShellMID = nullptr;

	/** True if this is an extra part (not the character primary mesh). */
	bool bExtraPart = false;

	/**
	 * Morph transition wears Substrate slime as base skin on every slot (no DefaultLit overlay).
	 * OverlayMaterial cannot cover Hair / Substrate Toon / extra parts cleanly.
	 */
	bool bBaseSkinMorphPath = false;

	/** Material slot indices whose sections were hidden after a failed slime-skin swap. */
	TArray<int32> HiddenMaterialSlots;
};

UENUM(BlueprintType)
enum class ESlimeMorphPhase : uint8
{
	Idle,
	Spreading,
	Growing,
	Blending,
	Morphed,
	Unblending,
	Shrinking,
	Reforming
};

/**
 *  Drives the slime → enemy morph sequence: spread → grow mask → material blend → possess.
 *
 *  Reuses USlimeBodyComponent::SetSpread for the pancake, reads a capture from
 *  USlimeDevourComponent::PhantomSlots, and swaps the player controller between the slime
 *  and the spawned enemy pawn. The enemy's own UEnemyCombatComponent handles attack
 *  execution; this component only routes player combat keys onto it.
 */
UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API USlimeMorphComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeMorphComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Toggle between morph (slime→enemy) and unmorph (enemy→slime). No-op mid-sequence. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Morph")
	void ToggleMorph();

	/**
	 *  Force the morph body back to the slime. Called by the enemy's death handler when the
	 *  morph body is killed. When bConsumeSlot is true, the phantom slot is consumed so the
	 *  same enemy cannot be morphed again this day.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|Morph")
	void ForceUnmorph(bool bConsumeSlot);

	UFUNCTION(BlueprintPure, Category = "Slime|Morph")
	bool IsMorphed() const { return Phase == ESlimeMorphPhase::Morphed; }

	UFUNCTION(BlueprintPure, Category = "Slime|Morph")
	bool IsMorphing() const { return Phase != ESlimeMorphPhase::Idle && Phase != ESlimeMorphPhase::Morphed; }

	UFUNCTION(BlueprintPure, Category = "Slime|Morph")
	ESlimeMorphPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "Slime|Morph")
	AEnemyCharacter* GetMorphTarget() const { return MorphTarget; }

	/** Morph target selection wheel (hold Z to open, release to commit). */
	UFUNCTION(BlueprintCallable, Category = "Slime|Morph|Wheel")
	bool TryOpenMorphWheel();

	UFUNCTION(BlueprintCallable, Category = "Slime|Morph|Wheel")
	void CloseMorphWheel(bool bCommit);

	UFUNCTION(BlueprintCallable, Category = "Slime|Morph|Wheel")
	void TickMorphWheelInput();

	UFUNCTION(BlueprintPure, Category = "Slime|Morph|Wheel")
	bool IsMorphWheelOpen() const { return bMorphWheelOpen; }

	// ---- Phase durations (morph = 1.5s total, unmorph = 1.5s total) -----------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Morph|Timing", meta = (ClampMin = "0.05", Units = "s"))
	float SpreadDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Morph|Timing", meta = (ClampMin = "0.05", Units = "s"))
	float GrowDuration = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Morph|Timing", meta = (ClampMin = "0.05", Units = "s"))
	float BlendDuration = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Morph|Timing", meta = (ClampMin = "0.05", Units = "s"))
	float UnblendDuration = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Morph|Timing", meta = (ClampMin = "0.05", Units = "s"))
	float ShrinkDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Morph|Timing", meta = (ClampMin = "0.05", Units = "s"))
	float ReformDuration = 0.6f;

	/** Possess the enemy at this fraction through the Blending phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Morph|Timing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlendPossessAlpha = 0.5f;

	/** Possess the slime back at this fraction through the Unblending phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Morph|Timing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UnblendUnpossessAlpha = 0.5f;

	/** Smooth spring-arm framing after slime↔morph possess (SocketZ / arm / TargetOffset). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Morph|Timing",
		meta = (ClampMin = "0.05", Units = "s",
			ToolTip = "幻形/解除幻形 Possess 后，机位从当前值平滑插到目标 framing 的时长。默认 0.45 秒。"))
	float CameraBlendDuration = 0.45f;

	/**
	 *  Soft edge width at the grow front, as a fraction of the model's height (not cm) — the
	 *  reveal mask lives in object space so it tracks the actor wherever it walks or jumps.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Morph|Visual", meta = (ClampMin = "0.01", ClampMax = "0.5"))
	float GrowEdgeSoftness = 0.08f;

	/** Hold Z this long before the morph wheel opens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Morph|Wheel", meta = (ClampMin = "0.05", Units = "s"))
	float MorphWheelHoldSeconds = 0.25f;

	/** Slow time while the morph wheel is open (same feel as the phantom wheel). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Morph|Wheel")
	bool bSlowTimeWhileWheelOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Morph|Wheel", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float WheelTimeDilation = 0.3f;

private:
	void BeginMorph();
	void BeginUnmorph();
	void EnterPhase(ESlimeMorphPhase Next);
	void TickPhase(float Dt);

	void SpawnMorphTarget();
	void DestroyMorphTarget();

	/**
	 *  Pushes the reveal/shell parameters onto every live M_SlimeMorph instance.
	 *  GrowProgress 0→1 reveals the model bottom-up in object space; ShellOpacity scales the
	 *  slime look so the enemy's real materials can fade in underneath.
	 */
	void UpdateMorphMaterial(float GrowProgress, float ShellOpacity);
	void UpdateSlimeOpacity(float Alpha);
	void PossessEnemy();
	void PossessSlime();
	void EnsureMorphDodge();
	void ClearMorphDodge();
	/** Ease SocketOffset / TargetArmLength / TargetOffset after possess. */
	void TickCameraBlend(float DeltaTime);
	void BeginCameraBlend(USpringArmComponent* Boom, float TargetArm, float TargetSocketZ, const FVector& TargetOffset);
	void SetSlimeMovementEnabled(bool bEnabled);
	void SetMorphTargetGameplayEnabled(bool bEnabled);
	void CacheUnmorphPoseAndFreezeTarget();
	void ConsumeMorphedSlotIfRequested();
	void SyncElementProfileToMorphMaterial();

	/** Mesh slots -> slime-skin MIDs (Substrate Slab on every visual mesh). */
	void ApplySlimeSkin();

	/** Mesh slots → the enemy's own materials, so the final look is always correct. */
	void ApplyOriginalMaterials();

	/** Toggles the M_SlimeMorph overlay that renders the slime shell on top of the real materials. */
	void SetShellActive(bool bActive);

	/** Loads M_SlimeMorph (translucent overlay) on demand and caches it. */
	UMaterialInterface* LoadMorphMaterial();

	/** Loads M_SlimeMorph_Grow (masked DefaultLit clip) on demand and caches it. */
	UMaterialInterface* LoadMorphGrowMaterial();

	/** Loads M_SlimeMorph_Substrate (Substrate Slab slime skin) on demand and caches it. */
	UMaterialInterface* LoadMorphSubstrateMaterial();

	/** Loads M_SlimeMorph_Hair (Masked MSM_HAIR slime skin) on demand and caches it. */
	UMaterialInterface* LoadMorphHairMaterial();

	/** True if any saved material is Substrate/Toon and should skip Overlay. */
	static bool MaterialNeedsBaseSkinMorphPath(UMaterialInterface* Mat);

	/** Hide / restore skeletal sections when a slot refused the slime MID. */
	void HideFailedMorphSlots(FSlimeMorphMeshVisual& Entry);
	void RestoreHiddenMorphSlots(FSlimeMorphMeshVisual& Entry);
	void RestoreAllHiddenMorphSlots();

	/** True if any morph visual uses the base-skin (no-overlay) path. */
	bool UsesBaseSkinMorphPath() const;

	/** Drives walk/idle montages on the morph target based on movement speed (for single-node-anim enemies). */
	void TickMorphLocomotion(float Dt);

	/** Polls the morph key while morphed so a tap unmorphs — the slime has no controller then. */
	void TickMorphedKeyInput(float DeltaTime);

	/** The morph target's controller while morphed, otherwise the owner's. */
	APlayerController* GetActivePlayerController() const;

	UPROPERTY(Transient)
	TObjectPtr<USlimeBodyComponent> Body;

	UPROPERTY(Transient)
	TObjectPtr<USlimeElementComponent> Element;

	UPROPERTY(Transient)
	TObjectPtr<USlimeDevourComponent> Devour;

	UPROPERTY(Transient)
	TObjectPtr<AEnemyCharacter> MorphTarget;

	/** Dodge component created on MorphTarget while player-possessed. */
	UPROPERTY(Transient)
	TObjectPtr<USlimeDodgeComponent> MorphDodge;

	/** Original materials / morph MIDs / shells for every visual mesh. */
	UPROPERTY(Transient)
	TArray<FSlimeMorphMeshVisual> MorphVisuals;

	/** Cached M_SlimeMorph material (translucent overlay for DefaultLit). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> MorphMaterial;

	/** Cached M_SlimeMorph_Grow (masked DefaultLit clip). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> MorphGrowMaterial;

	/** Cached M_SlimeMorph_Substrate (Substrate Slab slime skin). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> MorphSubstrateMaterial;

	/** Cached M_SlimeMorph_Hair (Masked MSM_HAIR slime skin). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> MorphHairMaterial;

	/** Hides GeneratedParts while grow mask is incomplete (avoids hair floating on slime). */
	void SetExtraPartsHidden(bool bHidden);

	ESlimeMorphPhase Phase = ESlimeMorphPhase::Idle;
	float PhaseElapsed = 0.f;
	bool bPossessDone = false;
	/** True once Blending has handed the mesh back to the enemy's own materials. */
	bool bOriginalMaterialsActive = false;
	bool bShellActive = false;
	bool bMorphedKeyDown = false;
	int32 MorphedSlotIndex = INDEX_NONE;
	bool bConsumeMorphedSlotOnExit = false;
	bool bHasCachedSlimeReturnTransform = false;
	FTransform CachedSlimeReturnTransform = FTransform::Identity;
	bool bHasCachedMorphTargetGameplayState = false;
	bool bHasCachedMorphTargetMeshCollisionState = false;
	uint8 CachedMorphTargetMeshCollisionEnabled = 0;
	uint8 CachedMorphTargetMovementMode = 0;
	uint8 CachedMorphTargetCustomMovementMode = 0;

	// Locomotion state for single-node-anim enemies.
	bool bMorphWalkPlaying = false;
	bool bMorphRunPlaying = false;
	bool bMorphJumpPlaying = false;
	float MorphIdleTimer = 0.f;

	/** Cached morph camera framing (tall bodies need higher SocketOffset / longer arm). */
	float MorphCameraHeightScale = 1.f;
	float MorphCameraSocketZ = 12.f;

	/** Active spring-arm framing blend after possess. */
	bool bCameraBlendActive = false;
	float CameraBlendElapsed = 0.f;
	TWeakObjectPtr<USpringArmComponent> CameraBlendBoom;
	float CameraBlendStartArm = 0.f;
	float CameraBlendTargetArm = 0.f;
	float CameraBlendStartSocketZ = 0.f;
	float CameraBlendTargetSocketZ = 0.f;
	FVector CameraBlendStartOffset = FVector::ZeroVector;
	FVector CameraBlendTargetOffset = FVector::ZeroVector;

	// Morph wheel state.
	bool bMorphWheelOpen = false;
	float SavedTimeDilation = 1.f;
	UPROPERTY(Transient)
	TObjectPtr<USlimePhantomWheelWidget> MorphWheelWidget;
};
