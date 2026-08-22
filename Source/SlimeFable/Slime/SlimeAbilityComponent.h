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
class USlimeElementWheelWidget;
class USlimeMorphComponent;
struct FInputActionValue;

/**
 *  Input and state machine for the four slime abilities plus the element wheel.
 *
 *  Owns no simulation state of its own: everything routes through the public API on
 *  USlimeBodyComponent and USlimeElementComponent.
 */
UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
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

	/** G: hold to aim, release to throw a chunk. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> LaunchAction;

	/** Tab: hold to open the element wheel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> ElementWheelAction;

	/** Mouse wheel axis: steps the element selection while the wheel is open. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> ElementCycleAction;

	// ---- Launch ----------------------------------------------------------------------

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

	// ---- Element wheel ---------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Wheel")
	TSubclassOf<USlimeElementWheelWidget> WheelWidgetClass;

	/** Optional Blueprint shell for the wheel, used when WheelWidgetClass is unset. */
	UPROPERTY(EditAnywhere, Category = "Slime|Wheel")
	TSoftClassPtr<USlimeElementWheelWidget> WheelWidgetClassPath;

	/** Minimum gap between wheel steps so one flick of the wheel moves exactly one slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Wheel", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float CycleCooldown = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Wheel")
	bool bSlowTimeWhileWheelOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Wheel", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float WheelTimeDilation = 0.3f;

	// ---- Queries ---------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool IsChargingLaunch() const { return bCharging; }

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
	TObjectPtr<USlimeElementWheelWidget> WheelWidget;

	FSlimeLaunchPath PendingLaunchPath;
	float LaunchExtraArcHeight = 80.f;
	float LaunchRange = 1200.f;
	float ChargeElapsed = 0.f;
	float CycleCooldownRemaining = 0.f;
	float SavedTimeDilation = 1.f;

	bool bCharging = false;
	bool bWheelOpen = false;

	/** Edge tracking for Tick polling (mirrors hold/release abilities). */
	bool bPollFlattenDown = false;
	bool bPollAbsorbDown = false;
	bool bPollLaunchDown = false;
	bool bPollWheelDown = false;
	bool bPollMorphDown = false;
	float MorphHoldSeconds = 0.f;
	bool bMorphWheelOpenedThisHold = false;
	bool bLoggedMissingMappingContext = false;
};
