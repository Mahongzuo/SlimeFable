// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeCombatComponent.h"
#include "SlimePathSwordComponent.h"

#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "SlimeAbilityComponent.h"
#include "SlimeBodyComponent.h"
#include "SlimeCombatCatalog.h"
#include "SlimeCharacter.h"
#include "EnemyCharacter.h"
#include "SlimeCombatHUDWidget.h"
#include "SlimeElementComponent.h"
#include "SlimeDevourComponent.h"
#include "SlimeDodgeComponent.h"
#include "SlimeHitProbe.h"
#include "SlimeHealthComponent.h"
#include "SlimeLockOnComponent.h"
#include "SlimeMorphComponent.h"
#include "SlimeFable.h"
#include "SlimeSkillProjectile.h"
#include "SlimeSkillVfxSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Settings/SlimeInputSettings.h"
#include "Settings/SlimeInputTypes.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"
#include "SlimeVehicleComponent.h"
#include "Inventory/SlimePlacementComponent.h"
#include "SlimeDodgeComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Settings/SlimeAudioPlay.h"

namespace SlimeCombatAudio
{
	static const TCHAR* DefaultComboSlash = TEXT("/Game/Audio/SFX/Combat/sfx_attack_01.sfx_attack_01");
	static const TCHAR* DefaultFinisher = TEXT("/Game/Audio/SFX/Combat/sfx_finisher_01.sfx_finisher_01");

	const TCHAR* DefaultSkillForElement(ESlimeElement /*Element*/)
	{
		return DefaultFinisher;
	}
	USoundBase* ResolveSound(const TSoftObjectPtr<USoundBase>& Soft, const TCHAR* FallbackPath)
	{
		if (!Soft.IsNull())
		{
			if (USoundBase* Loaded = Soft.LoadSynchronous())
			{
				return Loaded;
			}
		}
		return LoadObject<USoundBase>(nullptr, FallbackPath);
	}

	USoundBase* ResolveElementSkillSound(
		const TArray<TSoftObjectPtr<USoundBase>>& Overrides,
		ESlimeElement Element)
	{
		const int32 Index = static_cast<int32>(Element);
		if (Overrides.IsValidIndex(Index) && !Overrides[Index].IsNull())
		{
			if (USoundBase* Loaded = Overrides[Index].LoadSynchronous())
			{
				return Loaded;
			}
		}
		return LoadObject<USoundBase>(nullptr, DefaultSkillForElement(Element));
	}
}

USlimeCombatComponent::USlimeCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	FMemory::Memzero(SkillCd, sizeof(SkillCd));
	ComboAttackSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(SlimeCombatAudio::DefaultComboSlash));
	FinisherSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(SlimeCombatAudio::DefaultFinisher));
}

void USlimeCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		Body = Owner->FindComponentByClass<USlimeBodyComponent>();
		Element = Owner->FindComponentByClass<USlimeElementComponent>();
		Abilities = Owner->FindComponentByClass<USlimeAbilityComponent>();
		LockOn = Owner->FindComponentByClass<USlimeLockOnComponent>();
		Devour = Owner->FindComponentByClass<USlimeDevourComponent>();
		Placement = Owner->FindComponentByClass<USlimePlacementComponent>();
		Vehicle = Owner->FindComponentByClass<USlimeVehicleComponent>();
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsPlayerControlled() && GetPlayerController())
	{
		TSubclassOf<USlimeCombatHUDWidget> ClassToSpawn = HUDWidgetClass;
		if (!ClassToSpawn)
		{
			ClassToSpawn = USlimeCombatHUDWidget::StaticClass();
		}
		HUDWidget = CreateWidget<USlimeCombatHUDWidget>(GetPlayerController(), ClassToSpawn);
		if (HUDWidget)
		{
			HUDWidget->SetCombat(this);
			HUDWidget->AddToViewport(20);
		}
	}
}

void USlimeCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void USlimeCombatComponent::BindInput(UEnhancedInputComponent* EnhancedInput)
{
	if (!EnhancedInput)
	{
		return;
	}
	if (AttackAction)
	{
		EnhancedInput->BindAction(AttackAction, ETriggerEvent::Started, this, &USlimeCombatComponent::HandleAttack);
	}
	if (Skill1Action)
	{
		EnhancedInput->BindAction(Skill1Action, ETriggerEvent::Started, this, &USlimeCombatComponent::HandleSkill1);
	}
	if (Skill2Action)
	{
		EnhancedInput->BindAction(Skill2Action, ETriggerEvent::Started, this, &USlimeCombatComponent::HandleSkill2);
	}
	if (Skill3Action)
	{
		EnhancedInput->BindAction(Skill3Action, ETriggerEvent::Started, this, &USlimeCombatComponent::HandleSkill3);
	}
}

void USlimeCombatComponent::HandleAttack()
{
	if (bPollCombatKeys)
	{
		return;
	}
	TryComboAttack();
}
void USlimeCombatComponent::HandleSkill1()
{
	if (bPollCombatKeys)
	{
		return;
	}
	TrySkill(ESlimeSkillSlot::Skill1);
}
void USlimeCombatComponent::HandleSkill2() { TrySkill(ESlimeSkillSlot::Skill2); }
void USlimeCombatComponent::HandleSkill3() { TrySkill(ESlimeSkillSlot::Skill3); }

