// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "Farm/SlimeCropTypes.h"
#include "Farm/SlimeElementReceiver.h"
#include "Hub/SlimeHubInteractActor.h"
#include "SlimeFarmPlot.generated.h"

class UMaterialInstanceDynamic;
class USlimeProceduralPlantComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UWidgetComponent;
class ASlimeHubSkyDirector;

/** Where a soil mesh sits on the ground. bValid is false when the slope is too steep to bury the bed. */
struct SLIMEFABLE_API FSlimeBedPlacement
{
	bool bValid = false;
	FVector ActorLocation = FVector::ZeroVector;
	float YawDegrees = 0.f;
	FVector SoilScale = FVector::OneVector;
	float SoilRelativeZ = 0.f;
	float PlantLocalZ = 8.f;
	float HalfX = 100.f;
	float HalfY = 100.f;
	int32 CellsX = 1;
	int32 CellsY = 1;
};

UCLASS(Blueprintable, meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeFarmPlot : public ASlimeHubInteractActor, public ISlimeElementReceiver
{
	GENERATED_BODY()

public:
	ASlimeFarmPlot();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool TryInteract(APawn* Interactor) override;
	virtual FText GetInteractPromptVerb() const override;
	virtual FVector GetPromptWorldLocation() const override;

	virtual void ReceiveElement_Implementation(ESlimeElement Element, AActor* SourceActor, float Strength) override;

	bool CanReceiveLightning() const;
	bool GetNpcDamagePoint(const FVector& From, float Range, FVector& OutPoint) const;
	bool DestroyCropByNpc();

	/** Plant the chosen crop. Seeds are not consumed. */
	bool PlantCrop(USlimeCropDefinition* NextCrop);

	/** Skip the EndPlay save so a demolished bed does not write its record back. */
	void MarkRemoved();

	UStaticMeshComponent* GetSoilComponent() const { return Soil; }

	/** Optional soil mesh. Planters pass bInRaisedBed = false so the pot sits on the ground. */
	void ConfigureSoil(UStaticMesh* Mesh, bool bInRaisedBed);

	/**
	 * Fit a soil mesh to the 50cm grid and the ground under CenterXY.
	 * Thin axes (under 20cm) keep the long-axis scale so fences and furrows are not stretched.
	 * Raised beds put their top 8cm above the highest hit and sink at least 5cm below the lowest.
	 */
	static FSlimeBedPlacement ComputeBedPlacement(
		UWorld* World,
		const AActor* Ignore,
		UStaticMesh* Mesh,
		bool bRaisedBed,
		const FVector& CenterXY,
		float YawDegrees);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Farm",
		meta = (ToolTip = "存档键。同一块地保持这个名字，换关再回来进度还在。留空则用关卡里这个 Actor 的名字。"))
	FName PlotId = NAME_None;

protected:
	void LoadAndCatchUp();
	void SaveRecord(bool bFlush);
	void RefreshVisual();
	void RefreshStatus();
	void UpdateStatusVisibility();
	float ComputeGrowthPerSecond(bool bOffline) const;
	ASlimeHubSkyDirector* FindSky() const;
	void NoteElement(ESlimeElement Element);
	bool HadElementRecently(ESlimeElement Element, float WindowSeconds) const;
	void RaiseQuality();
	void FitSoil();
	void RebuildPlants(bool bForce);
	int32 ResolvePlantSeed() const;
	void CollectPlantLayout(int32& OutNx, int32& OutNy, float& OutAvgSize, float& OutMaxSize) const;
	void GetPlantSpan(float& OutSpanX, float& OutSpanY, bool& bOutRidge) const;
	float PlantSizeAt(int32 Slot, int32 Nx) const;
	const FSlimeCropStage* PickStage(float& OutScale) const;
	void PreloadCropMeshes();
	void HandleCropMeshesLoaded();
	void CacheSoilLook();

	UPROPERTY(VisibleAnywhere, Category = "Z_Components", meta = (AdvancedDisplay))
	TObjectPtr<UStaticMeshComponent> Soil;

	UPROPERTY(VisibleAnywhere, Category = "Z_Components", meta = (AdvancedDisplay))
	TObjectPtr<USlimeProceduralPlantComponent> Plant;

	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> PlantIsms;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SoilMeshAsset;

	bool bRaisedBed = true;
	bool bHarvestedLook = false;
	bool bSkipSaveOnEnd = false;
	bool bSoilUsesColor = false;
	int32 PlantSeed = 0;
	float PopLeft = 0.f;
	int32 LastStageIndex = INDEX_NONE;
	int32 LastVisualKey = MIN_int32;
	float PlantLocalZ = 8.f;
	float BedHalfX = 100.f;
	float BedHalfY = 100.f;

	UPROPERTY(VisibleAnywhere, Category = "Z_Components", meta = (AdvancedDisplay))
	TObjectPtr<UWidgetComponent> StatusWidget;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SoilMid;

	UPROPERTY()
	TObjectPtr<USlimeCropDefinition> Crop;

	ESlimeFarmPlotState State = ESlimeFarmPlotState::Empty;
	ESlimeCropQuality Quality = ESlimeCropQuality::Common;
	float Growth01 = 0.f;
	float Moisture = 0.45f;
	float Fertility = 0.35f;
	float HueShift = 0.f;
	int64 LastUpdateUnix = 0;
	float DrySeconds = 0.f;
	float SteamSeconds = 0.f;
	float FireBoostSeconds = 0.f;
	float Heat = 0.f;
	float PreferredSeconds = 0.f;
	float SaveAccumulator = 0.f;
	float StatusAccumulator = 0.f;
	float LastWet01 = -1.f;

	TSharedPtr<FStreamableHandle> CropMeshHandle;

	struct FRecentElement
	{
		ESlimeElement Element = ESlimeElement::Water;
		float TimeSeconds = 0.f;
	};
	TArray<FRecentElement> RecentElements;

	mutable TWeakObjectPtr<ASlimeHubSkyDirector> CachedSky;
};
