// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeAbilityComponent.h"

#include "Blueprint/UserWidget.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "SlimeBodyComponent.h"
#include "SlimeClingComponent.h"
#include "SlimeDevourComponent.h"
#include "SlimeElementComponent.h"
#include "SlimeFable.h"
#include "Settings/SlimeInputSettings.h"
#include "Settings/SlimeInputTypes.h"
#include "UI/SlimeElementWheelWidget.h"
#include "Engine/GameInstance.h"

USlimeAbilityComponent::USlimeAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;
}

void USlimeAbilityComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		Body = Owner->FindComponentByClass<USlimeBodyComponent>();
		Element = Owner->FindComponentByClass<USlimeElementComponent>();
	}

	if (!Body)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("SlimeAbilityComponent on '%s' found no SlimeBodyComponent; abilities will do nothing."), *GetNameSafe(GetOwner()));
	}

	RegisterMappingContext();
}

void USlimeAbilityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Leaving the world mid wheel would otherwise strand the global time dilation.
	if (bWheelOpen)
	{
		CloseWheel(false);
	}
	Super::EndPlay(EndPlayReason);
}

APlayerController* USlimeAbilityComponent::GetOwningPlayerController() const
{
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		return Cast<APlayerController>(Pawn->GetController());
	}
	return nullptr;
}

void USlimeAbilityComponent::RegisterMappingContext()
{
	if (!SlimeMappingContext)
	{
		if (!bLoggedMissingMappingContext)
		{
			bLoggedMissingMappingContext = true;
			UE_LOG(LogSlimeFable, Warning, TEXT("SlimeAbilityComponent on '%s': SlimeMappingContext is null; Enhanced Input abilities will not fire (Tick key polling still works if enabled)."), *GetNameSafe(GetOwner()));
		}
		return;
	}

	const APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController)
	{
		return;
	}

	if (const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Subsystem->AddMappingContext(SlimeMappingContext, MappingPriority);
		}
	}
}

void USlimeAbilityComponent::BindInput(UEnhancedInputComponent* EnhancedInput)
{
	if (!EnhancedInput)
	{
		return;
	}

	if (FlattenAction)
	{
		EnhancedInput->BindAction(FlattenAction, ETriggerEvent::Started, this, &USlimeAbilityComponent::HandleFlattenStarted);
		EnhancedInput->BindAction(FlattenAction, ETriggerEvent::Completed, this, &USlimeAbilityComponent::HandleFlattenCompleted);
		EnhancedInput->BindAction(FlattenAction, ETriggerEvent::Canceled, this, &USlimeAbilityComponent::HandleFlattenCompleted);
	}

	if (ResetAction)
	{
		EnhancedInput->BindAction(ResetAction, ETriggerEvent::Started, this, &USlimeAbilityComponent::HandleResetTriggered);
	}

	if (AbsorbAction)
	{
		EnhancedInput->BindAction(AbsorbAction, ETriggerEvent::Started, this, &USlimeAbilityComponent::HandleAbsorbStarted);
		EnhancedInput->BindAction(AbsorbAction, ETriggerEvent::Completed, this, &USlimeAbilityComponent::HandleAbsorbCompleted);
		EnhancedInput->BindAction(AbsorbAction, ETriggerEvent::Canceled, this, &USlimeAbilityComponent::HandleAbsorbCompleted);
	}

	if (LaunchAction)
	{
		EnhancedInput->BindAction(LaunchAction, ETriggerEvent::Started, this, &USlimeAbilityComponent::HandleLaunchStarted);
		EnhancedInput->BindAction(LaunchAction, ETriggerEvent::Completed, this, &USlimeAbilityComponent::HandleLaunchCompleted);
		EnhancedInput->BindAction(LaunchAction, ETriggerEvent::Canceled, this, &USlimeAbilityComponent::HandleLaunchCompleted);
	}

	if (ElementWheelAction)
	{
		EnhancedInput->BindAction(ElementWheelAction, ETriggerEvent::Started, this, &USlimeAbilityComponent::HandleWheelStarted);
		EnhancedInput->BindAction(ElementWheelAction, ETriggerEvent::Completed, this, &USlimeAbilityComponent::HandleWheelCompleted);
		EnhancedInput->BindAction(ElementWheelAction, ETriggerEvent::Canceled, this, &USlimeAbilityComponent::HandleWheelCompleted);
	}

	if (ElementCycleAction)
	{
		EnhancedInput->BindAction(ElementCycleAction, ETriggerEvent::Triggered, this, &USlimeAbilityComponent::HandleCycle);
	}
}

void USlimeAbilityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Undilated: slowing the world for the wheel must not also slow the wheel's own throttle.
	CycleCooldownRemaining = FMath::Max(CycleCooldownRemaining - float(FApp::GetDeltaTime()), 0.f);

	if (bPollAbilityKeys)
	{
		PollAbilityKeys(DeltaTime);
	}

	if (bCharging)
	{
		ChargeElapsed += DeltaTime;
		BuildLaunchPath(PendingLaunchPath);
		if (bDrawTrajectoryPreview)
		{
			DrawLaunchPath(PendingLaunchPath);
		}
	}
}

void USlimeAbilityComponent::PollAbilityKeys(float DeltaTime)
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	const USlimeInputSettings* InputSettings = nullptr;
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			InputSettings = GI->GetSubsystem<USlimeInputSettings>();
		}
	}

	auto IsDown = [PlayerController, InputSettings](ESlimeInputAction Action, const FKey& Fallback) -> bool
	{
		if (InputSettings)
		{
			return InputSettings->IsKeyDown(PlayerController, Action);
		}
		return PlayerController->IsInputKeyDown(Fallback);
	};
	auto WasPressed = [PlayerController, InputSettings](ESlimeInputAction Action, const FKey& Fallback) -> bool
	{
		if (InputSettings)
		{
			return InputSettings->WasKeyPressed(PlayerController, Action);
		}
		return PlayerController->WasInputKeyJustPressed(Fallback);
	};

	// Drive Body/UI directly so Enhanced Input handlers can hard-return while polling is on.
	const bool bFlatten = IsDown(ESlimeInputAction::Flatten, EKeys::C);
	const USlimeDevourComponent* Devour = GetOwner() ? GetOwner()->FindComponentByClass<USlimeDevourComponent>() : nullptr;
	const bool bCombatLocked = Devour && Devour->IsCombatLocked();
	const bool bPhantomWheelOpen = Devour && Devour->IsPhantomWheelOpen();
	if (bFlatten != bPollFlattenDown)
	{
		bPollFlattenDown = bFlatten;
		if (Body && !(bFlatten && bCombatLocked))
		{
			Body->SetSpread(bFlatten);
		}
	}

	if (WasPressed(ESlimeInputAction::ResetBody, EKeys::T))
	{
		bool bDetached = false;
		if (USlimeClingComponent* Cling = GetOwner() ? GetOwner()->FindComponentByClass<USlimeClingComponent>() : nullptr)
		{
			bDetached = Cling->TryDetach();
		}
		if (!bDetached && Body && !bCombatLocked)
		{
			Body->ResetBody();
		}
	}

	const bool bAbsorb = IsDown(ESlimeInputAction::Absorb, EKeys::X);
	bPollAbsorbDown = bAbsorb;
	const bool bDevourOwnsShots = Devour && Devour->IsDevouring();
	if (Body && !bDevourOwnsShots)
	{
		Body->SetRecalling(bAbsorb);
	}

	const bool bLaunch = IsDown(ESlimeInputAction::Launch, EKeys::G);
	if (bLaunch && !bPollLaunchDown)
	{
		bPollLaunchDown = true;
		if (!bCombatLocked && !bPhantomWheelOpen)
		{
			BeginLaunchCharge();
		}
	}
	else if (!bLaunch && bPollLaunchDown)
	{
		bPollLaunchDown = false;
		ReleaseLaunchCharge();
	}

	const bool bWheel = IsDown(ESlimeInputAction::ElementWheel, EKeys::Tab);
	if (bWheel && !bPollWheelDown)
	{
		bPollWheelDown = true;
		if (!bPhantomWheelOpen && !bCombatLocked)
		{
			OpenWheel();
		}
	}
	else if (!bWheel && bPollWheelDown)
	{
		bPollWheelDown = false;
		CloseWheel(true);
	}

	if (CycleCooldownRemaining <= 0.f)
	{
		int32 Step = 0;
		if (PlayerController->WasInputKeyJustPressed(EKeys::MouseScrollUp))
		{
			Step = 1;
		}
		else if (PlayerController->WasInputKeyJustPressed(EKeys::MouseScrollDown))
		{
			Step = -1;
		}
		else
		{
			const float WheelAxis = PlayerController->GetInputAnalogKeyState(EKeys::MouseWheelAxis);
			if (WheelAxis > 0.1f)
			{
				Step = 1;
			}
			else if (WheelAxis < -0.1f)
			{
				Step = -1;
			}
		}

		if (Step != 0)
		{
			if (bWheelOpen && Element)
			{
				CycleCooldownRemaining = CycleCooldown;
				const ESlimeElement Next = Element->CycleElement(Step);
				if (WheelWidget)
				{
					WheelWidget->SetHighlightedElement(Next);
				}
			}
			else if (bCharging)
			{
				CycleCooldownRemaining = CycleCooldown;
				AdjustLaunchRange(Step);
			}
		}
	}

	(void)DeltaTime;
}

