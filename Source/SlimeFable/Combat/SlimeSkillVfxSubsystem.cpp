// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeSkillVfxSubsystem.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "NiagaraSystem.h"
#include "SlimeCombatTypes.h"
#include "SlimeElementTypes.h"
#include "SlimeFable.h"

namespace
{
	void AddVfxPath(const TSoftObjectPtr<UNiagaraSystem>& SoftSystem, TSet<FSoftObjectPath>& OutPaths)
	{
		if (!SoftSystem.IsNull())
		{
			OutPaths.Add(SoftSystem.ToSoftObjectPath());
		}
	}

	void AddSkillPaths(const FSlimeSkillDef& Skill, TSet<FSoftObjectPath>& OutPaths)
	{
		AddVfxPath(Skill.NiagaraSystem, OutPaths);
		AddVfxPath(Skill.ImpactNiagaraSystem, OutPaths);
	}
}

void USlimeSkillVfxSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CollectSkillVfxPaths();
	if (RequestedPaths.IsEmpty())
	{
		bPreloadComplete = true;
		return;
	}

	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	PreloadHandle = StreamableManager.RequestAsyncLoad(
		RequestedPaths,
		FStreamableDelegate::CreateUObject(this, &USlimeSkillVfxSubsystem::HandlePreloadComplete),
		FStreamableManager::DefaultAsyncLoadPriority,
		false,
		false,
		TEXT("SlimeSkillVfx"));

	if (!PreloadHandle.IsValid())
	{
		FailedAssetCount = RequestedPaths.Num();
		bPreloadComplete = true;
		UE_LOG(LogSlimeFable, Error, TEXT("Skill VFX preload could not create a streamable handle (%d assets)"), RequestedPaths.Num());
	}
}

void USlimeSkillVfxSubsystem::Deinitialize()
{
	if (PreloadHandle.IsValid())
	{
		PreloadHandle->CancelHandle();
		PreloadHandle.Reset();
	}
	LoadedSystems.Reset();
	RequestedPaths.Reset();
	FailedAssetCount = 0;
	bPreloadComplete = false;

	Super::Deinitialize();
}

void USlimeSkillVfxSubsystem::CollectSkillVfxPaths()
{
	TSet<FSoftObjectPath> UniquePaths;
	for (int32 ElementIndex = 0; ElementIndex < SlimeElement::Count; ++ElementIndex)
	{
		const FSlimeElementKitData Kit = SlimeCombat::MakeDefaultKit(SlimeElement::FromIndex(ElementIndex));
		for (const FSlimeSkillDef& Combo : Kit.Combos)
		{
			AddSkillPaths(Combo, UniquePaths);
		}
		AddSkillPaths(Kit.Skill1, UniquePaths);
		AddSkillPaths(Kit.Skill2, UniquePaths);
		AddSkillPaths(Kit.Skill3, UniquePaths);
	}

	TArray<FSlimeReactionRow> Reactions;
	SlimeCombat::FillDefaultReactions(Reactions);
	for (const FSlimeReactionRow& Reaction : Reactions)
	{
		AddVfxPath(Reaction.NiagaraSystem, UniquePaths);
	}

	RequestedPaths = UniquePaths.Array();
	RequestedPaths.Sort([](const FSoftObjectPath& A, const FSoftObjectPath& B)
	{
		return A.ToString() < B.ToString();
	});
}

void USlimeSkillVfxSubsystem::HandlePreloadComplete()
{
	LoadedSystems.Reset();
	FailedAssetCount = 0;

	for (const FSoftObjectPath& Path : RequestedPaths)
	{
		UNiagaraSystem* System = Cast<UNiagaraSystem>(Path.ResolveObject());
		if (!System)
		{
			++FailedAssetCount;
			UE_LOG(LogSlimeFable, Error, TEXT("Skill VFX preload failed: %s"), *Path.ToString());
			continue;
		}

		LoadedSystems.Add(System);
		System->PrecacheAssetPSOs();
	}

	bPreloadComplete = true;
	if (FailedAssetCount == 0)
	{
		UE_LOG(LogSlimeFable, Log, TEXT("Skill VFX preload complete: %d loaded; Niagara PSO precache requested"), LoadedSystems.Num());
	}
	else
	{
		UE_LOG(
			LogSlimeFable,
			Warning,
			TEXT("Skill VFX preload complete: %d loaded, %d failed; Niagara PSO precache requested"),
			LoadedSystems.Num(),
			FailedAssetCount);
	}
}

UNiagaraSystem* USlimeSkillVfxSubsystem::ResolveLoadedSystem(
	const TSoftObjectPtr<UNiagaraSystem>& SoftSystem,
	const UObject* Requester)
{
	if (SoftSystem.IsNull())
	{
		return nullptr;
	}
	if (UNiagaraSystem* System = SoftSystem.Get())
	{
		return System;
	}

	static TSet<FSoftObjectPath> WarnedPaths;
	const FSoftObjectPath Path = SoftSystem.ToSoftObjectPath();
	if (!WarnedPaths.Contains(Path))
	{
		WarnedPaths.Add(Path);
		UE_LOG(
			LogSlimeFable,
			Warning,
			TEXT("Skipping unloaded skill VFX '%s' requested by '%s'; gameplay continues without blocking"),
			*Path.ToString(),
			*GetNameSafe(Requester));
	}
	return nullptr;
}
