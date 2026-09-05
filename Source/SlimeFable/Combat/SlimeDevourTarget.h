// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Templates/Function.h"
#include "Enemy/EnemyCombatTypes.h"
#include "SlimeDevourTarget.generated.h"

class APawn;
class UCapsuleComponent;
class UEnemyCombatComponent;
class UMeshComponent;
class USkeletalMeshComponent;
class USlimeHealthComponent;
class USlimeStatusComponent;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class USlimeDevourTarget : public UInterface
{
	GENERATED_BODY()
};

/**
 * Devour / morph / phantom contract for both AEnemyCharacter (CMC) and AGaspMoverEnemy (Mover).
 */
class SLIMEFABLE_API ISlimeDevourTarget
{
	GENERATED_BODY()

public:
	virtual bool IsDevourableNow() const = 0;
	virtual float GetDevourHealthThreshold() const = 0;
	virtual float GetHealthPercent() const = 0;
	virtual FText GetResolvedDisplayName() const = 0;
	virtual FLinearColor ResolveDevourWheelTint() const = 0;

	virtual USkeletalMeshComponent* GetPrimarySkeletalMesh() const = 0;
	virtual USkeletalMeshComponent* GetDevourPreviewMesh() const = 0;
	/** Mesh Morph should treat as primary (skin + visibility). Default = GetPrimarySkeletalMesh. */
	virtual USkeletalMeshComponent* GetMorphVisualMesh() const { return GetPrimarySkeletalMesh(); }
	virtual UCapsuleComponent* GetDevourCapsule() const = 0;
	virtual USlimeHealthComponent* GetEnemyHealth() const = 0;
	virtual USlimeStatusComponent* GetEnemyStatus() const = 0;
	virtual UEnemyCombatComponent* GetEnemyCombat() const = 0;
	/** Combat move kit for AI / morph player input. Default empty. */
	virtual const TArray<FEnemyMoveDef>& GetEnemyMoves() const
	{
		static const TArray<FEnemyMoveDef> Empty;
		return Empty;
	}

	virtual void ForEachVisualMesh(TFunctionRef<void(UMeshComponent*)> Fn) const = 0;
	virtual void InitAsMorphTarget(AActor* Master) = 0;
	virtual void InitAsPhantom(float LifeSeconds, AActor* Master) = 0;
	virtual void BeginDevouredDeath(AActor* Devourer) = 0;
	virtual void SetDevourLocked(bool bLocked) = 0;
	virtual bool IsDevourLocked() const = 0;
	virtual bool IsMorphTarget() const = 0;
	virtual bool IsInDeathSequence() const = 0;
	virtual bool IsDevouredDeath() const = 0;
	virtual bool UsesSingleNodeAnims() const = 0;
	virtual bool UsesMoverMovement() const = 0;
	virtual void FreezeForDevour() = 0;
	virtual void RestoreFromDevour() = 0;
	virtual void StopMeshAnimation() = 0;
	virtual void ClearElementAuraFlash() = 0;
	virtual void PlayElementAuraFlash(ESlimeElement Element, float Duration)
	{
		(void)Element;
		(void)Duration;
	}
	virtual void PlayElementAuraFlashByColor(FLinearColor Color, float Duration)
	{
		(void)Color;
		(void)Duration;
	}
	virtual FVector GetVisualBoundsCenter() const = 0;
	virtual FVector GetHudAnchorLocation() const = 0;
	virtual float GetHealthBarZOffset() const = 0;
	virtual bool GetStableMeshBounds(FBox& OutBox) const = 0;
	virtual float GetMorphCameraArmLengthMin() const = 0;
	virtual void SetMorphGameplayEnabled(bool bEnabled) = 0;
	virtual void RefreshHealthBarAnchor() = 0;
	virtual TSubclassOf<APawn> GetDevourSpawnClass() const = 0;
};

namespace SlimeDevourUtil
{
	inline ISlimeDevourTarget* As(AActor* Actor)
	{
		return Cast<ISlimeDevourTarget>(Actor);
	}

	inline const ISlimeDevourTarget* As(const AActor* Actor)
	{
		return Cast<ISlimeDevourTarget>(Actor);
	}

	USkeletalMeshComponent* GetPrimaryMesh(AActor* Actor);
	inline USkeletalMeshComponent* GetPrimaryMesh(const AActor* Actor)
	{
		return GetPrimaryMesh(const_cast<AActor*>(Actor));
	}
	USkeletalMeshComponent* GetPreviewMesh(AActor* Actor);
	inline USkeletalMeshComponent* GetPreviewMesh(const AActor* Actor)
	{
		return GetPreviewMesh(const_cast<AActor*>(Actor));
	}
	UCapsuleComponent* GetCapsule(AActor* Actor);
	inline UCapsuleComponent* GetCapsule(const AActor* Actor)
	{
		return GetCapsule(const_cast<AActor*>(Actor));
	}
}
