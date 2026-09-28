// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hub/HomeBuild/SlimeBuildModeComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Hub/HomeBuild/SlimeHomeBuildCatalog.h"
#include "Hub/HomeBuild/SlimeHomeBuildManager.h"
#include "Hub/HomeBuild/SlimeHomeBuildSubsystem.h"
#include "Inventory/SlimeInventorySubsystem.h"
#include "Inventory/SlimeItemDefinition.h"
#include "Inventory/SlimePlacePreview.h"
#include "Inventory/SlimePlacedActor.h"
#include "Settings/SlimeInputSettings.h"
#include "SlimeFable.h"
#include "SlimeFablePlayerController.h"
#include "UI/SlimeHomeBuildWidget.h"

namespace
{
	bool TraceBuildAim(UWorld* World, const FVector& Start, const FVector& End, const FCollisionQueryParams& Params, FHitResult& Out)
	{
		FHitResult Vis;
		FHitResult Fluid;
		const bool bVis = World->LineTraceSingleByChannel(Vis, Start, End, ECC_Visibility, Params);
		const bool bFluid = World->LineTraceSingleByChannel(Fluid, Start, End, ECC_GameTraceChannel2, Params);
		if (bFluid && (!bVis || Fluid.Distance <= Vis.Distance + 8.f))
		{
			Out = Fluid;
			return true;
		}
		if (bVis)
		{
			Out = Vis;
			return true;
		}
		return false;
	}

	bool BottomTransform(UStaticMesh* Mesh, const FVector& Scale, const FRotator& Rotation, const FVector& CenterXY, float BaseZ, FTransform& Out)
	{
		if (!Mesh)
		{
			return false;
		}
		const FBox Box = Mesh->GetBoundingBox();
		const FTransform Local(Rotation, FVector::ZeroVector, Scale);
		FVector Min(FLT_MAX);
		FVector Max(-FLT_MAX);
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector Point = Local.TransformPosition(FVector(
				(Corner & 1) ? Box.Max.X : Box.Min.X,
				(Corner & 2) ? Box.Max.Y : Box.Min.Y,
				(Corner & 4) ? Box.Max.Z : Box.Min.Z));
			Min = Min.ComponentMin(Point);
			Max = Max.ComponentMax(Point);
		}
		const FVector BoundsCenter = (Min + Max) * 0.5f;
		const FVector Origin(CenterXY.X - BoundsCenter.X, CenterXY.Y - BoundsCenter.Y, BaseZ - Min.Z);
		Out = FTransform(Rotation, Origin, Scale);
		return true;
	}
}

USlimeBuildModeComponent::USlimeBuildModeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USlimeBuildModeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!Pawn || !PC || !Pawn->IsLocallyControlled())
	{
		return;
	}

	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const USlimeInputSettings* Input = GI ? GI->GetSubsystem<USlimeInputSettings>() : nullptr;
	auto Pressed = [PC, Input](ESlimeInputAction Action, const FKey& Fallback)
	{
		if (Input)
		{
			return Input->WasKeyPressed(PC, Action);
		}
		return PC->WasInputKeyJustPressed(Fallback);
	};

	if (Pressed(ESlimeInputAction::BuildCatalog, EKeys::F1))
	{
		ToggleCatalog();
		return;
	}
	if (!bPlacing || bCatalogOpen)
	{
		return;
	}

	if (ModeToastLeft > 0.f)
	{
		ModeToastLeft -= DeltaTime;
		if (ModeToastLeft <= 0.f && CatalogWidget)
		{
			CatalogWidget->HideModeToast();
		}
	}

	if (Pressed(ESlimeInputAction::BuildClearMode, EKeys::X))
	{
		if (!ActiveId.IsNone())
		{
			bClearMode = !bClearMode;
			RefreshStatus();
		}
	}
	else if (!bClearMode && PC->WasInputKeyJustPressed(EKeys::R))
	{
		YawSteps = (YawSteps + 1) % 4;
	}

	const FKey Digits[9] = {
		EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
		EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine
	};
	for (int32 Key = 0; Key < 9; ++Key)
	{
		if (PC->WasInputKeyJustPressed(Digits[Key]) && BarIds.IsValidIndex(Key))
		{
			SelectBarIndex(Key);
		}
	}
	if (PC->WasInputKeyJustPressed(EKeys::MiddleMouseButton))
	{
		CycleAdjustMode();
	}

	const bool bCoarse = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);
	const float HeightStep = bCoarse ? 50.f : 10.f;
	int32 Scroll = 0;
	if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp))
	{
		Scroll = 1;
	}
	else if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown))
	{
		Scroll = -1;
	}
	if (Scroll != 0)
	{
		switch (AdjustMode)
		{
		case EPlaceAdjust::Yaw:
			YawSteps = (YawSteps + (Scroll > 0 ? 1 : 3)) % 4;
			break;
		case EPlaceAdjust::Pitch:
			PitchDegrees = FMath::Clamp(PitchDegrees + Scroll * 15.f, -90.f, 90.f);
			break;
		case EPlaceAdjust::Scale:
			if (!IsPlacingFluid())
			{
				UserScale = FMath::Clamp(UserScale + Scroll * 0.1f, 0.25f, 4.f);
			}
			break;
		case EPlaceAdjust::Height:
		default:
			HeightOffset += Scroll * HeightStep;
			break;
		}
		RefreshStatus();
	}

	UpdateAim();
	if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		if (bClearMode)
		{
			ClearAimed();
		}
		else
		{
			Confirm();
		}
	}
}

