// Copyright Epic Games, Inc. All Rights Reserved.

#include "Farm/SlimeFarmPlot.h"

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
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
#include "UI/SlimeFloatingTextWidget.h"

ASlimeFarmPlot::ASlimeFarmPlot()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Soil = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Soil"));
	Soil->SetupAttachment(Root);
	Soil->SetMobility(EComponentMobility::Movable);
	Soil->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Soil->SetRelativeLocation(FVector(0.f, 0.f, 6.f));
	Soil->SetRelativeScale3D(FVector(2.2f, 2.2f, 0.12f));
	Soil->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Soil->SetCollisionObjectType(ECC_WorldStatic);
	Soil->SetCollisionResponseToAllChannels(ECR_Block);
	Soil->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Soil->SetGenerateOverlapEvents(true);

	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		Soil->SetMaterial(0, Base);
	}

	Plant = CreateDefaultSubobject<USlimeProceduralPlantComponent>(TEXT("Plant"));
	Plant->SetupAttachment(Root);
	Plant->SetRelativeLocation(FVector(0.f, 0.f, 12.f));

	StatusWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("Status"));
	StatusWidget->SetupAttachment(Root);
	StatusWidget->SetWidgetSpace(EWidgetSpace::Screen);
	StatusWidget->SetWidgetClass(USlimePlotStatusWidget::StaticClass());
	StatusWidget->SetDrawSize(FVector2D(280.f, 36.f));
	StatusWidget->SetRelativeLocation(FVector(0.f, 0.f, 28.f));
	StatusWidget->SetHiddenInGame(true);
	StatusWidget->SetVisibility(false);
	StatusWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StatusWidget->SetGenerateOverlapEvents(false);
}

void ASlimeFarmPlot::BeginPlay()
{
	Super::BeginPlay();
	if (Soil)
	{
		SoilMid = Soil->CreateDynamicMaterialInstance(0);
		if (SoilMid)
		{
			SoilMid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.28f, 0.16f, 0.08f));
		}
	}
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
	SaveRecord(true);
	Super::EndPlay(EndPlayReason);
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
	LastUpdateUnix = Record.LastUpdateUnix;
	Crop = Farm->FindCrop(Record.CropId);

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
	SaveRecord(true);
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

	if (SoilMid)
	{
		const FLinearColor Dry(0.32f, 0.18f, 0.08f);
		const FLinearColor Wet(0.12f, 0.08f, 0.05f);
		SoilMid->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(Dry, Wet, Moisture));
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
		SaveRecord(true);
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
				WeatherMul = 0.f;
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
	return (1.f / Crop->GrowSeconds) * MoistureMul * DayMul * WeatherMul * Bonus;
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
	if (!Plant)
	{
		return;
	}
	Plant->UpdatePlant(
		Crop,
		PlotId,
		State == ESlimeFarmPlotState::Empty ? 0.f : FMath::Max(Growth01, 0.04f),
		HueShift,
		Quality,
		State == ESlimeFarmPlotState::Withered);
}