float USlimeAbilityComponent::GetLaunchCharge() const
{
	const float Period = FMath::Max(FullChargeTime, KINDA_SMALL_NUMBER);
	return FMath::Fmod(ChargeElapsed, Period) / Period;
}

FLinearColor USlimeAbilityComponent::GetLaunchPreviewColor() const
{
	FLinearColor Base = FLinearColor::White;
	if (Element)
	{
		Base = Element->GetProfile(Element->GetPreviewElement()).BaseColor;
	}
	FLinearColor Pale = FMath::Lerp(Base, FLinearColor::White, 0.28f);
	Pale.R = FMath::Min(Pale.R * 1.08f, 1.f);
	Pale.G = FMath::Min(Pale.G * 1.08f, 1.f);
	Pale.B = FMath::Min(Pale.B * 1.08f, 1.f);
	return Pale;
}

void USlimeAbilityComponent::HandleFlattenStarted()
{
	if (bPollAbilityKeys)
	{
		return;
	}
	if (Body)
	{
		Body->SetSpread(true);
	}
}

void USlimeAbilityComponent::HandleFlattenCompleted()
{
	if (bPollAbilityKeys)
	{
		return;
	}
	if (Body)
	{
		Body->SetSpread(false);
	}
}

void USlimeAbilityComponent::HandleResetTriggered()
{
	if (bPollAbilityKeys)
	{
		return;
	}
	if (USlimeClingComponent* Cling = GetOwner() ? GetOwner()->FindComponentByClass<USlimeClingComponent>() : nullptr)
	{
		if (Cling->TryDetach())
		{
			return;
		}
	}
	if (Body)
	{
		Body->ResetBody();
	}
}

void USlimeAbilityComponent::HandleAbsorbStarted()
{
	if (bPollAbilityKeys)
	{
		return;
	}
	if (Body)
	{
		// No-op without fragments, so mashing F costs nothing.
		Body->SetRecalling(true);
	}
}

void USlimeAbilityComponent::HandleAbsorbCompleted()
{
	if (bPollAbilityKeys)
	{
		return;
	}
	if (Body)
	{
		Body->SetRecalling(false);
	}
}

void USlimeAbilityComponent::HandleLaunchStarted()
{
	if (bPollAbilityKeys)
	{
		return;
	}
	BeginLaunchCharge();
}

void USlimeAbilityComponent::HandleLaunchCompleted()
{
	if (bPollAbilityKeys)
	{
		return;
	}
	ReleaseLaunchCharge();
}