bool USlimeBuildModeComponent::HandleEscape()
{
	if (bCatalogOpen || bPlacing)
	{
		ExitPlacement(TEXT("escape"));
		return true;
	}
	return false;
}

bool USlimeBuildModeComponent::BeginBagPlacement(USlimePlaceableDefinition* Definition)
{
	if (!Definition)
	{
		return false;
	}
	NotifyCatalogChosen(Definition->ItemId, true);
	return bPlacing;
}

void USlimeBuildModeComponent::BeginClearMode()
{
	USlimeHomeBuildSubsystem* Home = GetHome();
	if (!Home || !Home->IsMuseum())
	{
		Screen(TEXT("回到时光博物馆再建造"));
		return;
	}
	bPlacing = true;
	bClearMode = true;
	ActiveId = NAME_None;
	APlayerController* PC = Cast<APlayerController>(Cast<APawn>(GetOwner())->GetController());
	if (!PC)
	{
		return;
	}
	DiscardCatalogWidget();
	USlimeHomeBuildWidget::SetCreateMode(true);
	CatalogWidget = CreateWidget<USlimeHomeBuildWidget>(PC, USlimeHomeBuildWidget::StaticClass());
	if (CatalogWidget)
	{
		CatalogWidget->OpenHotbar(this, BarIds, BarFromBag, BarIndex, true);
		CatalogWidget->AddToViewport(8);
		RefreshStatus();
	}
}

