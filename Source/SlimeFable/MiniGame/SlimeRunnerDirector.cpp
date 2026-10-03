#include "MiniGame/SlimeRunnerDirector.h"
#include "MiniGame/SlimeRunnerHUDWidget.h"
#include "Quest/QuestSubsystem.h"
#include "CombatDamageable.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

namespace SlimeRunnerPrivate
{
	const FRotator SideYaw(0.f, -90.f, 0.f);
}

ASlimeRunnerDirector::ASlimeRunnerDirector()
{
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	StartLabel = FText::FromString(TEXT("起点"));
	EndLabel = FText::FromString(TEXT("终点"));
	FinaleKicker = FText::FromString(TEXT("到达"));
}

ASlimeRunnerDirector* ASlimeRunnerDirector::Find(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<ASlimeRunnerDirector> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ASlimeRunnerDirector::EnterMode(ACharacter* Character, APlayerController* PC)
{
	FVector Start = Character->GetActorLocation();
	Start.Y = PlaneY;
	Character->TeleportTo(Start, SlimeRunnerPrivate::SideYaw, false, true);
	CheckpointLocation = Start;

	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		bSavedConstrain = Move->bConstrainToPlane;
		SavedPlaneNormal = Move->GetPlaneConstraintNormal();
		Move->SetPlaneConstraintNormal(FVector(0.f, 1.f, 0.f));
		Move->SetPlaneConstraintOrigin(FVector(0.f, PlaneY, 0.f));
		Move->SetPlaneConstraintEnabled(true);
	}

	PC->SetControlRotation(SlimeRunnerPrivate::SideYaw);
	PC->SetIgnoreLookInput(true);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	RunnerCamera = GetWorld()->SpawnActor<ACameraActor>(Start, FRotator::ZeroRotator, Params);
	if (RunnerCamera)
	{
		RunnerCamera->GetCameraComponent()->SetFieldOfView(CameraFOV);
		RunnerCamera->GetCameraComponent()->bConstrainAspectRatio = false;
		UpdateFollowCamera(0.f, true);
		PreviousViewTarget = PC->GetViewTarget();
		PC->SetViewTargetWithBlend(RunnerCamera, 0.4f);
	}

	if (FlagStart.IsZero() && RaisedFlag) FlagStart = RaisedFlag->GetActorLocation();

	HUD = CreateWidget<USlimeRunnerHUDWidget>(PC, USlimeRunnerHUDWidget::StaticClass());
	if (HUD)
	{
		HUD->SetLabels(StartLabel, EndLabel);
		HUD->AddToViewport(5);
		UpdateHUD();
	}

	if (GetWeekIndex() >= 3 && Week3TimeLimit > 0.f)
	{
		if (UQuestSubsystem* Quests = GetGameInstance()->GetSubsystem<UQuestSubsystem>())
		{
			Quests->ShowCenterBanner(FText::FromString(TEXT("三周目")),
				FText::FromString(FString::Printf(TEXT("%d 秒内到达终点"), FMath::RoundToInt(Week3TimeLimit))), 3.f);
		}
	}
}

void ASlimeRunnerDirector::ExitMode(ACharacter* Character, APlayerController* PC)
{
	if (HUD)
	{
		HUD->RemoveFromParent();
		HUD = nullptr;
	}
	if (Character)
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->SetPlaneConstraintEnabled(bSavedConstrain);
			Move->SetPlaneConstraintNormal(SavedPlaneNormal);
		}
	}
	if (PC)
	{
		PC->ResetIgnoreLookInput();
		if (PreviousViewTarget.IsValid()) PC->SetViewTarget(PreviousViewTarget.Get());
		else if (Character) PC->SetViewTarget(Character);
	}
	if (RunnerCamera)
	{
		RunnerCamera->Destroy();
		RunnerCamera = nullptr;
	}
}

void ASlimeRunnerDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsModeActive()) return;

	if (bFinale)
	{
		TickFinale(DeltaSeconds);
		UpdateHUD();
		return;
	}

	if (APlayerController* PC = GetPlayerController())
	{
		PC->SetControlRotation(SlimeRunnerPrivate::SideYaw);
		if (PC->GetViewTarget() != RunnerCamera && RunnerCamera) PC->SetViewTarget(RunnerCamera);
	}
	UpdateFollowCamera(DeltaSeconds, false);

	ElapsedSeconds += DeltaSeconds;
	if (GetWeekIndex() >= 3 && Week3TimeLimit > 0.f && ElapsedSeconds > Week3TimeLimit)
	{
		if (UQuestSubsystem* Quests = GetGameInstance()->GetSubsystem<UQuestSubsystem>())
		{
			Quests->ShowCenterBanner(FText::FromString(TEXT("超时")), FText::FromString(TEXT("再跑一次")), 2.f);
		}
		FailMiniGame();
		return;
	}
	UpdateHUD();
}

void ASlimeRunnerDirector::UpdateFollowCamera(float DeltaSeconds, bool bSnap)
{
	const ACharacter* Character = GetPlayerCharacter();
	if (!RunnerCamera || !Character) return;

	const FVector PlayerLocation = Character->GetActorLocation();
	const float VelocityX = Character->GetVelocity().X;
	if (FMath::Abs(VelocityX) > 60.f) LookAheadSign = FMath::Sign(VelocityX);
	CurrentLookAhead = bSnap
		? CameraLookAhead * LookAheadSign
		: FMath::FInterpTo(CurrentLookAhead, CameraLookAhead * LookAheadSign, DeltaSeconds, 1.6f);

	const float TargetX = FMath::Clamp(PlayerLocation.X + CurrentLookAhead, CourseStartX, CourseEndX);
	const FVector Target(TargetX, PlaneY + CameraDistance, PlayerLocation.Z + CameraHeight);
	FVector Location = RunnerCamera->GetActorLocation();
	if (bSnap)
	{
		Location = Target;
	}
	else
	{
		Location.X = FMath::FInterpTo(Location.X, Target.X, DeltaSeconds, 6.f);
		Location.Y = Target.Y;
		Location.Z = FMath::FInterpTo(Location.Z, Target.Z, DeltaSeconds, 3.f);
	}
	RunnerCamera->SetActorLocationAndRotation(Location, FRotator(CameraPitch, -90.f, 0.f));
}

void ASlimeRunnerDirector::CollectFlag(int32 Value)
{
	FlagsCollected += FMath::Max(1, Value);
	UpdateHUD();
}

void ASlimeRunnerDirector::SetCheckpoint(const FVector& Location)
{
	CheckpointLocation = FVector(Location.X, PlaneY, Location.Z);
}

void ASlimeRunnerDirector::HurtPlayer(AActor* Source)
{
	ACharacter* Character = GetPlayerCharacter();
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Character || bFinale || Now < InvulnerableUntil) return;
	InvulnerableUntil = Now + HurtInvulnerableSeconds;

	const float Away = Source && Source->GetActorLocation().X > Character->GetActorLocation().X ? -1.f : 1.f;
	const FVector Impulse(Away * 650.f, 0.f, 520.f);
	Character->LaunchCharacter(Impulse, true, true);
	if (ICombatDamageable* Damageable = Cast<ICombatDamageable>(Character))
	{
		Damageable->ApplyDamage(HazardDamage, Source, Character->GetActorLocation(), FVector::ZeroVector);
	}
}

void ASlimeRunnerDirector::RespawnPlayer()
{
	ACharacter* Character = GetPlayerCharacter();
	if (!Character || bFinale) return;
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
	}
	Character->TeleportTo(CheckpointLocation, SlimeRunnerPrivate::SideYaw, false, true);
	InvulnerableUntil = GetWorld()->GetTimeSeconds() + HurtInvulnerableSeconds;
	if (RunnerCamera) UpdateFollowCamera(0.f, true);
	if (ICombatDamageable* Damageable = Cast<ICombatDamageable>(Character))
	{
		Damageable->ApplyDamage(FallDamage, this, Character->GetActorLocation(), FVector::ZeroVector);
	}
}

