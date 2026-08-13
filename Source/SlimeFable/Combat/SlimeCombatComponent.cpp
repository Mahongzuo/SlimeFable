// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeCombatComponent.h"

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
#include "SlimeCombatHUDWidget.h"
#include "SlimeElementComponent.h"
#include "SlimeHitProbe.h"
#include "SlimeHealthComponent.h"
#include "SlimeLockOnComponent.h"
#include "SlimeSkillProjectile.h"
#include "Blueprint/UserWidget.h"

USlimeCombatComponent::USlimeCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	FMemory::Memzero(SkillCd, sizeof(SkillCd));
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

void USlimeCombatComponent::HandleAttack() { TryComboAttack(); }
void USlimeCombatComponent::HandleSkill1() { TrySkill(ESlimeSkillSlot::Skill1); }
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

AActor* USlimeCombatComponent::FindNearestHostile(float MaxRange) const
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || MaxRange <= 0.f)
	{
		return nullptr;
	}

	const FVector Origin = GetBlobOrigin();
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
	if (AActor* Target = FindNearestHostile(SeekRange))
	{
		return Target->GetActorLocation();
	}
	return ResolveGroundPoint(400.f);
}

FVector USlimeCombatComponent::ResolveSkillHitOrigin(const FSlimeSkillDef& Def) const
{
	const bool bTargetedStrike =
		(Def.Element == ESlimeElement::Lightning && Def.Exec == ESlimeSkillExec::AoE
			&& (Def.Slot == ESlimeSkillSlot::Skill1 || Def.Slot == ESlimeSkillSlot::Skill3))
		|| (Def.Element == ESlimeElement::Dark && Def.Slot == ESlimeSkillSlot::Skill3)
		|| (Def.Slot == ESlimeSkillSlot::Combo4);

	if (bTargetedStrike)
	{
		return ResolveFinisherLocation(1000.f);
	}

	if (Def.Exec == ESlimeSkillExec::AoE)
	{
		if (LockOn && LockOn->IsLockedOn())
		{
			if (AActor* Target = LockOn->GetLockedTarget())
			{
				return Target->GetActorLocation();
			}
		}
		if (AActor* Near = FindNearestHostile(FMath::Max(Def.Hit.Radius, 400.f)))
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
	return true;
}

void USlimeCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TickCooldowns(DeltaTime);

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
		PollCombatKeys();
	}

	if (bAttacking)
	{
		TickAction(DeltaTime);
	}
}

void USlimeCombatComponent::PollCombatKeys()
{
	APlayerController* PC = GetPlayerController();
	if (!PC)
	{
		return;
	}

	if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		TryComboAttack();
	}
	if (PC->WasInputKeyJustPressed(EKeys::One))
	{
		TrySkill(ESlimeSkillSlot::Skill1);
	}
	if (PC->WasInputKeyJustPressed(EKeys::Two))
	{
		TrySkill(ESlimeSkillSlot::Skill2);
	}
	if (PC->WasInputKeyJustPressed(EKeys::Three))
	{
		TrySkill(ESlimeSkillSlot::Skill3);
	}
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

void USlimeCombatComponent::TickCooldowns(float DeltaTime)
{
	for (int32 Index = 0; Index < 18; ++Index)
	{
		SkillCd[Index] = FMath::Max(SkillCd[Index] - DeltaTime, 0.f);
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
			ActiveHitOrigin = ResolveFinisherLocation(1000.f);
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
			|| (Def.Element == ESlimeElement::Dark && Def.Slot == ESlimeSkillSlot::Skill3);
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

	const bool bComboFinisherVfx = bFromCombo && Def.Slot == ESlimeSkillSlot::Combo4 && !Def.NiagaraSystem.IsNull();
	const bool bTargetedSkillVfx = !bFromCombo && bUseExplicitHitOrigin && Def.Exec != ESlimeSkillExec::Projectile;
	if ((!bFromCombo && Def.Exec != ESlimeSkillExec::Projectile) || bComboFinisherVfx)
	{
		FVector VfxLoc = ActiveHitOrigin;
		if (bComboFinisherVfx || bTargetedSkillVfx)
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

	if (AActor* Near = FindNearestHostile(300.f))
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
		Hits = USlimeHitProbe::PerformHit(GetOwner(), HitDef, Origin, ActiveForward, AlreadyHit);
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
		Projectile->InitProjectile(GetOwner(), Def, Aim * Def.ProjectileSpeed);
	}
}

void USlimeCombatComponent::ExecuteChain(const FSlimeSkillDef& Def, const FVector& Origin, const FVector& Forward)
{
	USlimeHitProbe::PerformHit(GetOwner(), Def, Origin, Forward, AlreadyHit);

	UWorld* World = GetWorld();
	if (!World)
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

void USlimeCombatComponent::SpawnVfx(const FSlimeSkillDef& Def, const FVector& Location) const
{
	UNiagaraSystem* System = Def.NiagaraSystem.LoadSynchronous();
	if (!System || !GetOwner())
	{
		return;
	}

	UNiagaraComponent* FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetOwner(),
		System,
		Location,
		ActiveAim.Rotation(),
		FVector(1.f),
		true,
		true);
	if (!FX)
	{
		return;
	}

	FX->SetAutoDestroy(true);
	const float KillAfter = FMath::Clamp(Def.HitEnd + Def.Recovery + 0.85f, 1.1f, 3.2f);
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
}