APlayerController* USlimeCombatComponent::GetPlayerController() const
{
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		return Cast<APlayerController>(Pawn->GetController());
	}
	return nullptr;
}

FSlimeElementKitData USlimeCombatComponent::GetCurrentKit() const
{
	const ESlimeElement Current = Element ? Element->CurrentElement : ESlimeElement::Water;
	if (Catalog)
	{
		return Catalog->GetKit(Current);
	}
	return USlimeCombatCatalog::GetBuiltinKit(Current);
}

FVector USlimeCombatComponent::GetBlobOrigin() const
{
	if (Body)
	{
		return Body->GetBlobCenter();
	}
	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

FVector USlimeCombatComponent::GetAimDirection() const
{
	const FVector Origin = GetBlobOrigin();
	if (LockOn && LockOn->IsLockedOn())
	{
		if (AActor* Target = LockOn->GetLockedTarget())
		{
			FVector Dir = (Target->GetActorLocation() + FVector(0.f, 0.f, 40.f)) - Origin;
			if (!Dir.IsNearlyZero())
			{
				return Dir.GetSafeNormal();
			}
		}
	}

	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (const AController* Controller = Pawn->GetController())
		{
			const FVector Dir = Controller->GetControlRotation().Vector();
			if (!Dir.IsNearlyZero())
			{
				return Dir.GetSafeNormal();
			}
		}
		return Pawn->GetActorForwardVector();
	}
	return FVector::ForwardVector;
}

FVector USlimeCombatComponent::GetAimForward() const
{
	FVector Dir = GetAimDirection();
	Dir.Z = 0.f;
	if (!Dir.IsNearlyZero())
	{
		return Dir.GetSafeNormal();
	}
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		return Pawn->GetActorForwardVector();
	}
	return FVector::ForwardVector;
}

AActor* USlimeCombatComponent::GetLockedRestrictTarget() const
{
	if (!LockOn || !LockOn->IsLockedOn())
	{
		return nullptr;
	}
	AActor* Target = LockOn->GetLockedTarget();
	if (!Target)
	{
		return nullptr;
	}
	if (const USlimeHealthComponent* Health = Target->FindComponentByClass<USlimeHealthComponent>())
	{
		if (!Health->IsAlive())
		{
			return nullptr;
		}
	}
	return Target;
}

AActor* USlimeCombatComponent::FindNearestHostile(float MaxRange, float MinFacingDot) const
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || MaxRange <= 0.f)
	{
		return nullptr;
	}

	const FVector Origin = GetBlobOrigin();
	const FVector Forward = GetAimForward();
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeSeekHostile), false, Owner);
	World->OverlapMultiByObjectType(
		Overlaps,
		Origin,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(MaxRange),
		Params);

	AActor* Best = nullptr;
	float BestDistSq = MaxRange * MaxRange;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!Actor || !USlimeHitProbe::IsHostile(Owner, Actor))
		{
			continue;
		}
		if (const USlimeHealthComponent* Health = Actor->FindComponentByClass<USlimeHealthComponent>())
		{
			if (!Health->IsAlive())
			{
				continue;
			}
		}
		if (MinFacingDot > -1.f)
		{
			FVector ToTarget = Actor->GetActorLocation() - Origin;
			ToTarget.Z = 0.f;
			if (ToTarget.IsNearlyZero() || FVector::DotProduct(Forward, ToTarget.GetSafeNormal()) < MinFacingDot)
			{
				continue;
			}
		}
		const float DistSq = FVector::DistSquared(Origin, Actor->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Actor;
		}
	}
	return Best;
}

FVector USlimeCombatComponent::ResolveGroundPoint(float ForwardCm) const
{
	const FVector Origin = GetBlobOrigin();
	const FVector Aim = GetAimDirection();
	const FVector Horizontal = GetAimForward();
	const FVector Fallback = Origin + Horizontal * ForwardCm;

	UWorld* World = GetWorld();
	if (!World)
	{
		return Fallback;
	}

	const FVector Start = Origin + FVector(0.f, 0.f, 50.f);
	const FVector End = Start + Aim * FMath::Max(ForwardCm, 100.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeSkillGround), false, GetOwner());
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return Hit.ImpactPoint;
	}

	const FVector Probe = Origin + Horizontal * ForwardCm + FVector(0.f, 0.f, 200.f);
	if (World->LineTraceSingleByChannel(Hit, Probe, Probe - FVector(0.f, 0.f, 600.f), ECC_Visibility, Params))
	{
		return Hit.ImpactPoint;
	}
	return Fallback;
}

FVector USlimeCombatComponent::ResolveFinisherLocation(float SeekRange) const
{
	if (AActor* Locked = GetLockedRestrictTarget())
	{
		if (FVector::DistSquared(GetBlobOrigin(), Locked->GetActorLocation()) <= FMath::Square(SeekRange))
		{
			return Locked->GetActorLocation();
		}
	}
	if (AActor* Target = FindNearestHostile(SeekRange, 0.25f))
	{
		return Target->GetActorLocation();
	}
	return ResolveGroundPoint(100.f);
}

