// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeMoodTypes.h"

FSlimeFacePose FSlimeFacePose::ForMood(ESlimeMood Mood)
{
	// Values are the 2D reference POSES table verbatim.
	FSlimeFacePose P;
	P.Id = Mood;
	switch (Mood)
	{
	case ESlimeMood::Idle:   P.EyeH = 3.8f; P.EyeW = 3.2f; P.Brow = 0.f;   P.Mouth = ESlimeMouth::Smile; P.Curve = 10.5f; P.Blush = 0.f;  break;
	case ESlimeMood::Move:   P.EyeH = 3.6f; P.EyeW = 3.1f; P.Brow = 0.15f; P.Mouth = ESlimeMouth::Smile; P.Curve = 10.f;  P.Blush = 0.f;  break;
	case ESlimeMood::Jump:   P.EyeH = 4.6f; P.EyeW = 2.5f; P.Brow = 0.4f;  P.Mouth = ESlimeMouth::O;     P.Curve = 2.f;  P.Blush = 0.f;   break;
	case ESlimeMood::Attack: P.EyeH = 2.4f; P.EyeW = 3.2f; P.Brow = -0.8f; P.Mouth = ESlimeMouth::Grin;  P.Curve = 3.f;  P.Blush = 0.15f; break;
	case ESlimeMood::Hurt:   P.EyeH = 1.4f; P.EyeW = 3.4f; P.Brow = -0.4f; P.Mouth = ESlimeMouth::Frown; P.Curve = 2.f;  P.Blush = 0.4f;  break;
	case ESlimeMood::Happy:  P.EyeH = 1.1f; P.EyeW = 3.2f; P.Brow = 0.7f;  P.Mouth = ESlimeMouth::Grin;  P.Curve = 10.f; P.Blush = 0.35f; break;
	case ESlimeMood::Spit:   P.EyeH = 4.8f; P.EyeW = 2.4f; P.Brow = 0.3f;  P.Mouth = ESlimeMouth::O;     P.Curve = 1.f;  P.Blush = 0.f;   break;
	case ESlimeMood::Focus:  P.EyeH = 3.2f; P.EyeW = 2.8f; P.Brow = -0.3f; P.Mouth = ESlimeMouth::Flat;  P.Curve = 5.f;  P.Blush = 0.f;   break;
	case ESlimeMood::Squash: P.EyeH = 1.0f; P.EyeW = 3.4f; P.Brow = 0.1f;  P.Mouth = ESlimeMouth::Squint; P.Curve = 5.f; P.Blush = 0.f;   break;
	case ESlimeMood::Bliss:  P.EyeH = 0.7f; P.EyeW = 2.8f; P.Brow = 0.f;   P.Mouth = ESlimeMouth::Pucker; P.Curve = 2.f;  P.Blush = 0.f;   break;
	case ESlimeMood::Sleep:  P.EyeH = 0.65f; P.EyeW = 3.1f; P.Brow = 0.f;  P.Mouth = ESlimeMouth::Pucker; P.Curve = 2.f;  P.Blush = 0.45f; break;
	case ESlimeMood::Wicked: P.EyeH = 1.6f; P.EyeW = 2.3f; P.Brow = -1.15f; P.Mouth = ESlimeMouth::Flat; P.Curve = 3.f;  P.Blush = 0.f;   break;
	}
	return P;
}

void FSlimeMoodDirector::Pulse(ESlimeMood InId, float Duration, int32 InPriority)
{
	if (InPriority < Priority && Time < Until)
	{
		return;
	}
	Id = InId;
	Until = Time + Duration;
	Priority = InPriority;
}

void FSlimeMoodDirector::Tick(float Dt, const FSense& Sense)
{
	Time += Dt;
	if (Sense.Hurt > 0.f)
	{
		Pulse(ESlimeMood::Hurt, 0.12f, 90);
	}
	if (Time >= Until)
	{
		Priority = 0;
		Id = Sense.Hurt > 0.f ? ESlimeMood::Hurt
			: !Sense.bGround ? ESlimeMood::Jump
			: FMath::Abs(Sense.Move) > 0.2f ? ESlimeMood::Move
			: Sense.Still >= 10.f ? ESlimeMood::Sleep
			: ESlimeMood::Idle;
	}
}

FSlimeFacePose FSlimeMoodDirector::Pose(float Look) const
{
	FSlimeFacePose P = FSlimeFacePose::ForMood(Id);
	P.Look = Look;
	return P;
}
