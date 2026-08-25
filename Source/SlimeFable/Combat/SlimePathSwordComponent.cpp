// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimePathSwordComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "SlimeBodyComponent.h"

namespace SlimePathSwordNs
{
	static const FName SocketStart(TEXT("TrailStart"));
	static const FName SocketEnd(TEXT("TrailEnd"));

	static const TCHAR* TrailPath(ESlimeElement Element)
	{
		switch (Element)
		{
		case ESlimeElement::Fire:
			return TEXT("/Game/TrailMaster/Particle/Trail/Trail_001-010/Trail_001/P_Trail_001.P_Trail_001");
		case ESlimeElement::Wind:
			return TEXT("/Game/TrailMaster/Particle/Trail/Trail_001-010/Trail_003/P_Trail_003.P_Trail_003");
		case ESlimeElement::Water:
			return TEXT("/Game/TrailMaster/Particle/Trail/Trail_001-010/Trail_005/P_Trail_005.P_Trail_005");
		case ESlimeElement::Lightning:
			return TEXT("/Game/TrailMaster/Particle/Trail/Trail_001-010/Trail_002/P_Trail_002.P_Trail_002");
		case ESlimeElement::Dark:
			return TEXT("/Game/TrailMaster/Particle/Trail/Trail_011-020/Trail_012/P_Trail_012.P_Trail_012");
		case ESlimeElement::Physical:
		default:
			return TEXT("/Game/TrailMaster/Particle/Trail/Trail_011-020/Trail_011/P_Trail_011.P_Trail_011");
		}
	}

	static const TCHAR* ParticlePath(ESlimeElement Element)
	{
		switch (Element)
		{
		case ESlimeElement::Fire:
			return TEXT("/Game/TrailMaster/Particle/Trail/Trail_001-010/Trail_001/P_Particle_001.P_Particle_001");
		case ESlimeElement::Wind:
			return TEXT("/Game/TrailMaster/Particle/Trail/Trail_001-010/Trail_003/P_Particle_003.P_Particle_003");
		case ESlimeElement::Water:
			return TEXT("/Game/TrailMaster/Particle/Trail/Trail_001-010/Trail_005/P_Particle_005.P_Particle_005");
		case ESlimeElement::Lightning:
			return TEXT("/Game/TrailMaster/Particle/Trail/Trail_001-010/Trail_002/P_Particle_002.P_Particle_002");
		case ESlimeElement::Dark:
			return TEXT("/Game/TrailMaster/Particle/Trail/Trail_011-020/Trail_012/P_Particle_012.P_Particle_012");
		case ESlimeElement::Physical:
		default:
			return TEXT("/Game/TrailMaster/Particle/Trail/Trail_011-020/Trail_011/P_Particle_011.P_Particle_011");
		}
	}
}

USlimePathSwordComponent::USlimePathSwordComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	BladeMeshAsset = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Game/_Slime/FX/SM_SlimeTrailBlade.SM_SlimeTrailBlade")));
}

void USlimePathSwordComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureVisuals();
	SetTrailVisible(false);
}

void USlimePathSwordComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AbortSwing();
	Super::EndPlay(EndPlayReason);
}

void USlimePathSwordComponent::HideBladeMesh()
{
	if (!BladeMesh)
	{
		return;
	}
	// Do NOT propagate — TrailPSC / DebrisPSC are children and must stay drawable.
	BladeMesh->SetHiddenInGame(true, /*bPropagateToChildren=*/false);
	BladeMesh->SetVisibility(false, /*bPropagateToChildren=*/false);
	BladeMesh->SetCastShadow(false);
	BladeMesh->bCastDynamicShadow = false;
	BladeMesh->bCastContactShadow = false;
	BladeMesh->bCastHiddenShadow = false;
}

