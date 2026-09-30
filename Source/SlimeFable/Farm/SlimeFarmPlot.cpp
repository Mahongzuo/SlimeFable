// Copyright Epic Games, Inc. All Rights Reserved.

#include "Farm/SlimeFarmPlot.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "EngineUtils.h"
#include "Farm/SlimeFarmSubsystem.h"
#include "Farm/SlimePlotStatusWidget.h"
#include "Farm/SlimeProceduralPlantComponent.h"
#include "Hub/SlimeHubSkyDirector.h"
#include "Inventory/SlimeInventorySubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/DateTime.h"
#include "SlimeFable.h"
#include "SlimeFablePlayerController.h"
#include "UI/SlimeFloatingTextWidget.h"
#include "UI/SlimeSeedPickerWidget.h"

ASlimeFarmPlot::ASlimeFarmPlot()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Soil = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Soil"));
	Soil->SetupAttachment(Root);
	Soil->SetMobility(EComponentMobility::Movable);
	SoilMeshAsset = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/UltimateFarming/Meshes/SM_FertileGround.SM_FertileGround"));
	if (!SoilMeshAsset)
	{
		SoilMeshAsset = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		Soil->SetRelativeLocation(FVector(0.f, 0.f, 6.f));
		Soil->SetRelativeScale3D(FVector(2.2f, 2.2f, 0.12f));
	}
	Soil->SetStaticMesh(SoilMeshAsset);
	Soil->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Soil->SetCollisionObjectType(ECC_WorldStatic);
	Soil->SetCollisionResponseToAllChannels(ECR_Block);
	Soil->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Soil->SetGenerateOverlapEvents(true);

	if (SoilMeshAsset && SoilMeshAsset->GetName().Contains(TEXT("Cube")))
	{
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			Soil->SetMaterial(0, Base);
		}
	}

	Plant = CreateDefaultSubobject<USlimeProceduralPlantComponent>(TEXT("Plant"));
	Plant->SetupAttachment(Root);
	Plant->SetRelativeLocation(FVector(0.f, 0.f, 12.f));

	StatusWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("Status"));
	StatusWidget->SetupAttachment(Root);
	StatusWidget->SetWidgetSpace(EWidgetSpace::Screen);
	StatusWidget->SetWidgetClass(USlimePlotStatusWidget::StaticClass());
	StatusWidget->SetDrawSize(FVector2D(280.f, 64.f));
	StatusWidget->SetRelativeLocation(FVector(0.f, 0.f, 28.f));
	StatusWidget->SetHiddenInGame(true);
	StatusWidget->SetVisibility(false);
	StatusWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StatusWidget->SetGenerateOverlapEvents(false);
}

void ASlimeFarmPlot::BeginPlay()
{
	Super::BeginPlay();
	CacheSoilLook();
	FitSoil();
	if (PlotId.IsNone())
	{
		PlotId = GetFName();
	}
	LoadAndCatchUp();
	RefreshVisual();
	RefreshStatus();
}

void ASlimeFarmPlot::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (!bSkipSaveOnEnd)
	{
		SaveRecord(false);
	}
	Super::EndPlay(EndPlayReason);
}

void ASlimeFarmPlot::MarkRemoved()
{
	bSkipSaveOnEnd = true;
}

void ASlimeFarmPlot::CacheSoilLook()
{
	bSoilUsesColor = false;
	if (!Soil)
	{
		return;
	}
	SoilMid = Soil->CreateDynamicMaterialInstance(0);
	if (Soil->GetStaticMesh() && Soil->GetStaticMesh()->GetName().Contains(TEXT("Cube")))
	{
		bSoilUsesColor = true;
		if (SoilMid)
		{
			SoilMid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.28f, 0.16f, 0.08f));
		}
	}
	LastWet01 = -1.f;
}

void ASlimeFarmPlot::LoadAndCatchUp()
{
	UGameInstance* GI = GetGameInstance();
	USlimeFarmSubsystem* Farm = GI ? GI->GetSubsystem<USlimeFarmSubsystem>() : nullptr;
	if (!Farm)
	{
		return;
	}

	FSlimeFarmPlotRecord Record;
	if (!Farm->GetPlotRecord(PlotId, Record))
	{
		LastUpdateUnix = FDateTime::UtcNow().ToUnixTimestamp();
		return;
	}

	State = Record.State;
	Quality = Record.Quality;
	Growth01 = Record.Growth01;
	Moisture = Record.Moisture;
	Fertility = Record.Fertility;
	HueShift = Record.HueShift;
	PlantSeed = Record.PlantSeed;
	bHarvestedLook = Record.bHarvestedLook;
	LastUpdateUnix = Record.LastUpdateUnix;
	Crop = Farm->FindCrop(Record.CropId);
	if (!Crop && State != ESlimeFarmPlotState::Empty)
	{
		State = ESlimeFarmPlotState::Empty;
		Growth01 = 0.f;
		Quality = ESlimeCropQuality::Common;
		bHarvestedLook = false;
	}

	if (State == ESlimeFarmPlotState::Growing && Crop && LastUpdateUnix > 0)
	{
		const int64 Now = FDateTime::UtcNow().ToUnixTimestamp();
		const double Elapsed = static_cast<double>(Now - LastUpdateUnix);
		if (Elapsed > 1.0)
		{
			const float Rate = ComputeGrowthPerSecond(true);
			const float Need = (1.f - Growth01) / FMath::Max(Rate, 0.0001f);
			Growth01 = FMath::Clamp(Growth01 + Rate * FMath::Min(static_cast<float>(Elapsed), Need), 0.f, 1.f);
			if (Growth01 >= 0.999f)
			{
				Growth01 = 1.f;
				State = ESlimeFarmPlotState::Mature;
			}
		}
	}
	LastUpdateUnix = FDateTime::UtcNow().ToUnixTimestamp();
	PreloadCropMeshes();
	SaveRecord(false);
}