void USlimeBuildModeComponent::NotifyCatalogChosen(FName EntryId, bool bFromBag)
{
	ActiveId = EntryId;
	bActiveFromBag = bFromBag;
	bPlacing = true;
	bClearMode = false;
	YawSteps = 0;
	HeightOffset = 0.f;
	PitchDegrees = 0.f;
	UserScale = 1.f;
	if (IsPlacingFluid() && AdjustMode == EPlaceAdjust::Scale)
	{
		AdjustMode = EPlaceAdjust::Height;
	}

	int32 Existing = INDEX_NONE;
	for (int32 Index = 0; Index < BarIds.Num(); ++Index)
	{
		if (BarIds[Index] == EntryId && BarFromBag.IsValidIndex(Index) && BarFromBag[Index] == bFromBag)
		{
			Existing = Index;
			break;
		}
	}
	if (Existing == INDEX_NONE)
	{
		BarIds.Insert(EntryId, 0);
		BarFromBag.Insert(bFromBag, 0);
		if (BarIds.Num() > 9)
		{
			BarIds.SetNum(9);
			BarFromBag.SetNum(9);
		}
		BarIndex = 0;
	}
	else
	{
		BarIndex = Existing;
	}
	CloseCatalog();

	if (!PreviewActor && GetWorld())
	{
		FActorSpawnParameters Params;
		Params.Owner = GetOwner();
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PreviewActor = GetWorld()->SpawnActor<ASlimePlacePreview>(ASlimePlacePreview::StaticClass(), GetOwner()->GetActorLocation(), FRotator::ZeroRotator, Params);
	}
	UStaticMesh* Mesh = nullptr;
	FVector Scale = FVector::OneVector;
	if (bFromBag)
	{
		if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (USlimeInventorySubsystem* Inv = GI->GetSubsystem<USlimeInventorySubsystem>())
			{
				if (const USlimePlaceableDefinition* Def = Cast<USlimePlaceableDefinition>(Inv->FindDefinition(EntryId)))
				{
					Mesh = Def->PreviewMesh.LoadSynchronous();
					Scale = Def->PlacedMeshScale;
				}
			}
		}
	}
	else if (USlimeHomeBuildSubsystem* Home = GetHome())
	{
		if (const USlimeHomeBuildCatalog* Catalog = Home->GetCatalog())
		{
			if (const FSlimeHomeBuildEntry* Entry = Catalog->FindEntry(EntryId))
			{
				Mesh = Entry->Mesh.LoadSynchronous();
				Scale = FVector(Entry->UniformScale);
			}
		}
	}
	if (!Mesh)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("[HomeBuild] entry %s has no mesh"), *EntryId.ToString());
	}
	if (PreviewActor)
	{
		PreviewActor->SetPreviewMesh(Mesh);
		if (PreviewActor->Mesh)
		{
			const bool bFluid = IsPlacingFluid();
			PreviewActor->Mesh->SetRelativeScale3D(bFluid ? FVector::OneVector : Scale);
			PreviewActor->Mesh->SetVisibility(!bFluid, false);
		}
	}
	if (APlayerController* PC = Cast<APlayerController>(Cast<APawn>(GetOwner())->GetController()))
	{
		DiscardCatalogWidget();
		USlimeHomeBuildWidget::SetCreateMode(true);
		CatalogWidget = CreateWidget<USlimeHomeBuildWidget>(PC, USlimeHomeBuildWidget::StaticClass());
		if (CatalogWidget)
		{
			CatalogWidget->OpenHotbar(this, BarIds, BarFromBag, BarIndex, false);
			CatalogWidget->AddToViewport(8);
			RefreshStatus();
		}
	}
}

void USlimeBuildModeComponent::NotifyCatalogClear()
{
	CloseCatalog();
	BeginClearMode();
}

void USlimeBuildModeComponent::NotifyCatalogClosed()
{
	ExitPlacement(TEXT("catalog-closed"));
}

void USlimeBuildModeComponent::RequestExit(const TCHAR* Reason)
{
	ExitPlacement(Reason ? Reason : TEXT("request"));
}

void USlimeBuildModeComponent::SetPreviewMesh(UStaticMesh* Mesh)
{
	if (PreviewActor)
	{
		PreviewActor->SetPreviewMesh(Mesh);
	}
}

void USlimeBuildModeComponent::ToggleCatalog()
{
	if (bCatalogOpen || bPlacing)
	{
		ExitPlacement(TEXT("f1"));
		return;
	}
	USlimeHomeBuildSubsystem* Home = GetHome();
	if (!Home || !Home->IsMuseum())
	{
		Screen(TEXT("回到时光博物馆再建造"));
		return;
	}
	OpenCatalog();
}

void USlimeBuildModeComponent::OpenCatalog()
{
	APlayerController* PC = Cast<APlayerController>(Cast<APawn>(GetOwner())->GetController());
	if (!PC)
	{
		return;
	}
	DiscardCatalogWidget();
	USlimeHomeBuildWidget::SetCreateMode(false);
	CatalogWidget = CreateWidget<USlimeHomeBuildWidget>(PC, USlimeHomeBuildWidget::StaticClass());
	if (!CatalogWidget)
	{
		return;
	}
	CatalogWidget->OpenCatalog(this);
	CatalogWidget->AddToViewport(20);
	bCatalogOpen = true;
	if (ASlimeFablePlayerController* SlimePC = Cast<ASlimeFablePlayerController>(PC))
	{
		SlimePC->PushUIInput(ESlimeUIInputReason::HomeBuild, CatalogWidget);
	}
}

