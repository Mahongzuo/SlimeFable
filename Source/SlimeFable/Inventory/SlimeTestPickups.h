// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeWorldPickup.h"
#include "SlimeTestPickups.generated.h"

class UTexture2D;
class UFileMediaSource;

/** Drop into a level to test consumable pickup (HealJelly). */
UCLASS(Blueprintable)
class SLIMEFABLE_API ASlimePickupConsumable : public ASlimeWorldPickup
{
	GENERATED_BODY()

public:
	ASlimePickupConsumable();
};

/** Drop into a level to test placeable pickup (FlatStone). */
UCLASS(Blueprintable)
class SLIMEFABLE_API ASlimePickupPlaceable : public ASlimeWorldPickup
{
	GENERATED_BODY()

public:
	ASlimePickupPlaceable();

protected:
	virtual void PrepareDefinition(USlimeInventorySubsystem& Inventory) override;
};

/** Drop into a level to test souvenir pickup (OldPostcard). */
UCLASS(Blueprintable)
class SLIMEFABLE_API ASlimePickupSouvenir : public ASlimeWorldPickup
{
	GENERATED_BODY()

public:
	ASlimePickupSouvenir();

	/** 选一张贴图作为该纪念品的展示大图与背包图标；留空则使用默认的旧明信片。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Souvenir")
	TSoftObjectPtr<UTexture2D> SouvenirImage;

	/** 自定义纪念品名称；留空则用贴图资产名。仅在设置了 SouvenirImage 时生效。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Souvenir")
	FText SouvenirDisplayName;

	/** 自定义故事文字；留空则用默认文案。仅在设置了 SouvenirImage 时生效。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Souvenir", meta = (MultiLine = "true"))
	FText SouvenirStoryText;

	/** 故事视频（FileMediaSource）；留空则查看页不显示播放按钮。仅在设置了 SouvenirImage 时生效。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Souvenir")
	TSoftObjectPtr<UFileMediaSource> SouvenirVideo;

protected:
	virtual void PrepareDefinition(USlimeInventorySubsystem& Inventory) override;
};
