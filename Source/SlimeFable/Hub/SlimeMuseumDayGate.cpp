// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hub/SlimeMuseumDayGate.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "SlimeFablePlayerController.h"
#include "UI/LevelSelectWidget.h"

TWeakObjectPtr<ASlimeMuseumDayGate> ASlimeMuseumDayGate::OpenGate;

ASlimeMuseumDayGate::ASlimeMuseumDayGate()
{
	Frame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Frame"));
	SetRootComponent(Frame);
	Frame->SetMobility(EComponentMobility::Movable);
	Frame->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Frame->SetRelativeScale3D(FVector(0.35f, 1.4f, 2.6f));
	Frame->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Frame->SetCollisionObjectType(ECC_WorldDynamic);
	Frame->SetCollisionResponseToAllChannels(ECR_Block);
	Frame->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Frame->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		Frame->SetMaterial(0, Base);
	}
}

void ASlimeMuseumDayGate::BeginPlay()
{
	Super::BeginPlay();
	if (Frame)
	{
		if (UMaterialInstanceDynamic* Mid = Frame->CreateDynamicMaterialInstance(0))
		{
			Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.45f, 0.32f, 0.16f));
		}
	}
}

bool ASlimeMuseumDayGate::TryInteract(APawn* Interactor)
{
	if (!Interactor)
	{
		return false;
	}
	APlayerController* PC = Cast<APlayerController>(Interactor->GetController());
	if (!PC || !PC->IsLocalController())
	{
		return false;
	}
	if (Calendar && Calendar->IsInViewport() && Calendar->GetVisibility() != ESlateVisibility::Collapsed)
	{
		return true;
	}

	Calendar = CreateWidget<ULevelSelectWidget>(PC, ULevelSelectWidget::StaticClass());
	if (!Calendar)
	{
		return false;
	}
	Calendar->OnClosed.AddDynamic(this, &ASlimeMuseumDayGate::HandleCalendarClosed);
	Calendar->AddToViewport(30);
	Calendar->JumpToTodayMonth();
	Calendar->RefreshForCurrentMonth();
	OpenGate = this;

	if (ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(PC))
	{
		SlimePC->PushUIInput(ESlimeUIInputReason::MuseumCalendar, Calendar);
	}
	return true;
}

FText ASlimeMuseumDayGate::GetInteractPromptVerb() const
{
	return FText::FromString(TEXT("进入日历"));
}

FVector ASlimeMuseumDayGate::GetPromptWorldLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, 180.f);
}

void ASlimeMuseumDayGate::HandleCalendarClosed()
{
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(PC))
		{
			SlimePC->PopUIInput(ESlimeUIInputReason::MuseumCalendar);
		}
	}
	if (Calendar)
	{
		Calendar->RemoveFromParent();
		Calendar = nullptr;
	}
	if (OpenGate.Get() == this)
	{
		OpenGate.Reset();
	}
}

bool ASlimeMuseumDayGate::CloseOpenCalendar()
{
	ASlimeMuseumDayGate* Gate = OpenGate.Get();
	if (!Gate || !Gate->Calendar)
	{
		return false;
	}
	Gate->Calendar->SetVisibility(ESlateVisibility::Collapsed);
	Gate->HandleCalendarClosed();
	return true;
}