FVector USlimeCombatComponent::ResolveSkillHitOrigin(const FSlimeSkillDef& Def) const
{
	if (Def.Element == ESlimeElement::Physical && Def.Slot == ESlimeSkillSlot::Skill3)
	{
		// Arrow rain is authored vertically; place it in front of the slime and
		// let the hit probe use the same ground-centered origin.
		return ResolveGroundPoint(200.f);
	}

	if (AActor* Locked = GetLockedRestrictTarget())
	{
		if (FVector::DistSquared(GetBlobOrigin(), Locked->GetActorLocation()) <= FMath::Square(300.f))
		{
			return Locked->GetActorLocation();
		}
	}
	if (AActor* Near = FindNearestHostile(300.f, -1.f))
	{
		return Near->GetActorLocation();
	}

	const bool bTargetedStrike =
		(Def.Element == ESlimeElement::Lightning && Def.Exec == ESlimeSkillExec::AoE
			&& (Def.Slot == ESlimeSkillSlot::Skill1 || Def.Slot == ESlimeSkillSlot::Skill3))
		|| (Def.Element == ESlimeElement::Dark && Def.Slot == ESlimeSkillSlot::Skill3)
		|| (Def.Slot == ESlimeSkillSlot::Combo4);

	if (bTargetedStrike)
	{
		return ResolveFinisherLocation(FinisherSeekRange);
	}

	if (Def.Exec == ESlimeSkillExec::AoE)
	{
		if (AActor* Near = FindNearestHostile(FMath::Max(Def.Hit.Radius, 400.f), 0.15f))
		{
			return Near->GetActorLocation();
		}
		return ResolveGroundPoint(400.f);
	}

	return USlimeHitProbe::ResolveOrigin(GetOwner(), Def.Hit, GetAimForward());
}

bool USlimeCombatComponent::CanStartAction() const
{
	if (Abilities && (Abilities->IsWheelOpen() || Abilities->IsChargingLaunch()))
	{
		return false;
	}
	if (Body && Body->IsSpreading())
	{
		return false;
	}
	if (Devour && (Devour->IsCombatLocked() || Devour->IsPhantomWheelOpen()))
	{
		return false;
	}
	if (Placement && Placement->IsPlacing())
	{
		return false;
	}
	return true;
}

void USlimeCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TickCooldowns(DeltaTime);
	TickDamageBuff(DeltaTime);

	if (bComboOpen && !bAttacking)
	{
		ComboResetRemaining -= DeltaTime;
		if (ComboResetRemaining <= 0.f)
		{
			bComboOpen = false;
			ComboIndex = 0;
		}
	}

	if (!CanStartAction() && bAttacking)
	{
		InterruptCombat();
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (bPollCombatKeys && Pawn && Pawn->IsPlayerControlled())
	{
		PollCombatKeys(DeltaTime);
	}

	if (bAttacking)
	{
		TickAction(DeltaTime);
	}
}

void USlimeCombatComponent::PollCombatKeys(float DeltaTime)
{
	APlayerController* PC = GetPlayerController();
	if (!PC)
	{
		return;
	}

	// Lock combat inputs while a morph sequence is running or the player is morphed.
	if (USlimeMorphComponent* MorphComp = GetOwner() ? GetOwner()->FindComponentByClass<USlimeMorphComponent>() : nullptr)
	{
		if (MorphComp->IsMorphing() || MorphComp->IsMorphed())
		{
			return;
		}
	}

	const USlimeInputSettings* InputSettings = nullptr;
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			InputSettings = GI->GetSubsystem<USlimeInputSettings>();
		}
	}

	auto WasPressed = [PC, InputSettings](ESlimeInputAction Action, const FKey& Fallback) -> bool
	{
		if (InputSettings)
		{
			return InputSettings->WasKeyPressed(PC, Action);
		}
		return PC->WasInputKeyJustPressed(Fallback);
	};
	auto IsDown = [PC, InputSettings](ESlimeInputAction Action, const FKey& Fallback) -> bool
	{
		if (InputSettings)
		{
			return InputSettings->IsKeyDown(PC, Action);
		}
		return PC->IsInputKeyDown(Fallback);
	};

	if (WasPressed(ESlimeInputAction::Attack, EKeys::LeftMouseButton))
	{
		TryComboAttack();
	}

	USlimeDevourComponent* DevourComp = Devour.Get();
	bool bBlockSkill1Hold = false;
	if (Vehicle && Vehicle->IsUsingVehicle())
	{
		bBlockSkill1Hold = true;
	}
	if (Abilities && (Abilities->IsWheelOpen() || Abilities->IsChargingLaunch()))
	{
		bBlockSkill1Hold = true;
	}

	const bool bSkill1Down = !bBlockSkill1Hold && IsDown(ESlimeInputAction::Skill1, EKeys::Q);
	if (bSkill1Down)
	{
		if (!bPollSkill1Down)
		{
			bPollSkill1Down = true;
			Skill1HoldSeconds = 0.f;
			bPhantomWheelOpenedThisHold = false;
		}
		Skill1HoldSeconds += DeltaTime;
		if (DevourComp && !bPhantomWheelOpenedThisHold && Skill1HoldSeconds >= DevourComp->PhantomWheelHoldSeconds)
		{
			if (DevourComp->TryOpenPhantomWheel())
			{
				bPhantomWheelOpenedThisHold = true;
			}
		}
		if (DevourComp && bPhantomWheelOpenedThisHold)
		{
			DevourComp->TickPhantomWheelInput();
		}
	}
	else if (bPollSkill1Down)
	{
		bPollSkill1Down = false;
		if (DevourComp && bPhantomWheelOpenedThisHold)
		{
			DevourComp->ClosePhantomWheel(true);
		}
		else
		{
			TrySkill(ESlimeSkillSlot::Skill1);
		}
		Skill1HoldSeconds = 0.f;
		bPhantomWheelOpenedThisHold = false;
	}

	if (WasPressed(ESlimeInputAction::Skill2, EKeys::E))
	{
		TrySkill(ESlimeSkillSlot::Skill2);
	}
	if (WasPressed(ESlimeInputAction::Skill3, EKeys::R))
	{
		TrySkill(ESlimeSkillSlot::Skill3);
	}
}

