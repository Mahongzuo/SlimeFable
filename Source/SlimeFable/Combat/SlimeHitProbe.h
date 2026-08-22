// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SlimeCombatTypes.h"
#include "SlimeHitProbe.generated.h"

USTRUCT(BlueprintType)
struct SLIMEFABLE_API FSlimeHitResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FVector Normal = FVector::UpVector;
};

UCLASS()
class SLIMEFABLE_API USlimeHitProbe : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static int32 PerformHit(
		AActor* Instigator,
		const FSlimeSkillDef& Skill,
		const FVector& Origin,
		const FVector& Forward,
		TSet<TWeakObjectPtr<AActor>>& AlreadyHit,
		AActor* RestrictTarget = nullptr,
		TArray<FSlimeHitResult>* OutHits = nullptr);

	static bool IsHostile(const AActor* A, const AActor* B);
	static ESlimeTeam GetTeam(const AActor* Actor);
	static FVector ResolveOrigin(AActor* Instigator, const FSlimeHitSpec& Spec, const FVector& Forward);

	/** If Target implements ISlimeSliceable, invoke SliceAt and return true. */
	static bool TrySliceActor(
		AActor* Target,
		UPrimitiveComponent* HitComponent,
		const FVector& Origin,
		const FVector& Forward);

private:
	static bool GatherOverlaps(
		UWorld* World,
		AActor* Instigator,
		const FSlimeHitSpec& Spec,
		const FVector& Origin,
		const FVector& Forward,
		TArray<FOverlapResult>& OutOverlaps,
		AActor* RestrictTarget = nullptr);

	static void ApplyToActor(
		AActor* Instigator,
		AActor* Target,
		const FSlimeSkillDef& Skill,
		const FVector& HitLocation,
		const FVector& Forward);
};
