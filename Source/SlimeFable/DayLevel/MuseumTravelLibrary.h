#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MuseumTravelLibrary.generated.h"

UCLASS()
class SLIMEFABLE_API UMuseumTravelLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Museum", meta = (WorldContext = "Context"))
	static bool FindSafeGround(UObject* Context, FVector Desired, FVector Reference, FVector& Ground);
};