void USlimeBuildModeComponent::DiscardCatalogWidget()
{
	if (!CatalogWidget)
	{
		return;
	}
	if (CatalogWidget->IsInViewport() || CatalogWidget->GetParent())
	{
		CatalogWidget->RemoveFromParent();
	}
	CatalogWidget = nullptr;
}

void USlimeBuildModeComponent::CloseCatalog()
{
	bCatalogOpen = false;
	DiscardCatalogWidget();
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (ASlimeFablePlayerController* PC = Cast<ASlimeFablePlayerController>(Pawn->GetController()))
		{
			PC->PopUIInput(ESlimeUIInputReason::HomeBuild);
		}
	}
}

void USlimeBuildModeComponent::ExitPlacement(const TCHAR* Reason)
{
	UE_LOG(LogSlimeFable, Log, TEXT("[HomeBuild] exit placement (%s)"), Reason ? Reason : TEXT("unknown"));
	bPlacing = false;
	bClearMode = false;
	ActiveId = NAME_None;
	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}
	if (USlimeHomeBuildSubsystem* Home = GetHome())
	{
		if (ASlimeHomeBuildManager* Manager = Home->GetManager())
		{
			Manager->SetHighlight(INDEX_NONE);
		}
	}
	CloseCatalog();
}

void USlimeBuildModeComponent::UpdateAim()
{
	USlimeHomeBuildSubsystem* Home = GetHome();
	ASlimeHomeBuildManager* Manager = Home ? Home->GetManager() : nullptr;
	const USlimeHomeBuildCatalog* Catalog = Home ? Home->GetCatalog() : nullptr;
	const float Cell = Catalog ? Catalog->CellSize : 50.f;

	int32 FootX = 1;
	int32 FootY = 1;
	float Height = Cell;
	FVector Scale = FVector::OneVector;
	UStaticMesh* Mesh = nullptr;
	float MaxSlope = 12.f;
	bool bFluidPad = false;
	float Uniform = 1.f;
	if (!bActiveFromBag && Catalog)
	{
		if (const FSlimeHomeBuildEntry* Entry = Catalog->FindEntry(ActiveId))
		{
			FootX = FMath::Max(Entry->FootprintX, 1);
			FootY = FMath::Max(Entry->FootprintY, 1);
			Height = FMath::Max(Entry->HeightCm, 1.f);
			Uniform = Entry->UniformScale;
			Scale = FVector(Uniform);
			bFluidPad = Entry->bFluidPad;
			Mesh = Entry->Mesh.Get();
			if (!Mesh)
			{
				Mesh = Entry->Mesh.LoadSynchronous();
			}
		}
	}
	else if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (USlimeInventorySubsystem* Inv = GI->GetSubsystem<USlimeInventorySubsystem>())
		{
			if (const USlimePlaceableDefinition* Def = Cast<USlimePlaceableDefinition>(Inv->FindDefinition(ActiveId)))
			{
				MaxSlope = Def->MaxSlopeDegrees;
				Scale = Def->PlacedMeshScale;
				Mesh = Def->PreviewMesh.LoadSynchronous();
				if (Mesh)
				{
					const FBox Box = Mesh->GetBoundingBox();
					const FVector Size = (Box.Max - Box.Min) * Scale;
					Height = FMath::Max(Size.Z, 1.f);
					FootX = FMath::Max(FMath::RoundToInt(Size.X / Cell), 1);
					FootY = FMath::Max(FMath::RoundToInt(Size.Y / Cell), 1);
				}
			}
		}
	}
	if ((YawSteps & 1) != 0)
	{
		Swap(FootX, FootY);
	}
	if (bFluidPad)
	{
		Scale = FVector(Uniform * FootX, Uniform * FootY, 0.02f);
	}
	else
	{
		Scale *= FMath::Clamp(UserScale, 0.25f, 4.f);
	}

	int32 AnchorX = 0;
	int32 AnchorY = 0;
	float BaseZ = 0.f;
	bool bOnBuild = false;
	FVector Impact = FVector::ZeroVector;
	const bool bHit = ComputeAim(AnchorX, AnchorY, BaseZ, bOnBuild, Height, Impact);
	if (bHit)
	{
		BaseZ += HeightOffset;
	}
	AimAnchorX = AnchorX;
	AimAnchorY = AnchorY;
	AimBaseZ = BaseZ;
	bAimOnBuild = bOnBuild;
	AimedRecord = INDEX_NONE;

	EnsurePreview();
	if (bClearMode)
	{
		bAimValid = false;
		FVector HitPoint = Impact;
		bool bTraced = bHit;
		if (PreviewActor)
		{
			PreviewActor->SetActorHiddenInGame(false);
			PreviewActor->SetFootprintHighlight(false, 0.f, 0.f, false);
			if (PreviewActor->Mesh)
			{
				PreviewActor->Mesh->SetVisibility(false, false);
			}
		}
		if (Manager && GetWorld())
		{
			APawn* Pawn = Cast<APawn>(GetOwner());
			APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
			FVector CamLoc;
			FRotator CamRot;
			if (PC)
			{
				PC->GetPlayerViewPoint(CamLoc, CamRot);
				FHitResult Hit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeBuildClear), false, GetOwner());
				if (PreviewActor)
				{
					Params.AddIgnoredActor(PreviewActor);
				}
				if (TraceBuildAim(GetWorld(), CamLoc, CamLoc + CamRot.Vector() * TraceDistance, Params, Hit))
				{
					AimedRecord = Manager->FindRecordAtHit(Hit);
					HitPoint = Hit.ImpactPoint;
					bTraced = true;
				}
			}
			Manager->SetHighlight(AimedRecord);
		}
		if (PreviewActor)
		{
			const FLinearColor Color = AimedRecord != INDEX_NONE
				? FLinearColor(0.95f, 0.16f, 0.12f)
				: FLinearColor(0.72f, 0.72f, 0.72f);
			PreviewActor->SetAimVisual(GetOwner()->GetActorLocation(), HitPoint, Color, bTraced);
		}
		return;
	}

	bool bSlopeOk = bOnBuild;
	if (!bOnBuild && GetWorld())
	{
		APawn* Pawn = Cast<APawn>(GetOwner());
		APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		FVector CamLoc;
		FRotator CamRot;
		FHitResult Hit;
		if (PC)
		{
			PC->GetPlayerViewPoint(CamLoc, CamRot);
			FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeBuildSlope), false, GetOwner());
			if (PreviewActor)
			{
				Params.AddIgnoredActor(PreviewActor);
			}
			if (TraceBuildAim(GetWorld(), CamLoc, CamLoc + CamRot.Vector() * TraceDistance, Params, Hit))
			{
				const float Cos = FMath::Cos(FMath::DegreesToRadians(MaxSlope));
				bSlopeOk = FVector::DotProduct(Hit.ImpactNormal.GetSafeNormal(), FVector::UpVector) >= Cos;
			}
		}
	}

	const bool bOccupied = Manager && Manager->IsBlocked(AnchorX, AnchorY, FootX, FootY, BaseZ, Height, INDEX_NONE, bFluidPad);
	bAimValid = bHit && bSlopeOk && !bOccupied && !ActiveId.IsNone();

	const FVector Center((AnchorX + FootX * 0.5f) * Cell, (AnchorY + FootY * 0.5f) * Cell, BaseZ);
	if (bFluidPad && PreviewActor)
	{
		PreviewActor->SetActorHiddenInGame(false);
		PreviewActor->SetActorLocation(FVector(Center.X, Center.Y, BaseZ));
		PreviewActor->SetActorRotation(FRotator::ZeroRotator);
		if (PreviewActor->Mesh)
		{
			PreviewActor->Mesh->SetVisibility(false, false);
			PreviewActor->Mesh->SetRelativeScale3D(FVector::OneVector);
		}
		const FLinearColor Color = bAimValid
			? FLinearColor(0.15f, 0.95f, 0.35f)
			: FLinearColor(0.95f, 0.16f, 0.12f);
		PreviewActor->SetAimVisual(GetOwner()->GetActorLocation(), bHit ? Impact : FVector(Center.X, Center.Y, BaseZ), Color, bHit);
		PreviewActor->SetFootprintHighlight(bHit, FootX * Cell, FootY * Cell, bAimValid);
		return;
	}
	if (PreviewActor)
	{
		PreviewActor->SetFootprintHighlight(false, 0.f, 0.f, true);
	}
	FTransform Xform;
	const FRotator Rotation(PitchDegrees, YawSteps * 90.f, 0.f);
	if (PreviewActor && BottomTransform(Mesh, Scale, Rotation, FVector(Center.X, Center.Y, 0.f), BaseZ, Xform))
	{
		PreviewActor->SetActorHiddenInGame(false);
		PreviewActor->SetActorLocation(Xform.GetLocation());
		PreviewActor->SetActorRotation(Xform.Rotator());
		if (PreviewActor->Mesh)
		{
			PreviewActor->Mesh->SetVisibility(bHit, false);
			PreviewActor->Mesh->SetRelativeScale3D(Scale);
		}
		PreviewActor->SetValidPlacement(bAimValid);
		const FLinearColor Color = bAimValid
			? FLinearColor(0.82f, 0.86f, 0.78f)
			: FLinearColor(0.95f, 0.16f, 0.12f);
		PreviewActor->SetAimVisual(GetOwner()->GetActorLocation(), bHit ? Impact : Xform.GetLocation(), Color, bHit);
	}
}

