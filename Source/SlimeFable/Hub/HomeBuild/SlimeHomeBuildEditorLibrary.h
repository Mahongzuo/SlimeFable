// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SlimeHomeBuildEditorLibrary.generated.h"

class AActor;

/** Editor helper so Python can turn a placed FluidNinja actor into a blueprint. */
UCLASS()
class SLIMEFABLE_API USlimeHomeBuildEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HomeBuild")
	static UObject* CreateBlueprintFromActor(const FString& AssetPath, AActor* Actor);

	/** Block until meshes, shaders, and textures are ready to photograph. */
	UFUNCTION(BlueprintCallable, Category = "HomeBuild")
	static void FinishRenderAssets();
};