void ASlimeFarmPlot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (SteamSeconds > 0.f)
	{
		SteamSeconds = FMath::Max(0.f, SteamSeconds - DeltaSeconds);
	}
	if (FireBoostSeconds > 0.f)
	{
		FireBoostSeconds = FMath::Max(0.f, FireBoostSeconds - DeltaSeconds);
	}
	if (PreferredSeconds > 0.f)
	{
		PreferredSeconds = FMath::Max(0.f, PreferredSeconds - DeltaSeconds);
	}
	Heat = FMath::Max(0.f, Heat - DeltaSeconds * 0.12f);

	const ASlimeHubSkyDirector* Sky = FindSky();
	const float Rain = Sky ? Sky->GetRain01() : 0.f;
	const float Snow = Sky ? Sky->GetSnow01() : 0.f;
	if (Rain > 0.2f)
	{
		Moisture = FMath::Clamp(Moisture + DeltaSeconds * (0.12f + Rain * 0.2f), 0.f, 1.f);
		DrySeconds = 0.f;
	}
	else if (Snow < 0.3f)
	{
		Moisture = FMath::Clamp(Moisture - DeltaSeconds * 0.018f, 0.f, 1.f);
	}

	if (State == ESlimeFarmPlotState::Growing && Crop)
	{
		if (Moisture < 0.08f && Snow < 0.3f)
		{
			DrySeconds += DeltaSeconds;
			if (DrySeconds > 45.f)
			{
				State = ESlimeFarmPlotState::Withered;
				USlimeFloatingTextWidget::Spawn(this, GetActorLocation() + FVector(0, 0, 80), FText::FromString(TEXT("枯萎了")), FLinearColor(0.55f, 0.35f, 0.15f), true);
			}
		}
		else
		{
			DrySeconds = 0.f;
			const float Rate = ComputeGrowthPerSecond(false);
			Growth01 = FMath::Clamp(Growth01 + Rate * DeltaSeconds, 0.f, 1.f);
			if (Growth01 >= 0.999f)
			{
				Growth01 = 1.f;
				State = ESlimeFarmPlotState::Mature;
				USlimeFloatingTextWidget::Spawn(this, GetActorLocation() + FVector(0, 0, 90), FText::FromString(TEXT("成熟了")), FLinearColor(0.95f, 0.85f, 0.3f), true);
			}
		}
	}

	if (bHarvestedLook && Crop && Crop->Stages.Num() > 0)
	{
		float MatureAt = 0.f;
		for (const FSlimeCropStage& Stage : Crop->Stages)
		{
			MatureAt = FMath::Max(MatureAt, Stage.StartGrowth);
		}
		if (Growth01 >= MatureAt - KINDA_SMALL_NUMBER)
		{
			bHarvestedLook = false;
		}
	}
	if (PopLeft > 0.f)
	{
		PopLeft = FMath::Max(0.f, PopLeft - DeltaSeconds);
		LastVisualKey = MIN_int32;
	}

	if (SoilMid)
	{
		const float Wet01 = FMath::Max(Moisture, Rain);
		if (FMath::Abs(Wet01 - LastWet01) > 0.01f)
		{
			LastWet01 = Wet01;
			SoilMid->SetScalarParameterValue(TEXT("Wetness"), Wet01);
			SoilMid->SetScalarParameterValue(TEXT("Wet"), Wet01);
			if (bSoilUsesColor)
			{
				const FLinearColor Dry(0.32f, 0.18f, 0.08f);
				const FLinearColor Wet(0.12f, 0.08f, 0.05f);
				SoilMid->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(Dry, Wet, Wet01));
			}
		}
	}

	RefreshVisual();
	StatusAccumulator += DeltaSeconds;
	if (StatusAccumulator > 0.2f)
	{
		StatusAccumulator = 0.f;
		UpdateStatusVisibility();
	}

	SaveAccumulator += DeltaSeconds;
	if (SaveAccumulator > 5.f)
	{
		SaveAccumulator = 0.f;
		SaveRecord(false);
	}
}

float ASlimeFarmPlot::ComputeGrowthPerSecond(bool bOffline) const
{
	if (!Crop || Crop->GrowSeconds <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	const float MoistureMul = Moisture < 0.1f ? 0.2f : FMath::Lerp(0.45f, 1.2f, Moisture);
	float DayMul = 1.f;
	float WeatherMul = 1.f;
	if (!bOffline)
	{
		if (const ASlimeHubSkyDirector* Sky = FindSky())
		{
			const bool bDay = Sky->IsDaytime();
			if (Crop->bPrefersNight)
			{
				DayMul = bDay ? 0.55f : 1.35f;
			}
			else
			{
				DayMul = bDay ? 1.25f : 0.65f;
			}
			if (Sky->GetSnow01() > 0.35f)
			{
				WeatherMul = 0.5f;
			}
			else if (Sky->GetRain01() > 0.25f)
			{
				WeatherMul = 1.2f;
			}
			else if (Sky->GetCurrentWeatherTag() == TEXT("Clear") && bDay)
			{
				WeatherMul = 1.15f;
			}
			else if (Sky->GetCurrentWeatherTag() == TEXT("Fog"))
			{
				WeatherMul = 0.9f;
			}
		}
	}

	float Bonus = 1.f;
	if (SteamSeconds > 0.f)
	{
		Bonus *= 2.f;
	}
	if (FireBoostSeconds > 0.f)
	{
		Bonus *= 1.6f;
	}
	if (PreferredSeconds > 0.f)
	{
		Bonus *= 1.3f;
	}
	Bonus *= FMath::Lerp(0.85f, 1.15f, Fertility);
	const float GrowSeconds = FMath::Clamp(Crop->GrowSeconds, 10.f, 1800.f);
	return (1.f / GrowSeconds) * MoistureMul * DayMul * WeatherMul * Bonus;
}

ASlimeHubSkyDirector* ASlimeFarmPlot::FindSky() const
{
	if (CachedSky.IsValid())
	{
		return CachedSky.Get();
	}
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ASlimeHubSkyDirector> It(World); It; ++It)
		{
			CachedSky = *It;
			return *It;
		}
	}
	return nullptr;
}

void ASlimeFarmPlot::SaveRecord(bool bFlush)
{
	UGameInstance* GI = GetGameInstance();
	USlimeFarmSubsystem* Farm = GI ? GI->GetSubsystem<USlimeFarmSubsystem>() : nullptr;
	if (!Farm || PlotId.IsNone())
	{
		return;
	}
	FSlimeFarmPlotRecord Record;
	Record.PlotId = PlotId;
	Record.CropId = Crop ? Crop->CropId : NAME_None;
	Record.State = State;
	Record.Quality = Quality;
	Record.Growth01 = Growth01;
	Record.Moisture = Moisture;
	Record.Fertility = Fertility;
	Record.HueShift = HueShift;
	Record.PlantSeed = PlantSeed;
	Record.bHarvestedLook = bHarvestedLook;
	Record.LastUpdateUnix = FDateTime::UtcNow().ToUnixTimestamp();
	LastUpdateUnix = Record.LastUpdateUnix;
	Farm->WritePlotRecord(Record);
	if (bFlush)
	{
		Farm->Flush();
	}
}