void USlimeAbilityComponent::BeginLaunchCharge()
{
	bCharging = true;
	ChargeElapsed = 0.f;
	LaunchExtraArcHeight = DefaultLaunchArcHeight;
	LaunchRange = FMath::Clamp(DefaultLaunchRange, MinLaunchRange, MaxLaunchRange);
	PendingLaunchPath = FSlimeLaunchPath();
}

void USlimeAbilityComponent::ReleaseLaunchCharge()
{
	if (!bCharging)
	{
		return;
	}
	bCharging = false;

	if (Body)
	{
		if (!PendingLaunchPath.bValid)
		{
			BuildLaunchPath(PendingLaunchPath);
		}
		if (PendingLaunchPath.bValid)
		{
			const int32 Launched = Body->LaunchChunkAlongPath(PendingLaunchPath);
			if (Launched == 0)
			{
				UE_LOG(LogSlimeFable, Verbose, TEXT("Slime launch rejected: active shot limit or clone pool full."));
			}
		}
		else
		{
			FVector Direction;
			if (GetAimDirection(Direction))
			{
				const float Speed = FMath::Lerp(MinLaunchSpeed, MaxLaunchSpeed, GetLaunchCharge());
				Body->LaunchChunk(Direction * Speed);
			}
		}
	}

	ChargeElapsed = 0.f;
	PendingLaunchPath = FSlimeLaunchPath();
}

void USlimeAbilityComponent::AdjustLaunchRange(int32 Step)
{
	LaunchRange = FMath::Clamp(
		LaunchRange + float(Step) * LaunchRangeStep,
		MinLaunchRange,
		MaxLaunchRange);
}

bool USlimeAbilityComponent::GetAimDirection(FVector& OutDirection) const
{
	const APlayerController* PlayerController = GetOwningPlayerController();
	if (PlayerController)
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
		OutDirection = (ViewRotation.Vector() + FVector(0.0, 0.0, double(LaunchUpwardBias))).GetSafeNormal();
		return true;
	}

	if (const AActor* Owner = GetOwner())
	{
		OutDirection = (Owner->GetActorForwardVector() + FVector(0.0, 0.0, double(LaunchUpwardBias))).GetSafeNormal();
		return true;
	}
	return false;
}

bool USlimeAbilityComponent::ResolveLaunchTarget(FVector& OutStart, FVector& OutTarget) const
{
	UWorld* World = GetWorld();
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!World || !Body || !PlayerController)
	{
		return false;
	}

	int32 ViewX = 0;
	int32 ViewY = 0;
	PlayerController->GetViewportSize(ViewX, ViewY);
	FVector CamLoc = FVector::ZeroVector;
	FVector CamDir = FVector::ForwardVector;
	if (ViewX <= 0 || ViewY <= 0 ||
		!PlayerController->DeprojectScreenPositionToWorld(float(ViewX) * 0.5f, float(ViewY) * 0.5f, CamLoc, CamDir))
	{
		FRotator ViewRotation;
		PlayerController->GetPlayerViewPoint(CamLoc, ViewRotation);
		CamDir = ViewRotation.Vector();
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeLaunchAim), false, GetOwner());
	FHitResult Hit;

	OutStart = Body->GetBlobCenter() + FVector(0.f, 0.f, 40.f);
	FVector Flat(CamDir.X, CamDir.Y, 0.f);
	if (Flat.Normalize())
	{
		const FVector Probe = OutStart + Flat * LaunchRange + FVector(0.f, 0.f, 200.f);
		if (World->LineTraceSingleByChannel(Hit, Probe, Probe - FVector(0.f, 0.f, 5000.f), ECC_Visibility, Params))
		{
			OutTarget = Hit.ImpactPoint;
		}
		else
		{
			OutTarget = OutStart + Flat * LaunchRange;
			OutTarget.Z = OutStart.Z;
		}
	}
	else
	{
		OutTarget = OutStart + CamDir * LaunchRange;
	}

	return true;
}

