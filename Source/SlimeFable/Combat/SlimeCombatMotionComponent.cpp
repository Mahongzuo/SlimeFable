// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeCombatMotionComponent.h"
#include "SlimeBodyComponent.h"

USlimeCombatMotionComponent::USlimeCombatMotionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USlimeCombatMotionComponent::ApplyPose(const FSlimeCombatPoseState& Pose)
{
	if (!Body)
	{
		Body = GetOwner() ? GetOwner()->FindComponentByClass<USlimeBodyComponent>() : nullptr;
	}
	if (Body)
	{
		Body->SetCombatPose(Pose);
	}
}

void USlimeCombatMotionComponent::ClearPose()
{
	if (!Body)
	{
		Body = GetOwner() ? GetOwner()->FindComponentByClass<USlimeBodyComponent>() : nullptr;
	}
	if (Body)
	{
		Body->ClearCombatPose();
	}
}
