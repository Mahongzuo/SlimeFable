#pragma once

#include "CoreMinimal.h"

/** Deterministic shift accounting, shared by runtime and regression tests. */
struct FSlimeArchiveRules
{
	int32 Score = 0;
	int32 Correct = 0;
	int32 Chain = 0;
	int32 BestChain = 0;
	int32 Mistakes = 0;
	int32 Missed = 0;
	int32 Filed[4] = {0, 0, 0, 0};
	float ComboLeft = 0.f;

	int32 Multiplier() const { return FMath::Clamp(1 + (Chain - 1) / 4, 1, 4); }
	void Tick(float Dt)
	{
		ComboLeft = FMath::Max(0.f, ComboLeft - Dt);
		if (ComboLeft <= 0.f) Chain = 0;
	}
	bool Deposit(int32 Category, int32 Cabinet)
	{
		if (Category < 0 || Category >= 4 || Cabinet < 0 || Cabinet >= 4) return false;
		if (Category != Cabinet)
		{
			++Mistakes; Chain = 0; ComboLeft = 0.f;
			return false;
		}
		++Correct; ++Filed[Category]; ++Chain;
		BestChain = FMath::Max(BestChain, Chain);
		ComboLeft = 14.f;
		Score += 10 * Multiplier();
		return true;
	}
	void Miss() { ++Missed; Chain = 0; ComboLeft = 0.f; }
	bool Passed(int32 Target, int32 PerCabinet) const
	{
		if (Correct < Target) return false;
		for (int32 Count : Filed) if (Count < PerCabinet) return false;
		return true;
	}
};