void ASlimeFarmPlot::RefreshVisual()
{
	RebuildPlants(false);
}

void ASlimeFarmPlot::RefreshStatus()
{
	USlimePlotStatusWidget* Widget = StatusWidget ? Cast<USlimePlotStatusWidget>(StatusWidget->GetUserWidgetObject()) : nullptr;
	if (!Widget)
	{
		return;
	}

	if (State == ESlimeFarmPlotState::Empty || !Crop)
	{
		Widget->SetStatus(FText::FromString(TEXT("空地")), 0.f, 96.f, false);
		StatusWidget->SetDrawSize(FVector2D(220.f, 36.f));
		return;
	}

	const float GrowSeconds = FMath::Clamp(Crop->GrowSeconds, 10.f, 1800.f);
	const float BarWidth = FMath::Lerp(96.f, 280.f, GrowSeconds / 1800.f);
	const bool bShowBar = State == ESlimeFarmPlotState::Growing || State == ESlimeFarmPlotState::Mature;
	const bool bAccelerating = SteamSeconds > 0.f || FireBoostSeconds > 0.f || PreferredSeconds > 0.f;
	FString Line = Crop->DisplayName.ToString();
	if (State == ESlimeFarmPlotState::Growing && bAccelerating)
	{
		Line += TEXT("  加速生长中");
	}
	float ShownGrowth = Growth01;
	if (State == ESlimeFarmPlotState::Mature)
	{
		Line += TEXT("  可收获");
		ShownGrowth = 1.f;
	}
	else if (State == ESlimeFarmPlotState::Withered)
	{
		Line += TEXT("  枯萎");
	}
	else if (Moisture < 0.1f)
	{
		Line += TEXT("  缺水");
	}
	else
	{
		const float Rate = ComputeGrowthPerSecond(false);
		if (Rate <= KINDA_SMALL_NUMBER)
		{
			Line += TEXT("  暂停");
		}
		else
		{
			const int32 Left = FMath::Max(0, FMath::CeilToInt((1.f - Growth01) / Rate));
			const int32 Minutes = Left / 60;
			const int32 Seconds = Left % 60;
			if (Minutes > 0)
			{
				Line += FString::Printf(TEXT("  还要 %d分%02d秒"), Minutes, Seconds);
			}
			else
			{
				Line += FString::Printf(TEXT("  还要 %d秒"), Seconds);
			}
		}
	}
	Widget->SetStatus(FText::FromString(Line), ShownGrowth, BarWidth, bShowBar);
	StatusWidget->SetDrawSize(FVector2D(FMath::Max(BarWidth + 24.f, 220.f), bShowBar ? 64.f : 36.f));
}

void ASlimeFarmPlot::UpdateStatusVisibility()
{
	if (!StatusWidget || !GetWorld())
	{
		return;
	}

	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	bool bShow = false;
	if (const ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(PC))
	{
		if (SlimePC->ShouldHideWorldPrompts())
		{
			StatusWidget->SetHiddenInGame(true);
			StatusWidget->SetVisibility(false);
			return;
		}
	}
	if (Pawn)
	{
		constexpr float ShowRadius = 420.f;
		const FVector PlayerLoc = Pawn->GetActorLocation();
		const float MyDistSq = FVector::DistSquared2D(PlayerLoc, GetActorLocation());
		if (MyDistSq <= FMath::Square(ShowRadius))
		{
			bShow = true;
			for (TActorIterator<ASlimeFarmPlot> It(GetWorld()); It; ++It)
			{
				if (*It == this)
				{
					continue;
				}
				const float OtherSq = FVector::DistSquared2D(PlayerLoc, It->GetActorLocation());
				if (OtherSq < MyDistSq || (FMath::IsNearlyEqual(OtherSq, MyDistSq) && It->GetUniqueID() < GetUniqueID()))
				{
					bShow = false;
					break;
				}
			}
		}
	}

	StatusWidget->SetHiddenInGame(!bShow);
	StatusWidget->SetVisibility(bShow);
	if (bShow)
	{
		RefreshStatus();
	}
}

FText ASlimeFarmPlot::GetInteractPromptVerb() const
{
	if (!Crop || State == ESlimeFarmPlotState::Empty)
	{
		return FText::FromString(TEXT("选择作物"));
	}
	if (State == ESlimeFarmPlotState::Mature)
	{
		return FText::FromString(TEXT("收获"));
	}
	if (State == ESlimeFarmPlotState::Withered)
	{
		return FText::FromString(TEXT("清理"));
	}
	if (State == ESlimeFarmPlotState::Growing)
	{
		return FText::FromString(TEXT("查看"));
	}

	return FText::FromString(TEXT("选择作物"));
}

FVector ASlimeFarmPlot::GetPromptWorldLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, 90.f);
}

