// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeFoliageInteractSubsystem.h"

#include "SlimeFable.h"
#include "Camera/PlayerCameraManager.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/SpectatorPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "SlimeFoliageInteractComponent.h"
#include "UObject/SoftObjectPath.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarFoliageInteractDebug(
	TEXT("slime.FoliageInteract.Debug"),
	0,
	TEXT("Log foliage interact MPC writes (0=off, 1=once per 0.5s)."),
	ECVF_Cheat);

static TAutoConsoleVariable<FString> CVarFoliageInteractMpc(
	TEXT("slime.FoliageInteract.Mpc"),
	TEXT(""),
	TEXT("Override soft path for MPC_SlimeFoliage. Empty uses the default asset."),
	ECVF_Default);

const TCHAR* USlimeFoliageInteractSubsystem::MpcSoftPath =
	TEXT("/Game/_Slime/Environment/FoliageInteract/MPC_SlimeFoliage.MPC_SlimeFoliage");

void USlimeFoliageInteractSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	EnsureMpcLoaded();
}

void USlimeFoliageInteractSubsystem::Deinitialize()
{
	Interactors.Reset();
	MpcAsset = nullptr;
	Super::Deinitialize();
}

TStatId USlimeFoliageInteractSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USlimeFoliageInteractSubsystem, STATGROUP_Tickables);
}

void USlimeFoliageInteractSubsystem::RegisterInteractor(USlimeFoliageInteractComponent* Component)
{
	if (!Component)
	{
		return;
	}
	Interactors.AddUnique(Component);
}

void USlimeFoliageInteractSubsystem::UnregisterInteractor(USlimeFoliageInteractComponent* Component)
{
	Interactors.RemoveAll([Component](const TWeakObjectPtr<USlimeFoliageInteractComponent>& Ptr)
	{
		return !Ptr.IsValid() || Ptr.Get() == Component;
	});
}

void USlimeFoliageInteractSubsystem::EnsureMpcLoaded()
{
	if (MpcAsset)
	{
		return;
	}
	FString Path = CVarFoliageInteractMpc.GetValueOnGameThread();
	if (Path.IsEmpty())
	{
		Path = MpcSoftPath;
	}
	MpcAsset = Cast<UMaterialParameterCollection>(FSoftObjectPath(Path).TryLoad());
}

void USlimeFoliageInteractSubsystem::ClearAllSlots(UMaterialParameterCollectionInstance* Instance) const
{
	if (!Instance)
	{
		return;
	}
	for (int32 Slot = 0; Slot < MaxSlots; ++Slot)
	{
		WriteSlot(Instance, Slot, FVector::ZeroVector, FVector::ZeroVector, 0.f, 0.f);
	}
}

void USlimeFoliageInteractSubsystem::WriteSlot(UMaterialParameterCollectionInstance* Instance,
	int32 SlotIndex, const FVector& Location, const FVector& Velocity, float Radius, float Strength) const
{
	if (!Instance || SlotIndex < 0 || SlotIndex >= MaxSlots)
	{
		return;
	}

	const FName PosName(*FString::Printf(TEXT("Interact%d_Pos"), SlotIndex));
	const FName MoveName(*FString::Printf(TEXT("Interact%d_Move"), SlotIndex));

	const FLinearColor Pos(Location.X, Location.Y, Location.Z, Radius);
	const float Speed2D = FVector(Velocity.X, Velocity.Y, 0.f).Size();
	const float Speed01 = FMath::Clamp(Speed2D / 400.f, 0.f, 1.f);
	const FLinearColor Move(Velocity.X, Velocity.Y, Speed01, Strength);

	Instance->SetVectorParameterValue(PosName, Pos);
	Instance->SetVectorParameterValue(MoveName, Move);
}

void USlimeFoliageInteractSubsystem::WriteGlobals(UMaterialParameterCollectionInstance* Instance,
	float MaxBend, float TrailSeconds, float IdlePartStrength,
	float GrassHeight, float Flatten) const
{
	if (!Instance)
	{
		return;
	}
	Instance->SetScalarParameterValue(FName(TEXT("MaxBend")), MaxBend);
	Instance->SetScalarParameterValue(FName(TEXT("TrailSeconds")), TrailSeconds);
	Instance->SetScalarParameterValue(FName(TEXT("IdlePartStrength")), IdlePartStrength);
	Instance->SetScalarParameterValue(FName(TEXT("GrassHeight")), GrassHeight);
	Instance->SetScalarParameterValue(FName(TEXT("Flatten")), Flatten);
}

void USlimeFoliageInteractSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	const bool bGameplayPawn = PlayerPawn && !PlayerPawn->IsA<ASpectatorPawn>();
	if (!bGameplayPawn)
	{
		EnsureMpcLoaded();
		if (MpcAsset)
		{
			if (UMaterialParameterCollectionInstance* Instance = World->GetParameterCollectionInstance(MpcAsset))
			{
				ClearAllSlots(Instance);
				WriteGlobals(Instance, 36.f, 0.2f, 0.45f, 80.f, 20.f);
			}
		}
		return;
	}

	EnsureMpcLoaded();
	if (!MpcAsset)
	{
		return;
	}

	UMaterialParameterCollectionInstance* Instance = World->GetParameterCollectionInstance(MpcAsset);
	if (!Instance)
	{
		return;
	}

	Interactors.RemoveAll([](const TWeakObjectPtr<USlimeFoliageInteractComponent>& Ptr)
	{
		return !Ptr.IsValid();
	});

	struct FCandidate
	{
		USlimeFoliageInteractComponent* Comp = nullptr;
		FSlimeFoliageInteractSample Sample;
		float DistSq = 0.f;
		bool bPlayer = false;
	};

	const FVector PlayerLoc = PlayerPawn->GetActorLocation();

	TArray<FCandidate> Candidates;
	Candidates.Reserve(Interactors.Num());

	float GlobalMaxBend = 36.f;
	float GlobalTrailSeconds = 0.2f;
	float GlobalIdlePart = 0.45f;
	float GlobalGrassHeight = 80.f;
	float GlobalFlatten = 20.f;
	bool bHavePlayerGlobals = false;

	for (const TWeakObjectPtr<USlimeFoliageInteractComponent>& Weak : Interactors)
	{
		USlimeFoliageInteractComponent* Comp = Weak.Get();
		if (!Comp)
		{
			continue;
		}

		const FSlimeFoliageInteractSample Sample = Comp->GetLatestSample();
		if (Sample.Strength <= KINDA_SMALL_NUMBER || Sample.Radius <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FCandidate Cand;
		Cand.Comp = Comp;
		Cand.Sample = Sample;
		Cand.bPlayer = Sample.bPlayerControlled;
		Cand.DistSq = PlayerPawn
			? FVector::DistSquared(PlayerLoc, Sample.Location)
			: 0.f;
		Candidates.Add(Cand);

		if (Cand.bPlayer && !bHavePlayerGlobals)
		{
			GlobalMaxBend = Comp->MaxBend;
			GlobalTrailSeconds = Comp->TrailSeconds;
			GlobalIdlePart = Comp->IdlePartStrength;
			GlobalGrassHeight = Comp->GrassHeight;
			GlobalFlatten = Comp->Flatten;
			bHavePlayerGlobals = true;
		}
	}

	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		if (A.bPlayer != B.bPlayer)
		{
			return A.bPlayer; // player first
		}
		return A.DistSq < B.DistSq;
	});

	ClearAllSlots(Instance);
	WriteGlobals(Instance, GlobalMaxBend, GlobalTrailSeconds, GlobalIdlePart,
		GlobalGrassHeight, GlobalFlatten);

	FVector ViewLoc = FVector::ZeroVector;
	if (PC->PlayerCameraManager)
	{
		ViewLoc = PC->PlayerCameraManager->GetCameraLocation();
	}
	else if (PlayerPawn)
	{
		ViewLoc = PlayerPawn->GetActorLocation();
	}

	const int32 WriteCount = FMath::Min(MaxSlots, Candidates.Num());
	for (int32 Slot = 0; Slot < WriteCount; ++Slot)
	{
		const FSlimeFoliageInteractSample& S = Candidates[Slot].Sample;
		WriteSlot(Instance, Slot, S.Location - ViewLoc, S.Velocity, S.Radius, S.Strength);
	}

	const bool bDebug = CVarFoliageInteractDebug.GetValueOnGameThread() > 0;
	if (bDebug)
	{
		for (int32 Slot = 0; Slot < WriteCount; ++Slot)
		{
			const FSlimeFoliageInteractSample& S = Candidates[Slot].Sample;
			DrawDebugSphere(World, S.Location, S.Radius, 16,
				S.bPlayerControlled ? FColor::Green : FColor::Red,
				false, 0.f);
		}

		static float Acc = 0.f;
		Acc += DeltaTime;
		if (Acc >= 0.5f)
		{
			Acc = 0.f;
			UE_LOG(LogSlimeFable, Log,
				TEXT("FoliageInteract: PC=%s pawn=%s interactors=%d wrote=%d maxBend=%.1f view=(%.0f,%.0f,%.0f)"),
				*GetNameSafe(PC), *GetNameSafe(PlayerPawn),
				Interactors.Num(), WriteCount, GlobalMaxBend,
				ViewLoc.X, ViewLoc.Y, ViewLoc.Z);
			for (int32 Slot = 0; Slot < WriteCount; ++Slot)
			{
				const FSlimeFoliageInteractSample& S = Candidates[Slot].Sample;
				const FVector Rel = S.Location - ViewLoc;
				UE_LOG(LogSlimeFable, Log,
					TEXT("  slot%d abs=(%.0f,%.0f,%.0f) camRel=(%.0f,%.0f,%.0f) r=%.0f str=%.2f player=%d"),
					Slot, S.Location.X, S.Location.Y, S.Location.Z,
					Rel.X, Rel.Y, Rel.Z,
					S.Radius, S.Strength, S.bPlayerControlled ? 1 : 0);
			}
		}
	}
}
