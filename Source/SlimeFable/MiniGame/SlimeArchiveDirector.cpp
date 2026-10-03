#include "MiniGame/SlimeArchiveDirector.h"
#include "MiniGame/SlimeArchiveHUDWidget.h"
#include "Combat/SlimeDevourComponent.h"
#include "Quest/QuestInteractActor.h"
#include "Quest/QuestObjectiveComponent.h"
#include "Quest/QuestSubsystem.h"
#include "SlimeFable.h"
#include "Slime/SlimeAbilityComponent.h"
#include "Settings/SlimeCheatComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

namespace
{
	const TCHAR* Names[] = {TEXT("兰利"), TEXT("艾姆斯"), TEXT("刘易斯"), TEXT("新 NASA")};
	const TCHAR* Codes[] = {TEXT("1 LANGLEY"), TEXT("2 AMES"), TEXT("3 LEWIS"), TEXT("4 NASA")};
}

ASlimeArchiveDirector::ASlimeArchiveDirector()
{
	DisabledComponentClasses.Remove(TEXT("SlimeAbilityComponent"));
	DisabledComponentClasses.AddUnique(TEXT("SlimeCheatComponent"));
	ChapterId = TEXT("1958"); QuestId = TEXT("Archive"); FinishBranchId = TEXT("Shift");
	DisabledComponentClasses.Append({TEXT("SlimeMorphComponent"), TEXT("SlimeVehicleComponent"), TEXT("SlimeCombatComponent")});
}

void ASlimeArchiveDirector::EnterMode(ACharacter* Character, APlayerController* PC)
{
	// Saved maps may retain the previous component suppression list.
	if (auto* Ability = Character->FindComponentByClass<USlimeAbilityComponent>()) Ability->SetComponentTickEnabled(true);
	Devour = Character->FindComponentByClass<USlimeDevourComponent>();
	CargoBadge = NewObject<UTextRenderComponent>(Character);
	Character->AddInstanceComponent(CargoBadge);
	CargoBadge->SetupAttachment(Character->GetRootComponent());
	CargoBadge->SetRelativeLocation(FVector(0.f, 0.f, 85.f));
	CargoBadge->SetHorizontalAlignment(EHTA_Center);
	CargoBadge->SetWorldSize(30.f);
	CargoBadge->SetTextRenderColor(FColor(235, 190, 90));
	CargoBadge->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CargoBadge->RegisterComponent();
	HUD = CreateWidget<USlimeArchiveHUDWidget>(PC);
	if (HUD) HUD->AddToViewport(6);
	if (!Devour || !BeltStart || !BeltEnd || CabinetTargets.Num() != 4
		|| CabinetTargets.Contains(nullptr) || !FolderMesh || !EquipmentMesh || CategoryMaterials.Num() != 4)
	{
		Shift = EShift::Invalid;
		SetMessage(TEXT("档案室配置缺失，请检查传送带、四个柜子和模型。"));
		RefreshHUD();
		UE_LOG(LogSlimeFable, Error, TEXT("Archive: missing required room references"));
		return;
	}
	PlayerStart = Character->GetActorTransform();
	ResetShift();
}

void ASlimeArchiveDirector::ExitMode(ACharacter* Character, APlayerController* PC)
{
	ClearParcels();
	if (CargoBadge) CargoBadge->DestroyComponent();
	if (HUD) HUD->RemoveFromParent();
}

void ASlimeArchiveDirector::ClearParcels()
{
	if (Devour) Devour->ReleaseStoredProp(GetActorLocation());
	for (auto& Parcel : Parcels) if (IsValid(Parcel.Actor)) Parcel.Actor->Destroy();
	Parcels.Reset();
	if (CargoBadge) CargoBadge->SetVisibility(false);
	if (FlyingParcel.IsValid()) FlyingParcel->Destroy();
	FlyingParcel.Reset(); FlightAlpha = 1.f;
	SwallowingParcel.Reset(); SwallowAlpha = 1.f;
}

