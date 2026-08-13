// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SlimePlayGameMode.generated.h"

/**
 *  Game mode for slime gameplay maps such as the SlimeLab sandbox.
 *
 *  Kept separate from the menu game mode and never wired into GlobalDefaultGameMode, so the
 *  366 day levels keep whatever they already use.
 */
UCLASS()
class SLIMEFABLE_API ASlimePlayGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASlimePlayGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;

protected:
	/**
	 *  Preferred pawn. Resolved at InitGame rather than in the constructor so a missing
	 *  Blueprint degrades to the C++ class instead of spamming load errors.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Slime")
	TSoftClassPtr<APawn> SlimePawnClassPath;

	/**
	 *  Preferred player controller. The ThirdPerson BP registers IMC_Default / IMC_MouseLook;
	 *  without it the pawn's move/look InputActions never fire.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Slime")
	TSoftClassPtr<APlayerController> PlayerControllerClassPath;
};
