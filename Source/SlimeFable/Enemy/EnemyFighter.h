// Copyright Epic Games, Inc. All Rights Reserved.



#pragma once



#include "CoreMinimal.h"

#include "EnemyCharacter.h"

#include "EnemyCombatTypes.h"

#include "EnemyFighter.generated.h"



class USphereComponent;



UCLASS()

class SLIMEFABLE_API AEnemyFighter : public AEnemyCharacter

{

	GENERATED_BODY()



public:

	AEnemyFighter();



	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void BeginPlay() override;

	virtual bool IsInCombat() const override;

	virtual void OnRestoredToSpawn() override;



	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter", meta = (ClampMin = "100.0", Units = "cm"))

	float DetectRange = 1000.f;



	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter", meta = (ClampMin = "100.0", Units = "cm"))

	float LeashRange = 1500.f;



	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter", meta = (ClampMin = "50.0", Units = "cm"))

	float PreferredDistance = 280.f;



	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter")

	TArray<FEnemyMoveDef> Moves;



	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Fighter", meta = (ClampMin = "100.0"))

	float WalkSpeed = 450.f;



	const TArray<FEnemyMoveDef>& GetMoves() const { return Moves; }



protected:

	void SyncRangeVisuals();



	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)

	TObjectPtr<USphereComponent> DetectRangeVisual;



	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)

	TObjectPtr<USphereComponent> LeashRangeVisual;

};