float USlimeCombatComponent::GetSkill1HoldFraction() const
{
	if (!bPollSkill1Down || bPhantomWheelOpenedThisHold)
	{
		return 0.f;
	}
	float Hold = 0.35f;
	if (const USlimeDevourComponent* DevourComp = Devour.Get())
	{
		Hold = FMath::Max(DevourComp->PhantomWheelHoldSeconds, 0.05f);
		if (DevourComp->GetPhantomSlotCount() <= 0)
		{
			return 0.f;
		}
	}
	return FMath::Clamp(Skill1HoldSeconds / Hold, 0.f, 1.f);
}

bool USlimeCombatComponent::TryComboAttack()
{
	if (!CanStartAction())
	{
		return false;
	}

	if (bAttacking)
	{
		if (SlimeCombat::IsComboSlot(ActiveDef.Slot) && ActionElapsed >= ActiveDef.HitStart)
		{
			bComboQueued = true;
			return true;
		}
		return false;
	}

	const FSlimeElementKitData Kit = GetCurrentKit();
	int32 NextIndex = 0;
	if (bComboOpen && ComboResetRemaining > 0.f)
	{
		NextIndex = ComboIndex;
		if (NextIndex >= Kit.Combos.Num())
		{
			NextIndex = 0;
		}
	}

	const FSlimeSkillDef* Def = Kit.GetCombo(NextIndex);
	if (!Def)
	{
		return false;
	}
	return StartAction(*Def, true);
}

bool USlimeCombatComponent::TrySkill(ESlimeSkillSlot Slot)
{
	if (!SlimeCombat::IsComboSlot(Slot) && Slot != ESlimeSkillSlot::Skill1 && Slot != ESlimeSkillSlot::Skill2 && Slot != ESlimeSkillSlot::Skill3)
	{
		return false;
	}
	if (Vehicle && Vehicle->IsUsingVehicle())
	{
		return false;
	}
	if (!CanStartAction())
	{
		return false;
	}

	const FSlimeElementKitData Kit = GetCurrentKit();
	const FSlimeSkillDef* Def = Kit.GetSkillSlot(Slot);
	if (!Def)
	{
		return false;
	}
	if (GetSkillCooldownRemaining(Slot) > 0.f)
	{
		return false;
	}

	if (bAttacking)
	{
		InterruptCombat();
	}

	if (!StartAction(*Def, false))
	{
		return false;
	}

	const ESlimeElement Current = Element ? Element->CurrentElement : ESlimeElement::Water;
	SkillCd[SkillCdIndex(Current, Slot)] = Def->Cooldown;
	return true;
}

int32 USlimeCombatComponent::SkillCdIndex(ESlimeElement InElement, ESlimeSkillSlot Slot) const
{
	int32 SlotIndex = 0;
	if (Slot == ESlimeSkillSlot::Skill2)
	{
		SlotIndex = 1;
	}
	else if (Slot == ESlimeSkillSlot::Skill3)
	{
		SlotIndex = 2;
	}
	return SlimeElement::ToIndex(InElement) * 3 + SlotIndex;
}

float USlimeCombatComponent::GetSkillCooldownRemaining(ESlimeSkillSlot Slot) const
{
	if (Slot != ESlimeSkillSlot::Skill1 && Slot != ESlimeSkillSlot::Skill2 && Slot != ESlimeSkillSlot::Skill3)
	{
		return 0.f;
	}
	const ESlimeElement Current = Element ? Element->CurrentElement : ESlimeElement::Water;
	return SkillCd[SkillCdIndex(Current, Slot)];
}

void USlimeCombatComponent::ReduceSkillCooldowns(float Seconds)
{
	if (Seconds <= 0.f)
	{
		return;
	}
	for (int32 Index = 0; Index < 18; ++Index)
	{
		SkillCd[Index] = FMath::Max(SkillCd[Index] - Seconds, 0.f);
	}
}

void USlimeCombatComponent::ReduceSkillCooldownsPercent(float Percent)
{
	const float Clamped = FMath::Clamp(Percent, 0.f, 1.f);
	if (Clamped <= 0.f)
	{
		return;
	}
	for (int32 Index = 0; Index < 18; ++Index)
	{
		SkillCd[Index] *= (1.f - Clamped);
	}
}