bool ASlimeFarmPlot::TryInteract(APawn* Interactor)
{
	if (!Interactor)
	{
		return false;
	}
	UGameInstance* GI = GetGameInstance();
	USlimeFarmSubsystem* Farm = GI ? GI->GetSubsystem<USlimeFarmSubsystem>() : nullptr;
	USlimeInventorySubsystem* Inv = GI ? GI->GetSubsystem<USlimeInventorySubsystem>() : nullptr;
	if (!Farm || !Inv)
	{
		return false;
	}

	if (State == ESlimeFarmPlotState::Empty || !Crop)
	{
		if (!Crop && State != ESlimeFarmPlotState::Empty)
		{
			State = ESlimeFarmPlotState::Empty;
			Growth01 = 0.f;
			Quality = ESlimeCropQuality::Common;
			bHarvestedLook = false;
			SaveRecord(true);
			RefreshVisual();
			RefreshStatus();
		}
		USlimeSeedPickerWidget::OpenForPlot(this);
		return true;
	}

	if (State == ESlimeFarmPlotState::Withered)
	{
		Crop = nullptr;
		State = ESlimeFarmPlotState::Empty;
		Growth01 = 0.f;
		Quality = ESlimeCropQuality::Common;
		SaveRecord(true);
		RefreshVisual();
		RefreshStatus();
		return true;
	}

	if (State != ESlimeFarmPlotState::Mature || !Crop)
	{
		return false;
	}

	const FName YieldId = Crop->ResolveYieldItemId();
	int32 Count = FMath::RandRange(Crop->YieldMin, FMath::Max(Crop->YieldMin, Crop->YieldMax));
	if (Quality == ESlimeCropQuality::Fine)
	{
		Count += 1;
	}
	else if (Quality == ESlimeCropQuality::Mutated)
	{
		Count *= 2;
	}
	int32 Nx = 1;
	int32 Ny = 1;
	float AvgSize = 1.f;
	float MaxSize = 1.f;
	CollectPlantLayout(Nx, Ny, AvgSize, MaxSize);
	const int32 PlantCount = FMath::Max(1, Nx * Ny);
	const float Density = FMath::Max(1.f, FMath::Sqrt(static_cast<float>(PlantCount) / 4.f));
	Count = FMath::Max(1, FMath::RoundToInt(static_cast<float>(Count) * AvgSize * Density));
	if (!YieldId.IsNone())
	{
		Inv->AddItem(YieldId, Count);
	}

	const bool bGiant = MaxSize >= 1.5f;
	const FString Label = bGiant
		? FString::Printf(TEXT("巨大的%s！"), *Crop->DisplayName.ToString())
		: FString::Printf(TEXT("+%d %s"), Count, *Crop->DisplayName.ToString());
	USlimeFloatingTextWidget::Spawn(
		this,
		GetActorLocation() + FVector(0, 0, 100),
		FText::FromString(Label),
		bGiant ? FLinearColor(1.f, 0.82f, 0.2f) : FLinearColor(0.95f, 0.85f, 0.35f),
		true);

	if (Crop->bRegrowAfterHarvest)
	{
		Growth01 = FMath::Clamp(Crop->RegrowFromGrowth, 0.04f, 0.9f);
		State = ESlimeFarmPlotState::Growing;
		bHarvestedLook = Crop->HarvestedMeshes.Num() > 0;
		LastVisualKey = MIN_int32;
		SaveRecord(true);
		RefreshVisual();
		RefreshStatus();
		return true;
	}

	Crop = nullptr;
	State = ESlimeFarmPlotState::Empty;
	Growth01 = 0.f;
	bHarvestedLook = false;
	PlantSeed = 0;
	Quality = ESlimeCropQuality::Common;
	HueShift = 0.f;
	SaveRecord(true);
	RefreshVisual();
	RefreshStatus();
	return true;
}

bool ASlimeFarmPlot::CanReceiveLightning() const
{
	return State == ESlimeFarmPlotState::Growing || State == ESlimeFarmPlotState::Mature;
}

void ASlimeFarmPlot::NoteElement(ESlimeElement Element)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	RecentElements.Add({Element, Now});
	RecentElements.RemoveAll([Now](const FRecentElement& Entry)
	{
		return Now - Entry.TimeSeconds > 8.f;
	});
}

bool ASlimeFarmPlot::HadElementRecently(ESlimeElement Element, float WindowSeconds) const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	for (const FRecentElement& Entry : RecentElements)
	{
		if (Entry.Element == Element && Now - Entry.TimeSeconds <= WindowSeconds)
		{
			return true;
		}
	}
	return false;
}

void ASlimeFarmPlot::RaiseQuality()
{
	if (Quality == ESlimeCropQuality::Common)
	{
		Quality = ESlimeCropQuality::Fine;
	}
	else if (Quality == ESlimeCropQuality::Fine)
	{
		Quality = ESlimeCropQuality::Mutated;
	}
}

void ASlimeFarmPlot::ReceiveElement_Implementation(ESlimeElement Element, AActor* SourceActor, float Strength)
{
	const float Amount = FMath::Max(Strength, 0.1f);
	const bool bFromSky = Cast<ASlimeHubSkyDirector>(SourceActor) != nullptr;
	if (!bFromSky
		&& Crop && State != ESlimeFarmPlotState::Empty
		&& (Element == ESlimeElement::Fire || Element == ESlimeElement::Lightning)
		&& Element != Crop->PreferredElement)
	{
		const bool bFire = Element == ESlimeElement::Fire;
		Crop = nullptr;
		State = ESlimeFarmPlotState::Empty;
		Growth01 = 0.f;
		Quality = ESlimeCropQuality::Common;
		bHarvestedLook = false;
		Heat = 0.f;
		FireBoostSeconds = 0.f;
		SteamSeconds = 0.f;
		PreferredSeconds = 0.f;
		USlimeFloatingTextWidget::Spawn(
			this,
			GetActorLocation() + FVector(0.f, 0.f, 80.f),
			FText::FromString(bFire ? TEXT("烧毁") : TEXT("劈毁")),
			bFire ? FLinearColor(0.95f, 0.35f, 0.12f) : FLinearColor(0.7f, 0.75f, 1.f),
			true);
		SaveRecord(false);
		RefreshVisual();
		RefreshStatus();
		return;
	}

	const bool bHadWater = HadElementRecently(ESlimeElement::Water, 8.f);
	NoteElement(Element);
	bool bAccelerated = false;

	if (Element == ESlimeElement::Water)
	{
		Moisture = FMath::Clamp(Moisture + 0.4f * Amount, 0.f, 1.f);
		DrySeconds = 0.f;
		if (HadElementRecently(ESlimeElement::Fire, 8.f) && State == ESlimeFarmPlotState::Growing)
		{
			SteamSeconds = FMath::Max(SteamSeconds, 30.f);
			bAccelerated = true;
		}
	}
	else if (Element == ESlimeElement::Fire)
	{
		if (State == ESlimeFarmPlotState::Growing)
		{
			Growth01 = FMath::Clamp(Growth01 + 0.06f * Amount, 0.f, 1.f);
			FireBoostSeconds = FMath::Max(FireBoostSeconds, 8.f);
			bAccelerated = true;
		}
		Heat += 0.5f * Amount;
		if (Heat > 1.25f && State != ESlimeFarmPlotState::Empty)
		{
			State = ESlimeFarmPlotState::Withered;
			Heat = 0.f;
		}
		if (bHadWater && State == ESlimeFarmPlotState::Growing)
		{
			SteamSeconds = FMath::Max(SteamSeconds, 30.f);
			Heat = FMath::Max(0.f, Heat - 0.4f);
			bAccelerated = true;
			USlimeFloatingTextWidget::Spawn(this, GetActorLocation() + FVector(0, 0, 80), FText::FromString(TEXT("蒸汽温室")), FLinearColor(0.8f, 0.85f, 0.9f), true);
		}
	}
	else if (Element == ESlimeElement::Lightning)
	{
		if (bHadWater)
		{
			RaiseQuality();
			USlimeFloatingTextWidget::Spawn(this, GetActorLocation() + FVector(0, 0, 80), FText::FromString(TEXT("雷击催熟")), FLinearColor(0.7f, 0.75f, 1.f), true);
		}
		else if (FMath::FRand() < 0.4f * Amount)
		{
			RaiseQuality();
		}
	}
	else if (Element == ESlimeElement::Wind)
	{
		if (Crop && Crop->bFlower && State != ESlimeFarmPlotState::Empty)
		{
			HueShift += 0.08f * Amount;
		}
	}
	else if (Element == ESlimeElement::Dark)
	{
		Fertility = FMath::Clamp(Fertility + 0.25f * Amount, 0.f, 1.f);
	}
	else if (Element == ESlimeElement::Physical)
	{
		if (State == ESlimeFarmPlotState::Withered)
		{
			Crop = nullptr;
			State = ESlimeFarmPlotState::Empty;
			Growth01 = 0.f;
			Quality = ESlimeCropQuality::Common;
			Moisture = FMath::Max(Moisture, 0.3f);
			Fertility = FMath::Max(Fertility, 0.4f);
		}
		else
		{
			Fertility = FMath::Clamp(Fertility + 0.08f * Amount, 0.f, 1.f);
		}
	}

	if (Crop && Element == Crop->PreferredElement)
	{
		PreferredSeconds = FMath::Max(PreferredSeconds, 12.f);
		bAccelerated = true;
	}

	if (bAccelerated)
	{
		USlimeFloatingTextWidget::Spawn(
			this,
			GetActorLocation() + FVector(0.f, 0.f, 110.f),
			FText::FromString(TEXT("加速生长中")),
			FLinearColor(0.45f, 0.9f, 0.4f),
			true);
	}

	SaveRecord(false);
	RefreshVisual();
	RefreshStatus();
}

