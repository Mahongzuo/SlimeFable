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
	ElementWheel UMETA(DisplayName = "快捷栏轮盘"),
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
	Morph UMETA(DisplayName = "幻形"),
	Dodge UMETA(DisplayName = "闪避"),
	ShowCursor UMETA(DisplayName = "显示鼠标"),
	Element1 UMETA(DisplayName = "属性1"),
	Element2 UMETA(DisplayName = "属性2"),
	Element3 UMETA(DisplayName = "属性3"),
	Element4 UMETA(DisplayName = "属性4"),
	Element5 UMETA(DisplayName = "属性5"),
	Element6 UMETA(DisplayName = "属性6"),
	ElementFormation UMETA(DisplayName = "属性编队"),
	CheatConsole UMETA(DisplayName = "作弊台"),
	Sprint UMETA(DisplayName = "冲刺"),
	BodySkin UMETA(DisplayName = "史莱姆皮肤"),
	COUNT UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ESlimePlayInputMode : uint8
{
	Auto UMETA(DisplayName = "自动"),
	KeyboardMouse UMETA(DisplayName = "键鼠"),
	Gamepad UMETA(DisplayName = "手柄"),
	Touch UMETA(DisplayName = "触屏")
};

UENUM(BlueprintType)
enum class ESlimeResolvedInputMode : uint8
{
	KeyboardMouse UMETA(DisplayName = "键鼠"),
	Gamepad UMETA(DisplayName = "手柄"),
	Touch UMETA(DisplayName = "触屏")
};

UENUM(BlueprintType)
enum class ESlimeTouchHandedness : uint8
{
	Right UMETA(DisplayName = "右手"),
	Left UMETA(DisplayName = "左手")
};

UENUM(BlueprintType)
enum class ESlimeLastInputDevice : uint8
{
	None,
	KeyboardMouse,
	Gamepad
};