float USlimeCombatComponent::ResolveOutgoingDamage(const FSlimeSkillDef& Skill) const
{
	return FMath::Max((AttackPower * Skill.AtkScale + Skill.Damage) * OutgoingDamageMul, 0.f);
}

void USlimeCombatComponent::ApplyOutgoingDamageMul(float Mul, float DurationSeconds)
{
	OutgoingDamageMul = FMath::Max(Mul, 0.f);
	DamageBuffRemaining = FMath::Max(DurationSeconds, 0.f);
	if (DamageBuffRemaining <= 0.f)
	{
		OutgoingDamageMul = 1.f;
	}
}

void USlimeCombatComponent::TickCooldowns(float DeltaTime)
{
	for (int32 Index = 0; Index < 18; ++Index)
	{
		SkillCd[Index] = FMath::Max(SkillCd[Index] - DeltaTime, 0.f);
	}
}

void USlimeCombatComponent::TickDamageBuff(float DeltaTime)
{
	if (DamageBuffRemaining <= 0.f)
	{
		return;
	}
	DamageBuffRemaining = FMath::Max(DamageBuffRemaining - DeltaTime, 0.f);
	if (DamageBuffRemaining <= 0.f)
	{
		OutgoingDamageMul = 1.f;
	}
}

bool USlimeCombatComponent::StartAction(const FSlimeSkillDef& Def, bool bFromCombo)
{
	ActiveDef = Def;
	ActiveAim = GetAimDirection();
	ActiveForward = GetAimForward();
	ActionElapsed = 0.f;
	bAttacking = true;
	bHitFired = false;
	bComboQueued = false;
	AlreadyHit.Reset();
	bUseExplicitHitOrigin = false;
	ActiveHitOrigin = GetBlobOrigin();

	bComboReturnHome = false;
	if (bFromCombo)
	{
		ComboIndex = SlimeCombat::ComboIndex(Def.Slot) + 1;
		bComboOpen = ComboIndex < 4;
		ComboResetRemaining = ComboResetDelay;
		ApplyComboLunge();
		if (Def.Slot == ESlimeSkillSlot::Combo4)
		{
			ActiveHitOrigin = ResolveFinisherLocation(FinisherSeekRange);
			bUseExplicitHitOrigin = true;
		}
	}
	else
	{
		bComboOpen = false;
		ComboIndex = 0;
		ActiveHitOrigin = ResolveSkillHitOrigin(Def);
		bUseExplicitHitOrigin = Def.Exec == ESlimeSkillExec::AoE
			|| Def.Element == ESlimeElement::Lightning
			|| (Def.Element == ESlimeElement::Dark && Def.Slot == ESlimeSkillSlot::Skill3)
			|| (Def.Element == ESlimeElement::Physical && Def.Slot == ESlimeSkillSlot::Skill3);
	}

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		const FRotator Rot = ActiveForward.Rotation();
		Pawn->SetActorRotation(FRotator(0.f, Rot.Yaw, 0.f));
	}

	if (Body)
	{
		Body->SetCombatPose(SlimeCombat::MakePose(Def.Pose, ActiveForward, 1.f));
	}

	if (Def.Exec == ESlimeSkillExec::Dash)
	{
		ExecuteDash(Def, ActiveForward);
	}
	else if (Def.Exec == ESlimeSkillExec::Projectile)
	{
		ExecuteProjectile(Def, ActiveAim);
		bHitFired = true;
	}

	PlayCombatSound(bFromCombo);

	if (bFromCombo)
	{
		if (USlimePathSwordComponent* PathSword = GetOwner()->FindComponentByClass<USlimePathSwordComponent>())
		{
			ESlimeElement SwingElement = Def.Element;
			if (Element)
			{
				SwingElement = Element->CurrentElement;
			}
			const float SwingDuration = FMath::Max(Def.HitEnd - Def.HitStart, 0.12f);
			PathSword->PlaySwing(
				SwingElement,
				SlimeCombat::ComboIndex(Def.Slot),
				ActiveForward,
				SwingDuration);
		}
	}

	const bool bTargetedSkillVfx = !bFromCombo && bUseExplicitHitOrigin && Def.Exec != ESlimeSkillExec::Projectile;
	if (!bFromCombo && Def.Exec != ESlimeSkillExec::Projectile)
	{
		FVector VfxLoc = ActiveHitOrigin;
		if (bTargetedSkillVfx)
		{
			VfxLoc = ActiveHitOrigin;
		}
		else if (Def.Exec == ESlimeSkillExec::Melee || Def.Exec == ESlimeSkillExec::Chain)
		{
			VfxLoc = USlimeHitProbe::ResolveOrigin(GetOwner(), Def.Hit, ActiveForward);
		}
		else if (Def.Exec == ESlimeSkillExec::Dash)
		{
			VfxLoc = GetBlobOrigin() + ActiveForward * 80.f;
		}
	SpawnVfx(Def, VfxLoc);
	}

	// AI / non-player attacks open the player's perfect-dodge window.
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (!OwnerPawn->IsPlayerControlled())
		{
			USlimeDodgeComponent::NotifyPlayerIncomingAttack(this, GetOwner());
		}
	}

	return true;
}

