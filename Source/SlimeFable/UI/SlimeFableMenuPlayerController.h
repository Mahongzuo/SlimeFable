// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SlimeFableMenuPlayerController.generated.h"

class UMainMenuWidget;

UCLASS()
class SLIMEFABLE_API ASlimeFableMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASlimeFableMenuPlayerController();

	virtual void BeginPlay() override;

protected:
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UMainMenuWidget> MainMenuClass;

	UPROPERTY()
	TObjectPtr<UMainMenuWidget> MainMenuWidget;
};
