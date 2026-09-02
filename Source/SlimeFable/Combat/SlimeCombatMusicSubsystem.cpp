// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/SlimeCombatMusicSubsystem.h"
#include "Combat/SlimeCombatDetect.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/SlimeAudioPlay.h"
#include "Settings/SlimeAudioSettings.h"
#include "SlimeFable.h"
#include "SlimeFablePlayerController.h"
#include "Sound/SoundBase.h"

namespace
{
	const TCHAR* DefaultCombatBgm = TEXT("/Game/Audio/BGM/bgm_global_combat.bgm_global_combat");
}

void USlimeCombatMusicSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (USlimeAudioSettings* Settings = GI->GetSubsystem<USlimeAudioSettings>())
			{
				Settings->OnVolumesChanged.AddDynamic(this, &USlimeCombatMusicSubsystem::HandleVolumesChanged);
			}
		}
	}
}

void USlimeCombatMusicSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (USlimeAudioSettings* Settings = GI->GetSubsystem<USlimeAudioSettings>())
			{
				Settings->OnVolumesChanged.RemoveAll(this);
			}
		}
	}
	StopCombatMusicImmediate();
	Super::Deinitialize();
}

TStatId USlimeCombatMusicSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USlimeCombatMusicSubsystem, STATGROUP_Tickables);
}

void USlimeCombatMusicSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	// Only drive music for gameplay controllers (not menu Spectator PC).
	if (!Cast<ASlimeFablePlayerController>(PC))
	{
		if (bWasInCombat || CombatMusicComponent)
		{
			StopCombatMusicImmediate();
			bWasInCombat = false;
		}
		return;
	}

	const bool bInCombat = !World->IsPaused() && SlimeCombatDetect::IsLocalCombatActive(PC);
	if (bInCombat == bWasInCombat)
	{
		return;
	}
	bWasInCombat = bInCombat;
	SyncCombatMusic(bInCombat);
}

void USlimeCombatMusicSubsystem::SyncCombatMusic(bool bWantPlaying)
{
	if (bWantPlaying)
	{
		StartCombatMusic();
	}
	else
	{
		StopCombatMusic();
	}
}

USoundBase* USlimeCombatMusicSubsystem::LoadCombatMusic() const
{
	if (!CombatMusic.IsNull())
	{
		if (USoundBase* Loaded = CombatMusic.LoadSynchronous())
		{
			return Loaded;
		}
	}
	return LoadObject<USoundBase>(nullptr, DefaultCombatBgm);
}

void USlimeCombatMusicSubsystem::StartCombatMusic()
{
	bStopping = false;

	if (CombatMusicComponent && IsValid(CombatMusicComponent))
	{
		const float TargetVol = SlimeAudioPlay::MusicMul(this);
		if (CombatMusicComponent->IsPlaying())
		{
			CombatMusicComponent->FadeIn(FadeSeconds, TargetVol);
			return;
		}
		CombatMusicComponent->SetVolumeMultiplier(0.f);
		CombatMusicComponent->Play();
		CombatMusicComponent->FadeIn(FadeSeconds, TargetVol);
		return;
	}

	USoundBase* Music = LoadCombatMusic();
	if (!Music)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("Combat BGM missing at %s"), DefaultCombatBgm);
		return;
	}

	CombatMusicComponent = UGameplayStatics::SpawnSound2D(
		this,
		Music,
		0.f,
		1.f,
		0.f,
		nullptr,
		false,
		true);
	if (!CombatMusicComponent)
	{
		return;
	}

	CombatMusicComponent->bAllowSpatialization = false;
	CombatMusicComponent->bIsUISound = false;
	const float TargetVol = SlimeAudioPlay::MusicMul(this);
	CombatMusicComponent->SetVolumeMultiplier(0.f);
	CombatMusicComponent->FadeIn(FadeSeconds, TargetVol);
}

void USlimeCombatMusicSubsystem::StopCombatMusic()
{
	if (!CombatMusicComponent || !IsValid(CombatMusicComponent))
	{
		CombatMusicComponent = nullptr;
		bStopping = false;
		return;
	}

	bStopping = true;
	CombatMusicComponent->FadeOut(FadeSeconds, 0.f);
}

void USlimeCombatMusicSubsystem::StopCombatMusicImmediate()
{
	bStopping = false;
	if (CombatMusicComponent && IsValid(CombatMusicComponent))
	{
		CombatMusicComponent->Stop();
		CombatMusicComponent->DestroyComponent();
	}
	CombatMusicComponent = nullptr;
}

void USlimeCombatMusicSubsystem::RefreshVolume()
{
	if (!CombatMusicComponent || !IsValid(CombatMusicComponent) || bStopping)
	{
		return;
	}
	CombatMusicComponent->SetVolumeMultiplier(SlimeAudioPlay::MusicMul(this));
}

void USlimeCombatMusicSubsystem::HandleVolumesChanged()
{
	RefreshVolume();
}
