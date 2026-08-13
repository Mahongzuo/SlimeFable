// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatDamageable.h"
#include "SlimeCharacter.h"
#include "SlimeLockTarget.h"
#include "SlimeElementTypes.h"
#include "SlimeEnemyCharacter.generated.h"

class UWidgetComponent;
class USlimeWorldHealthBar;

UCLASS()
class SLIMEFABLE_API ASlimeEnemyCharacter : public ASlimeCharacter, public ISlimeLockTarget
{
	GENERATED_BODY()

public:
	ASlimeEnemyCharacter();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Enemy")
	ESlimeElement StartingElement = ESlimeElement::Wind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Enemy")
	bool bStationaryTraining = false;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual bool CanBeLockedOn() const override;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual FVector GetLockOnLocation() const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UWidgetComponent> HealthBar;

	void ApplyStartingElement();

	UFUNCTION()
	void HandleEnemyDied();
};