namespace
{
	constexpr float BedCell = 50.f;
	constexpr float BedInset = 2.f;
	constexpr float BedTop = 8.f;
	constexpr float BedSink = 5.f;
	constexpr float BedFlatSink = 1.f;
	constexpr float BedMaxSlopeCos = 0.9781476f;

	struct FBedSample
	{
		bool bHit = false;
		float Z = 0.f;
		FVector Normal = FVector::UpVector;
	};

	FBedSample TraceBed(UWorld* World, const AActor* Ignore, const FVector& XY, float GuessZ)
	{
		FBedSample Sample;
		if (!World)
		{
			return Sample;
		}
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(FarmBed), false, Ignore);
		const FVector Start(XY.X, XY.Y, GuessZ + 800.f);
		const FVector End(XY.X, XY.Y, GuessZ - 4000.f);
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
		{
			Sample.bHit = true;
			Sample.Z = Hit.ImpactPoint.Z;
			Sample.Normal = Hit.ImpactNormal.GetSafeNormal();
		}
		return Sample;
	}
}

void ASlimeFarmPlot::ConfigureSoil(UStaticMesh* Mesh, bool bInRaisedBed)
{
	if (Mesh)
	{
		SoilMeshAsset = Mesh;
		if (Soil)
		{
			Soil->SetStaticMesh(Mesh);
		}
	}
	bRaisedBed = bInRaisedBed;
	if (HasActorBegunPlay())
	{
		CacheSoilLook();
	}
}

FSlimeBedPlacement ASlimeFarmPlot::ComputeBedPlacement(
	UWorld* World,
	const AActor* Ignore,
	UStaticMesh* Mesh,
	bool bInRaisedBed,
	const FVector& CenterXY,
	float YawDegrees)
{
	FSlimeBedPlacement Out;
	Out.YawDegrees = YawDegrees;
	Out.ActorLocation = CenterXY;
	if (!Mesh)
	{
		return Out;
	}

	const FBox Box = Mesh->GetBoundingBox();
	const FVector Size = Box.GetSize();
	if (Size.X < 1.f || Size.Y < 1.f || Size.Z < 1.f)
	{
		return Out;
	}

	auto CellsFor = [](float Length) -> int32
	{
		if (Length < 20.f)
		{
			return 1;
		}
		return FMath::Max(FMath::RoundToInt(Length / BedCell), 1);
	};
	Out.CellsX = CellsFor(Size.X);
	Out.CellsY = CellsFor(Size.Y);
	const bool bThinX = Size.X < 20.f;
	const bool bThinY = Size.Y < 20.f;
	const float LongScale = (Size.X >= Size.Y ? (Out.CellsX * BedCell - BedInset * 2.f) / Size.X
		: (Out.CellsY * BedCell - BedInset * 2.f) / Size.Y);
	FVector Scale = FVector::OneVector;
	Scale.X = bThinX ? LongScale : (Out.CellsX * BedCell - BedInset * 2.f) / Size.X;
	Scale.Y = bThinY ? LongScale : (Out.CellsY * BedCell - BedInset * 2.f) / Size.Y;
	Scale.Z = 1.f;
	if (bInRaisedBed && Size.Z * Scale.Z < BedTop + BedSink)
	{
		Scale.Z = (BedTop + BedSink) / Size.Z;
	}

	const FRotator YawRot(0.f, YawDegrees, 0.f);
	const float HalfX = (bThinX ? Size.X * Scale.X : Out.CellsX * BedCell - BedInset * 2.f) * 0.5f;
	const float HalfY = (bThinY ? Size.Y * Scale.Y : Out.CellsY * BedCell - BedInset * 2.f) * 0.5f;
	Out.HalfX = HalfX;
	Out.HalfY = HalfY;
	const FVector AxisX = YawRot.RotateVector(FVector(HalfX, 0.f, 0.f));
	const FVector AxisY = YawRot.RotateVector(FVector(0.f, HalfY, 0.f));
	const FVector Points[5] = {
		CenterXY,
		CenterXY + AxisX + AxisY,
		CenterXY + AxisX - AxisY,
		CenterXY - AxisX + AxisY,
		CenterXY - AxisX - AxisY
	};

	FBedSample Samples[5];
	bool bAll = World != nullptr;
	float MaxZ = -BIG_NUMBER;
	float MinZ = BIG_NUMBER;
	float MinDot = 1.f;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		Samples[Index] = TraceBed(World, Ignore, Points[Index], CenterXY.Z);
		bAll = bAll && Samples[Index].bHit;
		if (Samples[Index].bHit)
		{
			MaxZ = FMath::Max(MaxZ, Samples[Index].Z);
			MinZ = FMath::Min(MinZ, Samples[Index].Z);
			MinDot = FMath::Min(MinDot, FVector::DotProduct(Samples[Index].Normal, FVector::UpVector));
		}
	}

	const bool bHugGround = Mesh->GetName().Contains(TEXT("FertileGround"));
	if (bHugGround)
	{
		const bool bCenter = Samples[0].bHit;
		Out.bValid = bCenter;
		Out.SoilScale = Scale;
		const float GroundZ = bCenter ? Samples[0].Z : CenterXY.Z;
		Out.ActorLocation = FVector(CenterXY.X, CenterXY.Y, GroundZ);
		Out.SoilRelativeZ = -BedFlatSink - Box.Min.Z * Scale.Z;
		Out.PlantLocalZ = Out.SoilRelativeZ + (Box.Min.Z + Size.Z * 0.42f) * Scale.Z;
		return Out;
	}

	const float Span = bAll ? MaxZ - MinZ : 0.f;
	bool bSlopeOk = bAll && MinDot >= BedMaxSlopeCos;
	if (bInRaisedBed && bAll)
	{
		const float Need = Span + BedTop + BedSink;
		if (Size.Z * Scale.Z + KINDA_SMALL_NUMBER < Need)
		{
			if (Need <= Size.Z * 1.35f)
			{
				Scale.Z = Need / Size.Z;
			}
			else
			{
				bSlopeOk = false;
			}
		}
	}

	Out.bValid = bSlopeOk;
	Out.SoilScale = Scale;
	const float GroundZ = bAll ? (bInRaisedBed ? MaxZ : Samples[0].Z) : CenterXY.Z;
	Out.ActorLocation = FVector(CenterXY.X, CenterXY.Y, GroundZ);
	if (bInRaisedBed)
	{
		Out.SoilRelativeZ = BedTop - Box.Max.Z * Scale.Z;
		Out.PlantLocalZ = BedTop;
	}
	else
	{
		Out.SoilRelativeZ = -BedFlatSink - Box.Min.Z * Scale.Z;
		Out.PlantLocalZ = Out.SoilRelativeZ + (Box.Min.Z + Size.Z * 0.42f) * Scale.Z;
	}
	return Out;
}