FVector USlimeAbilityComponent::SimulateLaunchTrajectory(const FVector& Start, const FVector& LaunchVelocity, TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();
	OutPoints.Add(Start);

	UWorld* World = GetWorld();
	if (!World || !Body)
	{
		return Start;
	}

	constexpr float SimDt = 0.02f;
	const float Damp = FMath::Exp(-Body->SolverParams.LinearDamping * SimDt);
	const float Gravity = Body->SolverParams.Gravity;
	const float Radius = FMath::Max(Body->SolverParams.RestRadius * FMath::Pow(Body->LaunchFraction, 1.f / 3.f), 8.f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeLaunchPreview), false, GetOwner());
	const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);

	FVector Position = Start;
	FVector Velocity = LaunchVelocity;
	FVector Previous = Start;

	for (int32 Step = 0; Step < 500; ++Step)
	{
		Velocity *= Damp;
		Velocity.Z += Gravity * SimDt;
		const FVector Next = Position + Velocity * SimDt;

		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, Previous, Next, FQuat::Identity, ECC_Visibility, Shape, Params))
		{
			OutPoints.Add(Hit.ImpactPoint);
			return Hit.ImpactPoint;
		}

		OutPoints.Add(Next);
		Previous = Next;
		Position = Next;
	}

	return Position;
}

bool USlimeAbilityComponent::BuildLaunchPath(FSlimeLaunchPath& OutPath) const
{
	OutPath = FSlimeLaunchPath();
	if (!Body)
	{
		return false;
	}

	FVector Start = FVector::ZeroVector;
	FVector Target = FVector::ZeroVector;
	if (!ResolveLaunchTarget(Start, Target))
	{
		return false;
	}

	FVector Delta = Target - Start;
	FVector Flat(Delta.X, Delta.Y, 0.f);
	const float Horiz = FMath::Max(Flat.Size(), 200.f);
	const float GravityAbs = FMath::Abs(Body->SolverParams.Gravity);
	const float FlightTime = FMath::Clamp(Horiz / 520.f + 0.35f, 0.7f, 3.4f);

	FVector V0 = Delta / FlightTime;
	V0.Z = Delta.Z / FlightTime + 0.5f * GravityAbs * FlightTime + LaunchExtraArcHeight / FlightTime;

	for (int32 Pass = 0; Pass < 3; ++Pass)
	{
		TArray<FVector> Scratch;
		const FVector Landing = SimulateLaunchTrajectory(Start, V0, Scratch);
		const FVector Miss = Target - Landing;
		V0.X += Miss.X / FlightTime * 0.85f;
		V0.Y += Miss.Y / FlightTime * 0.85f;
		V0.Z += Miss.Z / FlightTime * 0.35f;
	}

	OutPath.LaunchVelocity = V0;
	OutPath.Landing = SimulateLaunchTrajectory(Start, V0, OutPath.Points);
	OutPath.Duration = FMath::Max(float(FMath::Max(OutPath.Points.Num() - 1, 1)) * 0.02f, 0.02f);
	OutPath.bValid = OutPath.Points.Num() >= 2;
	return OutPath.bValid;
}

void USlimeAbilityComponent::DrawLaunchPath(const FSlimeLaunchPath& Path) const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	if (!World || !Path.bValid || Path.Points.Num() < 2)
	{
		return;
	}

	const FColor Color = GetLaunchPreviewColor().ToFColor(true);
	constexpr float Dash = 18.f;
	constexpr float Gap = 12.f;
	float Carry = 0.f;
	bool bDrawDash = true;
	for (int32 Index = 1; Index < Path.Points.Num(); ++Index)
	{
		FVector Cursor = Path.Points[Index - 1];
		const FVector End = Path.Points[Index];
		FVector Remain = End - Cursor;
		float RemainLen = Remain.Size();
		if (RemainLen <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		FVector Dir = Remain / RemainLen;
		while (RemainLen > KINDA_SMALL_NUMBER)
		{
			const float Slice = FMath::Min((bDrawDash ? Dash : Gap) - Carry, RemainLen);
			const FVector Next = Cursor + Dir * Slice;
			if (bDrawDash)
			{
				DrawDebugLine(World, Cursor, Next, Color, false, -1.f, 0, 2.f);
			}
			Carry += Slice;
			RemainLen -= Slice;
			Cursor = Next;
			if (Carry >= (bDrawDash ? Dash : Gap) - KINDA_SMALL_NUMBER)
			{
				Carry = 0.f;
				bDrawDash = !bDrawDash;
			}
		}
	}
	DrawDebugSphere(World, Path.Landing, 14.f, 10, Color, false, -1.f, 0, 1.5f);
#endif
}

