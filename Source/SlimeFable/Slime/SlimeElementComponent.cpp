// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeElementComponent.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "SlimeBodyComponent.h"
#include "SlimeFable.h"

namespace SlimeElementParams
{
	static const FName BaseColor(TEXT("BaseColor"));
	static const FName SubsurfaceColor(TEXT("SubsurfaceColor"));
	static const FName EmissiveColor(TEXT("EmissiveColor"));
	static const FName RimColor(TEXT("RimColor"));
	static const FName EmissiveIntensity(TEXT("EmissiveIntensity"));
	static const FName Opacity(TEXT("Opacity"));
	static const FName Roughness(TEXT("Roughness"));
	static const FName Refraction(TEXT("Refraction"));
	static const FName FlowSpeed(TEXT("FlowSpeed"));
	static const FName NoiseScale(TEXT("NoiseScale"));
	static const FName RimPower(TEXT("RimPower"));
	static const FName SqueezeAmount(TEXT("SqueezeAmount"));
}

USlimeElementComponent::USlimeElementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;
}

void USlimeElementComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolvedLibrary = ElementLibrary;
	if (!ResolvedLibrary && !ElementLibraryPath.IsNull())
	{
		ResolvedLibrary = ElementLibraryPath.LoadSynchronous();
	}

	if (AActor* Owner = GetOwner())
	{
		BodyComponent = Owner->FindComponentByClass<USlimeBodyComponent>();
		if (BodyComponent)
		{
			BodyComponent->OnSqueezeChanged.AddDynamic(this, &USlimeElementComponent::HandleSqueezeChanged);
		}
	}

	TransitionFrom = GetProfile(CurrentElement);
	TransitionTo = TransitionFrom;
	TransitionRemaining = 0.f;

	if (EnsureDynamicMaterial())
	{
		ApplyProfileToMaterial(TransitionTo);
	}
}

void USlimeElementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// The body creates its mesh section on its own schedule, so keep trying until a slot exists.
	if (!EnsureDynamicMaterial())
	{
		return;
	}

	if (TransitionRemaining > 0.f)
	{
		TransitionRemaining = FMath::Max(TransitionRemaining - DeltaTime, 0.f);
		const float Alpha = TransitionDuration > 0.f
			? 1.f - (TransitionRemaining / TransitionDuration)
			: 1.f;
		ApplyProfileToMaterial(BlendProfiles(TransitionFrom, TransitionTo, FMath::Clamp(Alpha, 0.f, 1.f)));
		if (TransitionRemaining <= 0.f)
		{
			TransitionFrom = TransitionTo;
		}
	}
}

bool USlimeElementComponent::EnsureDynamicMaterial()
{
	UProceduralMeshComponent* Mesh = BodyComponent ? BodyComponent->GetSurfaceMesh() : nullptr;
	if (!Mesh || Mesh->GetNumSections() == 0)
	{
		return BodyMaterial != nullptr;
	}

	// A quality change recreates the section and reassigns the base material, which leaves the
	// previous instance orphaned; detect that instead of silently driving a dead material.
	if (BodyMaterial && Mesh->GetMaterial(0) == BodyMaterial)
	{
		return true;
	}

	BodyMaterial = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	if (!BodyMaterial)
	{
		return false;
	}

	ApplyProfileToMaterial(TransitionRemaining > 0.f ? TransitionFrom : TransitionTo);
	return true;
}

void USlimeElementComponent::ApplyProfileToMaterial(const FSlimeElementProfile& Profile) const
{
	if (!BodyMaterial)
	{
		return;
	}

	BodyMaterial->SetVectorParameterValue(SlimeElementParams::BaseColor, Profile.BaseColor);
	BodyMaterial->SetVectorParameterValue(SlimeElementParams::SubsurfaceColor, Profile.SubsurfaceColor);
	BodyMaterial->SetVectorParameterValue(SlimeElementParams::EmissiveColor, Profile.EmissiveColor);
	BodyMaterial->SetVectorParameterValue(SlimeElementParams::RimColor, Profile.RimColor);
	BodyMaterial->SetScalarParameterValue(SlimeElementParams::EmissiveIntensity, Profile.EmissiveIntensity);
	BodyMaterial->SetScalarParameterValue(SlimeElementParams::Opacity, Profile.Opacity);
	BodyMaterial->SetScalarParameterValue(SlimeElementParams::Roughness, Profile.Roughness);
	BodyMaterial->SetScalarParameterValue(SlimeElementParams::Refraction, Profile.Refraction);
	BodyMaterial->SetScalarParameterValue(SlimeElementParams::FlowSpeed, Profile.FlowSpeed);
	BodyMaterial->SetScalarParameterValue(SlimeElementParams::NoiseScale, Profile.NoiseScale);
	BodyMaterial->SetScalarParameterValue(SlimeElementParams::RimPower, Profile.RimPower);
}