void USlimeBuildModeComponent::Confirm()
{
	if (!bAimValid || ActiveId.IsNone())
	{
		return;
	}
	USlimeHomeBuildSubsystem* Home = GetHome();
	const bool bMuseum = Home && Home->IsMuseum();
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	USlimeInventorySubsystem* Inv = GI ? GI->GetSubsystem<USlimeInventorySubsystem>() : nullptr;

	if (bActiveFromBag)
	{
		if (!Inv || !Inv->RemoveItem(ActiveId, 1))
		{
			Screen(TEXT("背包里没有这个物品"));
			return;
		}
	}

	if (bMuseum && Home)
	{
		FSlimeHomeBuildRecord Record;
		Record.EntryId = ActiveId;
		Record.bFromBag = bActiveFromBag;
		Record.AnchorX = AimAnchorX;
		Record.AnchorY = AimAnchorY;
		Record.BaseZ = AimBaseZ;
		Record.YawSteps = YawSteps;
		Record.PitchDegrees = PitchDegrees;
		Record.UserScale = 1.f;
		if (!bActiveFromBag)
		{
			if (const USlimeHomeBuildCatalog* Catalog = Home->GetCatalog())
			{
				if (const FSlimeHomeBuildEntry* Entry = Catalog->FindEntry(ActiveId))
				{
					Record.bFluid = Entry->bFluidPad;
					if (!Entry->bFluidPad)
					{
						Record.UserScale = UserScale;
					}
				}
			}
		}
		else
		{
			Record.UserScale = UserScale;
		}
		if (Home->CommitRecord(Record) == INDEX_NONE && bActiveFromBag && Inv)
		{
			Inv->AddItem(ActiveId, 1);
		}
		return;
	}

	if (!bActiveFromBag)
	{
		return;
	}
	USlimePlaceableDefinition* Def = Inv ? Cast<USlimePlaceableDefinition>(Inv->FindDefinition(ActiveId)) : nullptr;
	if (!Def || !GetWorld())
	{
		return;
	}
	FTransform Xform = FTransform::Identity;
	if (PreviewActor)
	{
		Xform = PreviewActor->GetActorTransform();
		Xform.SetScale3D(FVector::OneVector);
	}
	UClass* SpawnClass = Def->PlacedActorClass.LoadSynchronous();
	if (!SpawnClass)
	{
		SpawnClass = ASlimePlacedActor::StaticClass();
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ASlimePlacedActor* Placed = Cast<ASlimePlacedActor>(GetWorld()->SpawnActor<AActor>(SpawnClass, Xform, Params)))
	{
		Placed->ConfigureFromItem(ActiveId, Def);
		if (Placed->Mesh)
		{
			Placed->Mesh->SetRelativeScale3D(Placed->Mesh->GetRelativeScale3D() * FMath::Clamp(UserScale, 0.25f, 4.f));
		}
	}
}

