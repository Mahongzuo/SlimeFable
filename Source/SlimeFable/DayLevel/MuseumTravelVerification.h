#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MuseumTravelVerification.generated.h"

/** Opt-in headless integration checks. Never instantiated in ordinary play. */
UCLASS()
class UMuseumTravelVerification : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
private:
	bool Tick(float Delta);
	void Check(bool bPassed, const FString& Message);
	void Finish();
	FTSTicker::FDelegateHandle TickHandle;
	int32 Phase = 0;
	int32 FarmCount = 0;
	double Started = 0;
	bool bFailed = false;
	FString Report;
};
