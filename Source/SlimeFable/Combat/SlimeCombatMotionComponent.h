// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeTypes.h"
#include "SlimeCombatMotionComponent.generated.h"

class USlimeBodyComponent;

UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API USlimeCombatMotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeCombatMotionComponent();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ApplyPose(const FSlimeCombatPoseState& Pose);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ClearPose();

private:
	UPROPERTY(Transient)
	TObjectPtr<USlimeBodyComponent> Body;
};
