// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MenuButtonHoverHelper.generated.h"

class UButton;
class UTextBlock;

UCLASS()
class SLIMEFABLE_API UMenuButtonHoverHelper : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UButton> Button;

	UPROPERTY()
	TObjectPtr<UTextBlock> Label;

	FLinearColor NormalLabelColor = FLinearColor::White;

	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();
};
