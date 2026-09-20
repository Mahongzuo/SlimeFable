// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeFaceComponent.h"

#include "Combat/SlimeHealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "SlimeBodyComponent.h"
#include "SlimeClingComponent.h"
#include "SlimeElementComponent.h"
#include "SlimeSolver.h"

namespace SlimeFaceParams
{
	static const FName FaceForward(TEXT("FaceForward"));
	static const FName FaceUp(TEXT("FaceUp"));
	static const FName FaceInk(TEXT("FaceInk"));
	static const FName FaceShine(TEXT("FaceShine"));
	static const FName FaceMouth(TEXT("FaceMouth"));
	static const FName FaceBlushCol(TEXT("FaceBlushCol"));
	static const FName FaceEyeH(TEXT("FaceEyeH"));
	static const FName FaceEyeW(TEXT("FaceEyeW"));
	static const FName FaceBrow(TEXT("FaceBrow"));
	static const FName FaceMouthKind(TEXT("FaceMouthKind"));
	static const FName FaceCurve(TEXT("FaceCurve"));
	static const FName FaceBlush(TEXT("FaceBlush"));
	static const FName FaceLookX(TEXT("FaceLookX"));
	static const FName FaceVisible(TEXT("FaceVisible"));
	static const FName FaceHalfPx(TEXT("FaceHalfPx"));
	// ShotCenter0..4 are written by USlimeBodyComponent (shared slot table with the vertex colours).
	static const FName ShotForward[5] = {
		TEXT("ShotForward0"), TEXT("ShotForward1"), TEXT("ShotForward2"),
		TEXT("ShotForward3"), TEXT("ShotForward4")
	};
	static constexpr int32 MaxShotFaces = 5;
}

namespace SlimeFacePrivate
{
	static void SpringAngle(float& Angle, float& Velocity, float Target, float Frequency, float Damping, float Dt)
	{
		const float Delta = FMath::FindDeltaAngleDegrees(Angle, Target);
		const float Omega = 2.f * PI * FMath::Max(Frequency, 0.1f);
		const float Accel = Delta * Omega * Omega - Velocity * 2.f * Damping * Omega;
		Velocity += Accel * Dt;
		Angle = FRotator::NormalizeAxis(Angle + Velocity * Dt);
	}
}

USlimeFaceComponent::USlimeFaceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void USlimeFaceComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (AActor* Owner = GetOwner())
	{
		Body = Owner->FindComponentByClass<USlimeBodyComponent>();
		Element = Owner->FindComponentByClass<USlimeElementComponent>();
		Cling = Owner->FindComponentByClass<USlimeClingComponent>();
		Health = Owner->FindComponentByClass<USlimeHealthComponent>();
		if (Health)
		{
			Health->OnHealthChanged.AddDynamic(this, &USlimeFaceComponent::HandleHealthChanged);
			Health->OnDied.AddDynamic(this, &USlimeFaceComponent::HandleDied);
		}
	}

	if (Body)
	{
		PrimaryComponentTick.AddPrerequisite(Body, Body->PrimaryComponentTick);
	}

	BlinkTimer = FMath::FRandRange(2.f, 5.f);
}

void USlimeFaceComponent::PulseMood(ESlimeMood InMood, float Duration, int32 Priority)
{
	Mood.Pulse(InMood, Duration, Priority);
}

void USlimeFaceComponent::PulseBliss(float Duration)
{
	Mood.Pulse(ESlimeMood::Bliss, Duration, 55);
}

void USlimeFaceComponent::PulseWicked(float Duration)
{
	Mood.Pulse(ESlimeMood::Wicked, Duration, 50);
	StillSeconds = 0.f;
}

void USlimeFaceComponent::SetFaceSuppressed(bool bInSuppressed)
{
	bSuppressed = bInSuppressed;
	if (bSuppressed)
	{
		VisibleAlpha = 0.f;
		if (UMaterialInstanceDynamic* Mid = GetBodyMID())
		{
			Mid->SetScalarParameterValue(SlimeFaceParams::FaceVisible, 0.f);
		}
	}
}

