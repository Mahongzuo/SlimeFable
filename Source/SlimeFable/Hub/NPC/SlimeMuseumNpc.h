#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SlimeNpcCatalog.h"
#include "SlimeMuseumNpc.generated.h"

class ASlimeFarmPlot;

/** Deliberately has no combat, health, lock-on or devour interface. */
UCLASS()
class SLIMEFABLE_API ASlimeMuseumNpc : public ACharacter
{
	GENERATED_BODY()
public:
	ASlimeMuseumNpc();
	bool Configure(const FSlimeNpcSpecies& InSpecies, bool bPreview = false);
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	void SetHighlighted(bool bHighlighted);
	static bool ValidateLocation(UWorld* World, const FSlimeNpcSpecies& Species, const FVector& Feet,
		FVector& OutCenter, const AActor* Ignore = nullptr, FString* OutReason = nullptr);
	FName GetSpeciesId() const { return Definition.SpeciesId; }
	int32 RecordId = INDEX_NONE;
private:
	friend class USlimeNpcVerification;
	void UpdateWander(float DeltaSeconds);
	void UpdateCrops(float DeltaSeconds);
	bool TryStartWander();
	void BeginWalk();
	void StopWander();
	UPROPERTY() FSlimeNpcSpecies Definition;
	UPROPERTY() TArray<TObjectPtr<USkeletalMeshComponent>> Visuals;
	TWeakObjectPtr<ASlimeFarmPlot> CropTarget;
	FVector Anchor = FVector::ZeroVector;
	FVector DirectDest = FVector::ZeroVector;
	FVector WanderLastPos = FVector::ZeroVector;
	float RestSeconds = 1.5f;
	float MoveSeconds = 0.f;
	float WalkLimitSeconds = 0.f;
	float StuckSeconds = 0.f;
	float DwellSeconds = 0.f;
	float CooldownSeconds = 0.f;
	bool bIsPreview = false;
	bool bMoving = false;
	bool bDirectWalk = false;
};
