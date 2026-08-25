// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeStatusComponent.h"

#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "SlimeCombatTypes.h"
#include "SlimeHealthComponent.h"
#include "SlimeHitProbe.h"
#include "SlimeSkillVfxSubsystem.h"
#include "EnemyCharacter.h"
#include "UI/SlimeFloatingTextWidget.h"
#include "UI/SlimeAuraMarkerWidget.h"
#include "Components/WidgetComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

USlimeStatusComponent::USlimeStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USlimeStatusComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureAuraMarker();
	RefreshAuraMarker();
}

void USlimeStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bool bChanged = false;
	const int32 Before = AuraRemaining.Num();
	TArray<ESlimeElement> Expired;
	for (TPair<ESlimeElement, float>& Pair : AuraRemaining)
	{
		Pair.Value -= DeltaTime;
		if (Pair.Value <= 0.f)
		{
			Expired.Add(Pair.Key);
		}
	}
	for (ESlimeElement Element : Expired)
	{
		AuraRemaining.Remove(Element);
	}
	if (Before != AuraRemaining.Num() || Expired.Num() > 0)
	{
		bChanged = true;
	}

	if (ReactionResidueRemaining > 0.f)
	{
		ReactionResidueRemaining = FMath::Max(ReactionResidueRemaining - DeltaTime, 0.f);
		if (ReactionResidueRemaining <= 0.f)
		{
			ClearReactionResidue();
			bChanged = true;
		}
	}

	if (bChanged)
	{
		RefreshAuraMarker();
		SyncOwnerAuraFlash();
	}
}

bool USlimeStatusComponent::HasAura(ESlimeElement Element) const
{
	const float* Remaining = AuraRemaining.Find(Element);
	return Remaining && *Remaining > 0.f;
}

ESlimeElement USlimeStatusComponent::GetPrimaryAura(bool& bHasAura) const
{
	bHasAura = false;
	ESlimeElement Best = ESlimeElement::Water;
	float BestTime = 0.f;
	for (const TPair<ESlimeElement, float>& Pair : AuraRemaining)
	{
		if (Pair.Value > BestTime)
		{
			BestTime = Pair.Value;
			Best = Pair.Key;
			bHasAura = true;
		}
	}
	return Best;
}

void USlimeStatusComponent::ClearAura(ESlimeElement Element)
{
	AuraRemaining.Remove(Element);
	RefreshAuraMarker();
	SyncOwnerAuraFlash();
}

void USlimeStatusComponent::ClearAllAuras()
{
	AuraRemaining.Reset();
	ClearReactionResidue();
	RefreshAuraMarker();
	SyncOwnerAuraFlash();
}

void USlimeStatusComponent::ClearReactionResidue()
{
	ReactionResidueRemaining = 0.f;
}

void USlimeStatusComponent::BeginReactionResidue(
	ESlimeElement A,
	ESlimeElement B,
	ESlimeReactionKind Kind,
	float Duration)
{
	AuraRemaining.Reset();
	ReactionResidueKind = Kind;
	ReactionResidueColor = FMath::Lerp(
		SlimeCombat::GetElementVfxColor(A),
		SlimeCombat::GetElementVfxColor(B),
		0.5f);
	ReactionResidueColor.A = 1.f;
	ReactionResidueRemaining = Duration;
}

void USlimeStatusComponent::SyncOwnerAuraFlash()
{
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	if (!Enemy)
	{
		return;
	}

	if (ReactionResidueRemaining > 0.f)
	{
		Enemy->PlayElementAuraFlashByColor(ReactionResidueColor, ReactionResidueRemaining);
		return;
	}

	bool bHas = false;
	const ESlimeElement Primary = GetPrimaryAura(bHas);
	if (bHas)
	{
		const float* Time = AuraRemaining.Find(Primary);
		const float Duration = Time ? *Time : 8.f;
		Enemy->PlayElementAuraFlash(Primary, Duration);
	}
	else
	{
		Enemy->ClearElementAuraFlash();
	}
}

void USlimeStatusComponent::EnsureAuraMarker()
{
	AActor* Owner = GetOwner();
	if (!Owner || AuraMarker)
	{
		return;
	}

	AuraMarker = NewObject<UWidgetComponent>(Owner, TEXT("SlimeAuraMarker"));
	if (!AuraMarker)
	{
		return;
	}
	AuraMarker->SetupAttachment(Owner->GetRootComponent());
	float Z = AuraMarkerZOffset;
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			Z = Capsule->GetScaledCapsuleHalfHeight() + 28.f;
		}
	}
	AuraMarker->SetRelativeLocation(FVector(0.f, 0.f, Z));
	AuraMarker->SetWidgetSpace(EWidgetSpace::Screen);
	AuraMarker->SetDrawAtDesiredSize(true);
	AuraMarker->SetPivot(FVector2D(0.5f, 1.f));
	AuraMarker->SetWidgetClass(USlimeAuraMarkerWidget::StaticClass());
	AuraMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AuraMarker->RegisterComponent();
	AuraMarker->InitWidget();
	AuraMarker->SetHiddenInGame(true);
	AuraMarker->SetVisibility(false);
}