void USlimePathSwordComponent::EnsureTrailSockets()
{
	if (!BladeMesh)
	{
		return;
	}

	UStaticMesh* Mesh = BladeMesh->GetStaticMesh();
	if (!Mesh)
	{
		return;
	}

	auto EnsureSocket = [Mesh](FName Name, const FVector& RelLocation)
	{
		if (UStaticMeshSocket* Existing = Mesh->FindSocket(Name))
		{
			Existing->RelativeLocation = RelLocation;
			return;
		}
		UStaticMeshSocket* Socket = NewObject<UStaticMeshSocket>(Mesh);
		Socket->SocketName = Name;
		Socket->RelativeLocation = RelLocation;
		Socket->RelativeRotation = FRotator::ZeroRotator;
		Socket->RelativeScale = FVector::OneVector;
		Mesh->AddSocket(Socket);
	};

	// Radial blade along local +X, both sockets off the yaw pivot (TrailMaster sword).
	const float Inner = FMath::Max(HiltRadius, 10.f);
	const float Outer = Inner + FMath::Max(BladeLength, 40.f);
	const float Tilt = BladeTiltZ;
	EnsureSocket(SlimePathSwordNs::SocketStart, FVector(Inner, 0.f, -Tilt));
	EnsureSocket(SlimePathSwordNs::SocketEnd, FVector(Outer, 0.f, Tilt));

	if (!BladeMesh->DoesSocketExist(SlimePathSwordNs::SocketStart)
		|| !BladeMesh->DoesSocketExist(SlimePathSwordNs::SocketEnd))
	{
		UE_LOG(LogTemp, Warning, TEXT("SlimePathSword: TrailStart/TrailEnd sockets missing after ensure."));
	}
}

void USlimePathSwordComponent::EnsureVisuals()
{
	if (bVisualsReady)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetRootComponent())
	{
		return;
	}

	if (!SwingRoot)
	{
		SwingRoot = NewObject<USceneComponent>(Owner, TEXT("PathSwordSwingRoot"));
		SwingRoot->SetupAttachment(Owner->GetRootComponent());
		SwingRoot->RegisterComponent();
	}

	if (!BladeMesh)
	{
		BladeMesh = NewObject<UStaticMeshComponent>(Owner, TEXT("PathSwordBlade"));
		BladeMesh->SetupAttachment(SwingRoot);
		BladeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BladeMesh->SetGenerateOverlapEvents(false);
		BladeMesh->bReceivesDecals = false;
		BladeMesh->RegisterComponent();
	}

	if (UStaticMesh* Mesh = BladeMeshAsset.LoadSynchronous())
	{
		BladeMesh->SetStaticMesh(Mesh);
		BladeMesh->SetRelativeLocation(FVector::ZeroVector);
		BladeMesh->SetRelativeRotation(FRotator::ZeroRotator);
		BladeMesh->SetRelativeScale3D(FVector(1.f));
	}
	else if (UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		BladeMesh->SetStaticMesh(Cylinder);
		BladeMesh->SetRelativeLocation(FVector::ZeroVector);
		BladeMesh->SetRelativeScale3D(FVector(1.f));
		UE_LOG(LogTemp, Warning, TEXT("SlimePathSword: SM_SlimeTrailBlade missing, using cylinder."));
	}

	EnsureTrailSockets();
	HideBladeMesh();

	if (!TrailPSC)
	{
		TrailPSC = NewObject<UParticleSystemComponent>(Owner, TEXT("PathSwordTrail"));
		TrailPSC->bAutoActivate = false;
		TrailPSC->bAutoDestroy = false;
		TrailPSC->SecondsBeforeInactive = 0.f;
		TrailPSC->SetupAttachment(BladeMesh);
		TrailPSC->RegisterComponent();
	}

	if (!DebrisPSC)
	{
		DebrisPSC = NewObject<UParticleSystemComponent>(Owner, TEXT("PathSwordDebris"));
		DebrisPSC->bAutoActivate = false;
		DebrisPSC->bAutoDestroy = false;
		DebrisPSC->SecondsBeforeInactive = 0.f;
		DebrisPSC->SetupAttachment(BladeMesh, SlimePathSwordNs::SocketEnd);
		DebrisPSC->RegisterComponent();
	}

	bVisualsReady = true;
}

UParticleSystem* USlimePathSwordComponent::ResolveTrailForElement(ESlimeElement Element) const
{
	return LoadObject<UParticleSystem>(nullptr, SlimePathSwordNs::TrailPath(Element));
}

