// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeInteractComponent.generated.h"

class ASlimeWorldPickup;
class ASlimePlacedActor;
class AQuestInteractActor;
class USlimeInventoryWidget;

UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API USlimeInteractComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeInteractComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TryInteract();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ToggleInventory();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void CloseInventory();

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsInventoryOpen() const { return InventoryWidget != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	ASlimeWorldPickup* GetFocusedPickup() const { return FocusedPickup.Get(); }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	ASlimePlacedActor* GetFocusedPlaced() const { return FocusedPlaced.Get(); }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	AQuestInteractActor* GetFocusedQuest() const { return FocusedQuest.Get(); }

	/** World location for the F-prompt (pickup or placed). Zero if none. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool GetFocusedPromptWorldLocation(FVector& OutLocation) const;

	/** Verb for the focused interactable (e.g. 拾取 / 使用载具). Empty if none. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	FText GetFocusedPromptVerb() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "100.0", Units = "cm"))
	float InteractRadius = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bPollKeys = true;

protected:
	void PollKeys();
	void RefreshFocusedTarget();
	bool CanInteractNow() const;

	TWeakObjectPtr<ASlimeWorldPickup> FocusedPickup;
	TWeakObjectPtr<ASlimePlacedActor> FocusedPlaced;
	TWeakObjectPtr<AQuestInteractActor> FocusedQuest;

	UPROPERTY(Transient)
	TObjectPtr<USlimeInventoryWidget> InventoryWidget;
};