void ASlimeArchiveDirector::AdvanceTransfers(float Dt)
{
	if (SwallowingParcel.IsValid() && GetPlayerCharacter())
	{
		SwallowAlpha = FMath::Min(1.f, SwallowAlpha + Dt / 0.25f);
		SwallowingParcel->SetActorLocation(FMath::Lerp(SwallowStart, GetPlayerCharacter()->GetActorLocation(), SwallowAlpha));
		SwallowingParcel->SetActorScale3D(SwallowScale * FMath::Max(0.02f, 1.f - SwallowAlpha));
		if (SwallowAlpha >= 1.f) { SwallowingParcel.Reset(); }
	}
	if (Devour && Devour->GetStoredProp() && !SwallowingParcel.IsValid() && GetPlayerCharacter())
	{
		AActor* Prop = Devour->GetStoredProp();
		CargoPhase += Dt;
		Prop->SetActorHiddenInGame(false);
		Prop->SetActorScale3D(SwallowScale * .32f);
		Prop->SetActorLocation(GetPlayerCharacter()->GetActorLocation() + FVector(0.f, 0.f, -7.f + 3.f * FMath::Sin(CargoPhase * 2.f)));
		Prop->SetActorRotation(FRotator(0.f, CargoPhase * 55.f, 0.f));
		if (auto* Label = Prop->FindComponentByClass<UTextRenderComponent>()) Label->SetVisibility(false);
	}
	if (CargoBadge)
	{
		const int32 Category = HeldCategory();
		CargoBadge->SetVisibility(Category != INDEX_NONE);
		CargoBadge->SetText(FText::AsNumber(Category + 1));
		if (auto* PC = GetPlayerController())
			if (PC->PlayerCameraManager) CargoBadge->SetWorldRotation((PC->PlayerCameraManager->GetCameraLocation() - CargoBadge->GetComponentLocation()).Rotation());
	}
	if (FlyingParcel.IsValid())
	{
		FlightAlpha = FMath::Min(1.f, FlightAlpha + Dt / 0.35f);
		FlyingParcel->SetActorLocation(FMath::Lerp(FlightStart, FlightEnd, FlightAlpha)
			+ FVector(0.f, 0.f, 70.f * FMath::Sin(FlightAlpha * PI)));
		if (FlightAlpha >= 1.f) { FlyingParcel->Destroy(); FlyingParcel.Reset(); }
	}
}

void ASlimeArchiveDirector::ResetShift()
{
	ClearParcels(); Rules = FSlimeArchiveRules(); Serial = 0;
	Random.Initialize(19581001 + GetWeekIndex());
	TimeLeft = 180.f; ArrivalLeft = 0.f;
	Shift = EShift::Ready; bSuccess = false; bQualified = false;
	if (ACharacter* SlimePawn = GetPlayerCharacter())
	{
		SlimePawn->SetActorTransform(PlayerStart, false, nullptr, ETeleportType::TeleportPhysics);
		SlimePawn->GetCharacterMovement()->StopMovementImmediately();
	}
	SetMessage(TEXT("先走动熟悉四个柜子，Enter 开始计时。\n吞下一件 → 跑到同编号柜子 → E 投递。\n错柜会退回腹中，可继续改投。"));
	RefreshHUD();
}

void ASlimeArchiveDirector::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!IsModeActive() || IsFinished() || Shift == EShift::Invalid) return;
	auto* PC = GetPlayerController(); auto* SlimePawn = GetPlayerCharacter();
	if (!PC || !SlimePawn || PC->IsPaused()) return;
	if (PC->WasInputKeyJustPressed(EKeys::R)) { ResetShift(); return; }
	AdvanceTransfers(Dt);
	if (Shift == EShift::Results)
	{
		if (bSuccess && PC->WasInputKeyJustPressed(EKeys::Enter))
		{
			FinishMiniGame();
			if (auto* GI = GetGameInstance())
				if (auto* Quests = GI->GetSubsystem<UQuestSubsystem>()) Quests->TravelToHub(Quests->GetActiveDayId());
		}
		return;
	}
	if (Shift == EShift::Ready)
	{
		if (PC->WasInputKeyJustPressed(EKeys::Enter))
		{
			Shift = EShift::Running; SetMessage(FString::Printf(TEXT("3 分钟挑战：归档至少 %d 件，四柜各至少 %d 件。达标后继续冲分。"), TargetCounts[GetWeekIndex()-1], GetWeekIndex()+1));
		}
		RefreshHUD(); return;
	}
	TimeLeft = FMath::Max(0.f, TimeLeft - Dt); Rules.Tick(Dt);
	if (TimeLeft <= 0.f)
	{
		FinishShift(Rules.Passed(TargetCounts[GetWeekIndex() - 1], GetWeekIndex() + 1));
		RefreshHUD(); return;
	}
	MessageLeft = FMath::Max(0.f, MessageLeft - Dt);
	// A parcel already in the belly does not expire; mistakes remain recoverable.
	for (int32 Index = Parcels.Num() - 1; Index >= 0; --Index)
	{
		auto& Parcel = Parcels[Index];
		if (!IsValid(Parcel.Actor)) { Parcels.RemoveAt(Index); continue; }
		if (Devour->GetStoredProp() == Parcel.Actor) continue;
		Parcel.Age += Dt;
		if (Parcel.Age >= FMath::Max(1.f, BeltTravelSeconds))
		{
			Parcel.Actor->Destroy(); Parcels.RemoveAt(Index); Rules.Miss();
			SetMessage(TEXT("一件进入退件箱；连击重置，下一件继续。")); continue;
		}
		Parcel.Actor->SetActorLocation(FMath::Lerp(BeltStart->GetActorLocation(), BeltEnd->GetActorLocation(), Parcel.Age / BeltTravelSeconds));
		if (auto* Label = Parcel.Actor->FindComponentByClass<UTextRenderComponent>())
			if (PC->PlayerCameraManager) Label->SetWorldRotation((PC->PlayerCameraManager->GetCameraLocation() - Label->GetComponentLocation()).Rotation());
	}
	ArrivalLeft -= Dt;
	if (ArrivalLeft <= 0.f)
	{
		SpawnParcel(); ArrivalLeft += FMath::Max(2.f, ArrivalSeconds[GetWeekIndex() - 1]);
	}
	if (PC->WasInputKeyJustPressed(EKeys::F)) Swallow();
	if (PC->WasInputKeyJustPressed(EKeys::E)) Spit();
	const bool Passed = Rules.Passed(TargetCounts[GetWeekIndex() - 1], GetWeekIndex() + 1);
	if (Passed && !bQualified)
	{
		bQualified = true;
		SetMessage(TEXT("任务已达标！继续分拣冲分，3 分钟结束后统一结算。"));
	}
	RefreshHUD();
}

