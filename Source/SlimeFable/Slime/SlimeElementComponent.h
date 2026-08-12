// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeElementTypes.h"
#include "SlimeElementComponent.generated.h"

class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class USlimeBodyComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlimeElementChanged, ESlimeElement, NewElement, ESlimeElement, PreviousElement);

/**
 *  Holds the current element and drives the body material.
 *
 *  Switching never swaps material: it interpolates the parameters on one dynamic instance,
 *  so there is no shader compile or hitch at the moment of the swap.
 *
 *  Gameplay effects are deliberately out of scope for now. Subscribe to OnElementChanged when
 *  they land instead of adding branches here.
 */
UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API USlimeElementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeElementComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Palette. Falls back to the class defaults in USlimeElementDataAsset when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Element")
	TObjectPtr<USlimeElementDataAsset> ElementLibrary;

	UPROPERTY(EditAnywhere, Category = "Slime|Element")
	TSoftObjectPtr<USlimeElementDataAsset> ElementLibraryPath;

	/** Starting element. Water is the slime's baseline form. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slime|Element")
	ESlimeElement CurrentElement = ESlimeElement::Water;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Element", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float TransitionDuration = 0.25f;

	UPROPERTY(BlueprintAssignable, Category = "Slime|Element")
	FOnSlimeElementChanged OnElementChanged;

	/** Commits an element and broadcasts. No-op when it is already active. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Element")
	void SetElement(ESlimeElement NewElement, bool bInstant = false);

	/** Steps through the wheel order, wrapping at both ends. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Element")
	ESlimeElement CycleElement(int32 Delta);

	/**
	 *  Shows an element on the material without committing it. Used while the wheel is open so
	 *  scrolling reads immediately but only the release counts.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|Element")
	void PreviewElement(ESlimeElement Element);

	/** Turns the current preview into the real element. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Element")
	void CommitPreview();

	UFUNCTION(BlueprintCallable, Category = "Slime|Element")
	void CancelPreview();

	UFUNCTION(BlueprintPure, Category = "Slime|Element")
	ESlimeElement GetPreviewElement() const { return bHasPreview ? PreviewedElement : CurrentElement; }

	UFUNCTION(BlueprintPure, Category = "Slime|Element")
	FSlimeElementProfile GetProfile(ESlimeElement Element) const;

	UFUNCTION(BlueprintPure, Category = "Slime|Element")
	FSlimeElementProfile GetCurrentProfile() const { return GetProfile(CurrentElement); }

private:
	/** The material instance only exists once the body has created its mesh section. */
	bool EnsureDynamicMaterial();
	void ApplyProfileToMaterial(const FSlimeElementProfile& Profile) const;
	static FSlimeElementProfile BlendProfiles(const FSlimeElementProfile& From, const FSlimeElementProfile& To, float Alpha);

	UFUNCTION()
	void HandleSqueezeChanged(float SqueezeAmount);

	void StartTransition(ESlimeElement Target, bool bInstant);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	UPROPERTY(Transient)
	TObjectPtr<USlimeBodyComponent> BodyComponent;

	UPROPERTY(Transient)
	TObjectPtr<USlimeElementDataAsset> ResolvedLibrary;

	FSlimeElementProfile TransitionFrom;
	FSlimeElementProfile TransitionTo;
	float TransitionRemaining = 0.f;

	ESlimeElement PreviewedElement = ESlimeElement::Water;
	bool bHasPreview = false;
};