void USlimeCombatComponent::ApplyComboLunge()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	ComboHomeLocation = Owner->GetActorLocation();
	const FVector Origin = GetBlobOrigin();
	ComboLungeDistance = ActiveDef.DashDistance > 0.f ? ActiveDef.DashDistance : 100.f;

	if (AActor* Locked = GetLockedRestrictTarget())
	{
		FVector ToEnemy = Locked->GetActorLocation() - Origin;
		ToEnemy.Z = 0.f;
		const float Dist = ToEnemy.Size();
		if (!ToEnemy.IsNearlyZero() && Dist <= ComboLungeSeekRange)
		{
			ActiveForward = ToEnemy.GetSafeNormal();
			ActiveAim = (Locked->GetActorLocation() + FVector(0.f, 0.f, 30.f) - Origin).GetSafeNormal();
			ComboLungeDistance = FMath::Clamp(Dist * 0.55f, 90.f, 120.f);
		}
	}
	else if (AActor* Near = FindNearestHostile(ComboLungeSeekRange, 0.15f))
	{
		FVector ToEnemy = Near->GetActorLocation() - Origin;
		ToEnemy.Z = 0.f;
		if (!ToEnemy.IsNearlyZero())
		{
			ActiveForward = ToEnemy.GetSafeNormal();
			ActiveAim = (Near->GetActorLocation() + FVector(0.f, 0.f, 30.f) - Origin).GetSafeNormal();
			ComboLungeDistance = FMath::Clamp(ToEnemy.Size() * 0.55f, 90.f, 120.f);
		}
	}

	// Combo1-3: short lunge then return home. Combo4 keeps forward momentum / finisher VFX.
	const bool bReturnAfter = ActiveDef.Slot != ESlimeSkillSlot::Combo4;
	bComboReturnHome = bReturnAfter;

	if (ACharacter* Character = Cast<ACharacter>(Owner))
	{
		const float Duration = FMath::Max(ActiveDef.HitStart, 0.1f);
		const float Speed = ComboLungeDistance / Duration;
		Character->LaunchCharacter(ActiveForward * Speed + FVector(0.f, 0.f, 28.f), true, false);
	}
}

void USlimeCombatComponent::TickComboReturn(float DeltaTime)
{
	if (!bComboReturnHome || !GetOwner())
	{
		return;
	}
	if (ActionElapsed < ActiveDef.HitEnd)
	{
		return;
	}

	AActor* Owner = GetOwner();
	FVector Loc = Owner->GetActorLocation();
	const FVector Target(ComboHomeLocation.X, ComboHomeLocation.Y, Loc.Z);
	const float Alpha = FMath::Clamp((ActionElapsed - ActiveDef.HitEnd) / FMath::Max(ActiveDef.Recovery, 0.05f), 0.f, 1.f);
	Loc = FMath::Lerp(Loc, Target, FMath::Clamp(Alpha * 8.f * DeltaTime + 0.15f, 0.f, 1.f));
	Owner->SetActorLocation(Loc, true);

	if (ACharacter* Character = Cast<ACharacter>(Owner))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			FVector Vel = Movement->Velocity;
			Vel.X *= 0.35f;
			Vel.Y *= 0.35f;
			Movement->Velocity = Vel;
		}
	}
}

void USlimeCombatComponent::TickAction(float DeltaTime)
{
	ActionElapsed += DeltaTime;

	if (Body)
	{
		const float Peak = ActiveDef.HitStart > 0.f ? FMath::Clamp(ActionElapsed / ActiveDef.HitStart, 0.f, 1.f) : 1.f;
		const float Fade = ActiveDef.GetTotalDuration() > 0.f
			? 1.f - FMath::Clamp((ActionElapsed - ActiveDef.HitEnd) / FMath::Max(ActiveDef.Recovery, 0.05f), 0.f, 1.f)
			: 0.f;
		const float Strength = ActionElapsed < ActiveDef.HitEnd ? Peak : Fade;
		Body->SetCombatPose(SlimeCombat::MakePose(ActiveDef.Pose, ActiveForward, Strength));
	}

	TickComboReturn(DeltaTime);

	if (!bHitFired && ActionElapsed >= ActiveDef.HitStart)
	{
		FireHit();
		bHitFired = true;
	}

	if (ActionElapsed >= ActiveDef.GetTotalDuration())
	{
		FinishAction();
	}
}

