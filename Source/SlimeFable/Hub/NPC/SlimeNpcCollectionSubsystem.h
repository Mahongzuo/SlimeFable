#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SlimeNpcCollectionSubsystem.generated.h"

class USlimeNpcCatalog;
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSlimeNpcCollected, FName);

UCLASS()
class USlimeNpcCollectionSave : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) int32 Version = 1;
	UPROPERTY(SaveGame) TSet<FName> Unlocked;
};

UCLASS()
class SLIMEFABLE_API USlimeNpcCollectionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	UFUNCTION(BlueprintPure, Category="NPC") bool IsUnlocked(FName SpeciesId) const;
	bool ReportDefeat(AActor* Enemy);
	bool ReportDevour(AActor* Enemy);
	bool IsEligibleWorld(const UWorld* World) const;
	USlimeNpcCatalog* GetCatalog();
	FOnSlimeNpcCollected OnCollected;
private:
	bool TryCollect(AActor* Enemy, bool bAllowLiving);
	bool Flush();
	UPROPERTY() TObjectPtr<USlimeNpcCollectionSave> State;
	UPROPERTY() TObjectPtr<USlimeNpcCatalog> Catalog;
	bool bDirty = false;
	bool bLoadFailed = false;
};
