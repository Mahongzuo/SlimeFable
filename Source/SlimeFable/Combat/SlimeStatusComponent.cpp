// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeStatusComponent.h"

#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "SlimeCombatTypes.h"
#include "SlimeHealthComponent.h"
#include "SlimeHitProbe.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

USlimeStatusComponent::USlimeStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USlimeStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

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
}

void USlimeStatusComponent::ClearAllAuras()
{
	AuraRemaining.Reset();
}

void USlimeStatusComponent::ApplyAura(ESlimeElement Element, AActor* Instigator, float Duration)
{
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
		return;
	}

	ApplyReactionRow(*Match, Instigator);

	if (Match->bConsumeFirst)
	{
		ClearAura(Match->First);
	}
	if (Match->bConsumeSecond)
	{
		ClearAura(Match->Second);
	}
	if (Incoming != ESlimeElement::Physical && !Match->bConsumeFirst && Incoming == Match->First)
	{
		AuraRemaining.Add(Incoming, 8.f);
	}
	else if (Incoming != ESlimeElement::Physical && !HasAura(Incoming) && Incoming != Match->First && Incoming != Match->Second)
	{
		AuraRemaining.Add(Incoming, 8.f);
	}
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

	if (UNiagaraSystem* System = Row.NiagaraSystem.LoadSynchronous())
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
					OtherStatus->AuraRemaining.Add(Row.Kind == ESlimeReactionKind::MistSpread ? ESlimeElement::Water : ESlimeElement::Fire, 6.f);
				}
			}
		}
	}
}
