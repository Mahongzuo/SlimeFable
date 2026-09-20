// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeMoodTypes.h"
#include "SlimeFaceComponent.generated.h"

class ACharacter;
class UMaterialInstanceDynamic;
class USlimeBodyComponent;
class USlimeClingComponent;
class USlimeElementComponent;
class USlimeHealthComponent;

/**
 *  Face state driver. Mood / blink / element ink live here; the strokes themselves are
 *  drawn inside the body material by orthogonal projection onto the jelly's front surface.
 */
UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API USlimeFaceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeFaceComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Face", meta = (ClampMin = "10.0", ClampMax = "300.0",
		ToolTip = "水平速度超过此值视为在走（cm/s）。默认 40。"))
	float MoveSpeedThreshold = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Face", meta = (ClampMin = "0.2", ClampMax = "6.0",
		ToolTip = "转向弹簧频率（Hz）。越大转得越快。默认 2.2。"))
	float TurnFrequency = 2.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Face", meta = (ClampMin = "0.3", ClampMax = "1.5",
		ToolTip = "转向阻尼比。1 = 临界阻尼不回弹。默认 0.8。"))
	float TurnDamping = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Face", meta = (ClampMin = "2.0", ClampMax = "40.0",
		ToolTip = "椭球最短半轴小于此值（cm）时开始隐藏脸。默认 12。"))
	float HideAxisThreshold = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Face", meta = (ClampMin = "0.0", ClampMax = "1.0",
		ToolTip = "挤压程度超过此值开始隐藏脸。默认 0.75。"))
	float HideSqueezeThreshold = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Face", meta = (ClampMin = "8.0", ClampMax = "64.0",
		ToolTip = "脸半宽对应的参考像素。眼睛在 ±6.5，40 ≈ 半径 16%。默认 40。"))
	float FaceHalfPx = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Face|Eyes", meta = (ClampMin = "0.4", ClampMax = "12.0",
		ToolTip = "Idle/走路 眼睛半高（参考像素）。调大 + 半宽调小 = 细长竖椭圆。默认 3.8。"))
	float IdleEyeH = 3.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Face|Eyes", meta = (ClampMin = "0.4", ClampMax = "12.0",
		ToolTip = "Idle/走路 眼睛半宽（参考像素）。小于半高就是竖椭圆。默认 3.2。"))
	float IdleEyeW = 3.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Face|Eyes", meta = (ClampMin = "0.2", ClampMax = "3.0",
		ToolTip = "所有椭圆眼的高度倍率（跳/受伤等也乘）。默认 1。"))
	float EyeHScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Face|Eyes", meta = (ClampMin = "0.2", ClampMax = "3.0",
		ToolTip = "所有椭圆眼的宽度倍率。默认 1。"))
	float EyeWScale = 1.f;

	UFUNCTION(BlueprintCallable, Category = "Slime|Face")
	void PulseMood(ESlimeMood Mood, float Duration, int32 Priority);

	/** Closed line-eyes + pink 3 mouth. Dodge / blink / later poop. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Face")
	void PulseBliss(float Duration = 0.5f);

	/** Angry brows + tight eyes. Kill / devour. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Face")
	void PulseWicked(float Duration = 1.2f);

	UFUNCTION(BlueprintCallable, Category = "Slime|Face")
	void SetFaceSuppressed(bool bSuppressed);

	UFUNCTION(BlueprintPure, Category = "Slime|Face")
	bool IsFaceSuppressed() const { return bSuppressed; }

	void TriggerBlink();
	void NotifyLanded(float ImpactSpeed);

	UFUNCTION(BlueprintPure, Category = "Slime|Face")
	ESlimeMood GetCurrentMood() const { return Mood.Id; }

private:
	void UpdateSense(float DeltaTime);
	void UpdateOrientation(float DeltaTime);
	void UpdateBlink(float DeltaTime);
	void UpdateVisibility(float DeltaTime);
	void UpdateMaterial();
	UMaterialInstanceDynamic* GetBodyMID() const;

	UFUNCTION()
	void HandleHealthChanged(float CurrentHP, float MaxHP);

	UFUNCTION()
	void HandleDied();

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<USlimeBodyComponent> Body;

	UPROPERTY(Transient)
	TObjectPtr<USlimeElementComponent> Element;

	UPROPERTY(Transient)
	TObjectPtr<USlimeClingComponent> Cling;

	UPROPERTY(Transient)
	TObjectPtr<USlimeHealthComponent> Health;

	FSlimeMoodDirector Mood;

	float FaceYaw = 0.f;
	float FaceYawVelocity = 0.f;
	bool bHasYaw = false;

	float BlinkTimer = 3.f;
	float BlinkPhase = 0.f;
	static constexpr float BlinkDuration = 0.12f;

	float HurtRemaining = 0.f;
	float LastHP = -1.f;

	float VisibleAlpha = 1.f;
	bool bSuppressed = false;
	bool bDead = false;

	FVector FaceForward = FVector::ForwardVector;
	FVector FaceUp = FVector::UpVector;
	float LookX = 0.f;
	float StillSeconds = 0.f;
};
