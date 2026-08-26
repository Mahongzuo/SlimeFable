// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/SlimeCheatSubsystem.h"

#include "Inventory/SlimeInventorySubsystem.h"
#include "Engine/GameInstance.h"

namespace
{
	constexpr int32 GreedStack = 99;

	FString NormalizeCheat(const FString& Raw)
	{
		return Raw.TrimStartAndEnd().ToLower();
	}
}

bool USlimeCheatSubsystem::ExecuteCommand(const FString& RawCommand, FString& OutMessage)
{
	const FString Cmd = NormalizeCheat(RawCommand);
	if (Cmd.IsEmpty())
	{
		OutMessage.Reset();
		return true;
	}

	UGameInstance* GI = GetGameInstance();
	USlimeInventorySubsystem* Inv = GI ? GI->GetSubsystem<USlimeInventorySubsystem>() : nullptr;

	if (Cmd == TEXT("greedisgood"))
	{
		bGreedActive = !bGreedActive;
		if (Inv)
		{
			if (bGreedActive)
			{
				Inv->AddItem(TEXT("HealJelly"), GreedStack);
				Inv->AddItem(TEXT("PowerCandy"), GreedStack);
			}
			else
			{
				const int32 HealHave = Inv->GetItemCount(TEXT("HealJelly"));
				const int32 CandyHave = Inv->GetItemCount(TEXT("PowerCandy"));
				if (HealHave > 0)
				{
					Inv->RemoveItem(TEXT("HealJelly"), FMath::Min(HealHave, GreedStack));
				}
				if (CandyHave > 0)
				{
					Inv->RemoveItem(TEXT("PowerCandy"), FMath::Min(CandyHave, GreedStack));
				}
			}
		}
		OutMessage = bGreedActive
			? TEXT("作弊开启：物资")
			: TEXT("作弊取消：物资");
		return true;
	}

	if (Cmd == TEXT("whosyourdaddy"))
	{
		bGodMode = !bGodMode;
		OutMessage = bGodMode
			? TEXT("作弊开启：无敌")
			: TEXT("作弊取消：无敌");
		return true;
	}

	if (Cmd == TEXT("killyou"))
	{
		bKillYou = !bKillYou;
		OutMessage = bKillYou
			? TEXT("作弊开启：斩杀")
			: TEXT("作弊取消：斩杀");
		return true;
	}

	OutMessage = TEXT("未知命令");
	return false;
}
