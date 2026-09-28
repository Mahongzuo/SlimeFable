// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Farm/SlimeCropTypes.h"
#include "Farm/SlimeElementReceiver.h"
#include "Hub/SlimeHubInteractActor.h"
#include "SlimeFarmPlot.generated.h"

class UMaterialInstanceDynamic;
class USlimeProceduralPlantComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class ASlimeHubSkyDirector;

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

	UPROPERTY(VisibleAnywhere, Category = "Z_Components", meta = (AdvancedDisplay))
	TObjectPtr<UStaticMeshComponent> Soil;

	UPROPERTY(VisibleAnywhere, Category = "Z_Components", meta = (AdvancedDisplay))
	TObjectPtr<USlimeProceduralPlantComponent> Plant;

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

	struct FRecentElement
	{
		ESlimeElement Element = ESlimeElement::Water;
		float TimeSeconds = 0.f;
	};
	TArray<FRecentElement> RecentElements;

	mutable TWeakObjectPtr<ASlimeHubSkyDirector> CachedSky;
};
