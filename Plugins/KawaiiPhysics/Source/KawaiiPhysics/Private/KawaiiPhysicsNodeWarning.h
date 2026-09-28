// Copyright 2019-2026 pafuhana1213. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// 警告ログ用：ノードを特定するコンテキスト文字列の生成とログ出力マクロ。
// AnyThreadからUObjectに触れないよう、名前はPreUpdate(GameThread)でキャッシュ済みのものを使う。
// マクロはメンバを非修飾参照するためメンバ関数内でのみ使用可。
#if !UE_BUILD_SHIPPING
static FString BuildKawaiiNodeContextString(
	const FName AnimBPName, const FName ComponentName,
	const FName ActorName, const FName RootBoneName)
{
	return FString::Printf(
		TEXT("AnimBP: %s, Component: %s, Actor: %s, RootBone: %s"),
		*AnimBPName.ToString(), *ComponentName.ToString(),
		*ActorName.ToString(), *RootBoneName.ToString());
}

// ノード特定情報を末尾に付与してWarningを出す（移植性のためFormatの後に最低1つの可変引数が必要）
#define KAWAII_LOG_NODE_WARNING(CategoryName, Format, ...) \
	UE_LOG(CategoryName, Warning, Format TEXT(" (%s)"), __VA_ARGS__, \
		*BuildKawaiiNodeContextString(CachedAnimInstanceClassName, CachedComponentName, \
			CachedOwnerActorName, RootBone.BoneName))

// ノードごと1回だけWarning（GuardBoolはShipping除外メンバ）
#define KAWAII_LOG_NODE_WARNING_ONCE(GuardBool, CategoryName, Format, ...) \
	do { if (!(GuardBool)) { KAWAII_LOG_NODE_WARNING(CategoryName, Format, __VA_ARGS__); (GuardBool) = true; } } while (0)

// 1回ガードのリセット
#define KAWAII_RESET_NODE_WARNING_ONCE(GuardBool) (GuardBool) = false
#else
// Shipping：コンテキストなしの素のログ。GuardBool引数は展開で破棄され、除外メンバを参照しない
#define KAWAII_LOG_NODE_WARNING(CategoryName, Format, ...) \
	UE_LOG(CategoryName, Warning, Format, __VA_ARGS__)
#define KAWAII_LOG_NODE_WARNING_ONCE(GuardBool, CategoryName, Format, ...) \
	UE_LOG(CategoryName, Warning, Format, __VA_ARGS__)
#define KAWAII_RESET_NODE_WARNING_ONCE(GuardBool) ((void)0)
#endif
