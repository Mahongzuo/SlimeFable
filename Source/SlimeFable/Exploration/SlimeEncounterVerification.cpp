#include "SlimeEncounterDirector.h"

#if WITH_EDITOR
#include "SlimeEncounterMember.h"
#include "Combat/SlimeDevourTarget.h"
#include "Combat/SlimeHealthComponent.h"
#include "Enemy/LyraShooterEnemy.h"
#include "Enemy/LyraXinShooterEnemy.h"
#include "Enemy/EnemyCombatComponent.h"
#include "SlimeFable.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"

// Opt-in integration run in the actual map, isolated from production/PIE save slots.
// -game -nullrhi -RuinEncounterVerify. Uses the real 60-second UTC timer and real death delegates.
void ASlimeEncounterDirector::VerifyRuntime()
{
	auto Check = [&](bool bOK, const TCHAR* Message)
	{
		UE_LOG(LogSlimeFable, Display, TEXT("EncounterVerify %s: %s"), bOK ? TEXT("PASS") : TEXT("FAIL"), Message);
		bVerificationFailed |= !bOK;
	};
	if (VerificationPhase == 0)
	{
		if (Live.Num() != Points.Num())
		{
			if (GetWorld()->GetTimeSeconds() < 120.f) return;
			Check(false, TEXT("All 15 initial slots spawned within 120 seconds"));
			VerificationPhase = 99;
		}
		else
		{
			Check(Points.Num() == 15 && Live.Num() == 15, TEXT("15 navigable points and 15 live enemies"));
			Check(State->DailySpecies.Num() == 5, TEXT("Five distinct daily species"));
			int32 Lyra = 0;
			for (auto& Pair : Live)
			{
				Lyra += Pair.Value->IsA<ALyraShooterEnemy>() ? 1 : 0;
				auto* M = Pair.Value->FindComponentByClass<USlimeEncounterMember>();
				Check(M && M->IsSleeping() && Pair.Value->IsHidden(), TEXT("Distant enemy is asleep and hidden"));
				if (M)
				{
					auto* H = SlimeDevourUtil::As(Pair.Value)->GetEnemyHealth();
					const float Before = H->CurrentHP;
					M->SetSleeping(false);
					Check(!Pair.Value->IsHidden(), TEXT("Proximity wake restores rendering"));
					M->SetSleeping(true);
					Check(H->CurrentHP == Before, TEXT("Sleep/wake preserves health"));
				}
			}
			Check(Lyra <= 2, TEXT("Combined Lyra quota <=2"));
			VerificationVictim = State->Records[0].Id;
			APawn* Victim = Live.FindRef(VerificationVictim);
			Check(Victim != nullptr, TEXT("Victim exists"));
			if (!Victim) { VerificationPhase = 99; }
			else
			{
				auto* H = SlimeDevourUtil::As(Victim)->GetEnemyHealth();
				// Avoid float-rounded "now" briefly being greater than the double world clock.
				H->InvulnerableUntil = -1.f;
				H->ApplyDamage(H->MaxHP * 100.f, this, Victim->GetActorLocation(), FVector::ZeroVector);
				Check(KillLevel == 1 && Live.Num() == 14, TEXT("Real death adds one global level and frees one slot"));
				ReportDefeat(VerificationVictim);
				Check(KillLevel == 1, TEXT("Duplicate death callback is ignored"));
				SaveState();
				const int64 Due = State->Records[0].DueUtcTicks;
				LoadState();
				Check(KillLevel == 1 && State->Records[0].DueUtcTicks == Due, TEXT("Same-day save/reload preserves level and absolute respawn deadline"));
				VerificationStarted = FPlatformTime::Seconds();
				VerificationPhase = 1;
			}
		}
	}
	else if (VerificationPhase == 1)
	{
		const double Elapsed = FPlatformTime::Seconds() - VerificationStarted;
		if (Elapsed < 58.)
		{
			if (Live.Num() != 14) { Check(false, TEXT("No premature replacement before 60 seconds")); VerificationPhase = 99; }
			return;
		}
		if (Elapsed < 61.) return;
		Check(Live.Num() == 15, TEXT("Population returns to 15 after the real 60-second delay"));
		auto* R = State->Records.FindByPredicate([&](const auto& V) { return V.Id == VerificationVictim; });
		Check(R && !R->bPendingReplacement && R->Level == 1, TEXT("Replacement uses current global level"));
		if (APawn* P = Live.FindRef(VerificationVictim))
		{
			Check(FMath::IsNearlyEqual(USlimeEncounterMember::DamageMultiplier(P), 1.1f), TEXT("Replacement damage multiplier is 1.1"));
			auto* H = SlimeDevourUtil::As(P)->GetEnemyHealth();
			Check(H->MaxHP > 0 && FMath::IsNearlyEqual(H->CurrentHP, H->MaxHP), TEXT("Replacement begins at full scaled health"));
		}
		// The two-bar Xin must remain one enemy, retain both scaled bars, and save phase 2.
		if (ValidSpecies.Contains(TEXT("Xin")) && ValidSpecies.Contains(TEXT("Pig")))
		{
			StartNewDay();
			for (auto& Rec : State->Records) Rec.Species = TEXT("Pig");
			State->Records[0].Species = TEXT("Xin"); State->Records[0].Level = 4;
			TrySpawn(State->Records[0]);
			const FGuid XinId = State->Records[0].Id;
			auto* Xin = Cast<ALyraXinShooterEnemy>(Live.FindRef(XinId));
			Check(Xin != nullptr, TEXT("Xin spawns through the same data-driven path"));
			if (Xin)
			{
				auto* H = Xin->GetEnemyHealth();
				const auto* Base = Xin->GetClass()->GetDefaultObject<ALyraXinShooterEnemy>();
				Check(FMath::IsNearlyEqual(H->MaxHP, Base->Phase1MaxHP * 1.8f), TEXT("Xin first bar receives level-four health scaling"));
				H->InvulnerableUntil = -1.f;
				H->ApplyDamage(H->MaxHP * 100.f, this, Xin->GetActorLocation(), FVector::ZeroVector);
				Check(Xin->GetHealthPhase() == 2 && KillLevel == 0, TEXT("Xin first bar does not award a kill or release its quota"));
				Check(FMath::IsNearlyEqual(H->MaxHP, Base->Phase2MaxHP * 1.8f), TEXT("Xin second bar keeps level-four health scaling"));
				H->CurrentHP = H->MaxHP * .6f; H->OnHealthChanged.Broadcast(H->CurrentHP, H->MaxHP);
				SaveState(); Xin->Destroy(); Live.Remove(XinId); LoadState(); TrySpawn(State->Records[0]);
				Xin = Cast<ALyraXinShooterEnemy>(Live.FindRef(XinId));
				Check(Xin && Xin->GetHealthPhase() == 2 && FMath::IsNearlyEqual(Xin->GetEnemyHealth()->GetHealthPercent(), .6f), TEXT("Save/reload restores Xin phase two and remaining health"));
				if (Xin)
				{
					H = Xin->GetEnemyHealth(); H->InvulnerableUntil = -1.f;
					H->ApplyDamage(H->MaxHP * 100.f, this, Xin->GetActorLocation(), FVector::ZeroVector);
					Check(KillLevel == 1 && State->Records[0].bPendingReplacement, TEXT("Xin final bar awards exactly one kill and queues replacement"));
				}
			}
		}
		// Stress weighted selection and capacity reservations without repeatedly loading/spawning assets.
		bool bQuotasOK = true;
		for (int32 I = 0; I < 100; ++I)
		{
			StartNewDay();
			int32 Lyra = 0;
			for (const auto& Rec : State->Records) Lyra += GetGroup(Rec.Species) == TEXT("Lyra") ? 1 : 0;
			bQuotasOK &= Lyra <= 2 && State->DailySpecies.Num() == 5 && State->Records.Num() == 15;
		}
		Check(bQuotasOK, TEXT("100 daily selections retain 15 slots / five species / Lyra quota"));
		KillLevel = 12; State->LocalDate = TEXT("2000-01-01"); SaveState(); LoadState();
		Check(KillLevel == 0 && State->LocalDate == FDateTime::Now().ToString(TEXT("%Y-%m-%d")), TEXT("Next local date resets difficulty"));
		Check(SlimeEncounterRules::IsDue(50, 60) && !SlimeEncounterRules::IsDue(70, 60), TEXT("Absolute offline deadline comparison"));
		VerificationPhase = 99;
	}
	if (VerificationPhase == 99)
	{
		const FString Result = bVerificationFailed ? TEXT("FAILED") : TEXT("PASSED");
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("RuinEncounter");
		IFileManager::Get().MakeDirectory(*Directory, true);
		FFileHelper::SaveStringToFile(Result + TEXT("\n") + GetEncounterDiagnostics(), *(Directory / TEXT("runtime-verification.txt")));
		UE_LOG(LogSlimeFable, Display, TEXT("EncounterVerify RESULT %s"), *Result);
		VerificationPhase = 100;
		FPlatformMisc::RequestExitWithStatus(false, bVerificationFailed ? 1 : 0);
	}
}
#endif
