#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DayLevel/DayLevelTypes.h"
#include "MuseumPortalSubsystem.generated.h"

/** Only binds existing slots. Never rebuilds the museum or farm. */
UCLASS()
class SLIMEFABLE_API UMuseumPortalSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void OnWorldBeginPlay(UWorld& World) override;
	virtual void Deinitialize() override;
	void Refresh();
	static bool ValidateLayout(const FDayLevelEntry& Entry, const TArray<int32>& Slots, FString& Error);
private:
	FDelegateHandle DateChangedHandle;
};
