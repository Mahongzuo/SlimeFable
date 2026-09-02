// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SlimeCombatMusicSubsystem.generated.h"

class UAudioComponent;
class USoundBase;
class USlimeAudioSettings;

/**
 * Plays looping combat BGM while any local enemy is engaged or the player is locked on.
 * Stops (with fade) when combat clears. Does not run in menu worlds without a gameplay PC.
 */
UCLASS()
class SLIMEFABLE_API USlimeCombatMusicSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return false; }

	/** Force-stop combat music (level travel / return to menu). */
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void StopCombatMusicImmediate();

protected:
	void SyncCombatMusic(bool bWantPlaying);
	void StartCombatMusic();
	void StopCombatMusic();
	void RefreshVolume();
	USoundBase* LoadCombatMusic() const;

	UFUNCTION()
	void HandleVolumesChanged();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> CombatMusicComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Audio",
		meta = (ToolTip = "战斗 BGM Soft 路径。默认 /Game/Audio/BGM/bgm_global_combat。"))
	TSoftObjectPtr<USoundBase> CombatMusic;

	float FadeSeconds = 0.4f;
	bool bWasInCombat = false;
	bool bStopping = false;
};