void USlimeFaceComponent::TriggerBlink()
{
	BlinkPhase = BlinkDuration;
}

void USlimeFaceComponent::NotifyLanded(float ImpactSpeed)
{
	if (ImpactSpeed > 250.f)
	{
		Mood.Pulse(ESlimeMood::Squash, FMath::Clamp(ImpactSpeed / 4000.f, 0.12f, 0.28f), 40);
		TriggerBlink();
	}
}

void USlimeFaceComponent::HandleHealthChanged(float CurrentHP, float MaxHP)
{
	if (LastHP >= 0.f && CurrentHP < LastHP - KINDA_SMALL_NUMBER)
	{
		HurtRemaining = 0.6f;
	}
	LastHP = CurrentHP;
}

void USlimeFaceComponent::HandleDied()
{
	bDead = true;
}

UMaterialInstanceDynamic* USlimeFaceComponent::GetBodyMID() const
{
	UProceduralMeshComponent* Mesh = Body ? Body->GetSurfaceMesh() : nullptr;
	return Mesh ? Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)) : nullptr;
}

void USlimeFaceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Body)
	{
		return;
	}
	if (Health && Health->IsAlive())
	{
		bDead = false;
	}

	UpdateSense(DeltaTime);
	UpdateOrientation(DeltaTime);
	UpdateBlink(DeltaTime);
	UpdateVisibility(DeltaTime);
	UpdateMaterial();
}

void USlimeFaceComponent::UpdateSense(float DeltaTime)
{
	HurtRemaining = FMath::Max(HurtRemaining - DeltaTime, 0.f);

	FSlimeMoodDirector::FSense Sense;
	const FVector Vel = OwnerCharacter ? OwnerCharacter->GetVelocity() : FVector::ZeroVector;
	Sense.Move = Vel.Size2D() > MoveSpeedThreshold ? 1.f : 0.f;
	const UCharacterMovementComponent* Move = OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
	const bool bClinging = Cling && Cling->IsClinging();
	Sense.bGround = bClinging || !Move || !Move->IsFalling();
	Sense.Hurt = HurtRemaining;
	const bool bStill = Sense.bGround && Sense.Move < 0.5f && Sense.Hurt <= 0.f && !bSuppressed && !bDead;
	StillSeconds = bStill ? StillSeconds + DeltaTime : 0.f;
	Sense.Still = StillSeconds;
	Mood.Tick(DeltaTime, Sense);

	const FVector Right = FVector::CrossProduct(FaceUp, FaceForward).GetSafeNormal();
	const float Side = float(FVector::DotProduct(Vel, Right));
	LookX = FMath::FInterpTo(LookX, FMath::Clamp(Side * 0.006f, -2.f, 2.f), DeltaTime, 8.f);
}

void USlimeFaceComponent::UpdateOrientation(float DeltaTime)
{
	FVector Desired = FVector::ForwardVector;
	const bool bClinging = Cling && Cling->IsClinging();
	if (bClinging)
	{
		const FVector N = Cling->GetWallNormal();
		if (!N.IsNearlyZero())
		{
			Desired = N.GetSafeNormal();
		}
	}
	else if (OwnerCharacter)
	{
		Desired = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
	}

	const float TargetYaw = float(FMath::RadiansToDegrees(FMath::Atan2(Desired.Y, Desired.X)));
	if (!bHasYaw)
	{
		FaceYaw = TargetYaw;
		FaceYawVelocity = 0.f;
		bHasYaw = true;
	}
	const float Dt = FMath::Clamp(DeltaTime, 0.f, 0.05f);
	SlimeFacePrivate::SpringAngle(FaceYaw, FaceYawVelocity, TargetYaw, TurnFrequency, TurnDamping, Dt);

	if (bClinging)
	{
		FaceForward = Desired;
	}
	else
	{
		FaceForward = FRotator(0.f, FaceYaw, 0.f).Vector();
	}

	FaceUp = FVector::UpVector - FaceForward * FVector::DotProduct(FVector::UpVector, FaceForward);
	if (FaceUp.IsNearlyZero())
	{
		FaceUp = FVector::ForwardVector;
	}
	FaceUp.Normalize();
}

