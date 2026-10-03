#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeArchiveHUDWidget.generated.h"

class UTextBlock;

UCLASS()
class SLIMEFABLE_API USlimeArchiveHUDWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetDisplay(const FString& Status, const FString& Cargo, const FString& Message);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TimerText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CargoText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageText;
	FString CachedStatus, CachedCargo, CachedMessage;
};