void USlimeCombatComponent::FireHit()
{
	const FVector Origin = bUseExplicitHitOrigin
		? ActiveHitOrigin
		: USlimeHitProbe::ResolveOrigin(GetOwner(), ActiveDef.Hit, ActiveForward);
	int32 Hits = 0;
	AActor* Restrict = (ActiveDef.Exec == ESlimeSkillExec::AoE) ? nullptr : GetLockedRestrictTarget();
	if (ActiveDef.Exec == ESlimeSkillExec::Chain)
	{
		ExecuteChain(ActiveDef, Origin, ActiveForward);
		Hits = AlreadyHit.Num();
	}
	else if (ActiveDef.Exec != ESlimeSkillExec::Projectile)
	{
		FSlimeSkillDef HitDef = ActiveDef;
		if (bUseExplicitHitOrigin)
		{
			// Point strikes (combo finisher / dark stone) as a sphere at the resolved location.
			HitDef.Hit.Shape = ESlimeHitShape::Sphere;
			HitDef.Hit.Range = 0.f;
			HitDef.Hit.OriginForwardOffset = 0.f;
			HitDef.Hit.Radius = FMath::Max(HitDef.Hit.Radius, 100.f);
		}
		// Ensure combo / elemental strikes apply aura using the slime's live element.
		if (Element && Cast<ASlimeCharacter>(GetOwner()))
		{
			HitDef.Element = Element->CurrentElement;
			HitDef.bAppliesElementAura = true;
			HitDef.VfxColor = SlimeCombat::GetElementVfxColor(Element->CurrentElement);
		}
		Hits = USlimeHitProbe::PerformHit(
			GetOwner(), HitDef, Origin, ActiveForward, AlreadyHit, Restrict);
	}
	AwardResources(ActiveDef, Hits);
}

void USlimeCombatComponent::ExecuteDash(const FSlimeSkillDef& Def, const FVector& Forward)
{
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		FVector DashDir = Forward;
		DashDir.Z = 0.f;
		DashDir = DashDir.GetSafeNormal();
		if (DashDir.IsNearlyZero())
		{
			DashDir = Character->GetActorForwardVector();
		}
		const float Duration = FMath::Max(Def.HitEnd, 0.12f);
		const float Speed = Def.DashDistance / Duration;
		Character->LaunchCharacter(DashDir * Speed + FVector(0.f, 0.f, 40.f), true, false);
	}
}

void USlimeCombatComponent::ExecuteProjectile(const FSlimeSkillDef& Def, const FVector& Forward)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FVector Aim = Forward.GetSafeNormal();
	if (Aim.IsNearlyZero())
	{
		Aim = GetAimDirection();
	}
	// Flatten pitch slightly so the shot does not immediately bury into the floor.
	Aim.Z = FMath::Clamp(Aim.Z, -0.15f, 0.65f);
	Aim = Aim.GetSafeNormal();
	const FVector Origin = GetBlobOrigin() + Aim * 60.f + FVector(0.f, 0.f, 35.f);
	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.Instigator = Cast<APawn>(GetOwner());
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ASlimeSkillProjectile* Projectile = World->SpawnActor<ASlimeSkillProjectile>(Origin, Aim.Rotation(), Params))
	{
		Projectile->InitProjectile(GetOwner(), Def, Aim * Def.ProjectileSpeed, GetLockedRestrictTarget());
	}
}

void USlimeCombatComponent::ExecuteChain(const FSlimeSkillDef& Def, const FVector& Origin, const FVector& Forward)
{
	AActor* Restrict = GetLockedRestrictTarget();
	USlimeHitProbe::PerformHit(GetOwner(), Def, Origin, Forward, AlreadyHit, Restrict);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (Restrict && !bChainIgnoresLock)
	{
		return;
	}

	int32 Extra = Def.ChainCount - 1;
	FVector From = Origin;
	while (Extra > 0)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeChain), false, GetOwner());
		World->OverlapMultiByObjectType(
			Overlaps,
			From,
			FQuat::Identity,
			FCollisionObjectQueryParams(ECC_Pawn),
			FCollisionShape::MakeSphere(Def.ChainRange),
			Params);

		AActor* Next = nullptr;
		float BestDist = TNumericLimits<float>::Max();
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Actor = Overlap.GetActor();
			if (!Actor || AlreadyHit.Contains(Actor) || !USlimeHitProbe::IsHostile(GetOwner(), Actor))
			{
				continue;
			}
			const float Dist = FVector::DistSquared(From, Actor->GetActorLocation());
			if (Dist < BestDist)
			{
				BestDist = Dist;
				Next = Actor;
			}
		}
		if (!Next)
		{
			break;
		}

		FSlimeSkillDef Bolt = Def;
		Bolt.Hit.Shape = ESlimeHitShape::Sphere;
		Bolt.Hit.Radius = 60.f;
		Bolt.Hit.Range = 0.f;
		Bolt.Hit.OriginForwardOffset = 0.f;
		USlimeHitProbe::PerformHit(GetOwner(), Bolt, Next->GetActorLocation(), Forward, AlreadyHit);
		From = Next->GetActorLocation();
		--Extra;
	}
}

void USlimeCombatComponent::PlayCombatSound(bool bFromCombo) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	USoundBase* Sound = nullptr;
	const bool bCombo = bFromCombo || SlimeCombat::IsComboSlot(ActiveDef.Slot);
	if (bCombo && ActiveDef.Slot == ESlimeSkillSlot::Combo4)
	{
		Sound = SlimeCombatAudio::ResolveSound(FinisherSound, SlimeCombatAudio::DefaultFinisher);
	}
	else if (bCombo)
	{
		Sound = SlimeCombatAudio::ResolveSound(ComboAttackSound, SlimeCombatAudio::DefaultComboSlash);
	}
	else
	{
		const int32 ElemIdx = static_cast<int32>(ActiveDef.Element);
		if (ElementSkillSounds.IsValidIndex(ElemIdx) && !ElementSkillSounds[ElemIdx].IsNull())
		{
			Sound = ElementSkillSounds[ElemIdx].LoadSynchronous();
		}
		if (!Sound)
		{
			Sound = SlimeCombatAudio::ResolveSound(FinisherSound, SlimeCombatAudio::DefaultFinisher);
		}
	}

	if (Sound)
	{
		SlimeAudioPlay::PlaySfxAt(Owner, Sound, Owner->GetActorLocation());
	}
}