void ASlimeFarmPlot::FitSoil()
{
	UStaticMesh* Mesh = SoilMeshAsset;
	if (!Mesh && Soil)
	{
		Mesh = Soil->GetStaticMesh();
	}
	const FSlimeBedPlacement Bed = ComputeBedPlacement(
		GetWorld(), this, Mesh, bRaisedBed, GetActorLocation(), GetActorRotation().Yaw);
	BedHalfX = Bed.HalfX;
	BedHalfY = Bed.HalfY;
	PlantLocalZ = Bed.PlantLocalZ;
	if (Bed.bValid)
	{
		SetActorLocation(Bed.ActorLocation);
	}
	if (Soil && Mesh)
	{
		Soil->SetStaticMesh(Mesh);
		Soil->SetRelativeScale3D(Bed.SoilScale);
		Soil->SetRelativeLocation(FVector(0.f, 0.f, Bed.SoilRelativeZ));
	}
}

bool ASlimeFarmPlot::PlantCrop(USlimeCropDefinition* NextCrop)
{
	if (!NextCrop || State != ESlimeFarmPlotState::Empty)
	{
		return false;
	}
	Crop = NextCrop;
	State = ESlimeFarmPlotState::Growing;
	Growth01 = 0.04f;
	Quality = ESlimeCropQuality::Common;
	HueShift = 0.f;
	bHarvestedLook = false;
	PlantSeed = FMath::RandRange(1, 2147483646);
	Moisture = FMath::Max(Moisture, 0.55f);
	Fertility = FMath::Max(Fertility, 0.3f);
	DrySeconds = 0.f;
	LastStageIndex = INDEX_NONE;
	LastVisualKey = MIN_int32;
	PopLeft = 0.25f;
	PreloadCropMeshes();
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USlimeFarmSubsystem* Farm = GI->GetSubsystem<USlimeFarmSubsystem>())
		{
			Farm->SetLastPlantedCrop(NextCrop->CropId);
		}
	}
	SaveRecord(true);
	RefreshVisual();
	RefreshStatus();
	return true;
}

int32 ASlimeFarmPlot::ResolvePlantSeed() const
{
	if (PlantSeed != 0)
	{
		return PlantSeed;
	}
	const uint32 Hash = GetTypeHash(PlotId);
	return static_cast<int32>(Hash == 0 ? 1u : Hash);
}

float ASlimeFarmPlot::PlantSizeAt(int32 Slot, int32 Nx) const
{
	const int32 Seed = ResolvePlantSeed();
	FRandomStream Stream(Seed * 17 + Slot * 131 + Nx * 17);
	const float Roll = Stream.FRand();
	const float SizeMin = Crop ? Crop->SizeMin : 0.8f;
	const float SizeMax = Crop ? Crop->SizeMax : 1.7f;
	float Size = Roll < 0.82f
		? FMath::Lerp(FMath::Max(SizeMin, 0.85f), FMath::Min(SizeMax, 1.25f), Stream.FRand())
		: FMath::Lerp(FMath::Min(SizeMax, 1.25f), SizeMax, Stream.FRand());
	if (Quality == ESlimeCropQuality::Fine)
	{
		Size += 0.1f;
	}
	else if (Quality == ESlimeCropQuality::Mutated)
	{
		Size += 0.25f;
	}
	return FMath::Clamp(Size, SizeMin, SizeMax);
}

void ASlimeFarmPlot::PreloadCropMeshes()
{
	if (!Crop)
	{
		return;
	}
	TArray<FSoftObjectPath> Paths;
	auto AddMesh = [&Paths](const TSoftObjectPtr<UStaticMesh>& Mesh)
	{
		if (!Mesh.IsNull())
		{
			Paths.AddUnique(Mesh.ToSoftObjectPath());
		}
	};
	for (const FSlimeCropStage& Stage : Crop->Stages)
	{
		for (const TSoftObjectPtr<UStaticMesh>& Mesh : Stage.Meshes)
		{
			AddMesh(Mesh);
		}
	}
	for (const TSoftObjectPtr<UStaticMesh>& Mesh : Crop->HarvestedMeshes)
	{
		AddMesh(Mesh);
	}
	AddMesh(Crop->ProduceMesh);
	if (Paths.Num() == 0)
	{
		return;
	}
	CropMeshHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Paths,
		FStreamableDelegate::CreateUObject(this, &ASlimeFarmPlot::HandleCropMeshesLoaded));
}

