// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeAbilityComponent.h"

#include "Blueprint/UserWidget.h"
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
#include "SlimeElementComponent.h"
#include "SlimeFable.h"
#include "UI/SlimeElementWheelWidget.h"

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
		if (bDrawTrajectoryPreview)
		{
			DrawTrajectory();
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

	// Drive Body/UI directly so Enhanced Input handlers can hard-return while polling is on.
	// Z = flatten, X = absorb/recall, T = reset (was E / F / R).
	const bool bFlatten = PlayerController->IsInputKeyDown(EKeys::Z);
	if (bFlatten != bPollFlattenDown)
	{
		bPollFlattenDown = bFlatten;
		if (Body)
		{
			Body->SetSpread(bFlatten);
		}
	}

	if (PlayerController->WasInputKeyJustPressed(EKeys::T) && Body)
	{
		Body->ResetBody();
	}

	const bool bAbsorb = PlayerController->IsInputKeyDown(EKeys::X);
	bPollAbsorbDown = bAbsorb;
	if (Body)
	{
		Body->SetRecalling(bAbsorb);
	}

	const bool bLaunch = PlayerController->IsInputKeyDown(EKeys::Q);
	if (bLaunch && !bPollLaunchDown)
	{
		bPollLaunchDown = true;
		bCharging = true;
		ChargeElapsed = 0.f;
	}
	else if (!bLaunch && bPollLaunchDown)
	{
		bPollLaunchDown = false;
		if (bCharging)
		{
			bCharging = false;
			FVector Direction;
			if (Body && GetAimDirection(Direction))
			{
				const float Speed = FMath::Lerp(MinLaunchSpeed, MaxLaunchSpeed, GetLaunchCharge());
				Body->LaunchChunk(Direction * Speed);
			}
			ChargeElapsed = 0.f;
		}
	}

	const bool bWheel = PlayerController->IsInputKeyDown(EKeys::Tab);
	if (bWheel && !bPollWheelDown)
	{
		bPollWheelDown = true;
		OpenWheel();
	}
	else if (!bWheel && bPollWheelDown)
	{
		bPollWheelDown = false;
		CloseWheel(true);
	}

	if (bWheelOpen && Element && CycleCooldownRemaining <= 0.f)
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

		if (Step != 0)
		{
			CycleCooldownRemaining = CycleCooldown;
			const ESlimeElement Next = Element->CycleElement(Step);
			if (WheelWidget)
			{
				WheelWidget->SetHighlightedElement(Next);
			}
		}
	}

	(void)DeltaTime;
}

float USlimeAbilityComponent::GetLaunchCharge() const
{
	return FMath::Clamp(ChargeElapsed / FMath::Max(FullChargeTime, KINDA_SMALL_NUMBER), 0.f, 1.f);
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
	bCharging = true;
	ChargeElapsed = 0.f;
}

void USlimeAbilityComponent::HandleLaunchCompleted()
{
	if (bPollAbilityKeys)
	{
		return;
	}
	if (!bCharging)
	{
		return;
	}
	bCharging = false;

	FVector Direction;
	if (!Body || !GetAimDirection(Direction))
	{
		ChargeElapsed = 0.f;
		return;
	}

	const float Speed = FMath::Lerp(MinLaunchSpeed, MaxLaunchSpeed, GetLaunchCharge());
	const int32 Launched = Body->LaunchChunk(Direction * Speed);
	ChargeElapsed = 0.f;

	if (Launched == 0)
	{
		UE_LOG(LogSlimeFable, Verbose, TEXT("Slime launch rejected: active shot limit or clone pool full."));
	}
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

void USlimeAbilityComponent::DrawTrajectory() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	FVector Direction;
	if (!World || !Body || !GetAimDirection(Direction))
	{
		return;
	}

	const float Speed = FMath::Lerp(MinLaunchSpeed, MaxLaunchSpeed, GetLaunchCharge());
	const FVector Velocity = Direction * Speed;
	const double Gravity = double(Body->SolverParams.Gravity);

	const FVector Origin = Body->GetBlobCenter();
	const FLinearColor Tint = Element ? Element->GetProfile(Element->GetPreviewElement()).BaseColor : FLinearColor::White;
	const FColor Color = Tint.ToFColor(true);

	constexpr int32 Samples = 22;
	constexpr double Step = 0.055;
	FVector Previous = Origin;
	for (int32 Sample = 1; Sample <= Samples; ++Sample)
	{
		const double Time = Step * Sample;
		const FVector Point = Origin + Velocity * Time + FVector(0.0, 0.0, 0.5 * Gravity * Time * Time);
		DrawDebugLine(World, Previous, Point, Color, false, -1.f, 0, 2.f);
		Previous = Point;
	}
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