void USlimeAbilityComponent::HandleWheelStarted()
{
	if (bPollAbilityKeys)
	{
		return;
	}
	OpenWheel();
}

void USlimeAbilityComponent::HandleWheelCompleted()
{
	if (bPollAbilityKeys)
	{
		return;
	}
	CloseWheel(true);
}

void USlimeAbilityComponent::HandleCycle(const FInputActionValue& Value)
{
	// Tick polling already steps the wheel from mouse scroll while open.
	if (bPollAbilityKeys)
	{
		return;
	}
	if (!bWheelOpen || !Element)
	{
		return;
	}

	const float Axis = Value.Get<float>();
	if (FMath::IsNearlyZero(Axis) || CycleCooldownRemaining > 0.f)
	{
		return;
	}

	// One notch per step: without the throttle a single flick of the wheel jumps several slots.
	CycleCooldownRemaining = CycleCooldown;
	const ESlimeElement Next = Element->CycleElement(Axis > 0.f ? 1 : -1);

	if (WheelWidget)
	{
		WheelWidget->SetHighlightedElement(Next);
	}
}

void USlimeAbilityComponent::OpenWheel()
{
	if (bWheelOpen || !Element)
	{
		if (!Element)
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("SlimeAbilityComponent: cannot open element wheel — no SlimeElementComponent."));
		}
		return;
	}
	if (const USlimeDevourComponent* Devour = GetOwner() ? GetOwner()->FindComponentByClass<USlimeDevourComponent>() : nullptr)
	{
		if (Devour->IsPhantomWheelOpen())
		{
			return;
		}
	}

	if (!WheelWidget)
	{
		APlayerController* PlayerController = GetOwningPlayerController();
		if (!PlayerController)
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("SlimeAbilityComponent: cannot open element wheel — pawn has no PlayerController."));
			return;
		}

		UClass* WidgetClass = WheelWidgetClass;
		if (!WidgetClass && !WheelWidgetClassPath.IsNull())
		{
			WidgetClass = WheelWidgetClassPath.LoadSynchronous();
		}
		if (!WidgetClass)
		{
			// No Blueprint shell authored: the pure C++ widget builds its own layout.
			WidgetClass = USlimeElementWheelWidget::StaticClass();
		}

		WheelWidget = CreateWidget<USlimeElementWheelWidget>(PlayerController, WidgetClass);
		if (!WheelWidget)
		{
			return;
		}
		WheelWidget->SetElementComponent(Element);
	}

	bWheelOpen = true;
	WheelWidget->SetHighlightedElement(Element->GetPreviewElement());
	if (!WheelWidget->IsInViewport())
	{
		WheelWidget->AddToViewport(50);
	}
	WheelWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (bSlowTimeWhileWheelOpen)
	{
		if (UWorld* World = GetWorld())
		{
			SavedTimeDilation = UGameplayStatics::GetGlobalTimeDilation(World);
			UGameplayStatics::SetGlobalTimeDilation(World, WheelTimeDilation);
		}
	}
}

void USlimeAbilityComponent::CloseWheel(bool bCommit)
{
	if (!bWheelOpen)
	{
		return;
	}
	bWheelOpen = false;

	if (WheelWidget)
	{
		WheelWidget->SetVisibility(ESlateVisibility::Collapsed);
		WheelWidget->RemoveFromParent();
	}

	if (Element)
	{
		if (bCommit)
		{
			Element->CommitPreview();
		}
		else
		{
			Element->CancelPreview();
		}
	}

	if (bSlowTimeWhileWheelOpen)
	{
		if (UWorld* World = GetWorld())
		{
			UGameplayStatics::SetGlobalTimeDilation(World, SavedTimeDilation);
		}
	}
}
