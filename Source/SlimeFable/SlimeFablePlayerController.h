// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SlimeFablePlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UPauseMenuWidget;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings and in-game pause menu (ESC).
 */
UCLASS(abstract)
class ASlimeFablePlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UPauseMenuWidget> PauseMenuClass;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSoftClassPtr<UPauseMenuWidget> PauseMenuClassPath;

	UPROPERTY()
	TObjectPtr<UPauseMenuWidget> PauseMenuWidget;

	/** Gameplay initialization */
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	void TogglePauseMenu();
	void OpenPauseMenu();
	void ClosePauseMenu();
	void UpdateAltCursor();
	bool IsPauseMenuOpen() const;

	UFUNCTION()
	void HandlePauseContinue();

	UFUNCTION()
	void HandlePauseLevelSelect();

	UFUNCTION()
	void HandlePauseMainMenu();

	UFUNCTION()
	void HandlePauseReturnToHub();

	UFUNCTION()
	void HandlePauseResetDay();
};