void ASlimeRunnerDirector::BeginFinale()
{
	if (bFinale || IsFinished() || !IsModeActive()) return;
	bFinale = true;
	FinaleSeconds = 0.f;
	SaluteTimer = 0.f;
	SalutesFired = 0;

	ACharacter* Character = GetPlayerCharacter();
	APlayerController* PC = GetPlayerController();
	if (Character)
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
		}
		if (PC) Character->DisableInput(PC);
	}
	if (RunnerCamera)
	{
		FinaleFromLocation = RunnerCamera->GetActorLocation();
		FinaleFromRotation = RunnerCamera->GetActorRotation();
	}
	if (RaisedFlag) FlagStart = RaisedFlag->GetActorLocation();
}

void ASlimeRunnerDirector::TickFinale(float DeltaSeconds)
{
	FinaleSeconds += DeltaSeconds;

	if (RunnerCamera)
	{
		const float Alpha = FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp(FinaleSeconds / FinaleBlendSeconds, 0.f, 1.f), 2.f);
		const FVector To = GetActorTransform().TransformPosition(FinaleCameraLocation);
		RunnerCamera->SetActorLocationAndRotation(
			FMath::Lerp(FinaleFromLocation, To, Alpha),
			FMath::Lerp(FinaleFromRotation, FinaleCameraRotation, Alpha));
	}

	const float ShowSeconds = FinaleSeconds - FinaleBlendSeconds;
	if (ShowSeconds < 0.f) return;

	if (RaisedFlag)
	{
		const float Rise = FMath::Clamp(ShowSeconds / FlagRiseSeconds, 0.f, 1.f);
		RaisedFlag->SetActorLocation(FlagStart + FVector(0.f, 0.f, FlagRiseHeight * Rise));
	}

	const int32 SaluteCount = FMath::Max(1, FlagsCollected);
	SaluteTimer -= DeltaSeconds;
	if (SalutesFired < SaluteCount && SaluteTimer <= 0.f)
	{
		FireSalute();
		SaluteTimer = SaluteInterval;
	}

	if (!bBannerShown && !FinaleText.IsEmpty())
	{
		bBannerShown = true;
		if (UQuestSubsystem* Quests = GetGameInstance()->GetSubsystem<UQuestSubsystem>())
		{
			Quests->ShowCenterBanner(FinaleKicker, FinaleText, FlagRiseSeconds + 2.f);
		}
	}

	const float SaluteSeconds = SaluteCount * SaluteInterval;
	if (ShowSeconds > FMath::Max(FlagRiseSeconds, SaluteSeconds) + FinaleTailSeconds)
	{
		FinishMiniGame();
	}
}

void ASlimeRunnerDirector::FireSalute()
{
	FVector Spot = GetActorLocation() + FVector(0.f, 0.f, 1500.f);
	if (SaluteSpots.Num() > 0)
	{
		Spot = GetActorTransform().TransformPosition(SaluteSpots[SalutesFired % SaluteSpots.Num()]);
	}
	Spot += FVector(FMath::FRandRange(-250.f, 250.f), FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-120.f, 200.f));
	if (SaluteFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, SaluteFX, Spot);
	}
	if (SaluteSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SaluteSound, Spot);
	}
	++SalutesFired;
}

void ASlimeRunnerDirector::UpdateHUD()
{
	if (!HUD) return;
	const ACharacter* Character = GetPlayerCharacter();
	const float Span = FMath::Max(1.f, CourseEndX - CourseStartX);
	const float Progress = bFinale ? 1.f
		: Character ? FMath::Clamp((Character->GetActorLocation().X - CourseStartX) / Span, 0.f, 1.f) : 0.f;
	const float TimeLeft = GetWeekIndex() >= 3 && Week3TimeLimit > 0.f
		? FMath::Max(0.f, Week3TimeLimit - ElapsedSeconds) : -1.f;
	HUD->SetStats(FlagsCollected, GetFlagsTotal(), Progress, TimeLeft);
}
