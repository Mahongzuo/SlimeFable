// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeMoodTypes.generated.h"

/**
 *  Face moods, ported from the 2D reference (src/face/mood.ts). Order is stable: the face
 *  material receives the mouth shape as an int.
 */
UENUM(BlueprintType)
enum class ESlimeMood : uint8
{
	Idle,
	Move,
	Jump,
	Attack,
	Hurt,
	Happy,
	Spit,
	Focus,
	/** Landing squash: brief squint. Not in the 2D set. */
	Squash,
	/** Closed line-eyes + small 3 mouth. Dodge / blink / later poop. */
	Bliss,
	/** 10s idle: closed eyes + blush + small 3. */
	Sleep,
	/** Kill / devour: angry brows, tight eyes. */
	Wicked
};

UENUM(BlueprintType)
enum class ESlimeMouth : uint8
{
	Smile,
	Flat,
	O,
	Frown,
	Grin,
	Squint,
	/** Tiny 3 / ω. Used by Bliss. */
	Pucker
};

/** One authored face pose. Units are 2D reference pixels; the material rescales. */
USTRUCT(BlueprintType)
struct SLIMEFABLE_API FSlimeFacePose
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	ESlimeMood Id = ESlimeMood::Idle;

	/** Eye half height (ref px). Blink drives this toward ~0.55. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	float EyeH = 4.1f;

	/** Eye half width (ref px). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	float EyeW = 2.7f;

	/** Brow tilt: + raises the outer end, - lowers it (angry). |brow| < 0.05 hides brows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	float Brow = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	ESlimeMouth Mouth = ESlimeMouth::Smile;

	/** Smile curve depth (ref px). Only used by Smile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	float Curve = 8.f;

	/** 0..1 blush strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	float Blush = 0.f;

	/** Pupil horizontal look offset (ref px, ±2). Written per frame, not authored. */
	UPROPERTY(BlueprintReadOnly, Category = "Face")
	float Look = 0.f;

	static FSlimeFacePose ForMood(ESlimeMood Mood);
};

/**
 *  Priority stack for moods. pulse() only overrides a lower-priority pulse that has not
 *  expired; once expired the mood falls back to the sensed state (hurt > jump > move > idle).
 *  Plain struct: owned by USlimeFaceComponent, no reflection needed on the hot path.
 */
struct SLIMEFABLE_API FSlimeMoodDirector
{
	struct FSense
	{
		/** |horizontal speed| normalised 0..1 (0.2 = move threshold). */
		float Move = 0.f;
		bool bGround = true;
		/** > 0 while hurt (invulnerability / hit flash window). */
		float Hurt = 0.f;
		/** Seconds spent still on the ground. Sleep kicks in at 10. */
		float Still = 0.f;
	};

	ESlimeMood Id = ESlimeMood::Idle;
	float Until = 0.f;
	int32 Priority = 0;
	float Time = 0.f;

	/** Ignored if a higher-priority pulse is still running. */
	void Pulse(ESlimeMood InId, float Duration, int32 InPriority);

	void Tick(float Dt, const FSense& Sense);

	FSlimeFacePose Pose(float Look = 0.f) const;
};
