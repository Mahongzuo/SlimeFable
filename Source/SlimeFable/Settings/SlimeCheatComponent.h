// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeCheatComponent.generated.h"

class USlimeCheatConsoleWidget;

UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API USlimeCheatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeCheatComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool IsConsoleOpen() const;
	void OpenConsole();
	void CloseConsole();
	/** Run command, resume gameplay, show brief toast. */
	void HandleCommand(const FString& RawCommand);

protected:
	UPROPERTY()
	TObjectPtr<USlimeCheatConsoleWidget> ConsoleWidget = nullptr;

	bool bInputOpen = false;
};