UParticleSystem* USlimePathSwordComponent::ResolveParticleForElement(ESlimeElement Element) const
{
	return LoadObject<UParticleSystem>(nullptr, SlimePathSwordNs::ParticlePath(Element));
}

FVector USlimePathSwordComponent::GetBlobCenter() const
{
	if (const AActor* Owner = GetOwner())
	{
		if (const USlimeBodyComponent* Body = Owner->FindComponentByClass<USlimeBodyComponent>())
		{
			return Body->GetBlobCenter();
		}
		return Owner->GetActorLocation();
	}
	return FVector::ZeroVector;
}

void USlimePathSwordComponent::SetTrailVisible(bool bVisible)
{
	HideBladeMesh();
	if (TrailPSC)
	{
		TrailPSC->SetHiddenInGame(!bVisible, false);
		TrailPSC->SetVisibility(bVisible, false);
	}
	if (DebrisPSC)
	{
		DebrisPSC->SetHiddenInGame(!bVisible, false);
		DebrisPSC->SetVisibility(bVisible, false);
	}
}

void USlimePathSwordComponent::ApplySwingPose(float YawDeg, float PitchDeg)
{
	if (!SwingRoot)
	{
		return;
	}
	if (BladeMesh)
	{
		BladeMesh->SetRelativeLocation(FVector::ZeroVector);
		BladeMesh->SetRelativeRotation(FRotator::ZeroRotator);
		BladeMesh->SetRelativeScale3D(FVector(SwingScale));
	}
	const FVector Pivot = GetBlobCenter() + FVector(0.f, 0.f, HeightOffset);
	SwingRoot->SetWorldLocation(Pivot);
	const FQuat Face = SwingForward.ToOrientationQuat();
	const FQuat LocalYaw = FQuat(FVector::UpVector, FMath::DegreesToRadians(YawDeg));
	const FQuat LocalPitch = FQuat(FVector::RightVector, FMath::DegreesToRadians(PitchDeg));
	SwingRoot->SetWorldRotation(Face * LocalYaw * LocalPitch);
}

void USlimePathSwordComponent::SetTrailManualTick(bool bManual)
{
	bTrailManualTick = bManual;
	if (TrailPSC)
	{
		TrailPSC->SetComponentTickEnabled(!bManual);
	}
}

void USlimePathSwordComponent::SampleTrailAtPose(float YawDeg, float PitchDeg, float SubstepDelta)
{
	ApplySwingPose(YawDeg, PitchDeg);
	HideBladeMesh();
	if (!TrailPSC || !bTrailActive)
	{
		return;
	}
	TrailPSC->SetTrailSourceData(
		SlimePathSwordNs::SocketStart,
		SlimePathSwordNs::SocketEnd,
		ETrailWidthMode::ETrailWidthMode_FromCentre,
		TrailWidth);
	TrailPSC->TickComponent(SubstepDelta, LEVELTICK_All, nullptr);
}

void USlimePathSwordComponent::EndTrailInternal()
{
	if (TrailPSC && bTrailActive)
	{
		TrailPSC->EndTrails();
	}
	bTrailActive = false;
	if (DebrisPSC)
	{
		DebrisPSC->DeactivateSystem();
	}
	SetTrailManualTick(false);
}

void USlimePathSwordComponent::FinishTrailVisuals()
{
	EndTrailInternal();
	LingerRemaining = 0.f;
	if (TrailPSC)
	{
		TrailPSC->DeactivateSystem();
	}
	if (DebrisPSC)
	{
		DebrisPSC->DeactivateSystem();
	}
	SetTrailVisible(false);
}

