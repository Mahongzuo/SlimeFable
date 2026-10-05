#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Character.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialShared.h"
#include "ShaderCompiler.h"
#include "RHI.h"
#include "Slime/SlimeBodyComponent.h"
#include "Slime/SlimeTrailComponent.h"
#include "Slime/SlimeElementComponent.h"
#include "Settings/SlimeGraphicsSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlimeLuminousSettingsTest, "SlimeFable.Luminous.SettingsCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlimeLuminousSettingsTest::RunTest(const FString& Parameters)
{
	static_assert(int(ESlimeBodySkin::Classic) == 0 && int(ESlimeBodySkin::Spectral) == 1 && int(ESlimeBodySkin::Volumetric) == 2);
	const FString TempIni = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/LuminousSettings-") + FGuid::NewGuid().ToString() + TEXT(".ini"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(TempIni), true);
	// 5.8 SetInt does not create an unknown config branch. Register the isolated file
	// explicitly so this test never writes the user's real GameUserSettings.ini.
	FConfigFile Seed;
	Seed.bCanSaveAllSections = true;
	GConfig->Add(TempIni, Seed);
	{
		TGuardValue<FString> Guard(GGameUserSettingsIni, TempIni);
		UGameInstance* GI = NewObject<UGameInstance>();
		USlimeGraphicsSettings* Settings = NewObject<USlimeGraphicsSettings>(GI);
		TestEqual(TEXT("Default is luminous"), Settings->GetBodySkin(), ESlimeBodySkin::Luminous);
		const ESlimeBodySkin Cycle[] = { ESlimeBodySkin::Spectral, ESlimeBodySkin::Volumetric, ESlimeBodySkin::Classic, ESlimeBodySkin::Luminous };
		for (ESlimeBodySkin Expected : Cycle)
		{
			Settings->CycleBodySkin();
			TestEqual(TEXT("Cycle order"), Settings->GetBodySkin(), Expected);
			int32 SavedValue = -1;
			TestTrue(TEXT("Selection was written to config"),GConfig->GetInt(TEXT("SlimeGraphics"),TEXT("BodySkin"),SavedValue,TempIni));
			TestEqual(TEXT("Serialized enum value"),SavedValue,static_cast<int32>(Expected));
			USlimeGraphicsSettings* Reloaded = NewObject<USlimeGraphicsSettings>(GI);
			Reloaded->Load();
			TestEqual(TEXT("Selection survives settings reload"), Reloaded->GetBodySkin(), Expected);
		}
		Settings->SetBodySkin(ESlimeBodySkin::Spectral);
		Settings->SetBodySkin(ESlimeBodySkin::COUNT);
		TestEqual(TEXT("Invalid selection fallback"), Settings->GetBodySkin(), ESlimeBodySkin::Luminous);
	}
	GConfig->UnloadFile(TempIni);
	IFileManager::Get().Delete(*TempIni);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlimeLuminousBodyShapeTest, "SlimeFable.Luminous.BodyShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlimeLuminousBodyShapeTest::RunTest(const FString& Parameters)
{
	static_assert(int(ESlimeBodyShape::Ball) == 0 && int(ESlimeBodyShape::Dome) == 1);
	const FString TempIni = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/BodyShape-") + FGuid::NewGuid().ToString() + TEXT(".ini"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(TempIni), true);
	FConfigFile Seed;
	Seed.bCanSaveAllSections = true;
	GConfig->Add(TempIni, Seed);
	{
		TGuardValue<FString> Guard(GGameUserSettingsIni, TempIni);
		UGameInstance* GI = NewObject<UGameInstance>();
		USlimeGraphicsSettings* Settings = NewObject<USlimeGraphicsSettings>(GI);
		TestEqual(TEXT("Default is ball"), Settings->GetBodyShape(), ESlimeBodyShape::Ball);
		int32 Broadcasts = 0;
		Settings->OnBodyShapeChanged.AddLambda([&Broadcasts](ESlimeBodyShape) { ++Broadcasts; });
		const ESlimeBodyShape Cycle[] = { ESlimeBodyShape::Dome, ESlimeBodyShape::Ball };
		for (ESlimeBodyShape Expected : Cycle)
		{
			Settings->CycleBodyShape();
			TestEqual(TEXT("Cycle order"), Settings->GetBodyShape(), Expected);
			int32 SavedValue = -1;
			TestTrue(TEXT("Shape was written to config"), GConfig->GetInt(TEXT("SlimeGraphics"), TEXT("BodyShape"), SavedValue, TempIni));
			TestEqual(TEXT("Serialized enum value"), SavedValue, static_cast<int32>(Expected));
			USlimeGraphicsSettings* Reloaded = NewObject<USlimeGraphicsSettings>(GI);
			Reloaded->Load();
			TestEqual(TEXT("Shape survives settings reload"), Reloaded->GetBodyShape(), Expected);
		}
		TestEqual(TEXT("Each change broadcasts"), Broadcasts, 2);
		Settings->SetBodyShape(ESlimeBodyShape::Dome);
		Settings->SetBodyShape(ESlimeBodyShape::COUNT);
		TestEqual(TEXT("Invalid shape fallback"), Settings->GetBodyShape(), ESlimeBodyShape::Ball);
	}
	GConfig->UnloadFile(TempIni);
	IFileManager::Get().Delete(*TempIni);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlimeLuminousPuddleSizeTest, "SlimeFable.Luminous.PuddleSize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlimeLuminousPuddleSizeTest::RunTest(const FString& Parameters)
{
	const FVector FromDiameter = USlimeTrailComponent::DecalSizeFromDiameter(40.f, 8.f);
	TestTrue(TEXT("Diameter 40 is a 20cm half-extent"), FromDiameter.Equals(FVector(8.f, 20.f, 20.f), 0.01f));

	const FVector FromContact = USlimeTrailComponent::DecalSizeFromContact(FVector2D(30.f, 20.f), 1.1f, 8.f);
	TestTrue(TEXT("Contact half-axes scale into decal extents"), FromContact.Equals(FVector(8.f, 33.f, 22.f), 0.01f));

	const FVector Tiny = USlimeTrailComponent::DecalSizeFromContact(FVector2D(0.1f, 0.1f), 1.f, 0.2f);
	TestTrue(TEXT("Depth and extents stay above the decal floor"), Tiny.X >= 1.f && Tiny.Y >= 0.5f && Tiny.Z >= 0.5f);

	TestTrue(TEXT("Puddle box margin"), FMath::IsNearlyEqual(USlimeTrailComponent::PuddleBoxMarginFrom(0.28f, 0.08f), 1.36f, 0.001f));

	// A filled disc of radius R has variance R^2/4 along every axis, so the half-axes come back as R.
	FSlimeFloorFootprint Disc;
	Disc.Count = 64;
	Disc.Cxx = 25.0;
	Disc.Cyy = 25.0;
	Disc.Cxy = 0.0;
	Disc.CellSize = 0.f;
	const FVector2D DiscAxes = USlimeBodyComponent::FootprintHalfAxesFromMoments(Disc, FVector2D(1.f, 0.f));
	TestTrue(TEXT("Disc moments recover the radius"), DiscAxes.Equals(FVector2D(10.f, 10.f), 0.01f));

	const FVector2D Particles(30.f, 20.f);
	const FVector2D Visual(44.f, 36.f);
	const FVector Ball = USlimeTrailComponent::DecalSizeForShape(Particles, 1.1f, Visual, 1.f, 0.f, 8.f);
	TestTrue(TEXT("Dome weight 0 keeps the particle puddle"), Ball.Equals(USlimeTrailComponent::DecalSizeFromContact(Particles, 1.1f, 8.f), 0.01f));
	const FVector Dome = USlimeTrailComponent::DecalSizeForShape(Particles, 1.1f, Visual, 1.f, 1.f, 8.f);
	TestTrue(TEXT("Dome weight 1 uses the mesh footprint"), Dome.Equals(USlimeTrailComponent::DecalSizeFromContact(Visual, 1.f, 8.f), 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlimeLuminousXRayOutlineTest, "SlimeFable.Luminous.XRayOutline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlimeLuminousXRayOutlineTest::RunTest(const FString& Parameters)
{
	UMaterial* Outline = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_SlimeXRayOutlinePP.M_SlimeXRayOutlinePP"));
	if (!TestNotNull(TEXT("X-ray outline post process loads"), Outline))
	{
		return false;
	}
	TestEqual(TEXT("Post process domain"), (int32)Outline->MaterialDomain, (int32)EMaterialDomain::MD_PostProcess);
	float Width = 0.f;
	TestTrue(TEXT("OutlineWidth parameter"), Outline->GetScalarParameterValue(FMaterialParameterInfo(TEXT("OutlineWidth")), Width));
	TestTrue(TEXT("OutlineWidth default is 2px"), FMath::IsNearlyEqual(Width, 2.f, 0.01f));
	float Bias = 0.f;
	TestTrue(TEXT("OcclusionBias parameter"), Outline->GetScalarParameterValue(FMaterialParameterInfo(TEXT("OcclusionBias")), Bias));
	TestTrue(TEXT("OcclusionBias default is 3cm"), FMath::IsNearlyEqual(Bias, 3.f, 0.01f));
	FLinearColor Color;
	TestTrue(TEXT("XRayColor parameter"), Outline->GetVectorParameterValue(FMaterialParameterInfo(TEXT("XRayColor")), Color));

	UMaterial* Proxy = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_SlimeXRayDepthProxy.M_SlimeXRayDepthProxy"));
	if (!TestNotNull(TEXT("X-ray depth proxy loads"), Proxy))
	{
		return false;
	}
	TestEqual(TEXT("Depth proxy is translucent"), (int32)Proxy->GetBlendMode(), (int32)BLEND_Translucent);
	TestTrue(TEXT("Depth proxy disables depth test"), Proxy->bDisableDepthTest != 0);
	TestTrue(TEXT("Depth proxy allows custom depth writes"), Proxy->AllowTranslucentCustomDepthWrites != 0);
	TestTrue(TEXT("Depth proxy writes custom depth"), Proxy->IsTranslucencyWritingCustomDepth());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlimeLuminousRuntimeTest, "SlimeFable.Luminous.MaterialAndRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlimeLuminousRuntimeTest::RunTest(const FString& Parameters)
{
	UMaterial* Master = LoadObject<UMaterial>(nullptr,TEXT("/Game/Characters/Slime/Materials/M_SlimeBody_Luminous.M_SlimeBody_Luminous"));
	if (!TestNotNull(TEXT("Luminous master loads"), Master)) return false;
	for (const TCHAR* Name : { TEXT("Water"),TEXT("Wind"),TEXT("Fire"),TEXT("Lightning"),TEXT("Dark"),TEXT("Physical") })
	{
		const FString Path = FString::Printf(TEXT("/Game/Characters/Slime/Materials/MI_SlimeLuminous_%s.MI_SlimeLuminous_%s"),Name,Name);
		UMaterialInstanceConstant* MI = LoadObject<UMaterialInstanceConstant>(nullptr,*Path);
		if (TestNotNull(TEXT("Element preview loads"),MI))
		{
			TestTrue(TEXT("Preview has correct parent"),MI->Parent == Master);
			float Floor = 0.f;
			TestTrue(TEXT("Readability parameter present"),MI->GetScalarParameterValue(FMaterialParameterInfo(TEXT("ReadabilityFloor")),Floor));
			TestEqual(TEXT("Preview inherits runtime floor"),Floor,0.65f);
		}
	}
	if (!GUsingNullRHI)
	{
		GShaderCompilingManager->FinishAllCompilation();
		FMaterialResource* Resource = Master->GetMaterialResource(GMaxRHIShaderPlatform);
		if (TestNotNull(TEXT("Rendered material resource"),Resource))
		{
			for (const FString& Error : Resource->GetCompileErrors()) AddError(Error);
			TestNotNull(TEXT("Compiled shader map, no default-material fallback"),Resource->GetGameThreadShaderMap());
			TestEqual(TEXT("Validation uses SM6"),GMaxRHIFeatureLevel,ERHIFeatureLevel::SM6);
		}
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game,false);
	World->AddToRoot();
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	ACharacter* Owner = World->SpawnActor<ACharacter>(FVector(0,0,100),FRotator::ZeroRotator);
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(Owner);
	Owner->AddInstanceComponent(Mesh);
	Mesh->SetupAttachment(Owner->GetRootComponent());
	Mesh->RegisterComponent();
	USlimeBodyComponent* Body = NewObject<USlimeBodyComponent>(Owner);
	Owner->AddInstanceComponent(Body);
	Body->SetSurfaceMesh(Mesh);
	Body->RegisterComponent();
	USlimeElementComponent* Element = NewObject<USlimeElementComponent>(Owner);
	Owner->AddInstanceComponent(Element);
	Element->RegisterComponent();
	Owner->DispatchBeginPlay();
	Body->ApplyBodySkin(ESlimeBodySkin::Luminous);
	Element->TickComponent(1.f/60.f,LEVELTICK_All,nullptr);
	TestTrue(TEXT("Runtime selected luminous"),Body->GetResolvedBodyMaterial() == Master);
	TestFalse(TEXT("Luminous avoids density atlas"),Body->IsVolumetricSkinActive());
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
	if (TestNotNull(TEXT("Runtime MID"),MID))
	{
		MID->SetScalarParameterValue(TEXT("Absorption"),0.037f);
		for (ESlimeElement Kind : { ESlimeElement::Water,ESlimeElement::Wind,ESlimeElement::Fire,ESlimeElement::Lightning,ESlimeElement::Dark,ESlimeElement::Physical })
		{
			Element->SetElement(Kind,true);
			Element->PlayHitFlash();
			Element->TickComponent(0.05f,LEVELTICK_All,nullptr);
			float Absorption = 0.f;
			MID->GetScalarParameterValue(TEXT("Absorption"),Absorption);
			TestEqual(TEXT("Element and hit flash preserve authored look"),Absorption,0.037f);
			Element->TickComponent(1.f,LEVELTICK_All,nullptr);
		}
		Element->SetOpacityScale(0.4f);
		float Opacity = 0.f;
		MID->GetScalarParameterValue(TEXT("Opacity"),Opacity);
		TestTrue(TEXT("Fading preserves element opacity without spectral multiplier"),FMath::IsNearlyEqual(Opacity,Element->GetCurrentProfile().Opacity*0.4f));
		float Visibility = 0.f;
		MID->GetScalarParameterValue(TEXT("BodyVisibility"),Visibility);
		TestEqual(TEXT("Fading also fades the rim and face"),Visibility,0.4f);
		Element->SetOpacityScale(1.f);
		Body->SetSpread(true);
		Body->TickComponent(1.f/30.f,LEVELTICK_All,nullptr);
		Body->SetSpread(false);
		Body->LaunchChunk(FVector(100,0,200));
		Body->TickComponent(1.f/30.f,LEVELTICK_All,nullptr);
		TestTrue(TEXT("Deformation and shot rebuild retain MID"),Mesh->GetMaterial(0) == MID);
		Body->ResetBody();
		Element->TickComponent(1.f/60.f,LEVELTICK_All,nullptr);
		TestTrue(TEXT("Reset retains selected skin"),Body->GetResolvedBodyMaterial() == Master);
	}
	Body->ApplyBodySkin(ESlimeBodySkin::Volumetric);
	TestTrue(TEXT("Existing volumetric still activates"),Body->IsVolumetricSkinActive());
	Body->ApplyBodySkin(ESlimeBodySkin::Luminous);
	TestFalse(TEXT("Leaving volume releases capture"),Body->IsVolumetricSkinActive());
	Owner->RouteEndPlay(EEndPlayReason::LevelTransition);
	GEngine->ShutdownWorldNetDriver(World);
	World->DestroyWorld(true);
	World->SetPhysicsScene(nullptr);
	GEngine->DestroyWorldContext(World);
	World->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlimeLuminousMorphSkinTest, "SlimeFable.Luminous.MorphSkin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlimeLuminousMorphSkinTest::RunTest(const FString& Parameters)
{
	UMaterial* Master = LoadObject<UMaterial>(nullptr,
		TEXT("/Game/Characters/Slime/Materials/M_SlimeMorph_Luminous.M_SlimeMorph_Luminous"));
	if (!TestNotNull(TEXT("Morph luminous master loads"), Master))
	{
		return false;
	}
	TestEqual(TEXT("Translucent glass, not a masked toon"), Master->GetBlendMode(), BLEND_Translucent);
	TestFalse(TEXT("Single sided so the back shell does not sort over the front"), Master->IsTwoSided());
	TestTrue(TEXT("Usable on skeletal meshes"), Master->GetUsageByFlag(MATUSAGE_SkeletalMesh));
	TestTrue(TEXT("Nanite usage is set even though the transition disables Nanite"), Master->GetUsageByFlag(MATUSAGE_Nanite));
	float Grow = -1.f;
	TestTrue(TEXT("GrowProgress parameter present"),
		Master->GetScalarParameterValue(FMaterialParameterInfo(TEXT("GrowProgress")), Grow));
	TestTrue(TEXT("IsSlimeMorphMaterial path rule"),
		Master->GetPathName().Contains(TEXT("M_SlimeMorph"), ESearchCase::IgnoreCase));
	return true;
}
#endif