void USlimeStatusComponent::RefreshAuraMarker()
{
	EnsureAuraMarker();
	if (!AuraMarker)
	{
		return;
	}

	USlimeAuraMarkerWidget* Marker = Cast<USlimeAuraMarkerWidget>(AuraMarker->GetWidget());
	if (!Marker)
	{
		AuraMarker->InitWidget();
		Marker = Cast<USlimeAuraMarkerWidget>(AuraMarker->GetWidget());
	}

	bool bShow = false;
	if (ReactionResidueRemaining > 0.f)
	{
		bShow = true;
		if (Marker)
		{
			Marker->SetReactionResidue(
				SlimeCombat::GetReactionDisplayName(ReactionResidueKind),
				ReactionResidueColor);
		}
	}
	else
	{
		bool bHas = false;
		const ESlimeElement Primary = GetPrimaryAura(bHas);
		bShow = bHas;
		if (Marker)
		{
			Marker->SetAura(Primary, bHas);
		}
	}

	AuraMarker->SetHiddenInGame(!bShow);
	AuraMarker->SetVisibility(bShow);
}

void USlimeStatusComponent::ApplyAura(ESlimeElement Element, AActor* Instigator, float Duration)
{
	// New element clears reaction residue (e.g. lightning after vaporize blend).
	if (ReactionResidueRemaining > 0.f)
	{
		ClearReactionResidue();
		AuraRemaining.Reset();
	}

	if (Element == ESlimeElement::Physical)
	{
		bool bHas = false;
		const ESlimeElement Existing = GetPrimaryAura(bHas);
		if (bHas)
		{
			TriggerReaction(Element, Existing, Instigator);
		}
		return;
	}

	TArray<ESlimeElement> Others;
	for (const TPair<ESlimeElement, float>& Pair : AuraRemaining)
	{
		if (Pair.Key != Element && Pair.Value > 0.f)
		{
			Others.Add(Pair.Key);
		}
	}

	if (Others.Num() > 0)
	{
		TriggerReaction(Element, Others[0], Instigator);
		return;
	}

	AuraRemaining.Add(Element, Duration);
	RefreshAuraMarker();
	SyncOwnerAuraFlash();
}

void USlimeStatusComponent::TriggerReaction(ESlimeElement Incoming, ESlimeElement Existing, AActor* Instigator)
{
	TArray<FSlimeReactionRow> Rows;
	SlimeCombat::FillDefaultReactions(Rows);

	const FSlimeReactionRow* Match = nullptr;
	for (const FSlimeReactionRow& Row : Rows)
	{
		const bool Direct = Row.First == Incoming && Row.Second == Existing;
		const bool Swap = Row.First == Existing && Row.Second == Incoming;
		if (Direct || Swap)
		{
			Match = &Row;
			break;
		}
	}
	if (!Match)
	{
		AuraRemaining.Add(Incoming, 8.f);
		RefreshAuraMarker();
		SyncOwnerAuraFlash();
		return;
	}

	ApplyReactionRow(*Match, Instigator);

	// Consume both, then hang blended residue for 8s.
	AuraRemaining.Reset();
	BeginReactionResidue(Match->First, Match->Second, Match->Kind, 8.f);
	RefreshAuraMarker();
	SyncOwnerAuraFlash();
}

void USlimeStatusComponent::ApplyReactionRow(const FSlimeReactionRow& Row, AActor* Instigator)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (USlimeHealthComponent* Health = Owner->FindComponentByClass<USlimeHealthComponent>())
	{
		const FVector Impulse = FVector::UpVector * (Row.Kind == ESlimeReactionKind::Overload ? 420.f : 80.f);
		Health->ApplyDamage(Row.ExtraDamage, Instigator, Owner->GetActorLocation(), Impulse);
	}

	{
		const FString Line = FString::Printf(
			TEXT("元素反应：%s\n+%.0f"),
			*SlimeCombat::GetReactionDisplayName(Row.Kind).ToString(),
			Row.ExtraDamage);
		USlimeFloatingTextWidget::Spawn(
			Owner,
			Owner->GetActorLocation() + FVector(0.f, 0.f, 90.f),
			FText::FromString(Line),
			FLinearColor(1.f, 0.82f, 0.35f, 1.f),
			true);
	}

	if (UNiagaraSystem* System = USlimeSkillVfxSubsystem::ResolveLoadedSystem(Row.NiagaraSystem, Owner))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(Owner, System, Owner->GetActorLocation());
	}

	if (Row.AoERadius > 1.f)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeReactionAoE), false, Owner);
		Owner->GetWorld()->OverlapMultiByObjectType(
			Overlaps,
			Owner->GetActorLocation(),
			FQuat::Identity,
			FCollisionObjectQueryParams(ECC_Pawn),
			FCollisionShape::MakeSphere(Row.AoERadius),
			Params);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Other = Overlap.GetActor();
			if (!Other || Other == Owner || Other == Instigator)
			{
				continue;
			}
			if (!USlimeHitProbe::IsHostile(Instigator, Other))
			{
				continue;
			}
			if (USlimeHealthComponent* OtherHealth = Other->FindComponentByClass<USlimeHealthComponent>())
			{
				OtherHealth->ApplyDamage(Row.ExtraDamage * 0.5f, Instigator, Other->GetActorLocation(), FVector::ZeroVector);
			}
			if (Row.Kind == ESlimeReactionKind::MistSpread || Row.Kind == ESlimeReactionKind::Combustion)
			{
				if (USlimeStatusComponent* OtherStatus = Other->FindComponentByClass<USlimeStatusComponent>())
				{
					OtherStatus->ClearReactionResidue();
					OtherStatus->AuraRemaining.Add(
						Row.Kind == ESlimeReactionKind::MistSpread ? ESlimeElement::Water : ESlimeElement::Fire, 6.f);
					OtherStatus->RefreshAuraMarker();
					OtherStatus->SyncOwnerAuraFlash();
				}
			}
		}
	}
}
