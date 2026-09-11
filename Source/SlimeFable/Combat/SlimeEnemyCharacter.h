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

UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeEnemyCharacter : public ASlimeCharacter, public ISlimeLockTarget
{
	GENERATED_BODY()

public:
	ASlimeEnemyCharacter();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Enemy")
	ESlimeElement StartingElement = ESlimeElement::Wind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Enemy")
	bool bStationaryTraining = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD", meta = (ClampMin = "-200.0", ClampMax = "800.0", Units = "cm"))
	float HealthBarZOffset = 72.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|HUD", meta = (ClampMin = "100.0", Units = "cm",
		ToolTip = "超过这个距离不显示头顶血条。默认 500（5 米）。"))
	float HealthBarVisibleRange = 500.f;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual bool CanBeLockedOn() const override;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual FVector GetLockOnLocation() const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UWidgetComponent> HealthBar;

	void ApplyHealthBarOffset();
	void RefreshWorldHealthBarVisibility();
	void ApplyStartingElement();

	UFUNCTION()
	void HandleEnemyDied();
};
