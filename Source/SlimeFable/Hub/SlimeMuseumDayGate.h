// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Hub/SlimeHubInteractActor.h"
#include "SlimeMuseumDayGate.generated.h"

class UStaticMeshComponent;
class ULevelSelectWidget;

/** F opens the calendar. Picking a date travels to that MMDD level. */
UCLASS(Blueprintable, meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeMuseumDayGate : public ASlimeHubInteractActor
{
	GENERATED_BODY()

public:
	ASlimeMuseumDayGate();

	virtual void BeginPlay() override;
	virtual bool TryInteract(APawn* Interactor) override;
	virtual FText GetInteractPromptVerb() const override;
	virtual FVector GetPromptWorldLocation() const override;

	/** Close the calendar if this gate opened it. Used by Esc. */
	static bool CloseOpenCalendar();

	UFUNCTION()
	void HandleCalendarClosed();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Z_Components", meta = (AdvancedDisplay))
	TObjectPtr<UStaticMeshComponent> Frame;

	UPROPERTY()
	TObjectPtr<ULevelSelectWidget> Calendar;

	static TWeakObjectPtr<ASlimeMuseumDayGate> OpenGate;
};
