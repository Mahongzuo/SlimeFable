// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "SideScrollingCameraManager.generated.h"

/**
 *  Simple side scrolling camera with smooth scrolling and horizontal bounds
 */
UCLASS()
class ASideScrollingCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()
	
public:

	/** Overrides the default camera view target calculation */
	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;

public:

	/** How close we want to stay to the view target */
	UPROPERTY(EditAnywhere, Category="Side Scrolling Camera", meta=(ClampMin=0, ClampMax=10000, Units="cm"))
	float CurrentZoom = 1000.0f;

	/** How far above the target do we want the camera to focus */
	UPROPERTY(EditAnywhere, Category="Side Scrolling Camera", meta=(ClampMin=0, ClampMax=10000, Units="cm"))
	float CameraZOffset = 100.0f;

	/** Minimum camera scrolling bounds in world space */
	UPROPERTY(EditAnywhere, Category="Side Scrolling Camera", meta=(ClampMin=-100000, ClampMax=100000, Units="cm"))
	float CameraXMinBounds = -400.0f;

	/** Maximum camera scrolling bounds in world space */
	UPROPERTY(EditAnywhere, Category="Side Scrolling Camera", meta=(ClampMin=-100000, ClampMax=100000, Units="cm"))
	float CameraXMaxBounds = 10000.0f;

protected:

	/** Last cached camera vertical location. The camera only adjusts its height if necessary. */
	float CurrentZ = 0.0f;

	/** First-time update camera setup flag */
	bool bSetup = true;
};