void USlimeBuildModeComponent::ClearAimed()
{
	if (AimedRecord == INDEX_NONE)
	{
		return;
	}
	if (USlimeHomeBuildSubsystem* Home = GetHome())
	{
		Home->RemoveRecord(AimedRecord, true);
	}
}

bool USlimeBuildModeComponent::ComputeAim(int32& OutAnchorX, int32& OutAnchorY, float& OutBaseZ, bool& bOutOnBuild, float IncomingHeight, FVector& OutImpact) const
{
	bOutOnBuild = false;
	OutImpact = FVector::ZeroVector;
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* World = GetWorld();
	if (!Pawn || !PC || !World)
	{
		return false;
	}
	USlimeHomeBuildSubsystem* Home = GetHome();
	const USlimeHomeBuildCatalog* Catalog = Home ? Home->GetCatalog() : nullptr;
	const float Cell = Catalog ? Catalog->CellSize : 50.f;

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeBuildAim), false, GetOwner());
	if (PreviewActor)
	{
		Params.AddIgnoredActor(PreviewActor);
	}
	if (!TraceBuildAim(World, CamLoc, CamLoc + CamRot.Vector() * TraceDistance, Params, Hit))
	{
		return false;
	}
	OutImpact = Hit.ImpactPoint;

	ASlimeHomeBuildManager* Manager = Home ? Home->GetManager() : nullptr;
	const int32 HitId = Manager ? Manager->FindRecordAtHit(Hit) : INDEX_NONE;
	bOutOnBuild = HitId != INDEX_NONE;
	if (!bOutOnBuild)
	{
		OutAnchorX = FMath::FloorToInt(Hit.ImpactPoint.X / Cell);
		OutAnchorY = FMath::FloorToInt(Hit.ImpactPoint.Y / Cell);
		OutBaseZ = Hit.ImpactPoint.Z;
		return true;
	}

	float HitBaseZ = 0.f;
	float HitHeight = Cell;
	if (!Manager->GetRecordStack(HitId, HitBaseZ, HitHeight))
	{
		OutAnchorX = FMath::FloorToInt(Hit.ImpactPoint.X / Cell);
		OutAnchorY = FMath::FloorToInt(Hit.ImpactPoint.Y / Cell);
		OutBaseZ = Hit.ImpactPoint.Z;
		return true;
	}

	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	if (Normal.Z > 0.55f)
	{
		OutBaseZ = HitBaseZ + HitHeight;
		OutAnchorX = FMath::FloorToInt(Hit.ImpactPoint.X / Cell);
		OutAnchorY = FMath::FloorToInt(Hit.ImpactPoint.Y / Cell);
	}
	else if (Normal.Z < -0.55f)
	{
		OutBaseZ = HitBaseZ - FMath::Max(IncomingHeight, 1.f);
		OutAnchorX = FMath::FloorToInt(Hit.ImpactPoint.X / Cell);
		OutAnchorY = FMath::FloorToInt(Hit.ImpactPoint.Y / Cell);
	}
	else
	{
		OutBaseZ = HitBaseZ;
		const FVector Point = Hit.ImpactPoint + Normal * (Cell * 0.5f);
		OutAnchorX = FMath::FloorToInt(Point.X / Cell);
		OutAnchorY = FMath::FloorToInt(Point.Y / Cell);
	}
	return true;
}