FSlimeElementProfile USlimeElementComponent::BlendProfiles(const FSlimeElementProfile& From, const FSlimeElementProfile& To, float Alpha)
{
	FSlimeElementProfile Result = To;
	Result.BaseColor = FMath::Lerp(From.BaseColor, To.BaseColor, Alpha);
	Result.SubsurfaceColor = FMath::Lerp(From.SubsurfaceColor, To.SubsurfaceColor, Alpha);
	Result.EmissiveColor = FMath::Lerp(From.EmissiveColor, To.EmissiveColor, Alpha);
	Result.RimColor = FMath::Lerp(From.RimColor, To.RimColor, Alpha);
	Result.EmissiveIntensity = FMath::Lerp(From.EmissiveIntensity, To.EmissiveIntensity, Alpha);
	Result.Opacity = FMath::Lerp(From.Opacity, To.Opacity, Alpha);
	Result.Roughness = FMath::Lerp(From.Roughness, To.Roughness, Alpha);
	Result.Refraction = FMath::Lerp(From.Refraction, To.Refraction, Alpha);
	Result.FlowSpeed = FMath::Lerp(From.FlowSpeed, To.FlowSpeed, Alpha);
	Result.NoiseScale = FMath::Lerp(From.NoiseScale, To.NoiseScale, Alpha);
	Result.RimPower = FMath::Lerp(From.RimPower, To.RimPower, Alpha);
	return Result;
}

FSlimeElementProfile USlimeElementComponent::GetProfile(ESlimeElement Element) const
{
	return ResolvedLibrary ? ResolvedLibrary->GetProfile(Element) : USlimeElementDataAsset::MakeDefaultProfile(Element);
}

void USlimeElementComponent::StartTransition(ESlimeElement Target, bool bInstant)
{
	// Start from whatever is on screen right now, not from the last committed element, so a
	// switch mid transition does not snap.
	TransitionFrom = TransitionRemaining > 0.f && TransitionDuration > 0.f
		? BlendProfiles(TransitionFrom, TransitionTo, 1.f - (TransitionRemaining / TransitionDuration))
		: TransitionTo;

	TransitionTo = GetProfile(Target);
	TransitionRemaining = bInstant ? 0.f : TransitionDuration;

	if (TransitionRemaining <= 0.f)
	{
		TransitionFrom = TransitionTo;
		if (EnsureDynamicMaterial())
		{
			ApplyProfileToMaterial(TransitionTo);
		}
	}
}

void USlimeElementComponent::SetElement(ESlimeElement NewElement, bool bInstant)
{
	bHasPreview = false;

	if (NewElement == CurrentElement)
	{
		// Still resync the material: a cancelled preview leaves it showing something else.
		StartTransition(CurrentElement, bInstant);
		return;
	}

	const ESlimeElement Previous = CurrentElement;
	CurrentElement = NewElement;
	StartTransition(CurrentElement, bInstant);

	UE_LOG(LogSlimeFable, Verbose, TEXT("Slime element %d -> %d"), int32(Previous), int32(CurrentElement));
	OnElementChanged.Broadcast(CurrentElement, Previous);
}

ESlimeElement USlimeElementComponent::CycleElement(int32 Delta)
{
	const ESlimeElement Next = SlimeElement::FromIndex(SlimeElement::ToIndex(GetPreviewElement()) + Delta);
	PreviewElement(Next);
	return Next;
}

void USlimeElementComponent::PreviewElement(ESlimeElement Element)
{
	if (bHasPreview && PreviewedElement == Element)
	{
		return;
	}
	PreviewedElement = Element;
	bHasPreview = true;
	StartTransition(Element, false);
}

void USlimeElementComponent::CommitPreview()
{
	if (!bHasPreview)
	{
		return;
	}
	const ESlimeElement Target = PreviewedElement;
	bHasPreview = false;
	// Nothing to announce when the wheel closes on the element it opened with.
	if (Target != CurrentElement)
	{
		SetElement(Target, false);
	}
}

void USlimeElementComponent::CancelPreview()
{
	if (!bHasPreview)
	{
		return;
	}
	bHasPreview = false;
	StartTransition(CurrentElement, false);
}

void USlimeElementComponent::HandleSqueezeChanged(float SqueezeAmount)
{
	if (BodyMaterial)
	{
		BodyMaterial->SetScalarParameterValue(SlimeElementParams::SqueezeAmount, SqueezeAmount);
	}
}