void USlimeFaceComponent::UpdateBlink(float DeltaTime)
{
	if (Mood.Id == ESlimeMood::Sleep)
	{
		BlinkPhase = 0.f;
		return;
	}
	if (BlinkPhase > 0.f)
	{
		BlinkPhase = FMath::Max(BlinkPhase - DeltaTime, 0.f);
		return;
	}
	BlinkTimer -= DeltaTime;
	if (BlinkTimer <= 0.f)
	{
		BlinkPhase = BlinkDuration;
		BlinkTimer = FMath::FRandRange(3.f, 6.f);
	}
}

void USlimeFaceComponent::UpdateVisibility(float DeltaTime)
{
	FVector Axes = Body->GetShellAxes();
	const float Applied = FMath::Max(Body->GetAppliedBodyScale(), 0.05f);
	const float Requested = Body->GetBodyScale();
	if (Body->bVisualOnlyBodyScale && Requested > Applied + 0.01f)
	{
		Axes *= Requested / Applied;
	}

	float Target = 1.f;
	if (bSuppressed || bDead || Body->IsSpreading())
	{
		Target = 0.f;
	}
	else if (Mood.Id != ESlimeMood::Bliss)
	{
		const float MinAxis = float(Axes.GetMin());
		const float AxisFade = FMath::Clamp((MinAxis - HideAxisThreshold) / FMath::Max(HideAxisThreshold * 0.5f, 1.f), 0.f, 1.f);
		const float Squeeze = Body->GetSqueezeAmount();
		const float SqueezeFade = FMath::Clamp((HideSqueezeThreshold + 0.15f - Squeeze) / 0.15f, 0.f, 1.f);
		Target = FMath::Min(AxisFade, SqueezeFade);
	}
	VisibleAlpha = FMath::FInterpTo(VisibleAlpha, Target, DeltaTime, 10.f);
}