void USlimeBuildModeComponent::EnsurePreview()
{
	if (PreviewActor || !GetWorld())
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PreviewActor = GetWorld()->SpawnActor<ASlimePlacePreview>(
		ASlimePlacePreview::StaticClass(), GetOwner()->GetActorLocation(), FRotator::ZeroRotator, Params);
}

void USlimeBuildModeComponent::RefreshStatus()
{
	if (!CatalogWidget || bCatalogOpen)
	{
		return;
	}
	const TCHAR* Adjust = AdjustModeLabel();
	const TCHAR* Prefix = bClearMode ? TEXT("清除模式") : TEXT("建造模式");
	CatalogWidget->SetModeHint(FText::FromString(FString::Printf(TEXT("%s　%s　中键切换"), Prefix, Adjust)));
	FString Text = bClearMode
		? FString::Printf(TEXT("清除模式：左键拆掉   滚轮调%s   中键切换   F1 退出"), Adjust)
		: FString::Printf(TEXT("左键放置   滚轮调%s   中键切换   X 清除   1-9 切换   F1 退出"), Adjust);
	if (AdjustMode == EPlaceAdjust::Height && FMath::Abs(HeightOffset) > 0.1f)
	{
		Text += FString::Printf(TEXT("   高度 %+.0fcm"), HeightOffset);
	}
	else if (AdjustMode == EPlaceAdjust::Pitch && FMath::Abs(PitchDegrees) > 0.1f)
	{
		Text += FString::Printf(TEXT("   俯仰 %+.0f°"), PitchDegrees);
	}
	else if (AdjustMode == EPlaceAdjust::Scale)
	{
		Text += FString::Printf(TEXT("   缩放 %.1f"), UserScale);
	}
	CatalogWidget->SetStatusLine(FText::FromString(Text));
}