void ASlimeFarmPlot::RefreshStatus()
{
	USlimePlotStatusWidget* Widget = StatusWidget ? Cast<USlimePlotStatusWidget>(StatusWidget->GetUserWidgetObject()) : nullptr;
	if (!Widget)
	{
		return;
	}

	FString Line;
	if (State == ESlimeFarmPlotState::Empty || !Crop)
	{
		Line = TEXT("空地");
	}
	else
	{
		const TCHAR* QualityName = TEXT("普通");
		if (Quality == ESlimeCropQuality::Fine) QualityName = TEXT("优良");
		if (Quality == ESlimeCropQuality::Mutated) QualityName = TEXT("变异");
		const TCHAR* StateName = TEXT("生长");
		if (State == ESlimeFarmPlotState::Mature) StateName = TEXT("成熟");
		if (State == ESlimeFarmPlotState::Withered) StateName = TEXT("枯萎");
		Line = FString::Printf(TEXT("%s  %s  %s  %d%%  水%d%%"),
			*Crop->DisplayName.ToString(),
			QualityName,
			StateName,
			FMath::RoundToInt(Growth01 * 100.f),
			FMath::RoundToInt(Moisture * 100.f));
		if (SteamSeconds > 0.f)
		{
			Line += TEXT("  温室");
		}
	}
	Widget->SetStatusText(FText::FromString(Line));
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

	UGameInstance* GI = GetGameInstance();
	USlimeFarmSubsystem* Farm = GI ? GI->GetSubsystem<USlimeFarmSubsystem>() : nullptr;
	USlimeInventorySubsystem* Inv = GI ? GI->GetSubsystem<USlimeInventorySubsystem>() : nullptr;
	TArray<USlimeSeedDefinition*> Seeds;
	if (Farm)
	{
		Farm->GetSeedsInBag(Inv, Seeds);
	}
	if (Seeds.Num() == 0)
	{
		return FText::FromString(TEXT("没有种子"));
	}
	return FText::FromString(FString::Printf(TEXT("播种%s"), *Seeds[0]->DisplayName.ToString()));
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

	if (State == ESlimeFarmPlotState::Empty)
	{
		TArray<USlimeSeedDefinition*> Seeds;
		Farm->GetSeedsInBag(Inv, Seeds);
		if (Seeds.Num() == 0)
		{
			return false;
		}
		USlimeSeedDefinition* Seed = Seeds[0];
		USlimeCropDefinition* Next = Farm->FindCrop(Seed->CropId);
		if (!Next || !Inv->RemoveItem(Seed->ItemId, 1))
		{
			return false;
		}
		Crop = Next;
		State = ESlimeFarmPlotState::Growing;
		Growth01 = 0.04f;
		Quality = ESlimeCropQuality::Common;
		HueShift = 0.f;
		Moisture = FMath::Max(Moisture, 0.55f);
		Fertility = FMath::Max(Fertility, 0.3f);
		DrySeconds = 0.f;
		SaveRecord(true);
		RefreshVisual();
		RefreshStatus();
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
	if (!YieldId.IsNone())
	{
		Inv->AddItem(YieldId, Count);
	}
	if (USlimeSeedDefinition* Seed = Farm->FindSeedForCrop(Crop->CropId))
	{
		const float Chance = Crop->SeedReturnChance * (Quality == ESlimeCropQuality::Mutated ? 1.5f : 1.f);
		if (FMath::FRand() < Chance)
		{
			Inv->AddItem(Seed->ItemId, 1);
		}
	}

	USlimeFloatingTextWidget::Spawn(
		this,
		GetActorLocation() + FVector(0, 0, 100),
		FText::FromString(FString::Printf(TEXT("+%d %s"), Count, *Crop->DisplayName.ToString())),
		FLinearColor(0.95f, 0.85f, 0.35f),
		true);

	Crop = nullptr;
	State = ESlimeFarmPlotState::Empty;
	Growth01 = 0.f;
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
	(void)SourceActor;
	const float Amount = FMath::Max(Strength, 0.1f);
	const bool bHadWater = HadElementRecently(ESlimeElement::Water, 8.f);
	NoteElement(Element);

	if (Element == ESlimeElement::Water)
	{
		Moisture = FMath::Clamp(Moisture + 0.4f * Amount, 0.f, 1.f);
		DrySeconds = 0.f;
		if (HadElementRecently(ESlimeElement::Fire, 8.f) && State == ESlimeFarmPlotState::Growing)
		{
			SteamSeconds = FMath::Max(SteamSeconds, 30.f);
		}
	}
	else if (Element == ESlimeElement::Fire)
	{
		if (State == ESlimeFarmPlotState::Growing)
		{
			Growth01 = FMath::Clamp(Growth01 + 0.06f * Amount, 0.f, 1.f);
			FireBoostSeconds = FMath::Max(FireBoostSeconds, 8.f);
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
			if (UGameInstance* GI = GetGameInstance())
			{
				if (USlimeFarmSubsystem* Farm = GI->GetSubsystem<USlimeFarmSubsystem>())
				{
					if (USlimeInventorySubsystem* Inv = GI->GetSubsystem<USlimeInventorySubsystem>())
					{
						if (USlimeSeedDefinition* Seed = Farm->FindSeedForCrop(Crop->CropId))
						{
							if (FMath::FRand() < 0.45f)
							{
								Inv->AddItem(Seed->ItemId, 1);
							}
						}
					}
				}
			}
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
	}

	SaveRecord(false);
	RefreshVisual();
	RefreshStatus();
}
