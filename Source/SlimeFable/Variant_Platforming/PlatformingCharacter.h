// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "PlatformingCharacter.generated.h"


class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;
class UAnimMontage;

/**
 *  An enhanced Third Person Character with the following functionality:
 *  - Platforming game character movement physics
 *  - Press and Hold Jump
 *  - Double Jump
 *  - Wall Jump
 *  - Dash
 */
UCLASS(abstract)
class APlatformingCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Dash Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* DashAction;

public:

	/** Constructor */
	APlatformingCharacter();

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called for dash input */
	void Dash();

	/** Called for jump pressed to check for advanced multi-jump conditions */
	void MultiJump();

	/** Resets the wall jump input lock */
	void ResetWallJump();

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles dash inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoDash();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

protected:

	/** Called from a delegate when the dash montage ends */
	void DashMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Passes control to Blueprint to enable or disable jump trails */
	UFUNCTION(BlueprintImplementableEvent, Category="Platforming")
	void SetJumpTrailState(bool bEnabled);

public:

	/** Ends the dash state */
	void EndDash();

public:

	/** Returns true if the character has just double jumped */
	UFUNCTION(BlueprintPure, Category="Platforming")
	bool HasDoubleJumped() const;

	/** Returns true if the character has just wall jumped */
	UFUNCTION(BlueprintPure, Category="Platforming")
	bool HasWallJumped() const;

public:	
	
	/** EndPlay cleanup */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Sets up input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Handle landings to reset dash and advanced jump state */
	virtual void Landed(const FHitResult& Hit) override;

	/** Handle movement mode changes to keep track of coyote time jumps */
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

protected:

	/** movement state flag bits, packed into a uint8 for memory efficiency */
	uint8 bHasWallJumped : 1;
	uint8 bHasDoubleJumped : 1;
	uint8 bHasDashed : 1;
	uint8 bIsDashing : 1;

	/** timer for wall jump input reset */
	FTimerHandle WallJumpTimer;

	/** Dash montage ended delegate */
	FOnMontageEnded OnDashMontageEnded;

	/** Distance to trace ahead of the character to look for walls to jump from */
	UPROPERTY(EditAnywhere, Category="Wall Jump", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float WallJumpTraceDistance = 50.0f;

	/** Radius of the wall jump sphere trace check */
	UPROPERTY(EditAnywhere, Category="Wall Jump", meta = (ClampMin = 0, ClampMax = 100, Units = "cm"))
	float WallJumpTraceRadius = 25.0f;

	/** Impulse to apply away from the wall when wall jumping */
	UPROPERTY(EditAnywhere, Category="Wall Jump", meta = (ClampMin = 0, ClampMax = 10000, Units = "cm/s"))
	float WallJumpBounceImpulse = 800.0f;

	/** Vertical impulse to apply when wall jumping */
	UPROPERTY(EditAnywhere, Category="Wall Jump", meta = (ClampMin = 0, ClampMax = 10000, Units = "cm/s"))
	float WallJumpVerticalImpulse = 900.0f;

	/** Time to ignore jump inputs after a wall jump */
	UPROPERTY(EditAnywhere, Category="Wall Jump", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float DelayBetweenWallJumps = 0.1f;

	/** AnimMontage to use for the Dash action */
	UPROPERTY(EditAnywhere, Category="Dash")
	UAnimMontage* DashMontage;

	/** Last recorded time when this character started falling */
	float LastFallTime = 0.0f;

	/** Max amount of time that can pass since we started falling when we allow a regular jump */
	UPROPERTY(EditAnywhere, Category="Coyote Time", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float MaxCoyoteTime = 0.16f;

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

};
