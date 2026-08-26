// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeElementTypes.h"
#include "SlimeAuraMarkerWidget.generated.h"

class UTextBlock;

/** Tiny screen-space label above enemies showing applied element aura. */
UCLASS()
class SLIMEFABLE_API USlimeAuraMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void SetAura(ESlimeElement Element, float RemainingSec, bool bVisible);
	void SetReactionResidue(const FText& ReactionName, const FLinearColor& Color);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	bool bBuiltInCode = false;
	bool bAuraVisible = false;
	ESlimeElement DisplayedElement = ESlimeElement::Water;
	float DisplayedRemaining = 0.f;
};