void ASlimeArchiveDirector::SpawnParcel()
{
	if (Serial % 4 == 0)
	{
		for (int32 I = 0; I < 4; ++I) Bag[I] = I;
		for (int32 I = 3; I > 0; --I) Swap(Bag[I], Bag[Random.RandRange(0, I)]);
	}
	const int32 Category = Bag[Serial % 4];
	const bool Equipment = Serial % 3 == 2;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	AActor* Actor = GetWorld()->SpawnActor<AActor>(!Equipment && PaperClass ? PaperClass.Get() : AStaticMeshActor::StaticClass(),
		BeltStart->GetActorLocation(), FRotator::ZeroRotator, Params);
	if (!Actor) return;
	if (auto* Paper = Cast<AQuestInteractActor>(Actor))
	{
		Paper->Configure(NAME_None, NAME_None, NAME_None, FText::GetEmpty(), FLinearColor::White, 1.f);
		if (Paper->Objective) Paper->Objective->SetConsumed(true);
	}
	Actor->SetActorScale3D(FVector::OneVector);
	Actor->SetActorEnableCollision(false);
	if (auto* Mesh = Actor->FindComponentByClass<UStaticMeshComponent>())
	{
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(Equipment ? EquipmentMesh : FolderMesh);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetMaterial(0, CategoryMaterials[Category]);
	}
	auto* Label = NewObject<UTextRenderComponent>(Actor);
	Actor->AddInstanceComponent(Label); Label->SetupAttachment(Actor->GetRootComponent());
	Label->SetText(FText::FromString(Codes[Category])); Label->SetWorldSize(17.f);
	Label->SetHorizontalAlignment(EHTA_Center); Label->SetTextRenderColor(FColor(245, 232, 194));
	Label->SetRelativeLocation(FVector(0.f, 0.f, Equipment ? 70.f : 38.f));
	Label->SetWorldRotation(FRotator(0.f, -90.f, 0.f));
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision); Label->RegisterComponent();
	FSlimeArchiveParcel Parcel; Parcel.Actor = Actor; Parcel.Category = Category;
	Parcels.Add(Parcel); ++Serial;
}

int32 ASlimeArchiveDirector::HeldCategory() const
{
	if (!Devour) return INDEX_NONE;
	for (const auto& Parcel : Parcels) if (Parcel.Actor == Devour->GetStoredProp()) return Parcel.Category;
	return INDEX_NONE;
}

void ASlimeArchiveDirector::Swallow()
{
	if (Devour->GetStoredProp()) { SetMessage(TEXT("腹中已有一件，先去对应柜子投递。")); return; }
	AActor* Nearest = nullptr; float Best = FMath::Square(InteractionRange);
	for (const auto& Parcel : Parcels)
	{
		if (!IsValid(Parcel.Actor)) continue;
		const float Dist = FVector::DistSquared(Parcel.Actor->GetActorLocation(), GetPlayerCharacter()->GetActorLocation());
		if (Dist < Best) { Best = Dist; Nearest = Parcel.Actor; }
	}
	if (Nearest && Devour->TrySwallowProp(Nearest, InteractionRange))
	{
		SwallowingParcel = Nearest; SwallowStart = Nearest->GetActorLocation();
		SwallowScale = Nearest->GetActorScale3D(); SwallowAlpha = 0.f; CargoPhase = 0.f;
		Nearest->SetActorHiddenInGame(false);
		SetMessage(FString::Printf(TEXT("已吞下：%d 号 · %s。去同编号柜子按 E。"), HeldCategory() + 1, Names[HeldCategory()]));
	}
	else SetMessage(TEXT("靠近传送带上的物品，再按 F 吞下。"));
}

