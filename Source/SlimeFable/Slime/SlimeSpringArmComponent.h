// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpringArmComponent.h"
#include "SlimeSpringArmComponent.generated.h"

/**
 * SpringArm with collision off by default; safe foot clamp when not clinging.
 * Toggle "Do Collision Test" on the CameraBoom in the editor if needed.
 */
UCLASS(ClassGroup = (Slime), meta = (BlueprintSpawnableComponent, PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API USlimeSpringArmComponent : public USpringArmComponent
{
	GENERATED_BODY()

public:
	USlimeSpringArmComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Camera",
		meta = (ClampMin = "0.0", Units = "cm",
			ToolTip = "相对胶囊底的最低抬高。默认 10。"))
	float MinCameraClearance = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Camera",
		meta = (ClampMin = "50.0", Units = "cm",
			ToolTip = "半径内叠脚下地面抬升；硬地板夹（胶囊底+Clearance）始终生效。默认 150。"))
	float FootClampRadius = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Camera",
		meta = (ClampMin = "0.0", Units = "cm",
			ToolTip = "相对胶囊底最大抬升，防止抬到屋顶。默认 80。"))
	float MaxFootLift = 80.f;

protected:
	virtual void UpdateDesiredArmLocation(
		bool bDoTrace,
		bool bDoLocationLag,
		bool bDoRotationLag,
		float DeltaTime) override;

	void ApplyFootClamp(FVector& InOutLoc) const;
	bool IsOwnerClinging() const;
};