void USlimeFaceComponent::UpdateMaterial()
{
	UMaterialInstanceDynamic* Mid = GetBodyMID();
	if (!Mid)
	{
		return;
	}

	FLinearColor Base(0.36f, 0.53f, 0.85f, 1.f);
	if (Element)
	{
		Base = Element->GetProfile(Element->GetPreviewElement()).BaseColor;
	}
	Base.A = 1.f;
	FLinearColor Ink = FMath::Lerp(FLinearColor::White, Base, 0.04f);
	Ink.A = 1.f;
	FLinearColor Shine = FLinearColor(1.f, 1.f, 0.98f, 1.f);
	FLinearColor MouthC = Ink;
	FLinearColor BlushC = FMath::Lerp(FLinearColor(0.96f, 0.55f, 0.59f, 1.f), Base, 0.3f);

	const FSlimeFacePose Pose = Mood.Pose(LookX);
	if (Pose.Id == ESlimeMood::Hurt || HurtRemaining > 0.f)
	{
		Ink = FLinearColor(1.f, 0.35f, 0.4f, 1.f);
		Shine = FLinearColor(1.f, 0.91f, 0.89f, 1.f);
		MouthC = Ink;
	}
	else if (Pose.Id == ESlimeMood::Bliss)
	{
		MouthC = FLinearColor(1.f, 0.62f, 0.68f, 1.f);
	}

	float EyeH = Pose.EyeH;
	float EyeW = Pose.EyeW;
	if (Pose.Id == ESlimeMood::Idle || Pose.Id == ESlimeMood::Move)
	{
		EyeH = IdleEyeH;
		EyeW = IdleEyeW;
	}
	if (BlinkPhase > 0.f)
	{
		EyeH = 0.55f;
	}
	else
	{
		EyeH *= EyeHScale;
		EyeW *= EyeWScale;
	}

	float Curve = Pose.Curve;
	if (Pose.Id == ESlimeMood::Idle || Pose.Id == ESlimeMood::Move)
	{
		Curve = IdleMouthCurve;
	}
	if (Pose.Mouth == ESlimeMouth::Smile || Pose.Mouth == ESlimeMouth::Frown
		|| Pose.Mouth == ESlimeMouth::Grin || Pose.Mouth == ESlimeMouth::Squint)
	{
		Curve *= MouthCurveScale;
	}

	Mid->SetVectorParameterValue(SlimeFaceParams::FaceForward, FLinearColor(float(FaceForward.X), float(FaceForward.Y), float(FaceForward.Z), 0.f));
	Mid->SetVectorParameterValue(SlimeFaceParams::FaceUp, FLinearColor(float(FaceUp.X), float(FaceUp.Y), float(FaceUp.Z), 0.f));
	Mid->SetVectorParameterValue(SlimeFaceParams::FaceInk, Ink);
	Mid->SetVectorParameterValue(SlimeFaceParams::FaceShine, Shine);
	Mid->SetVectorParameterValue(SlimeFaceParams::FaceMouth, MouthC);
	Mid->SetVectorParameterValue(SlimeFaceParams::FaceBlushCol, BlushC);
	Mid->SetScalarParameterValue(SlimeFaceParams::FaceEyeH, EyeH);
	Mid->SetScalarParameterValue(SlimeFaceParams::FaceEyeW, EyeW);
	Mid->SetScalarParameterValue(SlimeFaceParams::FaceBrow, Pose.Brow);
	Mid->SetScalarParameterValue(SlimeFaceParams::FaceMouthKind, float(static_cast<uint8>(Pose.Mouth)));
	Mid->SetScalarParameterValue(SlimeFaceParams::FaceCurve, Curve);
	Mid->SetScalarParameterValue(SlimeFaceParams::FaceBlush, Pose.Blush);
	Mid->SetScalarParameterValue(SlimeFaceParams::FaceLookX, Pose.Look);
	Mid->SetScalarParameterValue(SlimeFaceParams::FaceVisible, VisibleAlpha);
	Mid->SetScalarParameterValue(SlimeFaceParams::FaceHalfPx, FaceHalfPx);

	// ShotCenter{i} (centre + radius) is owned by USlimeBodyComponent, which also tags the surface
	// vertices with the same slot; only the facing direction is written here, by the same slot table.
	Body->RefreshShotStates();
	const TArray<FSlimeSolver::FShotState>& Shots = Body->GetShotStates();
	const TArray<uint8>& SlotIds = Body->GetShotSlotIds();
	for (int32 i = 0; i < SlimeFaceParams::MaxShotFaces; ++i)
	{
		FLinearColor Forward(float(FaceForward.X), float(FaceForward.Y), float(FaceForward.Z), 0.f);
		if (SlotIds.IsValidIndex(i))
		{
			const uint8 WantedId = SlotIds[i];
			const FSlimeSolver::FShotState* Shot = Shots.FindByPredicate([WantedId](const FSlimeSolver::FShotState& S) { return S.Id == WantedId; });
			if (Shot)
			{
				const FVector Horiz(Shot->Velocity.X, Shot->Velocity.Y, 0.f);
				if (Horiz.SizeSquared() > 100.f)
				{
					const FVector Dir = Horiz.GetSafeNormal();
					Forward = FLinearColor(float(Dir.X), float(Dir.Y), float(Dir.Z), 0.f);
				}
			}
		}
		Mid->SetVectorParameterValue(SlimeFaceParams::ShotForward[i], Forward);
	}
}
