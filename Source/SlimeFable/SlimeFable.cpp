// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeFable.h"
#include "Modules/ModuleManager.h"

#include "CombatDamageable.h"
#include "Combat/SlimeHealthComponent.h"
#include "Engine/HitResult.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Weapons/LyraRangedWeaponInstance.h"

DEFINE_LOG_CATEGORY(LogSlimeFable)

namespace
{
	/**
	 * Lyra bullets on targets without an AbilitySystemComponent (the slime, GASP costume enemies,
	 * training dummies). Lyra's GE damage cannot reach them; feed the Slime damage pipeline instead.
	 */
	void HandleLyraPlainWeaponHit(AActor* HitActor, float Damage, const FHitResult& Hit, APawn* InstigatorPawn, ULyraRangedWeaponInstance* Weapon)
	{
		(void)Weapon;
		if (!HitActor || Damage <= 0.f)
		{
			return;
		}
		const FVector ShotDir = (Hit.ImpactPoint - Hit.TraceStart).GetSafeNormal();
		const FVector Impulse = ShotDir * Damage * 40.f;
		UE_LOG(LogSlimeFable, Verbose, TEXT("Lyra bullet: %s -> %s dmg=%.1f"), *GetNameSafe(InstigatorPawn), *HitActor->GetName(), Damage);

		if (ICombatDamageable* Damageable = Cast<ICombatDamageable>(HitActor))
		{
			Damageable->ApplyDamage(Damage, InstigatorPawn, Hit.ImpactPoint, Impulse);
		}
		else if (USlimeHealthComponent* Health = HitActor->FindComponentByClass<USlimeHealthComponent>())
		{
			Health->ApplyDamage(Damage, InstigatorPawn, Hit.ImpactPoint, Impulse);
		}
		else
		{
			UGameplayStatics::ApplyPointDamage(HitActor, Damage, ShotDir, Hit,
				InstigatorPawn ? InstigatorPawn->GetController() : nullptr, InstigatorPawn, nullptr);
		}

		// Shooter revealed itself by hitting the player; same rule as SlimeHitProbe.
		if (InstigatorPawn && HitActor)
		{
			const APawn* HitPawn = Cast<APawn>(HitActor);
			if (HitPawn && HitPawn->IsPlayerControlled())
			{
				if (USlimeHealthComponent* InstHealth = InstigatorPawn->FindComponentByClass<USlimeHealthComponent>())
				{
					InstHealth->RevealWorldHealthBar();
				}
			}
		}
	}
}

class FSlimeFableModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		FLyraPlainWeaponDamage::OnHit.BindStatic(&HandleLyraPlainWeaponHit);
	}

	virtual void ShutdownModule() override
	{
		FLyraPlainWeaponDamage::OnHit.Unbind();
		FDefaultGameModuleImpl::ShutdownModule();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FSlimeFableModule, SlimeFable, "SlimeFable");