int32 ASlimeArchiveDirector::NearestCabinet() const
{
	if (!GetPlayerCharacter()) return INDEX_NONE;
	int32 Nearest = INDEX_NONE; float Best = FMath::Square(InteractionRange);
	for (int32 I = 0; I < CabinetTargets.Num(); ++I)
	{
		if (!CabinetTargets[I]) continue;
		const float Dist = FVector::DistSquared(GetPlayerCharacter()->GetActorLocation(), CabinetTargets[I]->GetActorLocation());
		if (Dist < Best) { Best = Dist; Nearest = I; }
	}
	return Nearest;
}

void ASlimeArchiveDirector::Spit()
{
	if (SwallowingParcel.IsValid()) return;
	const int32 Category = HeldCategory();
	if (Category == INDEX_NONE) { SetMessage(TEXT("腹中是空的，先靠近物品按 F。")); return; }
	const int32 Cabinet = NearestCabinet();
	if (Cabinet == INDEX_NONE) { SetMessage(TEXT("再靠近柜子前的投递台，然后按 E。")); return; }
	if (!Rules.Deposit(Category, Cabinet))
	{
		SetMessage(FString::Printf(TEXT("这件属于 %d 号 %s；柜子退回了物品，仍在腹中。"), Category + 1, Names[Category]));
		return;
	}
	FlightStart = GetPlayerCharacter()->GetActorLocation() + FVector(0.f, 0.f, 25.f);
	FlightEnd = CabinetTargets[Cabinet]->GetActorLocation();
	if (FlyingParcel.IsValid()) FlyingParcel->Destroy();
	FlyingParcel = Devour->ReleaseStoredProp(FlightStart); FlightAlpha = 0.f;
	if (CargoBadge) CargoBadge->SetVisibility(false);
	Parcels.RemoveAll([&](const FSlimeArchiveParcel& Parcel) { return Parcel.Actor == FlyingParcel.Get(); });
	SetMessage(FString::Printf(TEXT("归档成功！%d 连击 · 本件 +%d 分"), Rules.Chain, 10 * Rules.Multiplier()));
}

void ASlimeArchiveDirector::FinishShift(bool Success)
{
	bSuccess = Success; Shift = EShift::Results;
	if (auto* SlimePawn = GetPlayerCharacter()) SlimePawn->GetCharacterMovement()->StopMovementImmediately();
	SetMessage(Success
		? FString::Printf(TEXT("3 分钟结束 · 最终 %d 分 · 归档 %d 件\n任务达标！Enter 保存通关并返回博物馆 · R 再来一班"), Rules.Score, Rules.Correct)
		: FString::Printf(TEXT("3 分钟结束 · 最终 %d 分 · 归档 %d 件\n任务未达标：共需 %d 件，四柜各至少 %d 件。R 重试。"), Rules.Score, Rules.Correct, TargetCounts[GetWeekIndex()-1], GetWeekIndex()+1));
}

void ASlimeArchiveDirector::SetMessage(const FString& Text) { Message = Text; MessageLeft = 4.f; }

void ASlimeArchiveDirector::RefreshHUD()
{
	if (!HUD) return;
	const int32 Week = GetWeekIndex(); const int32 Seconds = FMath::CeilToInt(TimeLeft);
	const FString Status = FString::Printf(TEXT("第 %d 班  ·  %02d:%02d  ·  %d 分\n%s %d / %d    连击 %d  ×%d\n四柜：%d / %d / %d / %d（各需 %d 件）\n错投 %d  ·  退件 %d"),
		Week, Seconds / 60, Seconds % 60, Rules.Score, bQualified ? TEXT("已达标") : TEXT("归档"), Rules.Correct, TargetCounts[Week - 1], Rules.Chain, Rules.Multiplier(),
		Rules.Filed[0], Rules.Filed[1], Rules.Filed[2], Rules.Filed[3], Week + 1, Rules.Mistakes, Rules.Missed);
	const int32 Held = HeldCategory();
	const FString Cargo = Held != INDEX_NONE ? FString::Printf(TEXT("腹中：%d 号 · %s"), Held + 1, Names[Held]) : TEXT("腹中：空");
	FString Hint = Message;
	if (Shift == EShift::Running && MessageLeft <= 0.f)
	{
		const int32 Near = NearestCabinet();
		Hint = Near != INDEX_NONE ? FString::Printf(TEXT("身旁：%d 号 · %s 投递柜"), Near + 1, Names[Near]) : TEXT("14 秒内正确投递续连击 · 每 4 件升倍率 · 最高 ×4");
	}
	HUD->SetDisplay(Status, Cargo, Hint);
}
