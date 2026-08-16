// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeInputTypes.generated.h"

UENUM(BlueprintType)
enum class ESlimeInputAction : uint8
{
	MoveForward UMETA(DisplayName = "前进"),
	MoveBack UMETA(DisplayName = "后退"),
	MoveLeft UMETA(DisplayName = "左移"),
	MoveRight UMETA(DisplayName = "右移"),
	Jump UMETA(DisplayName = "跳跃"),
	Flatten UMETA(DisplayName = "压扁"),
	Absorb UMETA(DisplayName = "吸收/召回"),
	ResetBody UMETA(DisplayName = "重置身体"),
	Launch UMETA(DisplayName = "发射"),
	ElementWheel UMETA(DisplayName = "元素轮盘"),
	Attack UMETA(DisplayName = "攻击"),
	Skill1 UMETA(DisplayName = "技能1"),
	Skill2 UMETA(DisplayName = "技能2"),
	Skill3 UMETA(DisplayName = "技能3"),
	LockOn UMETA(DisplayName = "锁定"),
	Inventory UMETA(DisplayName = "背包"),
	QuestLog UMETA(DisplayName = "史书"),
	Interact UMETA(DisplayName = "拾取/交互"),
	Hotbar1 UMETA(DisplayName = "消耗品快捷1"),
	Hotbar2 UMETA(DisplayName = "消耗品快捷2"),
	Hotbar3 UMETA(DisplayName = "消耗品快捷3"),
	Hotbar4 UMETA(DisplayName = "放置品快捷1"),
	Hotbar5 UMETA(DisplayName = "放置品快捷2"),
	Hotbar6 UMETA(DisplayName = "放置品快捷3"),
	Dodge UMETA(DisplayName = "闪避"),
	ShowCursor UMETA(DisplayName = "显示鼠标"),
	COUNT UMETA(Hidden)
};
