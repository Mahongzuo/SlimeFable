/*
* Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
* NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
* property and proprietary rights in and to this material, related
* documentation and any modifications thereto. Any use, reproduction,
* disclosure or distribution of this material and related documentation
* without an express license agreement from NVIDIA CORPORATION or
* its affiliates is strictly prohibited.
*/

#pragma once

#include "DLSSUpscaler.h"

#include "CoreMinimal.h"
#if ENGINE_SUPPORTS_UPSCALER_MODULAR_FEATURE
#include "IUpscalerModularFeature.h"
#include "Misc/Optional.h"
#include "StructUtils/PropertyBag.h"
#include "Templates/SharedPointer.h"
#endif

#include "DLSSUpscalerModularFeature.generated.h"


class ISceneViewExtension;
struct FSceneViewExtensionContext;

#if ENGINE_SUPPORTS_UPSCALER_MODULAR_FEATURE
using namespace UE::VirtualProduction;
#endif


/** DLSS Quality modes. */
UENUM()
enum class EDLSSUpscalerModularFeatureQuality : uint8
{
	Auto             UMETA(DisplayName = "Auto", ToolTip = "Use Auto to select best quality setting for a given resolution"),
	UltraQuality     UMETA(DisplayName = "Ultra Quality"),
	Quality          UMETA(DisplayName = "Quality"),
	Balanced         UMETA(DisplayName = "Balanced"),
	Performance      UMETA(DisplayName = "Performance"),
	UltraPerformance UMETA(DisplayName = "Ultra Performance"),
	DLAA             UMETA(DisplayName = "DLAA"),
	Count            UMETA(Hidden)
};

/**
* DLSS settings used by the Modular Feature.
*/
USTRUCT(BlueprintType)
struct FDLSSUpscalerModularFeatureSettings
{
	GENERATED_BODY()

	/** DLSS quality. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DLSS", meta = (DisplayName = "Quality"))
	EDLSSUpscalerModularFeatureQuality Quality = EDLSSUpscalerModularFeatureQuality::Auto;
};


#if ENGINE_SUPPORTS_UPSCALER_MODULAR_FEATURE
/**
 * DLSS temporal upscaler modular feature
 */
class FDLSSTemporalUpscalerModularFeature
	: public IUpscalerModularFeature
	, public TSharedFromThis<FDLSSTemporalUpscalerModularFeature, ESPMode::ThreadSafe>
{
public:
	virtual ~FDLSSTemporalUpscalerModularFeature() = default;

public:
	/** Auxiliary function for obtaining the singleton of the TemporalUpscaler MF. */
	static const FDLSSTemporalUpscalerModularFeature* Get()
	{
		return ModularFeatureSingleton.Get();
	}

	static void RegisterModularFeature();
	static void UnregisterModularFeature();

public:
	//~ Begin IUpscalerModularFeature
	virtual const FName& GetName() const override;
	virtual const FText& GetDisplayName() const override;
	virtual const FText& GetTooltipText() const override;

	virtual bool IsFeatureEnabled() const override;

	virtual bool AddSceneViewExtensionIsActiveFunctor(const FSceneViewExtensionIsActiveFunctor& IsActiveFunction) override;
	virtual bool RemoveSceneViewExtensionIsActiveFunctor(const FGuid& FunctorGuid) override;

	virtual bool GetSettings(FInstancedPropertyBag& OutSettings) const override;

	virtual void SetupSceneView(
		const FInstancedPropertyBag& InUpscalerSettings,
		FSceneView& InOutView) override;

	virtual bool PostConfigureViewFamily(
		const FInstancedPropertyBag& InUpscalerSettings,
		const FUpscalerModularFeatureParameters& InUpscalerParam,
		FSceneViewFamilyContext& InOutViewFamily) override;
	//~ End IUpscalerModularFeature

public:
	/** Iterate over all registered Functors and return a consolidated result. */
	TOptional<bool> SceneViewExtensionIsActive(const ISceneViewExtension* SceneViewExtension, const FSceneViewExtensionContext& Context) const;

	/** Return custom settings for the view. (GameThread) */
	const FInstancedPropertyBag* GetCustomSettings(const FSceneView& View) const;
	/** Return custom settings for the view. (RenderThread) */
	const FInstancedPropertyBag* GetCustomSettings_RenderThread(const FSceneView& View) const;

	/** return quality mode from custom settings. */
	static TOptional<EDLSSQualityMode> GetQualityMode(const FInstancedPropertyBag& InSettings, int32 PixelCount);

private:
	// Array of Functors that can be used to activate an extension for the current frame and given context.
	TArray<FSceneViewExtensionIsActiveFunctor> IsActiveThisFrameFunctions;

	// Custom DLSS settings (GameThread data)
	TMap<uint32, FInstancedPropertyBag> CustomSettings;

	// Custom DLSS settings (RenderThread data)
	TMap<uint32, FInstancedPropertyBag> CustomSettings_RenderThread;

	// CustomSettings expire each frame.
	uint64 LastFrameCounter = 0;

	// Singleton object ofr this modular feature implementation
	static TSharedPtr<FDLSSTemporalUpscalerModularFeature, ESPMode::ThreadSafe> ModularFeatureSingleton;
};
#endif // ENGINE_SUPPORTS_UPSCALER_MODULAR_FEATURE

