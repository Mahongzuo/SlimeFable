// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeAuraMarkerWidget.generated.h"

class UTextBlock;
enum class ESlimeElement : uint8;

/** Tiny screen-space label above enemies showing applied element aura. */
UCLASS()
class SLIMEFABLE_API USlimeAuraMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void SetAura(ESlimeElement Element, bool bVisible);
	void SetReactionResidue(const FText& ReactionName, const FLinearColor& Color);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	bool bBuiltInCode = false;
};
