#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SlimeNpcVerification.generated.h"

/** Opt-in integration run. Requires a separate RuntimeProfile UserDir. */
UCLASS()
class USlimeNpcVerification : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
private:
	bool Tick(float Dt);
	void Check(bool Result, const FString& Message);
	void Finish();
	void KillSpecies(FName Id);
	void TestPlacement();
	void TestCrops();
	void CheckRestored();
	FTSTicker::FDelegateHandle Handle;
	double Started = 0;
	int32 Phase = 0;
	bool bFailed = false;
	FString Report;
};
