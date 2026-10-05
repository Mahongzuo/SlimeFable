// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeElementTypes.h"
#include "SlimeTypes.h"
#include "SlimeAbilityComponent.generated.h"

class APlayerController;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class USlimeBodyComponent;
class USlimeElementComponent;
class USlimeHotbarWheelWidget;
class USlimeElementFormationWidget;
class USlimeHotbarConfirmWidget;
class USlimeMorphComponent;
struct FInputActionValue;

/**
 *  Input and state machine for the four slime abilities plus the element wheel.
 *
 *  Owns no simulation state of its own: everything routes through the public API on
 *  USlimeBodyComponent and USlimeElementComponent.
 */
UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent, PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API USlimeAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeAbilityComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Called from the owning character's SetupPlayerInputComponent. */
	void BindInput(UEnhancedInputComponent* EnhancedInput);

	/** Called when the controller changes so the slime context is layered in. */
	void RegisterMappingContext();

	/** Remove slime ability IMC (e.g. while possessing a GASP morph that uses IMC_Sandbox). */
	void UnregisterMappingContext();

	// ---- Input assets, assigned on the character Blueprint ----------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputMappingContext> SlimeMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0"))
	int32 MappingPriority = 1;

	/** E: hold to pancake. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> FlattenAction;

	/** R: reset the body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> ResetAction;

	/** F: hold to absorb fragments. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> AbsorbAction;

	/** G: hold to aim, click attack to fire; release cancels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> LaunchAction;

	/** Tab: hold to open the hotbar wheel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> ElementWheelAction;

	/** Mouse wheel axis: steps hotbar selection while the wheel is open. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> ElementCycleAction;

	// ---- Launch ----------------------------------------------------------------------
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Launch", meta=(ClampMin="100", ToolTip="炮弹默认初速度，单位cm/s，默认2800；低弧弹道补偿重力"))
 float CannonSpeed = 2800.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Launch", meta=(ClampMin="100", ToolTip="不可达时允许提升的最高弹速，默认3600cm/s，与默认弹速联动"))
 float CannonMaxSpeed = 3600.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Launch", meta=(ClampMin="1", ToolTip="炮弹向下重力，默认980cm/s²；预测与飞行共用，不使用身体阻尼"))
 float CannonGravity = 980.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Launch", meta=(ClampMin="100", ToolTip="中心准星射线最远距离，默认4000cm，即40米"))
 float CannonAimDistance = 4000.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Launch", meta=(ClampMin="0.01", ToolTip="单次点击发射间隔，默认0.35秒；按住攻击不连射"))
 float CannonFireInterval = 0.35f;

 bool IsAimingLaunch() const { return bCharging && CanBeginLaunchAim(); }
 bool CanFireLaunch() const;
 bool IsLaunchPathBlocked() const { return bAimBlocked || bSeparationBlocked; }
 bool IsLaunchSeparationBlocked() const { return bSeparationBlocked; }
 FVector GetLaunchMuzzle() const { return AimMuzzle; }
 FVector2D GetLaunchScreenCenter() const;
 FVector GetLaunchAimTarget() const { return CannonAim.Target; }
 void BeginLaunchAim();
 void CancelLaunchAim();
 bool TryFireAimedLaunch();
 /** Returns true when attack belongs to aiming, even if cooldown prevents a shot. */
 bool ConsumeLaunchFireInput();


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Launch", meta = (ClampMin = "100.0"))
	float MinLaunchSpeed = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Launch", meta = (ClampMin = "100.0"))
	float MaxLaunchSpeed = 1700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Launch", meta = (ClampMin = "0.05", ClampMax = "4.0"))
	float FullChargeTime = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Launch", meta = (ClampMin = "100.0", Units = "cm"))
	float MinLaunchRange = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Launch", meta = (ClampMin = "200.0", Units = "cm"))
	float MaxLaunchRange = 2800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Launch", meta = (ClampMin = "500.0", Units = "cm"))
	float LaunchAimRange = 10000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Launch", meta = (ClampMin = "40.0", Units = "cm"))
	float DefaultLaunchArcHeight = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Launch", meta = (ClampMin = "200.0", Units = "cm"))
	float DefaultLaunchRange = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Launch", meta = (ClampMin = "20.0", Units = "cm"))
	float LaunchRangeStep = 150.f;

	/** Upward bias added to the aim direction so a flat aim still arcs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Launch", meta = (ClampMin = "0.0", ClampMax = "0.6"))
	float LaunchUpwardBias = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Launch")
	bool bDrawTrajectoryPreview = true;

	// ---- Hotbar wheel (Tab) ----------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Wheel")
	TSubclassOf<USlimeHotbarWheelWidget> WheelWidgetClass;

	/** Optional Blueprint shell for the wheel, used when WheelWidgetClass is unset. */
	UPROPERTY(EditAnywhere, Category = "Slime|Wheel")
	TSoftClassPtr<USlimeHotbarWheelWidget> WheelWidgetClassPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Formation")
	TSubclassOf<USlimeElementFormationWidget> FormationWidgetClass;

	/** Minimum gap between wheel steps so one flick of the wheel moves exactly one slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Wheel", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float CycleCooldown = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Wheel")
	bool bSlowTimeWhileWheelOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Wheel", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float WheelTimeDilation = 0.3f;

	// ---- Queries ---------------------------------------------------------------------

	void CloseFormation();
	void CloseHotbarConfirm();
	void TrySwitchOrderedElement(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool IsChargingLaunch() const { return IsAimingLaunch(); }

	UFUNCTION(BlueprintPure, Category = "Slime")
	float GetLaunchCharge() const;

	UFUNCTION(BlueprintPure, Category = "Slime")
	FLinearColor GetLaunchPreviewColor() const;

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool IsWheelOpen() const { return bWheelOpen; }

	/**
	 *  When true (default), E/R/F/Q/Tab are driven from Tick key polling like the SIM reference,
	 *  so other mapping contexts cannot swallow them. Enhanced Input bindings stay as a backup
	 *  and no-op while polling is active.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bPollAbilityKeys = true;

private:
	void HandleFlattenStarted();
	void HandleFlattenCompleted();
	void HandleResetTriggered();
	void HandleAbsorbStarted();
	void HandleAbsorbCompleted();
	void HandleLaunchStarted();
	void HandleLaunchCompleted();
	void HandleWheelStarted();
	void HandleWheelCompleted();
	void HandleCycle(const FInputActionValue& Value);

	/** SIM-style authoritative path: poll raw keys so IMC conflicts cannot mute abilities. */
	void PollAbilityKeys(float DeltaTime);

	void OpenWheel();
	void CloseWheel(bool bCommit);
	void OpenFormation();
	void OpenHotbarConfirm(int32 SlotIndex);
	bool CanBeginLaunchAim() const;
	void UpdateLaunchAim();
	void BeginLaunchCharge();
	void ReleaseLaunchCharge();
	void AdjustLaunchRange(int32 Step);
	bool BuildLaunchPath(FSlimeLaunchPath& OutPath) const;
	bool ResolveLaunchTarget(FVector& OutStart, FVector& OutTarget) const;
	FVector SimulateLaunchTrajectory(const FVector& Start, const FVector& LaunchVelocity, TArray<FVector>& OutPoints) const;
	void DrawLaunchPath(const FSlimeLaunchPath& Path) const;
	bool GetAimDirection(FVector& OutDirection) const;
	APlayerController* GetOwningPlayerController() const;

	UPROPERTY(Transient)
	TObjectPtr<USlimeBodyComponent> Body;

	UPROPERTY(Transient)
	TObjectPtr<USlimeElementComponent> Element;

	UPROPERTY(Transient)
	TObjectPtr<USlimeHotbarWheelWidget> WheelWidget;

	UPROPERTY(Transient)
	TObjectPtr<USlimeElementFormationWidget> FormationWidget;

	UPROPERTY(Transient)
	TObjectPtr<USlimeHotbarConfirmWidget> HotbarConfirmWidget;

 FVector AimMuzzle = FVector::ZeroVector;
 bool bSeparationBlocked = false;
 FSlimeCannonLaunch CannonAim;
 bool bAimReachable = false, bAimBlocked = false, bAimSuppressedUntilRelease = false;
 float CannonCooldown = 0.f;
 uint64 LastFireFrame = MAX_uint64;
 FSlimeLaunchPath PendingLaunchPath;
	float LaunchExtraArcHeight = 80.f;
	float LaunchRange = 1200.f;
	float ChargeElapsed = 0.f;
	float CycleCooldownRemaining = 0.f;
	float SavedTimeDilation = 1.f;

	bool bCharging = false;
	bool bWheelOpen = false;
	int32 HotbarWheelSlot = 0;

	/** Edge tracking for Tick polling (mirrors hold/release abilities). */
	bool bPollFlattenDown = false;
	bool bPollAbsorbDown = false;
	bool bPollLaunchDown = false;
	bool bPollLaunchKey = true;
	bool bGamepadALaunchArmed = false;
	float GamepadAHoldSeconds = 0.f;
	int32 GamepadElementSlot = 0;
	bool bPollWheelDown = false;
	bool bPollMorphDown = false;
	float MorphHoldSeconds = 0.f;
	bool bMorphWheelOpenedThisHold = false;
	bool bLoggedMissingMappingContext = false;
};