void USlimeBuildModeComponent::CycleAdjustMode()
{
	switch (AdjustMode)
	{
	case EPlaceAdjust::Height:
		AdjustMode = EPlaceAdjust::Yaw;
		break;
	case EPlaceAdjust::Yaw:
		AdjustMode = EPlaceAdjust::Pitch;
		break;
	case EPlaceAdjust::Pitch:
		AdjustMode = IsPlacingFluid() ? EPlaceAdjust::Height : EPlaceAdjust::Scale;
		break;
	case EPlaceAdjust::Scale:
	default:
		AdjustMode = EPlaceAdjust::Height;
		break;
	}
	ModeToastLeft = 2.f;
	if (CatalogWidget)
	{
		const TCHAR* Toast = TEXT("当前为高度调整模式");
		switch (AdjustMode)
		{
		case EPlaceAdjust::Yaw:
			Toast = TEXT("已切换左右旋转模式");
			break;
		case EPlaceAdjust::Pitch:
			Toast = TEXT("已切换上下旋转模式");
			break;
		case EPlaceAdjust::Scale:
			Toast = TEXT("已切换缩放模式");
			break;
		default:
			break;
		}
		CatalogWidget->ShowModeToast(FText::FromString(Toast));
	}
	RefreshStatus();
}

bool USlimeBuildModeComponent::IsPlacingFluid() const
{
	if (bActiveFromBag || ActiveId.IsNone())
	{
		return false;
	}
	const USlimeHomeBuildSubsystem* Home = GetHome();
	const USlimeHomeBuildCatalog* Catalog = Home ? Home->GetCatalog() : nullptr;
	const FSlimeHomeBuildEntry* Entry = Catalog ? Catalog->FindEntry(ActiveId) : nullptr;
	return Entry && Entry->bFluidPad;
}

const TCHAR* USlimeBuildModeComponent::AdjustModeLabel() const
{
	switch (AdjustMode)
	{
	case EPlaceAdjust::Yaw:
		return TEXT("左右旋转");
	case EPlaceAdjust::Pitch:
		return TEXT("上下旋转");
	case EPlaceAdjust::Scale:
		return TEXT("缩放调整");
	case EPlaceAdjust::Height:
	default:
		return TEXT("高度调整");
	}
}

void USlimeBuildModeComponent::SelectBarIndex(int32 Index)
{
	if (!BarIds.IsValidIndex(Index))
	{
		return;
	}
	BarIndex = Index;
	NotifyCatalogChosen(BarIds[Index], BarFromBag.IsValidIndex(Index) && BarFromBag[Index]);
}

USlimeHomeBuildSubsystem* USlimeBuildModeComponent::GetHome() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USlimeHomeBuildSubsystem>() : nullptr;
}

void USlimeBuildModeComponent::Screen(const FString& Text) const
{
	if (USlimeHomeBuildSubsystem* Home = GetHome())
	{
		Home->ScreenMessage(Text);
		return;
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2.5f, FColor::White, Text);
	}
}
