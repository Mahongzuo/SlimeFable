// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeBuildModeComponent.generated.h"

class USlimePlaceableDefinition;
class USlimeHomeBuildWidget;
class ASlimePlacePreview;
class USlimeHomeBuildCatalog;
class ASlimeHomeBuildManager;
class USlimeHomeBuildSubsystem;

UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent))
class SLIMEFABLE_API USlimeBuildModeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeBuildModeComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** True while the catalog is open or a piece is being aimed. Combat and skills should wait. */
	bool IsBuildInputActive() const { return bCatalogOpen || bPlacing; }

	/** True while placing or clearing. Scroll adjusts the current mode and does not zoom the camera. */
	bool IsPlacementActive() const { return bPlacing; }

	/** Escape while aiming exits build mode and consumes the key. */
	bool HandleEscape();

	bool BeginBagPlacement(USlimePlaceableDefinition* Definition);
	void BeginClearMode();

	void NotifyCatalogChosen(FName EntryId, bool bFromBag);
	void NotifyCatalogClear();
	void NotifyCatalogClosed();
	void RequestExit(const TCHAR* Reason);

	void SetPreviewMesh(class UStaticMesh* Mesh);

private:
	enum class EPlaceAdjust : uint8
	{
		Height,
		Yaw,
		Pitch,
		Scale
	};

	void ToggleCatalog();
	void OpenCatalog();
	void CloseCatalog();
	void DiscardCatalogWidget();
	void ExitPlacement(const TCHAR* Reason);
	void CycleAdjustMode();
	bool IsPlacingFluid() const;
	const TCHAR* AdjustModeLabel() const;
	void UpdateAim();
	void Confirm();
	void ClearAimed();
	bool ComputeAim(int32& OutAnchorX, int32& OutAnchorY, float& OutBaseZ, bool& bOutOnBuild, float IncomingHeight, FVector& OutImpact) const;
	void SelectBarIndex(int32 Index);
	void EnsurePreview();
	void RefreshStatus();
	USlimeHomeBuildSubsystem* GetHome() const;
	void Screen(const FString& Text) const;

	UPROPERTY()
	TObjectPtr<USlimeHomeBuildWidget> CatalogWidget;

	UPROPERTY()
	TObjectPtr<ASlimePlacePreview> PreviewActor;

	bool bCatalogOpen = false;
	bool bPlacing = false;
	bool bClearMode = false;
	bool bAimOnBuild = false;
	bool bAimValid = false;
	int32 AimAnchorX = 0;
	int32 AimAnchorY = 0;
	float AimBaseZ = 0.f;
	int32 AimedRecord = INDEX_NONE;
	int32 YawSteps = 0;
	float HeightOffset = 0.f;
	float PitchDegrees = 0.f;
	float UserScale = 1.f;
	float ModeToastLeft = 0.f;
	EPlaceAdjust AdjustMode = EPlaceAdjust::Height;

	FName ActiveId = NAME_None;
	bool bActiveFromBag = false;

	TArray<FName> BarIds;
	TArray<bool> BarFromBag;
	int32 BarIndex = INDEX_NONE;

	float TraceDistance = 1200.f;
};
