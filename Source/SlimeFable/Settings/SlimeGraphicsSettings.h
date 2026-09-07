// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SlimeGraphicsTypes.h"
#include "SlimeGraphicsSettings.generated.h"

/**
 * GPU probe, first-run quality bucket, and optional DLSS/FSR.
 * DLSS uses the official UDLSSLibrary (screen percentage + EnableDLSS), not NGX headers.
 */
UCLASS()
class SLIMEFABLE_API USlimeGraphicsSettings : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Graphics")
	FString GetAdapterName() const { return AdapterName; }

	UFUNCTION(BlueprintPure, Category = "Graphics")
	FString GetVendorName() const { return VendorName; }

	UFUNCTION(BlueprintPure, Category = "Graphics")
	int32 GetDedicatedVramMB() const { return DedicatedVramMB; }

	UFUNCTION(BlueprintPure, Category = "Graphics")
	bool HasHardwareRayTracing() const { return bHardwareRayTracing; }

	UFUNCTION(BlueprintPure, Category = "Graphics")
	bool IsIntegratedGpu() const { return bIntegratedGpu; }

	UFUNCTION(BlueprintPure, Category = "Graphics")
	bool IsNvidia() const { return bNvidia; }

	UFUNCTION(BlueprintPure, Category = "Graphics")
	bool IsDlssSupported() const;

	UFUNCTION(BlueprintPure, Category = "Graphics")
	bool IsFrameGenSupported() const;

	UFUNCTION(BlueprintPure, Category = "Graphics")
	bool IsFsrPluginPresent() const;

	UFUNCTION(BlueprintPure, Category = "Graphics")
	ESlimeUpscaler GetUpscaler() const { return Upscaler; }

	UFUNCTION(BlueprintPure, Category = "Graphics")
	ESlimeDLSSQuality GetDLSSQuality() const { return DLSSQuality; }

	UFUNCTION(BlueprintPure, Category = "Graphics")
	bool IsFrameGenEnabled() const { return bFrameGen; }

	UFUNCTION(BlueprintPure, Category = "Graphics")
	int32 GetQualityLevel() const;

	UFUNCTION(BlueprintPure, Category = "Graphics")
	FText GetStatusText() const;

	UFUNCTION(BlueprintPure, Category = "Graphics")
	FText GetUpscalerDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Graphics")
	FText GetDLSSQualityDisplayName() const;

	/** Recommend 0..3 from VRAM / RT / iGPU. Does not apply. */
	UFUNCTION(BlueprintPure, Category = "Graphics")
	int32 RecommendQualityLevel() const;

	UFUNCTION(BlueprintCallable, Category = "Graphics")
	void SetQualityLevel(int32 Level);

	/** Re-run hardware bucket and apply. Safe to call from the settings menu. */
	UFUNCTION(BlueprintCallable, Category = "Graphics")
	void AutoDetectQuality();

	UFUNCTION(BlueprintCallable, Category = "Graphics")
	bool TrySetUpscaler(ESlimeUpscaler NewMode, FText& OutError);

	UFUNCTION(BlueprintCallable, Category = "Graphics")
	void CycleUpscaler();

	UFUNCTION(BlueprintCallable, Category = "Graphics")
	void CycleDLSSQuality();

	UFUNCTION(BlueprintCallable, Category = "Graphics")
	bool TrySetFrameGen(bool bEnable, FText& OutError);

	UFUNCTION(BlueprintCallable, Category = "Graphics")
	void ToggleFrameGen();

	UFUNCTION(BlueprintPure, Category = "Graphics")
	bool IsPixelStreamingEnabled() const { return bPixelStreaming; }

	UFUNCTION(BlueprintPure, Category = "Graphics")
	FString GetPixelStreamingUrl() const { return PixelStreamingUrl; }

	UFUNCTION(BlueprintPure, Category = "Graphics")
	ESlimePixelStreamTarget GetPixelStreamTarget() const { return PixelStreamTarget; }

	UFUNCTION(BlueprintPure, Category = "Graphics")
	FText GetPixelStreamTargetDisplayName() const;

	UFUNCTION(BlueprintCallable, Category = "Graphics")
	void CyclePixelStreamTarget();

	UFUNCTION(BlueprintPure, Category = "Graphics")
	FString GetCloudPlayUrl() const;

	UFUNCTION(BlueprintPure, Category = "Graphics")
	FString GetLanPlayUrl() const;

	UFUNCTION(BlueprintCallable, Category = "Graphics")
	bool CopyCloudPlayUrl(FText& OutStatus);

	UFUNCTION(BlueprintCallable, Category = "Graphics")
	bool CopyLanPlayUrl(FText& OutStatus);

	UFUNCTION(BlueprintCallable, Category = "Graphics")
	bool TrySetPixelStreaming(bool bEnable, FText& OutError);

	UFUNCTION(BlueprintCallable, Category = "Graphics")
	void TogglePixelStreaming();

	void ApplyUpscaler() const;
	void ApplyPixelStreaming() const;
	void Save();
	void Load();

private:
	void StartPixelStreamingNow() const;

protected:
	void ProbeGpu();
	void WriteGpuLogFile() const;
	void ApplyFirstRunQualityIfNeeded();
	bool HasExistingScalabilitySave() const;
	bool IsDlssModuleAvailable() const;
	bool IsRtx40Or50() const;
	void SetCVarInt(const TCHAR* Name, int32 Value) const;
	void SetCVarFloat(const TCHAR* Name, float Value) const;
	void SetCVarString(const TCHAR* Name, const FString& Value) const;
	bool HasCVar(const TCHAR* Name) const;
	float ReadScreenPercentage() const;
	void CacheTsrScreenPercentage() const;
	void RestoreTsrScreenPercentage() const;
	void ApplyDlssMode() const;
	void ApplyPixelStreamTargetUrl();
	void LoadPlayToken();
	bool EnsureLocalSignalling(FText& OutStatus);
	bool LaunchLocalSignalling();
	static bool IsLocalTcpOpen(int32 Port);
	static FString DetectLanIPv4();
	static bool CopyTextToClipboard(const FString& Text);

	FString AdapterName;
	FString VendorName;
	FString DriverVersion;
	int32 DedicatedVramMB = 0;
	bool bHardwareRayTracing = false;
	bool bIntegratedGpu = false;
	bool bNvidia = false;
	bool bAmd = false;

	ESlimeUpscaler Upscaler = ESlimeUpscaler::Off;
	ESlimeDLSSQuality DLSSQuality = ESlimeDLSSQuality::Quality;
	bool bFrameGen = false;
	bool bHasUserOrAutoQuality = false;
	bool bPixelStreaming = false;
	ESlimePixelStreamTarget PixelStreamTarget = ESlimePixelStreamTarget::Cloud;
	FString PixelStreamingUrl;
	FString CloudPixelStreamingUrl;
	FString PixelStreamingPlayToken;

	/** Screen percentage from scalability / TSR, restored when DLSS turns off. */
	mutable float CachedTsrScreenPercentage = 100.f;
	mutable bool bHasCachedTsrScreenPercentage = false;
	double LastLocalSignallingLaunchSeconds = -1000.0;
};
