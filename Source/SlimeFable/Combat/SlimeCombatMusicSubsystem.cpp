// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/SlimeCombatMusicSubsystem.h"
#include "Combat/SlimeCombatDetect.h"
#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/SlimeAudioPlay.h"
#include "Settings/SlimeAudioSettings.h"
#include "Slime/SlimeCharacter.h"
#include "SlimeFable.h"
#include "Sound/SoundBase.h"
#include "UI/SlimeFableMenuGameMode.h"
#include "UI/SlimeFableMenuPlayerController.h"
#include "WFC/WfcDungeonGenerator.h"

namespace
{
	const TCHAR* DefaultCombatBgm = TEXT("/Game/Audio/BGM/bgm_global_combat.bgm_global_combat");

	void SetWorldExploreBgmDucked(UWorld* World, bool bDucked)
	{
		if (!World)
		{
			return;
		}
		for (TActorIterator<AWFCDungeonGenerator> It(World); It; ++It)
		{
			It->SetExploreBgmDucked(bDucked);
		}
	}
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
	if (!ShouldDriveCombatMusic(PC))
	{
		if (bWasInCombat || CombatMusicComponent)
		{
			StopCombatMusicImmediate();
			bWasInCombat = false;
		}
		return;
	}

	const bool bInCombat = !World->IsPaused() && SlimeCombatDetect::IsLocalCombatActive(PC);
	if (bInCombat)
	{
		if (!bWasInCombat || !IsCombatMusicHealthy())
		{
			StartCombatMusic();
		}
		bWasInCombat = true;
		return;
	}

	if (bWasInCombat)
	{
		StopCombatMusic();
	}
	bWasInCombat = false;
}

bool USlimeCombatMusicSubsystem::ShouldDriveCombatMusic(APlayerController* PC) const
{
	if (!PC)
	{
		return false;
	}
	if (Cast<ASlimeFableMenuPlayerController>(PC))
	{
		return false;
	}
	if (UWorld* World = GetWorld())
	{
		if (Cast<ASlimeFableMenuGameMode>(World->GetAuthGameMode()))
		{
			return false;
		}
	}
	if (Cast<ASpectatorPawn>(PC->GetPawn()))
	{
		return false;
	}
	return PC->GetPawn() != nullptr;
}

UAudioComponent* USlimeCombatMusicSubsystem::ResolveCombatBgmComponent() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		if (ASlimeCharacter* Slime = Cast<ASlimeCharacter>(PC->GetPawn()))
		{
			if (UAudioComponent* Bgm = Slime->GetCombatBgm())
			{
				return Bgm;
			}
		}
	}

	for (TActorIterator<ASlimeCharacter> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetCombatBgm())
		{
			return It->GetCombatBgm();
		}
	}
	return nullptr;
}

bool USlimeCombatMusicSubsystem::IsCombatMusicHealthy() const
{
	return CombatMusicComponent && IsValid(CombatMusicComponent) && !bStopping && CombatMusicComponent->IsPlaying();
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
	SetWorldExploreBgmDucked(GetWorld(), true);

	UAudioComponent* Bgm = ResolveCombatBgmComponent();
	if (!Bgm)
	{
		return;
	}

	USoundBase* Music = LoadCombatMusic();
	if (!Music)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("Combat BGM missing at %s"), DefaultCombatBgm);
		return;
	}

	CombatMusicComponent = Bgm;
	CombatMusicComponent->bAllowSpatialization = false;
	CombatMusicComponent->bIsUISound = false;
	CombatMusicComponent->bAutoDestroy = false;
	if (CombatMusicComponent->GetSound() != Music)
	{
		CombatMusicComponent->SetSound(Music);
	}

	const float TargetVol = FMath::Max(SlimeAudioPlay::MusicMul(this), 0.01f);
	if (CombatMusicComponent->IsPlaying())
	{
		CombatMusicComponent->FadeIn(FadeSeconds, TargetVol);
		return;
	}
	CombatMusicComponent->SetVolumeMultiplier(TargetVol);
	CombatMusicComponent->Play();
}

void USlimeCombatMusicSubsystem::StopCombatMusic()
{
	SetWorldExploreBgmDucked(GetWorld(), false);
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
	SetWorldExploreBgmDucked(GetWorld(), false);
	bStopping = false;
	if (CombatMusicComponent && IsValid(CombatMusicComponent))
	{
		CombatMusicComponent->Stop();
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