void ASlimeFarmPlot::HandleCropMeshesLoaded()
{
	if (bSkipSaveOnEnd)
	{
		return;
	}
	LastVisualKey = MIN_int32;
	RebuildPlants(true);
}

void ASlimeFarmPlot::GetPlantSpan(float& OutSpanX, float& OutSpanY, bool& bOutRidge) const
{
	const float FullX = FMath::Max(BedHalfX * 2.f, 30.f);
	const float FullY = FMath::Max(BedHalfY * 2.f, 30.f);
	const UStaticMesh* Mesh = Soil ? Soil->GetStaticMesh() : nullptr;
	if (!Mesh)
	{
		Mesh = SoilMeshAsset;
	}
	bOutRidge = Mesh && Mesh->GetName().Contains(TEXT("FertileGround"));
	if (!bOutRidge)
	{
		OutSpanX = FullX * 0.86f;
		OutSpanY = FullY * 0.86f;
		return;
	}
	// The mound tapers at both ends and is narrower than its box. Keep crops on the crown.
	if (FullX >= FullY)
	{
		OutSpanX = FullX * 0.62f;
		OutSpanY = FullY;
	}
	else
	{
		OutSpanX = FullX;
		OutSpanY = FullY * 0.62f;
	}
}

void ASlimeFarmPlot::CollectPlantLayout(int32& OutNx, int32& OutNy, float& OutAvgSize, float& OutMaxSize) const
{
	float AreaX = 30.f;
	float AreaY = 30.f;
	bool bRidge = false;
	GetPlantSpan(AreaX, AreaY, bRidge);
	AreaX = FMath::Max(AreaX, 30.f);
	AreaY = FMath::Max(AreaY, 30.f);
	// Tall crops are authored as PlantGrid 1. A square bed still gets a 2x2, and a long ridge keeps that spacing along its length.
	const int32 Grid = FMath::Max(Crop ? FMath::Clamp(Crop->PlantGrid, 1, 4) : 1, 2);
	constexpr float MinStep = 36.f;
	const float Short = FMath::Min(AreaX, AreaY);
	const float Step = FMath::Max(Short / static_cast<float>(Grid), MinStep);
	OutNx = FMath::Clamp(FMath::FloorToInt(AreaX / Step), 1, 16);
	OutNy = FMath::Clamp(FMath::FloorToInt(AreaY / Step), 1, 16);
	if (AreaX / static_cast<float>(Grid) >= MinStep)
	{
		OutNx = FMath::Max(OutNx, Grid);
	}
	if (AreaY / static_cast<float>(Grid) >= MinStep)
	{
		OutNy = FMath::Max(OutNy, Grid);
	}
	constexpr int32 MaxPlants = 48;
	while (OutNx * OutNy > MaxPlants)
	{
		if (OutNx >= OutNy && OutNx > 1)
		{
			--OutNx;
		}
		else if (OutNy > 1)
		{
			--OutNy;
		}
		else
		{
			break;
		}
	}
	if (bRidge)
	{
		if (BedHalfX >= BedHalfY)
		{
			OutNy = 1;
		}
		else
		{
			OutNx = 1;
		}
	}
	float Sum = 0.f;
	OutMaxSize = 0.f;
	const int32 Count = OutNx * OutNy;
	for (int32 Slot = 0; Slot < Count; ++Slot)
	{
		const float Size = PlantSizeAt(Slot, OutNx);
		Sum += Size;
		OutMaxSize = FMath::Max(OutMaxSize, Size);
	}
	OutAvgSize = Count > 0 ? Sum / static_cast<float>(Count) : 1.f;
}

const FSlimeCropStage* ASlimeFarmPlot::PickStage(float& OutScale) const
{
	OutScale = 1.f;
	if (!Crop || Crop->Stages.Num() == 0)
	{
		return nullptr;
	}
	const FSlimeCropStage* Best = nullptr;
	float NextStart = 1.f;
	for (const FSlimeCropStage& Stage : Crop->Stages)
	{
		if (Growth01 + KINDA_SMALL_NUMBER >= Stage.StartGrowth && (!Best || Stage.StartGrowth >= Best->StartGrowth))
		{
			Best = &Stage;
		}
	}
	if (!Best)
	{
		Best = &Crop->Stages[0];
	}
	for (const FSlimeCropStage& Stage : Crop->Stages)
	{
		if (Stage.StartGrowth > Best->StartGrowth + KINDA_SMALL_NUMBER)
		{
			NextStart = FMath::Min(NextStart, Stage.StartGrowth);
		}
	}
	const float Span = FMath::Max(NextStart - Best->StartGrowth, 0.01f);
	const float Alpha = FMath::Clamp((Growth01 - Best->StartGrowth) / Span, 0.f, 1.f);
	OutScale = FMath::Lerp(Best->ScaleFrom, Best->ScaleTo, Alpha);
	return Best;
}

