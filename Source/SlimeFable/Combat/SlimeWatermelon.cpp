// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeWatermelon.h"

#include "Components/StaticMeshComponent.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

ASlimeWatermelon::ASlimeWatermelon()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(
		TEXT("/Game/StaticMeshes/Food/Watermelon/S_watermelon.S_watermelon"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CapFinder(
		TEXT("/Game/StaticMeshes/Food/Watermelon/MI_watermelon_inside.MI_watermelon_inside"));
	static ConstructorHelpers::FObjectFinder<USoundBase> SoundFinder(
		TEXT("/Game/Audio/SFX/sfx_fruitslice_01.sfx_fruitslice_01"));

	if (MeshFinder.Succeeded() && SourceMesh)
	{
		SourceMesh->SetStaticMesh(MeshFinder.Object);
	}
	if (CapFinder.Succeeded())
	{
		CapMaterial = CapFinder.Object;
	}
	if (SoundFinder.Succeeded())
	{
		SliceSound = SoundFinder.Object;
	}
}