void USlimeCombatComponent::SpawnVfx(const FSlimeSkillDef& Def, const FVector& Location) const
{
	UNiagaraSystem* System = USlimeSkillVfxSubsystem::ResolveLoadedSystem(Def.NiagaraSystem, GetOwner());
	if (!System || !GetOwner())
	{
		return;
	}

	FRotator VfxRotation = FRotator::ZeroRotator;
	if (Def.Slot == ESlimeSkillSlot::Skill3
		&& (Def.Element == ESlimeElement::Dark || Def.Element == ESlimeElement::Water || Def.Element == ESlimeElement::Physical))
	{
		// Niagara assets use local +X as forward. Use the captured attack vector,
		// independent of actor or camera/world rotation conventions.
		VfxRotation = ActiveForward.Rotation();
	}
	else
	{
	switch (Def.VfxRotationPolicy)
	{
	case ESlimeVfxRotationPolicy::Owner:
		VfxRotation = GetOwner()->GetActorRotation();
		break;
	case ESlimeVfxRotationPolicy::World:
		break;
	case ESlimeVfxRotationPolicy::Aim:
	default:
		VfxRotation = ActiveAim.Rotation();
		break;
	}
	}
	const FVector VfxLocation = Location + VfxRotation.RotateVector(Def.VfxLocationOffset);

	UNiagaraComponent* FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetOwner(),
		System,
		VfxLocation,
		VfxRotation,
		Def.VfxScale,
		true,
		true);
	if (!FX)
	{
		return;
	}

	FX->SetAutoDestroy(true);
	FX->SetVariableLinearColor(TEXT("User.Color"), Def.VfxColor);
	FX->SetVariableLinearColor(TEXT("User.Tint"), Def.VfxColor);
	FX->SetVariableLinearColor(TEXT("User.ElementColor"), Def.VfxColor);

	const float KillAfter = FMath::Clamp(
		Def.VfxHardLifetime > 0.f ? FMath::Max(Def.VfxHardLifetime, Def.VfxMinVisibleTime) : 4.8f,
		0.05f,
		4.8f);
	if (KillAfter > 0.f)
	{
		if (UWorld* World = FX->GetWorld())
		{
			TWeakObjectPtr<UNiagaraComponent> WeakFX(FX);
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakFX]()
			{
				if (UNiagaraComponent* Comp = WeakFX.Get())
				{
					Comp->DeactivateImmediate();
					Comp->DestroyComponent();
				}
			}), KillAfter, false);
		}
	}
}

void USlimeCombatComponent::AwardResources(const FSlimeSkillDef& Def, int32 HitCount)
{
	if (HitCount <= 0)
	{
		return;
	}
	if (SlimeCombat::IsComboSlot(Def.Slot))
	{
		Resonance = FMath::Min(Resonance + 0.35f * HitCount, 3.f);
		Ultimate = FMath::Min(Ultimate + 4.f * HitCount, 100.f);
	}
	else if (Def.Slot == ESlimeSkillSlot::Skill1)
	{
		Resonance = FMath::Min(Resonance + 0.6f * HitCount, 3.f);
		Ultimate = FMath::Min(Ultimate + 12.f * HitCount, 100.f);
	}
	else if (Def.Slot == ESlimeSkillSlot::Skill2)
	{
		Ultimate = FMath::Min(Ultimate + 20.f, 100.f);
	}
}

void USlimeCombatComponent::FinishAction()
{
	if (bComboReturnHome && GetOwner())
	{
		FVector Loc = GetOwner()->GetActorLocation();
		Loc.X = ComboHomeLocation.X;
		Loc.Y = ComboHomeLocation.Y;
		GetOwner()->SetActorLocation(Loc, true);
	}
	bComboReturnHome = false;
	bAttacking = false;
	if (Body)
	{
		Body->ClearCombatPose();
	}

	if (SlimeCombat::IsComboSlot(ActiveDef.Slot))
	{
		if (ComboIndex >= 4)
		{
			bComboOpen = false;
			ComboIndex = 0;
			bComboQueued = false;
		}
		else
		{
			bComboOpen = true;
			ComboResetRemaining = ComboResetDelay;
			if (bComboQueued)
			{
				bComboQueued = false;
				TryComboAttack();
			}
		}
	}
}

void USlimeCombatComponent::InterruptCombat()
{
	bAttacking = false;
	bComboQueued = false;
	bComboOpen = false;
	bComboReturnHome = false;
	ComboIndex = 0;
	ComboResetRemaining = 0.f;
	if (Body)
	{
		Body->ClearCombatPose();
	}
	if (AActor* Owner = GetOwner())
	{
		if (USlimePathSwordComponent* PathSword = Owner->FindComponentByClass<USlimePathSwordComponent>())
		{
			PathSword->AbortSwing();
		}
	}
}