void USlimePathSwordComponent::PlaySwing(ESlimeElement Element, int32 ComboIndex, FVector Forward, float DurationSeconds)
{
	EnsureVisuals();
	if (!SwingRoot || !BladeMesh || !TrailPSC)
	{
		return;
	}

	AbortSwing();
	EnsureTrailSockets();

	Forward.Z = 0.f;
	if (!Forward.Normalize())
	{
		Forward = FVector::ForwardVector;
	}
	SwingForward = Forward;
	SwingDuration = FMath::Clamp(DurationSeconds, 0.22f, 0.55f);
	SwingElapsed = 0.f;
	LingerRemaining = 0.f;
	SwingScale = (ComboIndex == 3) ? FinisherTrailScale : NormalTrailScale;

	const float Half = SwingHalfAngleDeg;
	const float Diag = DiagonalPitchDeg;
	switch (ComboIndex)
	{
	case 1: // 2: left → right
		SwingStartYaw = -Half;
		SwingEndYaw = Half;
		SwingStartPitch = 0.f;
		SwingEndPitch = 0.f;
		break;
	case 2: // 3: upper-right → lower-left
		SwingStartYaw = Half;
		SwingEndYaw = -Half;
		SwingStartPitch = Diag;
		SwingEndPitch = -Diag;
		break;
	case 3: // 4: lower-left → upper-right finisher
		SwingStartYaw = -Half;
		SwingEndYaw = Half;
		SwingStartPitch = -Diag;
		SwingEndPitch = Diag;
		break;
	default: // 1: right → left
		SwingStartYaw = Half;
		SwingEndYaw = -Half;
		SwingStartPitch = 0.f;
		SwingEndPitch = 0.f;
		break;
	}

	ApplySwingPose(SwingStartYaw, SwingStartPitch);
	HideBladeMesh();

	if (UParticleSystem* PS = ResolveTrailForElement(Element))
	{
		TrailPSC->SetTemplate(PS);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SlimePathSword: missing P_Trail for element %d"),
			static_cast<int32>(Element));
		return;
	}

	if (DebrisPSC)
	{
		if (UParticleSystem* Debris = ResolveParticleForElement(Element))
		{
			DebrisPSC->SetTemplate(Debris);
			DebrisPSC->ActivateSystem(true);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SlimePathSword: missing P_Particle for element %d"),
				static_cast<int32>(Element));
			DebrisPSC->DeactivateSystem();
		}
	}

	SetTrailVisible(true);
	SetTrailManualTick(true);
	TrailPSC->ActivateSystem(true);
	TrailPSC->BeginTrails(
		SlimePathSwordNs::SocketStart,
		SlimePathSwordNs::SocketEnd,
		ETrailWidthMode::ETrailWidthMode_FromCentre,
		TrailWidth);
	bTrailActive = true;
	bSwinging = true;
}

void USlimePathSwordComponent::AbortSwing()
{
	bSwinging = false;
	SwingElapsed = 0.f;
	FinishTrailVisuals();
}

void USlimePathSwordComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bSwinging && SwingRoot)
	{
		HideBladeMesh();

		const float PrevElapsed = SwingElapsed;
		SwingElapsed += DeltaTime;
		const int32 Steps = FMath::Clamp(TrailSubsteps, 1, 8);
		const float SubDt = DeltaTime / static_cast<float>(Steps);

		for (int32 i = 1; i <= Steps; ++i)
		{
			const float SampleElapsed = FMath::Min(
				PrevElapsed + DeltaTime * (static_cast<float>(i) / static_cast<float>(Steps)),
				SwingDuration);
			const float Alpha = FMath::Clamp(SampleElapsed / FMath::Max(SwingDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
			const float Smooth = Alpha * Alpha * (3.f - 2.f * Alpha);
			const float Yaw = FMath::Lerp(SwingStartYaw, SwingEndYaw, Smooth);
			const float Pitch = FMath::Lerp(SwingStartPitch, SwingEndPitch, Smooth);
			SampleTrailAtPose(Yaw, Pitch, SubDt);
		}

		if (SwingElapsed >= SwingDuration)
		{
			EndTrailInternal();
			bSwinging = false;
			LingerRemaining = TrailLinger;
			if (LingerRemaining <= 0.f)
			{
				FinishTrailVisuals();
			}
		}
		return;
	}

	if (LingerRemaining > 0.f)
	{
		LingerRemaining -= DeltaTime;
		if (LingerRemaining <= 0.f)
		{
			FinishTrailVisuals();
		}
	}
}