void ASlimeFarmPlot::RebuildPlants(bool bForce)
{
	(void)bForce;
	const bool bShowCrop = Crop && State != ESlimeFarmPlotState::Empty;
	float StageScale = 1.f;
	const FSlimeCropStage* Stage = bShowCrop && !bHarvestedLook ? PickStage(StageScale) : nullptr;
	const bool bUseHarvested = bShowCrop && bHarvestedLook && Crop->HarvestedMeshes.Num() > 0;
	const bool bHasMesh = bUseHarvested || (Stage && Stage->Meshes.Num() > 0);

	int32 StageIndex = INDEX_NONE;
	if (Stage && Crop)
	{
		StageIndex = Crop->Stages.IndexOfByPredicate([Stage](const FSlimeCropStage& Item)
		{
			return &Item == Stage;
		});
	}
	if (bHasMesh && StageIndex != LastStageIndex && LastStageIndex != INDEX_NONE)
	{
		PopLeft = 0.25f;
	}
	if (bHasMesh)
	{
		LastStageIndex = StageIndex;
	}

	const int32 Quant = FMath::FloorToInt(Growth01 * 50.f);
	const int32 Key = Quant
		+ StageIndex * 64
		+ (bHarvestedLook ? 100000 : 0)
		+ static_cast<int32>(State) * 1000003
		+ static_cast<int32>(Quality) * 17
		+ (PlantSeed % 997) * 131
		+ (Crop ? GetTypeHash(Crop->CropId) : 0);
	if (Key == LastVisualKey && PopLeft <= 0.f)
	{
		return;
	}
	LastVisualKey = Key;

	for (UInstancedStaticMeshComponent* Ism : PlantIsms)
	{
		if (Ism)
		{
			Ism->ClearInstances();
			Ism->SetVisibility(false);
		}
	}

	auto BucketFor = [this](UStaticMesh* Mesh) -> UInstancedStaticMeshComponent*
	{
		if (!Mesh)
		{
			return nullptr;
		}
		for (UInstancedStaticMeshComponent* Ism : PlantIsms)
		{
			if (Ism && Ism->GetStaticMesh() == Mesh)
			{
				Ism->SetVisibility(true);
				return Ism;
			}
		}
		UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(this);
		Ism->SetupAttachment(GetRootComponent());
		Ism->SetStaticMesh(Mesh);
		Ism->SetMobility(EComponentMobility::Movable);
		Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ism->SetCastShadow(true);
		Ism->SetGenerateOverlapEvents(false);
		Ism->RegisterComponent();
		PlantIsms.Add(Ism);
		return Ism;
	};

	if (!bHasMesh)
	{
		if (Plant)
		{
			Plant->SetVisibility(bShowCrop);
			Plant->UpdatePlant(
				bShowCrop ? Crop : nullptr,
				PlotId,
				bShowCrop ? FMath::Max(Growth01, 0.04f) : 0.f,
				HueShift,
				Quality,
				State == ESlimeFarmPlotState::Withered);
		}
		return;
	}

	if (Plant)
	{
		Plant->SetVisibility(false);
	}

	int32 Nx = 1;
	int32 Ny = 1;
	float Avg = 1.f;
	float MaxSize = 1.f;
	CollectPlantLayout(Nx, Ny, Avg, MaxSize);
	float AreaX = 30.f;
	float AreaY = 30.f;
	bool bRidgeSpan = false;
	GetPlantSpan(AreaX, AreaY, bRidgeSpan);
	AreaX = FMath::Max(AreaX, 30.f);
	AreaY = FMath::Max(AreaY, 30.f);
	const float StepX = Nx > 1 ? AreaX / static_cast<float>(Nx) : 0.f;
	const float StepY = Ny > 1 ? AreaY / static_cast<float>(Ny) : 0.f;
	const float Pop = PopLeft > 0.f ? FMath::Lerp(1.f, 0.82f, PopLeft / 0.25f) : 1.f;
	const float Wither = State == ESlimeFarmPlotState::Withered ? 0.75f : 1.f;
	const TArray<TSoftObjectPtr<UStaticMesh>>& Source = bUseHarvested ? Crop->HarvestedMeshes : Stage->Meshes;
	const int32 Seed = ResolvePlantSeed();

	for (int32 Y = 0; Y < Ny; ++Y)
	{
		for (int32 X = 0; X < Nx; ++X)
		{
			const int32 Slot = Y * Nx + X;
			UStaticMesh* Mesh = nullptr;
			if (Source.Num() > 0)
			{
				const int32 Pick = FMath::Abs(Seed * 13 + Slot * 7) % Source.Num();
				Mesh = Source[Pick].Get();
				if (!Mesh)
				{
					Mesh = Source[Pick].LoadSynchronous();
				}
				if (!Mesh)
				{
					Mesh = Source[0].Get();
					if (!Mesh)
					{
						Mesh = Source[0].LoadSynchronous();
					}
				}
			}
			UInstancedStaticMeshComponent* Ism = BucketFor(Mesh);
			if (!Ism)
			{
				continue;
			}
			FRandomStream Stream(Seed * 29 + Slot * 97);
			const float JitterX = (Stream.FRand() - 0.5f) * StepX * 0.22f;
			const float JitterY = (Stream.FRand() - 0.5f) * StepY * 0.22f;
			const float LocalX = (Nx > 1 ? -AreaX * 0.5f + StepX * (static_cast<float>(X) + 0.5f) : 0.f) + JitterX;
			const float LocalY = (Ny > 1 ? -AreaY * 0.5f + StepY * (static_cast<float>(Y) + 0.5f) : 0.f) + JitterY;
			const float Size = PlantSizeAt(Slot, Nx) * (bUseHarvested ? 1.f : StageScale) * Pop * Wither;
			const FTransform Instance(
				FRotator(0.f, Stream.FRand() * 360.f, 0.f),
				FVector(LocalX, LocalY, PlantLocalZ),
				FVector(Size));
			Ism->AddInstance(Instance, false);
		}
	}
}

// NPC damage is not harvesting: no inventory award and no regrowth shortcut.
bool ASlimeFarmPlot::GetNpcDamagePoint(const FVector& From, float Range, FVector& OutPoint) const
{
 if (!Crop || bSkipSaveOnEnd || (State != ESlimeFarmPlotState::Growing && State != ESlimeFarmPlotState::Mature)) return false;
 const FRotator Yaw(0, GetActorRotation().Yaw, 0);
 const FVector Local = Yaw.UnrotateVector(From - GetActorLocation());
 const FVector Edge(FMath::Clamp(Local.X, -BedHalfX, BedHalfX), FMath::Clamp(Local.Y, -BedHalfY, BedHalfY), PlantLocalZ + 40.f);
 OutPoint = GetActorLocation() + Yaw.RotateVector(Edge);
 return FVector::DistSquared2D(From, OutPoint) <= FMath::Square(Range) && FMath::Abs(From.Z - OutPoint.Z) < 150.f;
}

bool ASlimeFarmPlot::DestroyCropByNpc()
{
 if (!Crop || bSkipSaveOnEnd || (State != ESlimeFarmPlotState::Growing && State != ESlimeFarmPlotState::Mature)) return false;
 Crop = nullptr;
 State = ESlimeFarmPlotState::Empty;
 Growth01 = 0.f;
 bHarvestedLook = false;
 PlantSeed = 0;
 Quality = ESlimeCropQuality::Common;
 HueShift = 0.f;
 PopLeft = 0.f;
 LastVisualKey = MIN_int32;
 SaveRecord(true);
 RefreshVisual();
 RefreshStatus();
 return true;
}
