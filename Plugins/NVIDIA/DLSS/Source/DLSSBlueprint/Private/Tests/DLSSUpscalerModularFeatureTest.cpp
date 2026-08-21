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

#if ENGINE_SUPPORTS_UPSCALER_MODULAR_FEATURE

// plugin includes
#include "DLSSLibrary.h"

// engine includes
#include "Features/IModularFeatures.h"
#include "IUpscalerModularFeature.h"
#include "Misc/AutomationTest.h"
#include "StructUtils/PropertyBag.h"

// Why does this test live in the DLSSBlueprint module instead of the DLSS module?
// Because we'd like to ensure that all supported DLSS modes are covered by the DLSS modular upscaler, and the code to
// find supported DLSS modes lives in the DLSSBlueprint module. We can't easily look things up from the DLSS module
// which is a dependency of DLSSBlueprint.

#define TestNotNullExpr(...) TestNotNull(TEXT(#__VA_ARGS__), __VA_ARGS__)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDLSSTemporalUpscalerModularFeatureTest, "Nvidia.DLSS.ModularFeature",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
	EAutomationTestFlags::NonNullRHI | EAutomationTestFlags::ProductFilter
)

bool FDLSSTemporalUpscalerModularFeatureTest::RunTest(const FString& Parameters)
{
	using UE::VirtualProduction::IUpscalerModularFeature;

	// save original DLSS-SR state
	bool bOriginalDLSSEnabled = UDLSSLibrary::IsDLSSEnabled();

	// ensure modular feature works even when DLSS-SR is initially disabled
	if (UDLSSLibrary::IsDLSSSupported())
	{
		UDLSSLibrary::EnableDLSS(false);
	}
	ON_SCOPE_EXIT
	{
		if (UDLSSLibrary::IsDLSSSupported())
		{
			UDLSSLibrary::EnableDLSS(bOriginalDLSSEnabled);
		}
	};

	// make sure a "DLSS" modular feature exists when DLSS is enabled
	IModularFeatures::FScopedLockModularFeatureList ModularFeatureListLock;
	const TArray<IUpscalerModularFeature*> UpscalerModularFeatures =
		IModularFeatures::Get().GetModularFeatureImplementations<IUpscalerModularFeature>(IUpscalerModularFeature::ModularFeatureName);
	IUpscalerModularFeature* const* DLSSFeaturePtr =
		UpscalerModularFeatures.FindByPredicate([](const IUpscalerModularFeature* UpscalerModularFeature)
	{
		return (UpscalerModularFeature != nullptr) &&
			UpscalerModularFeature->IsFeatureEnabled() &&
			UpscalerModularFeature->GetName() == FName("DLSS");
	});

	if (!UDLSSLibrary::IsDLSSSupported())
	{
		// if DLSS is not supported, the DLSS modular feature shouldn't be enabled
		TestFalse("Found 'DLSS' modular feature when DLSS not supported", DLSSFeaturePtr && *DLSSFeaturePtr);
		return true;
	}

	if (TestTrue("Found 'DLSS' modular feature", DLSSFeaturePtr && *DLSSFeaturePtr))
	{
		IUpscalerModularFeature* DLSSFeature = *DLSSFeaturePtr;

		// Make sure it exports a "Quality" property
		FInstancedPropertyBag PropBag;
		TestTrueExpr(DLSSFeature->GetSettings(PropBag));
		const FPropertyBagPropertyDesc* QualityPropDesc = PropBag.FindPropertyDescByName(FName("Quality"));
		if (TestNotNullExpr(QualityPropDesc))
		{
			TestEqual(TEXT("'Quality' property is enum type"), QualityPropDesc->ValueType, EPropertyBagPropertyType::Enum);
		}
	}

	// Check that every supported DLSS mode exists in the modular quality enum
	UEnum* DLSSModeEnum = StaticEnum<UDLSSMode>();
	UTEST_NOT_NULL_EXPR(DLSSModeEnum);
	TArray<UDLSSMode> SupportedModes = UDLSSLibrary::GetSupportedDLSSModes();
	for (UDLSSMode SupportedMode : SupportedModes)
	{
		if (SupportedMode == UDLSSMode::Off)
		{
			// We intentionally don't provide an Off option
			continue;
		}
		FString ModeStr = DLSSModeEnum->GetNameStringByValue(static_cast<int64>(SupportedMode));
		FString ExpectedQualityModeStr = FString(TEXT("EDLSSUpscalerModularFeatureQuality::")) + ModeStr;
		UEnum* FoundEnum = nullptr;
		UEnum::LookupEnumName(TEXT("/Script/DLSS"), *ExpectedQualityModeStr,
			EFindFirstObjectOptions::None, &FoundEnum);
		TestNotNull(*(FString(TEXT("enum value ")) + ExpectedQualityModeStr), FoundEnum);
	}

	return true;
}

#endif	// ENGINE_SUPPORTS_UPSCALER_MODULAR_FEATURE

